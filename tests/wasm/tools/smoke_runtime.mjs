import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";

function runProcess(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      stdio: ["pipe", "pipe", "pipe"],
      cwd: options.cwd ?? process.cwd()
    });
    const stdoutChunks = [];
    const stderrChunks = [];

    child.stdout.on("data", (chunk) => stdoutChunks.push(Buffer.from(chunk)));
    child.stderr.on("data", (chunk) => stderrChunks.push(Buffer.from(chunk)));
    child.on("error", reject);
    child.on("close", (code) => {
      resolve({
        code: code ?? -1,
        stdout: Buffer.concat(stdoutChunks),
        stderr: Buffer.concat(stderrChunks).toString("utf8")
      });
    });

    if (options.stdin !== undefined) {
      child.stdin.end(options.stdin);
    } else {
      child.stdin.end();
    }
  });
}

function firstLine(value) {
  const text = (value || "").trim();
  if (!text) {
    return "";
  }
  return text.split("\n", 1)[0];
}

const runtimeDefaultKeyLo = 0x12345678 >>> 0;
const runtimeDefaultKeyHi = 0 >>> 0;

function runtimeShaderThreadgroupSizeX(workloadName) {
  return workloadName === "write_const" ? 1 : 64;
}

async function compileRuntimeSpirv(storeValue, workloadName) {
  const storeConst = `0x${(storeValue >>> 0).toString(16)}u`;
  const threadgroupSizeX = runtimeShaderThreadgroupSizeX(workloadName);
  const dxcWasmJs = process.env.WEBVULKAN_DXC_WASM_JS || "";
  if (dxcWasmJs) {
    const hlslSource = `
RWStructuredBuffer<uint> OutBuf : register(u0);

uint webvulkan_mix_const(uint v, uint salt) {
  v ^= (0x9e3779b9u + salt * 0x7f4a7c15u);
  v = (v << 5u) | (v >> 27u);
  v = v * 1664525u + 1013904223u;
  return v;
}

uint webvulkan_compile_time_chain() {
  uint v = 0x91e10da5u;
  [unroll]
  for (uint i = 0u; i < 16u; ++i) {
    v = webvulkan_mix_const(v, i);
  }
  return v;
}

[numthreads(${threadgroupSizeX}, 1, 1)]
void write_const(uint3 tid : SV_DispatchThreadID) {
  uint folded = webvulkan_compile_time_chain();
  if (folded == 0xdeadbeefu) {
    OutBuf[1] = folded;
  }
  OutBuf[0] = ${storeConst};
}
`;

    const scratchDir = await mkdtemp(join(tmpdir(), "webvulkan-dxc-wasm-"));
    const inputFile = "runtime_smoke.hlsl";
    const outputFile = "runtime_smoke.spv";
    const inputPath = join(scratchDir, inputFile);
    const outputPath = join(scratchDir, outputFile);
    await writeFile(inputPath, hlslSource, "utf8");

    const compileArgs = [
      dxcWasmJs,
      "-spirv",
      "-T",
      "cs_6_0",
      "-E",
      "write_const",
      "-Fo",
      outputFile,
      inputFile
    ];

    const compileResult = await runProcess(process.execPath, compileArgs, { cwd: scratchDir });
    if (compileResult.code !== 0) {
      const reason = firstLine(compileResult.stderr) || `exit_code=${compileResult.code}`;
      throw new Error(`failed to compile HLSL with dxc-wasm: ${reason}`);
    }

    const bytes = await readFile(outputPath);
    await rm(scratchDir, { recursive: true, force: true });

    const spirvMagic = Buffer.from([0x03, 0x02, 0x23, 0x07]);
    if (bytes.length < 4 || !bytes.subarray(0, 4).equals(spirvMagic)) {
      throw new Error("dxc-wasm did not produce valid SPIR-V");
    }

    return {
      provider: `dxc-wasm:${dxcWasmJs}`,
      bytes,
      entrypoint: "write_const"
    };
  }

  const wasmerBin = process.env.WEBVULKAN_WASMER_BIN;
  if (!wasmerBin) {
    throw new Error("WEBVULKAN_WASMER_BIN is required when SMOKE_REQUIRE_RUNTIME_SPIRV=1");
  }

  const clangPackage = process.env.WEBVULKAN_CLANG_WASM_PACKAGE || "clang/clang";
  const spirvPackage = process.env.WEBVULKAN_SPIRV_WASM_PACKAGE || clangPackage;
  const spirvEntrypoint = process.env.WEBVULKAN_SPIRV_WASM_ENTRYPOINT || "";
  const source = `
uint webvulkan_mix_const(uint v, uint salt) {
  v ^= (0x9e3779b9u + salt * 0x7f4a7c15u);
  v = rotate(v, (uint)5);
  v = v * 1664525u + 1013904223u;
  return v;
}

uint webvulkan_compile_time_chain() {
  uint v = 0x91e10da5u;
  for (uint i = 0u; i < 16u; ++i) {
    v = webvulkan_mix_const(v, i);
  }
  return v;
}

__attribute__((reqd_work_group_size(${threadgroupSizeX}, 1, 1)))
__kernel void write_const(__global uint* out) {
  uint folded = webvulkan_compile_time_chain();
  if (folded == (uint)0xdeadbeefu) {
    out[1] = folded;
  }
  out[0] = ${storeConst};
}
`;

  const attempts = [];
  if (spirvPackage && spirvEntrypoint) {
    attempts.push({
      provider: `${spirvPackage}#${spirvEntrypoint}`,
      args: [
        "run",
        "--quiet",
        spirvPackage,
        "-e",
        spirvEntrypoint,
        "--",
        "-",
        "-o",
        "-"
      ]
    });
  }

  attempts.push({
    provider: `${clangPackage} --target=spirv32`,
    args: [
      "run",
      "--quiet",
      clangPackage,
      "--",
      "--target=spirv32",
      "-x",
      "cl",
      "-cl-std=CL2.0",
      "-c",
      "-",
      "-o",
      "-"
    ]
  });

  const spirvMagic = Buffer.from([0x03, 0x02, 0x23, 0x07]);
  const failureReasons = [];

  for (const attempt of attempts) {
    const result = await runProcess(wasmerBin, attempt.args, { stdin: source });
    if (result.code === 0 && result.stdout.length >= 4 && result.stdout.subarray(0, 4).equals(spirvMagic)) {
      return {
        provider: attempt.provider,
        bytes: result.stdout,
        entrypoint: "write_const"
      };
    }

    const reason = firstLine(result.stderr) || `exit_code=${result.code}`;
    failureReasons.push(`${attempt.provider}: ${reason}`);
  }

  throw new Error(`failed to compile SPIR-V for runtime smoke: ${failureReasons.join(" | ")}`);
}

async function compileRuntimeLlvmirToWasm() {
  const wasmerBin = process.env.WEBVULKAN_WASMER_BIN;
  if (!wasmerBin) {
    throw new Error("WEBVULKAN_WASMER_BIN is required for runtime Wasm module smoke path");
  }

  const clangPackage = process.env.WEBVULKAN_CLANG_WASM_PACKAGE || "clang/clang";
  const runtimeCSource = `
typedef unsigned int u32;

static void store_u32(u32 address, u32 value) {
  *((u32*)(unsigned long)address) = value;
}

void __wasm_signal(void) {
}

void run(u32 dst, u32 offset, u32 value, u32 workload, u32 invocations, u32 workgroups) {
  if (workload == 1u) {
    store_u32(dst, invocations);
    return;
  }
  if (workload == 2u) {
    store_u32(dst, workgroups);
    return;
  }
  if (workload == 3u) {
    store_u32(dst, invocations);
    u32 base = dst + 4u;
    for (u32 i = 0u; i < invocations; ++i) {
      store_u32(base + (i * 4u), i + 1u);
    }
    return;
  }
  store_u32(dst + offset, value);
}
`;

  const compileResult = await runProcess(wasmerBin, [
    "run",
    "--quiet",
    clangPackage,
    "--",
    "--target=wasm32-unknown-unknown",
    "-O2",
    "-x",
    "c",
    "-",
    "-nostdlib",
    "-Wl,--no-entry",
    "-Wl,--export=__wasm_signal",
    "-Wl,--export=run",
    "-o",
    "-"
  ], { stdin: runtimeCSource });

  if (compileResult.code !== 0) {
    const reason = firstLine(compileResult.stderr) || `exit_code=${compileResult.code}`;
    throw new Error(`failed to compile runtime C -> Wasm: ${reason}`);
  }

  const wasmMagic = Buffer.from([0x00, 0x61, 0x73, 0x6d]);
  if (compileResult.stdout.length < 8 || !compileResult.stdout.subarray(0, 4).equals(wasmMagic)) {
    throw new Error("clang-in-wasm did not produce valid Wasm runtime module");
  }
  if (!WebAssembly.validate(compileResult.stdout)) {
    throw new Error("runtime C -> Wasm output failed WebAssembly.validate");
  }

  return {
    provider: `${clangPackage} c-runtime`,
    entrypoint: "run",
    bytes: compileResult.stdout
  };
}

const modulePath = process.env.SMOKE_MODULE;
if (!modulePath) {
  throw new Error("SMOKE_MODULE is not set");
}
const exportName = process.env.SMOKE_EXPORT || "_wasm_runtime_smoke";
const requireRuntimeSpirv = process.env.SMOKE_REQUIRE_RUNTIME_SPIRV === "1";
const runtimeExecutionMode = process.env.WEBVULKAN_RUNTIME_EXECUTION_MODE || "fast_wasm";
const runtimeBenchIterations = Number.parseInt(process.env.WEBVULKAN_RUNTIME_BENCH_ITERATIONS || "8", 10);
const runtimeWarmupIterations = Number.parseInt(process.env.WEBVULKAN_RUNTIME_WARMUP_ITERATIONS || "2", 10);
const runtimeBenchProfile = process.env.WEBVULKAN_RUNTIME_BENCH_PROFILE || "micro";
const runtimeBenchProfileMap = new Map([
  ["micro", 0],
  ["realistic", 1],
  ["hot_loop_single_dispatch", 2]
]);
const runtimeShaderWorkload = process.env.WEBVULKAN_RUNTIME_SHADER_WORKLOAD || "write_const";
const runtimeShaderWorkloadMap = new Map([
  ["write_const", 0],
  ["atomic_single_counter", 1],
  ["atomic_per_workgroup", 2],
  ["no_race_unique_writes", 3]
]);
const runtimeBenchProfileValue = runtimeBenchProfileMap.get(runtimeBenchProfile);
const runtimeShaderWorkloadValue = runtimeShaderWorkloadMap.get(runtimeShaderWorkload);

if (!Number.isInteger(runtimeBenchIterations) || runtimeBenchIterations <= 0) {
  throw new Error(`WEBVULKAN_RUNTIME_BENCH_ITERATIONS must be a positive integer, got ${runtimeBenchIterations}`);
}
if (!Number.isInteger(runtimeWarmupIterations) || runtimeWarmupIterations < 0) {
  throw new Error(`WEBVULKAN_RUNTIME_WARMUP_ITERATIONS must be a non-negative integer, got ${runtimeWarmupIterations}`);
}
if (runtimeBenchProfileValue === undefined) {
  throw new Error(`Unsupported WEBVULKAN_RUNTIME_BENCH_PROFILE='${runtimeBenchProfile}'`);
}
if (runtimeShaderWorkloadValue === undefined) {
  throw new Error(`Unsupported WEBVULKAN_RUNTIME_SHADER_WORKLOAD='${runtimeShaderWorkload}'`);
}
if (runtimeExecutionMode !== "fast_wasm" && runtimeExecutionMode !== "raw_llvm_ir") {
  throw new Error(`Unsupported WEBVULKAN_RUNTIME_EXECUTION_MODE='${runtimeExecutionMode}'`);
}

const moduleUrl = pathToFileURL(modulePath).href;
const imported = await import(moduleUrl);
const factory = imported.default;

if (typeof factory !== "function") {
  throw new Error("Expected default export factory from Emscripten module");
}

const runtime = await factory({
  print: (text) => process.stdout.write(String(text) + "\n"),
  printErr: (text) => process.stderr.write(String(text) + "\n"),
  onAbort: (what) => {
    process.stderr.write(`wasm abort: ${String(what)}\n`);
    process.stderr.write(`${new Error("wasm abort stack").stack}\n`);
  },
  quit_: (status, toThrow) => {
    process.stderr.write(`wasm quit status=${status}\n`);
    process.stderr.write(`${new Error("wasm quit stack").stack}\n`);
    throw toThrow;
  }
});

if (requireRuntimeSpirv) {
  if (typeof runtime.ccall !== "function") {
    throw new Error("Expected runtime method ccall for runtime shader registry");
  }
}

const smokeFn = runtime[exportName];
if (typeof smokeFn !== "function") {
  throw new Error(`Export ${exportName} not found`);
}

function invokeSmokeOnce() {
  let rc = 0;
  try {
    rc = smokeFn();
  } catch (error) {
    process.stderr.write(`smoke invocation threw: ${String(error)}\n`);
    if (error && typeof error === "object") {
      try {
        const ownKeys = Object.getOwnPropertyNames(error);
        process.stderr.write(`smoke invocation error keys: ${ownKeys.join(",")}\n`);
        for (const key of ownKeys) {
          process.stderr.write(`  ${key}=${String(error[key])}\n`);
        }
      } catch {
      }
    }
    if (error && error.stack) {
      process.stderr.write(`${error.stack}\n`);
    }
    throw error;
  }
  if (rc !== 0) {
    throw new Error(`Runtime smoke failed with rc=${rc}`);
  }
}

function invokeSmokeOnceWithTimingMs() {
  invokeSmokeOnce();
  const wallMs = runtime.ccall("webvulkan_get_last_dispatch_ms", "number", [], []);
  if (!Number.isFinite(wallMs) || wallMs < 0) {
    throw new Error(`Invalid dispatch wall time returned from smoke: ${wallMs}`);
  }
  return wallMs;
}

function summarizeDispatchTimings(mode, profile, samples) {
  if (!samples.length) {
    throw new Error(`No dispatch timing samples for mode=${mode}`);
  }
  let minMs = samples[0];
  let maxMs = samples[0];
  let sumMs = 0.0;
  for (const sample of samples) {
    if (sample < minMs) {
      minMs = sample;
    }
    if (sample > maxMs) {
      maxMs = sample;
    }
    sumMs += sample;
  }
  const avgMs = sumMs / samples.length;
  console.log("dispatch timing summary");
  console.log(`  mode=${mode}`);
  console.log(`  profile=${profile}`);
  console.log(`  samples=${samples.length}`);
  console.log(`  min_ms=${minMs.toFixed(6)}`);
  console.log(`  avg_ms=${avgMs.toFixed(6)}`);
  console.log(`  max_ms=${maxMs.toFixed(6)}`);
}

function setRuntimeDispatchMode(modeValue) {
  const setModeRc = runtime.ccall(
    "webvulkan_set_runtime_dispatch_mode",
    "number",
    ["number"],
    [modeValue]
  );
  if (setModeRc !== 0) {
    throw new Error(`webvulkan_set_runtime_dispatch_mode failed with rc=${setModeRc}`);
  }
}

function setRuntimeBenchProfile(profileValue) {
  const setProfileRc = runtime.ccall(
    "webvulkan_set_runtime_bench_profile",
    "number",
    ["number"],
    [profileValue]
  );
  if (setProfileRc !== 0) {
    throw new Error(`webvulkan_set_runtime_bench_profile failed with rc=${setProfileRc}`);
  }
}

function setRuntimeShaderWorkload(workloadValue) {
  const setWorkloadRc = runtime.ccall(
    "webvulkan_set_runtime_shader_workload",
    "number",
    ["number"],
    [workloadValue]
  );
  if (setWorkloadRc !== 0) {
    throw new Error(`webvulkan_set_runtime_shader_workload failed with rc=${setWorkloadRc}`);
  }
}

function setActiveShaderKey(keyLo, keyHi) {
  const setKeyRc = runtime.ccall(
    "webvulkan_set_runtime_active_shader_key",
    "number",
    ["number", "number"],
    [keyLo, keyHi]
  );
  if (setKeyRc !== 0) {
    throw new Error(`webvulkan_set_runtime_active_shader_key failed with rc=${setKeyRc}`);
  }
}

function registerExpectedDispatchValue(keyLo, keyHi, shaderValue) {
  const setExpectedRc = runtime.ccall(
    "webvulkan_set_runtime_expected_dispatch_value",
    "number",
    ["number", "number", "number"],
    [keyLo, keyHi, shaderValue]
  );
  if (setExpectedRc !== 0) {
    throw new Error(`webvulkan_set_runtime_expected_dispatch_value failed with rc=${setExpectedRc}`);
  }
}

function registerSpirvForKey(keyLo, keyHi, spirv) {
  const setSpirvRc = runtime.ccall(
    "webvulkan_register_runtime_shader_spirv",
    "number",
    ["number", "number", "array", "number", "string"],
    [keyLo, keyHi, spirv.bytes, spirv.bytes.length, spirv.entrypoint]
  );
  if (setSpirvRc !== 0) {
    throw new Error(`webvulkan_register_runtime_shader_spirv failed with rc=${setSpirvRc}`);
  }
}

async function runFastWasmSmoke(shaderValue) {
  const spirv = await compileRuntimeSpirv(shaderValue, runtimeShaderWorkload);
  const runtimeWasm = await compileRuntimeLlvmirToWasm();
  setRuntimeBenchProfile(runtimeBenchProfileValue);
  setRuntimeShaderWorkload(runtimeShaderWorkloadValue);
  runtime.ccall("webvulkan_reset_runtime_shader_registry", null, [], []);
  runtime.ccall("webvulkan_runtime_reset_captured_shader_key", null, [], []);
  setRuntimeDispatchMode(1);
  setActiveShaderKey(runtimeDefaultKeyLo, runtimeDefaultKeyHi);
  registerSpirvForKey(runtimeDefaultKeyLo, runtimeDefaultKeyHi, spirv);
  registerExpectedDispatchValue(runtimeDefaultKeyLo, runtimeDefaultKeyHi, shaderValue);

  console.log("runtime shader compile ok");
  console.log(`  mode=fast_wasm`);
  console.log(`  profile=${runtimeBenchProfile}`);
  console.log(`  shader.workload=${runtimeShaderWorkload}`);
  console.log(`  shader.value=0x${shaderValue.toString(16).padStart(8, "0")}`);
  console.log(`  spirv.provider=${spirv.provider}`);
  console.log(`  spirv.bytes=${spirv.bytes.length}`);
  console.log(`  spirv.entrypoint=${spirv.entrypoint}`);
  console.log(`  runtime_wasm.provider=${runtimeWasm.provider}`);
  console.log(`  runtime_wasm.entrypoint=${runtimeWasm.entrypoint}`);
  console.log(`  runtime_wasm.bytes=${runtimeWasm.bytes.length}`);

  console.log("runtime smoke discover_key");
  invokeSmokeOnce();

  const hasCapturedKey = runtime.ccall("webvulkan_runtime_has_captured_shader_key", "number", [], []) !== 0;
  if (!hasCapturedKey) {
    throw new Error("driver did not report runtime shader key");
  }

  const capturedKeyLo = runtime.ccall("webvulkan_runtime_get_captured_shader_key_lo", "number", [], []) >>> 0;
  const capturedKeyHi = runtime.ccall("webvulkan_runtime_get_captured_shader_key_hi", "number", [], []) >>> 0;
  registerSpirvForKey(capturedKeyLo, capturedKeyHi, spirv);
  const registerWasmRc = runtime.ccall(
    "webvulkan_register_runtime_wasm_module",
    "number",
    ["number", "number", "array", "number", "string", "string"],
    [
      capturedKeyLo,
      capturedKeyHi,
      runtimeWasm.bytes,
      runtimeWasm.bytes.length,
      runtimeWasm.entrypoint,
      runtimeWasm.provider
    ]
  );
  if (registerWasmRc !== 0) {
    throw new Error(`webvulkan_register_runtime_wasm_module failed with rc=${registerWasmRc}`);
  }
  registerExpectedDispatchValue(capturedKeyLo, capturedKeyHi, shaderValue);
  setActiveShaderKey(capturedKeyLo, capturedKeyHi);
  console.log(`runtime shader key captured=0x${capturedKeyHi.toString(16).padStart(8, "0")}${capturedKeyLo.toString(16).padStart(8, "0")}`);

  for (let i = 0; i < runtimeWarmupIterations; ++i) {
    console.log(`runtime smoke warmup mode=fast_wasm run=${i + 1}/${runtimeWarmupIterations}`);
    invokeSmokeOnce();
  }

  const samplesMs = [];
  for (let i = 0; i < runtimeBenchIterations; ++i) {
    console.log(`runtime smoke benchmark mode=fast_wasm run=${i + 1}/${runtimeBenchIterations}`);
    samplesMs.push(invokeSmokeOnceWithTimingMs());
  }

  const provider = runtime.ccall("webvulkan_get_runtime_wasm_provider", "string", [], []) || "none";
  const wasmUsed = runtime.ccall("webvulkan_get_runtime_wasm_used", "number", [], []) !== 0;
  if (!wasmUsed) {
    throw new Error("fast_wasm mode failed: runtime Wasm path was not used");
  }
  if (provider === "inline-wasm-module") {
    throw new Error("fast_wasm mode failed: provider is inline-wasm-module");
  }

  summarizeDispatchTimings("fast_wasm", runtimeBenchProfile, samplesMs);
  console.log("proof.execute_path=fast_wasm");
  console.log("proof.interpreter=disabled_for_dispatch");
  console.log(`proof.llvm_ir_wasm_provider=${provider}`);
}

async function runRawLlvmIrSmoke(shaderValue) {
  const spirv = await compileRuntimeSpirv(shaderValue, runtimeShaderWorkload);
  setRuntimeBenchProfile(runtimeBenchProfileValue);
  setRuntimeShaderWorkload(runtimeShaderWorkloadValue);
  runtime.ccall("webvulkan_reset_runtime_shader_registry", null, [], []);
  runtime.ccall("webvulkan_runtime_reset_captured_shader_key", null, [], []);
  setRuntimeDispatchMode(0);
  setActiveShaderKey(runtimeDefaultKeyLo, runtimeDefaultKeyHi);
  registerSpirvForKey(runtimeDefaultKeyLo, runtimeDefaultKeyHi, spirv);
  registerExpectedDispatchValue(runtimeDefaultKeyLo, runtimeDefaultKeyHi, shaderValue);

  console.log("runtime shader compile ok");
  console.log(`  mode=raw_llvm_ir`);
  console.log(`  profile=${runtimeBenchProfile}`);
  console.log(`  shader.workload=${runtimeShaderWorkload}`);
  console.log(`  shader.value=0x${shaderValue.toString(16).padStart(8, "0")}`);
  console.log(`  spirv.provider=${spirv.provider}`);
  console.log(`  spirv.bytes=${spirv.bytes.length}`);
  console.log(`  spirv.entrypoint=${spirv.entrypoint}`);

  console.log("runtime smoke discover_key");
  invokeSmokeOnce();

  const hasCapturedKey = runtime.ccall("webvulkan_runtime_has_captured_shader_key", "number", [], []) !== 0;
  if (!hasCapturedKey) {
    throw new Error("driver did not report runtime shader key");
  }

  const capturedKeyLo = runtime.ccall("webvulkan_runtime_get_captured_shader_key_lo", "number", [], []) >>> 0;
  const capturedKeyHi = runtime.ccall("webvulkan_runtime_get_captured_shader_key_hi", "number", [], []) >>> 0;
  registerSpirvForKey(capturedKeyLo, capturedKeyHi, spirv);
  registerExpectedDispatchValue(capturedKeyLo, capturedKeyHi, shaderValue);
  setActiveShaderKey(capturedKeyLo, capturedKeyHi);
  console.log(`runtime shader key captured=0x${capturedKeyHi.toString(16).padStart(8, "0")}${capturedKeyLo.toString(16).padStart(8, "0")}`);

  for (let i = 0; i < runtimeWarmupIterations; ++i) {
    console.log(`runtime smoke warmup mode=raw_llvm_ir run=${i + 1}/${runtimeWarmupIterations}`);
    invokeSmokeOnce();
  }

  const samplesMs = [];
  for (let i = 0; i < runtimeBenchIterations; ++i) {
    console.log(`runtime smoke benchmark mode=raw_llvm_ir run=${i + 1}/${runtimeBenchIterations}`);
    samplesMs.push(invokeSmokeOnceWithTimingMs());
  }

  const provider = runtime.ccall("webvulkan_get_runtime_wasm_provider", "string", [], []) || "none";
  const wasmUsed = runtime.ccall("webvulkan_get_runtime_wasm_used", "number", [], []) !== 0;
  if (wasmUsed) {
    throw new Error(`raw_llvm_ir mode failed: runtime Wasm path should be disabled, provider=${provider}`);
  }

  summarizeDispatchTimings("raw_llvm_ir", runtimeBenchProfile, samplesMs);
  console.log("proof.execute_path=raw_llvm_ir");
  console.log(`proof.fast_wasm_provider=${provider}`);
}

if (!requireRuntimeSpirv) {
  invokeSmokeOnce();
} else {
  const runtimeShaderValue = 0x12345678 >>> 0;
  if (runtimeExecutionMode === "fast_wasm") {
    await runFastWasmSmoke(runtimeShaderValue);
  } else {
    await runRawLlvmIrSmoke(runtimeShaderValue);
  }
}

console.log("runtime smoke passed");
