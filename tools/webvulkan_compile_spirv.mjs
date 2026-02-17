import { spawn } from "node:child_process";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";

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
    const child = spawn(command, args, {
      stdio: ["pipe", "pipe", "pipe"],
      cwd: options.cwd
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

function assertSpirv(bytes, errorContext) {
  const spirvMagic = Buffer.from([0x03, 0x02, 0x23, 0x07]);
  if (bytes.length < 4 || !bytes.subarray(0, 4).equals(spirvMagic)) {
    throw new Error(`${errorContext} did not produce valid SPIR-V`);
  }
}

async function compileHlslToSpirv(source, args) {
  const dxcWasmRaw = args["dxc-wasm-js"] || process.env.WEBVULKAN_DXC_WASM_JS || "";
  if (!dxcWasmRaw) {
    throw new Error("HLSL mode requires --dxc-wasm-js or WEBVULKAN_DXC_WASM_JS");
  }
  const dxcWasmJs = resolve(dxcWasmRaw);

  const hlslEntrypoint = args["hlsl-entrypoint"] || process.env.WEBVULKAN_HLSL_ENTRYPOINT || "main";
  const hlslProfile = args["hlsl-profile"] || process.env.WEBVULKAN_HLSL_PROFILE || "cs_6_0";

  const scratchDir = await mkdtemp(join(tmpdir(), "webvulkan-hlsl-dxc-"));
  const inputFile = "shader.hlsl";
  const outputFile = "shader.spv";
  const inputPath = join(scratchDir, inputFile);
  const outputPath = join(scratchDir, outputFile);

  try {
    await writeFile(inputPath, source, "utf8");
    const result = await runProcess(
      process.execPath,
      [
        dxcWasmJs,
        "-spirv",
        "-T",
        hlslProfile,
        "-E",
        hlslEntrypoint,
        "-Fo",
        outputFile,
        inputFile
      ],
      { stdin: undefined, cwd: scratchDir }
    );

    if (result.code !== 0) {
      const reason = firstLine(result.stderr) || `exit_code=${result.code}`;
      throw new Error(`failed to compile HLSL with dxc-wasm: ${reason}`);
    }

    const bytes = await readFile(outputPath);
    assertSpirv(bytes, "dxc-wasm");
    return {
      bytes,
      provider: `dxc-wasm:${dxcWasmJs}`
    };
  } finally {
    await rm(scratchDir, { recursive: true, force: true });
  }
}

const args = parseArgs(process.argv.slice(2));
const inputPath = args.input;
const outputPath = args.output;
if (!inputPath || !outputPath) {
  throw new Error("required arguments: --input <path> --output <path>");
}
const language = (args.language || "hlsl").toLowerCase();

const source = await readFile(inputPath, "utf8");
let compileResult;
if (language === "hlsl") {
  compileResult = await compileHlslToSpirv(source, args);
} else {
  throw new Error(`unsupported language: ${language}`);
}

await mkdir(dirname(outputPath), { recursive: true });
await writeFile(outputPath, compileResult.bytes);

console.log("webvulkan shader compile ok");
console.log(`  input=${inputPath}`);
console.log(`  output=${outputPath}`);
console.log(`  language=${language}`);
console.log(`  bytes=${compileResult.bytes.length}`);
console.log(`  provider=${compileResult.provider}`);
