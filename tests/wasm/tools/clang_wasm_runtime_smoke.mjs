import { spawn } from "node:child_process";

const wasmerBin = process.env.WEBVULKAN_WASMER_BIN;
if (!wasmerBin) {
  throw new Error("WEBVULKAN_WASMER_BIN is not set");
}

function runProcess(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, { stdio: ["pipe", "pipe", "pipe"] });
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

const shaderLikeSource = `
void __wasm_signal(void) {
}

int shader_add(int lhs, int rhs) {
  return lhs + rhs;
}

int shader_store(int value) {
  int transformed = value * 3;
  return transformed;
}
`;

const compileResult = await runProcess(
  wasmerBin,
  [
    "run",
    "--quiet",
    "clang/clang",
    "--",
    "--target=wasm32-unknown-unknown",
    "-O2",
    "-nostdlib",
    "-Wl,--no-entry",
    "-Wl,--export=shader_add",
    "-Wl,--export=shader_store",
    "-x",
    "c",
    "-",
    "-o",
    "-"
  ],
  { stdin: shaderLikeSource }
);

if (compileResult.code !== 0) {
  const stderr = compileResult.stderr.trim();
  throw new Error(`clang-in-wasm compilation failed with code=${compileResult.code}${stderr ? `\n${stderr}` : ""}`);
}

const wasmMagic = Buffer.from([0x00, 0x61, 0x73, 0x6d]);
const wasmBytes = compileResult.stdout;
if (wasmBytes.length < 8) {
  throw new Error(`clang-in-wasm returned too few bytes: ${wasmBytes.length}`);
}
if (!wasmBytes.subarray(0, 4).equals(wasmMagic)) {
  throw new Error("clang-in-wasm output is not a wasm module (missing wasm magic)");
}

if (!WebAssembly.validate(wasmBytes)) {
  throw new Error("clang-in-wasm output failed WebAssembly.validate");
}

const compiledModule = await WebAssembly.compile(wasmBytes);
const memory = new WebAssembly.Memory({ initial: 2, maximum: 65536, shared: true });
const instance = await WebAssembly.instantiate(compiledModule, { env: { memory } });

if (typeof instance.exports.shader_add !== "function" || typeof instance.exports.shader_store !== "function") {
  throw new Error("expected shader_add and shader_store exports");
}

const addResult = instance.exports.shader_add(20, 22);
const storeResult = instance.exports.shader_store(11);

if (addResult !== 42) {
  throw new Error(`shader_add returned ${addResult}, expected 42`);
}
if (storeResult !== 33) {
  throw new Error(`shader_store returned ${storeResult}, expected 33`);
}

const spirvProbeSource = `
__kernel void write_const(__global unsigned int* out) {
  out[0] = 0x12345678u;
}
`;

const spirvProbeResult = await runProcess(
  wasmerBin,
  [
    "run",
    "--quiet",
    "clang/clang",
    "--",
    "--target=spirv32",
    "-x",
    "cl",
    "-cl-std=CL2.0",
    "-c",
    "-",
    "-o",
    "-"
  ],
  { stdin: spirvProbeSource }
);

const spirvMagic = Buffer.from([0x03, 0x02, 0x23, 0x07]);
let spirvProbeStatus = "unavailable";
let spirvProbeReason = "unknown";
if (spirvProbeResult.code === 0 && spirvProbeResult.stdout.length >= 4 && spirvProbeResult.stdout.subarray(0, 4).equals(spirvMagic)) {
  spirvProbeStatus = "available";
  spirvProbeReason = `bytes=${spirvProbeResult.stdout.length}`;
} else {
  const stderr = spirvProbeResult.stderr.trim();
  if (stderr.includes("llvm-spirv")) {
    spirvProbeReason = "missing llvm-spirv tool inside clang/clang package";
  } else if (stderr) {
    spirvProbeReason = stderr.split("\n")[0];
  } else {
    spirvProbeReason = `exit_code=${spirvProbeResult.code}`;
  }
}

console.log("clang wasm smoke ok");
console.log("  tool=wasmer clang/clang");
console.log("  compiler.target=wasm32-unknown-unknown");
console.log("  output.magic=wasm");
console.log(`  export.shader_add=${addResult}`);
console.log(`  export.shader_store=${storeResult}`);
console.log(`  spirv_probe=${spirvProbeStatus}`);
console.log(`  spirv_probe_reason=${spirvProbeReason}`);
console.log("runtime smoke passed");
