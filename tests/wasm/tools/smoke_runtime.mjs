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

async function compileRuntimeSpirv() {
  const dxcWasmJs = process.env.WEBVULKAN_DXC_WASM_JS || "";
  if (dxcWasmJs) {
    const hlslSource = `
RWByteAddressBuffer OutBuf : register(u0);
[numthreads(1, 1, 1)]
void write_const(uint3 tid : SV_DispatchThreadID) {
  OutBuf.Store(0, 0x12345678u);
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
      bytes
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
  out[0] = 0x12345678u;
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
        bytes: result.stdout
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
    throw new Error("WEBVULKAN_WASMER_BIN is required for LLVM IR -> Wasm smoke path");
  }

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
  store i32 %value, ptr %ptr, align 4
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
    throw new Error("Expected runtime method ccall for runtime SPIR-V injection");
  }

  const spirv = await compileRuntimeSpirv();
  const setRc = runtime.ccall(
    "webvulkan_set_runtime_shader_spirv",
    "number",
    ["array", "number"],
    [spirv.bytes, spirv.bytes.length]
  );

  if (setRc !== 0) {
    throw new Error(`_webvulkan_set_runtime_shader_spirv failed with rc=${setRc}`);
  }

  console.log("runtime shader injection ok");
  console.log(`  spirv.provider=${spirv.provider}`);
  console.log(`  spirv.bytes=${spirv.bytes.length}`);

  const llvmIrWasm = await compileRuntimeLlvmirToWasm();
  globalThis.__webvulkan_llvm_ir_store_wasm = {
    bytes: new Uint8Array(llvmIrWasm.bytes),
    provider: llvmIrWasm.provider,
    entrypoint: llvmIrWasm.entrypoint
  };
  globalThis.__webvulkan_llvm_ir_store_wasm_used = false;
  globalThis.__webvulkan_llvm_ir_store_wasm_provider = "none";

  console.log("runtime llvmir->wasm injection ok");
  console.log(`  llvmir_wasm.provider=${llvmIrWasm.provider}`);
  console.log(`  llvmir_wasm.entrypoint=${llvmIrWasm.entrypoint}`);
  console.log(`  llvmir_wasm.bytes=${llvmIrWasm.bytes.length}`);
}

const smokeFn = runtime[exportName];
if (typeof smokeFn !== "function") {
  throw new Error(`Export ${exportName} not found`);
}

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

if (requireRuntimeSpirv) {
  const llvmIrWasmUsed = globalThis.__webvulkan_llvm_ir_store_wasm_used === true;
  const llvmIrWasmProvider = globalThis.__webvulkan_llvm_ir_store_wasm_provider || "none";
  console.log(`proof.codegen=${llvmIrWasmUsed ? "llvm_ir_to_wasm" : "unknown"}`);
  console.log(`proof.execute_path=${llvmIrWasmUsed ? "fast_wasm" : "fallback"}`);
  console.log(`proof.interpreter=${llvmIrWasmUsed ? "disabled_for_dispatch" : "unknown"}`);
  console.log(`proof.llvm_ir_wasm_provider=${llvmIrWasmProvider}`);
  if (!llvmIrWasmUsed) {
    throw new Error("LLVM IR -> Wasm fast path was not used by llvmpipe dispatch");
  }
}

console.log("runtime smoke passed");
