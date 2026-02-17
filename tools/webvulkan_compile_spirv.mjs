import { spawn } from "node:child_process";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname } from "node:path";

function parseArgs(argv) {
  const parsed = {};
  for (let i = 0; i < argv.length; ++i) {
    const arg = argv[i];
    if (!arg.startsWith("--")) {
      throw new Error(`unexpected argument: ${arg}`);
    }
    const key = arg.slice(2);
    const value = argv[i + 1];
    if (!value || value.startsWith("--")) {
      throw new Error(`missing value for --${key}`);
    }
    parsed[key] = value;
    ++i;
  }
  return parsed;
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

function firstLine(value) {
  const text = (value || "").trim();
  if (!text) {
    return "";
  }
  return text.split("\n", 1)[0];
}

const args = parseArgs(process.argv.slice(2));
const inputPath = args.input;
const outputPath = args.output;
if (!inputPath || !outputPath) {
  throw new Error("required arguments: --input <path> --output <path>");
}

const wasmerBin = args.wasmer || process.env.WEBVULKAN_WASMER_BIN || "wasmer";
const clangPackage = args["clang-package"] || process.env.WEBVULKAN_CLANG_WASM_PACKAGE || "clang/clang";
const spirvPackage = args["spirv-package"] || process.env.WEBVULKAN_SPIRV_WASM_PACKAGE || "lights0123/llvm-spir";
const spirvEntrypoint = args["spirv-entrypoint"] || process.env.WEBVULKAN_SPIRV_WASM_ENTRYPOINT || "clspv";

const source = await readFile(inputPath, "utf8");
const attempts = [];
if (spirvPackage && spirvEntrypoint) {
  attempts.push({
    provider: `${spirvPackage}#${spirvEntrypoint}`,
    args: ["run", "--quiet", spirvPackage, "-e", spirvEntrypoint, "--", "-", "-o", "-"]
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
let compiledSpirv = null;
let provider = "";

for (const attempt of attempts) {
  const result = await runProcess(wasmerBin, attempt.args, { stdin: source });
  if (result.code === 0 && result.stdout.length >= 4 && result.stdout.subarray(0, 4).equals(spirvMagic)) {
    compiledSpirv = result.stdout;
    provider = attempt.provider;
    break;
  }
  const reason = firstLine(result.stderr) || `exit_code=${result.code}`;
  failureReasons.push(`${attempt.provider}: ${reason}`);
}

if (!compiledSpirv) {
  throw new Error(`failed to compile SPIR-V: ${failureReasons.join(" | ")}`);
}

await mkdir(dirname(outputPath), { recursive: true });
await writeFile(outputPath, compiledSpirv);

console.log("webvulkan shader compile ok");
console.log(`  input=${inputPath}`);
console.log(`  output=${outputPath}`);
console.log(`  bytes=${compiledSpirv.length}`);
console.log(`  provider=${provider}`);
