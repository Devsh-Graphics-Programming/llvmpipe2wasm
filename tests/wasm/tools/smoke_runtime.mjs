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

const runtimeStoreOffsetBytes = 0 >>> 0;
const runtimeDefaultKeyLo = 0x12345678 >>> 0;
const runtimeDefaultKeyHi = 0 >>> 0;

async function compileRuntimeSpirv(storeValue) {
  const storeConst = `0x${(storeValue >>> 0).toString(16)}u`;
  const dxcWasmJs = process.env.WEBVULKAN_DXC_WASM_JS || "";
  if (dxcWasmJs) {
    const hlslSource = `
RWByteAddressBuffer OutBuf : register(u0);
[numthreads(1, 1, 1)]
void write_const(uint3 tid : SV_DispatchThreadID) {
  OutBuf.Store(0, ${storeConst});
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
__kernel void write_const(__global uint* out) {
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

async function compileRuntimeLlvmirToWasm(storeValue) {
  const wasmerBin = process.env.WEBVULKAN_WASMER_BIN;
  if (!wasmerBin) {
    throw new Error("WEBVULKAN_WASMER_BIN is required for LLVM IR -> Wasm smoke path");
  }

  const storeConst = `${storeValue | 0}`;
  const clangPackage = process.env.WEBVULKAN_CLANG_WASM_PACKAGE || "clang/clang";
  const llvmIr = `
; ModuleID = 'webvulkan_runtime_store_kernel'
source_filename = "webvulkan_runtime_store_kernel"
target triple = "wasm32-unknown-unknown"

define void @__wasm_signal() {
entry:
  ret void
}

define void @run(i32 %dst, i32 %offset, i32 %value) {
entry:
  %addr = add i32 %dst, %offset
  %ptr = inttoptr i32 %addr to ptr
  store i32 ${storeConst}, ptr %ptr, align 4
  ret void
}
`;

  const compileResult = await runProcess(wasmerBin, [
    "run",
    "--quiet",
    clangPackage,
    "--",
    "--target=wasm32-unknown-unknown",
    "-x",
    "ir",
    "-",
    "-nostdlib",
    "-Wl,--no-entry",
    "-Wl,--export=__wasm_signal",
    "-Wl,--export=run",
    "-o",
    "-"
  ], { stdin: llvmIr });

  if (compileResult.code !== 0) {
    const reason = firstLine(compileResult.stderr) || `exit_code=${compileResult.code}`;
    throw new Error(`failed to compile LLVM IR -> Wasm: ${reason}`);
  }

  const wasmMagic = Buffer.from([0x00, 0x61, 0x73, 0x6d]);
  if (compileResult.stdout.length < 8 || !compileResult.stdout.subarray(0, 4).equals(wasmMagic)) {
    throw new Error("clang-in-wasm did not produce valid Wasm from LLVM IR");
  }
  if (!WebAssembly.validate(compileResult.stdout)) {
    throw new Error("LLVM IR -> Wasm output failed WebAssembly.validate");
  }

  return {
    provider: `${clangPackage} llvm-ir`,
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

if (!requireRuntimeSpirv) {
  invokeSmokeOnce();
} else {
  const runtimeShaderValues = [0x12345678 >>> 0, 0x89abcdef >>> 0];
  for (let i = 0; i < runtimeShaderValues.length; ++i) {
    const shaderValue = runtimeShaderValues[i];
    const spirv = await compileRuntimeSpirv(shaderValue);
    const llvmIrWasm = await compileRuntimeLlvmirToWasm(shaderValue);

    runtime.ccall("webvulkan_reset_runtime_shader_registry", null, [], []);
    runtime.ccall("webvulkan_runtime_reset_captured_shader_key", null, [], []);

    const setDefaultKeyRc = runtime.ccall(
      "webvulkan_set_runtime_active_shader_key",
      "number",
      ["number", "number"],
      [runtimeDefaultKeyLo, runtimeDefaultKeyHi]
    );
    if (setDefaultKeyRc !== 0) {
      throw new Error(`webvulkan_set_runtime_active_shader_key failed with rc=${setDefaultKeyRc}`);
    }

    const setDefaultSpirvRc = runtime.ccall(
      "webvulkan_set_runtime_shader_spirv",
      "number",
      ["array", "number"],
      [spirv.bytes, spirv.bytes.length]
    );
    if (setDefaultSpirvRc !== 0) {
      throw new Error(`webvulkan_set_runtime_shader_spirv failed with rc=${setDefaultSpirvRc}`);
    }

    const setDefaultExpectedRc = runtime.ccall(
      "webvulkan_set_runtime_expected_dispatch_value",
      "number",
      ["number", "number", "number"],
      [runtimeDefaultKeyLo, runtimeDefaultKeyHi, shaderValue]
    );
    if (setDefaultExpectedRc !== 0) {
      throw new Error(`webvulkan_set_runtime_expected_dispatch_value failed with rc=${setDefaultExpectedRc}`);
    }

    console.log("runtime shader compile ok");
    console.log(`  shader.value=0x${shaderValue.toString(16).padStart(8, "0")}`);
    console.log(`  spirv.provider=${spirv.provider}`);
    console.log(`  spirv.bytes=${spirv.bytes.length}`);
    console.log(`  spirv.entrypoint=${spirv.entrypoint}`);
    console.log(`  llvmir_wasm.provider=${llvmIrWasm.provider}`);
    console.log(`  llvmir_wasm.entrypoint=${llvmIrWasm.entrypoint}`);
    console.log(`  llvmir_wasm.bytes=${llvmIrWasm.bytes.length}`);

    console.log(`runtime smoke discover_key run=${i + 1}/${runtimeShaderValues.length}`);
    invokeSmokeOnce();

    const hasCapturedKey = runtime.ccall("webvulkan_runtime_has_captured_shader_key", "number", [], []) !== 0;
    if (!hasCapturedKey) {
      throw new Error("driver did not report runtime shader key");
    }

    const capturedKeyLo = runtime.ccall("webvulkan_runtime_get_captured_shader_key_lo", "number", [], []) >>> 0;
    const capturedKeyHi = runtime.ccall("webvulkan_runtime_get_captured_shader_key_hi", "number", [], []) >>> 0;
    const registerCapturedSpirvRc = runtime.ccall(
      "webvulkan_register_runtime_shader_spirv",
      "number",
      ["number", "number", "array", "number", "string"],
      [capturedKeyLo, capturedKeyHi, spirv.bytes, spirv.bytes.length, spirv.entrypoint]
    );
    if (registerCapturedSpirvRc !== 0) {
      throw new Error(`webvulkan_register_runtime_shader_spirv failed with rc=${registerCapturedSpirvRc}`);
    }

    const registerWasmRc = runtime.ccall(
      "webvulkan_register_runtime_wasm_module",
      "number",
      ["number", "number", "array", "number", "string", "string"],
      [
        capturedKeyLo,
        capturedKeyHi,
        llvmIrWasm.bytes,
        llvmIrWasm.bytes.length,
        llvmIrWasm.entrypoint,
        llvmIrWasm.provider
      ]
    );
    if (registerWasmRc !== 0) {
      throw new Error(`webvulkan_register_runtime_wasm_module failed with rc=${registerWasmRc}`);
    }

    const setCapturedExpectedRc = runtime.ccall(
      "webvulkan_set_runtime_expected_dispatch_value",
      "number",
      ["number", "number", "number"],
      [capturedKeyLo, capturedKeyHi, shaderValue]
    );
    if (setCapturedExpectedRc !== 0) {
      throw new Error(`webvulkan_set_runtime_expected_dispatch_value failed with rc=${setCapturedExpectedRc}`);
    }

    const setCapturedActiveKeyRc = runtime.ccall(
      "webvulkan_set_runtime_active_shader_key",
      "number",
      ["number", "number"],
      [capturedKeyLo, capturedKeyHi]
    );
    if (setCapturedActiveKeyRc !== 0) {
      throw new Error(`webvulkan_set_runtime_active_shader_key failed with rc=${setCapturedActiveKeyRc}`);
    }

    console.log(`runtime shader key captured=0x${capturedKeyHi.toString(16).padStart(8, "0")}${capturedKeyLo.toString(16).padStart(8, "0")}`);
    console.log(`runtime smoke verify_fast_path run=${i + 1}/${runtimeShaderValues.length}`);
    invokeSmokeOnce();

    const provider = runtime.ccall("webvulkan_get_runtime_wasm_provider", "string", [], []) || "none";
    if (provider === "inline-wasm-module") {
      throw new Error("fast path verification failed: provider is inline-wasm-module");
    }
  }
}

if (requireRuntimeSpirv) {
  const llvmIrWasmUsed = runtime.ccall("webvulkan_get_runtime_wasm_used", "number", [], []) !== 0;
  const llvmIrWasmProvider = runtime.ccall("webvulkan_get_runtime_wasm_provider", "string", [], []) || "none";
  console.log(`proof.codegen=${llvmIrWasmUsed ? "llvm_ir_to_wasm" : "unknown"}`);
  console.log(`proof.execute_path=${llvmIrWasmUsed ? "fast_wasm" : "fallback"}`);
  console.log(`proof.interpreter=${llvmIrWasmUsed ? "disabled_for_dispatch" : "unknown"}`);
  console.log(`proof.llvm_ir_wasm_provider=${llvmIrWasmProvider}`);
  if (!llvmIrWasmUsed) {
    throw new Error("LLVM IR -> Wasm fast path was not used by llvmpipe dispatch");
  }
}

console.log("runtime smoke passed");
