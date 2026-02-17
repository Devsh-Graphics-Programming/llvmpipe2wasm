import { spawn } from "node:child_process";

const wasmerBin = process.env.WEBVULKAN_WASMER_BIN;
if (!wasmerBin) {
  throw new Error("WEBVULKAN_WASMER_BIN is not set");
}

const clangPackage = process.env.WEBVULKAN_CLANG_WASM_PACKAGE || "clang/clang";
const spirvPackage = process.env.WEBVULKAN_SPIRV_WASM_PACKAGE || clangPackage;
const spirvEntrypoint = process.env.WEBVULKAN_SPIRV_WASM_ENTRYPOINT || "";

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

function firstLine(value) {
  const text = (value || "").trim();
  if (!text) {
    return "";
  }
  return text.split("\n", 1)[0];
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
    clangPackage,
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
__kernel void write_const(__global uint* out) {
  out[0] = 0x12345678u;
}
`;

const spirvMagic = Buffer.from([0x03, 0x02, 0x23, 0x07]);
const probeAttempts = [];

if (spirvPackage && spirvEntrypoint) {
  probeAttempts.push({
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

probeAttempts.push({
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

let spirvProbeStatus = "unavailable";
let spirvProbeProvider = "none";
let spirvProbeReason = "unknown";
for (const attempt of probeAttempts) {
  const result = await runProcess(wasmerBin, attempt.args, { stdin: spirvProbeSource });
  if (result.code === 0 && result.stdout.length >= 4 && result.stdout.subarray(0, 4).equals(spirvMagic)) {
    spirvProbeStatus = "available";
    spirvProbeProvider = attempt.provider;
    spirvProbeReason = `bytes=${result.stdout.length}`;
    break;
  }

  const reason = firstLine(result.stderr) || `exit_code=${result.code}`;
  if (spirvProbeReason === "unknown") {
    spirvProbeReason = `${attempt.provider}: ${reason}`;
  } else {
    spirvProbeReason = `${spirvProbeReason} | ${attempt.provider}: ${reason}`;
  }
}

console.log("clang wasm smoke ok");
console.log("  tool=wasmer");
console.log(`  compiler.package=${clangPackage}`);
console.log("  compiler.target=wasm32-unknown-unknown");
console.log("  output.magic=wasm");
console.log(`  export.shader_add=${addResult}`);
console.log(`  export.shader_store=${storeResult}`);
console.log(`  spirv.package=${spirvPackage}`);
console.log(`  spirv.entrypoint=${spirvEntrypoint || "(none)"}`);
console.log(`  spirv_probe=${spirvProbeStatus}`);
console.log(`  spirv_probe_provider=${spirvProbeProvider}`);
console.log(`  spirv_probe_reason=${spirvProbeReason}`);
console.log("runtime smoke passed");

