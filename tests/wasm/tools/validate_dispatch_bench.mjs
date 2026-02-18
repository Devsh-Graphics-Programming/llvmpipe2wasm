import { readFile, writeFile } from "node:fs/promises";

function parsePositiveFloat(value, label) {
  const parsed = Number.parseFloat(value);
  if (!Number.isFinite(parsed) || parsed <= 0.0) {
    throw new Error(`Invalid ${label}: '${value}'`);
  }
  return parsed;
}

function envThresholdForProfile(profileName) {
  const profileEnvKey = `WEBVULKAN_BENCH_MIN_SPEEDUP_${profileName.toUpperCase()}`;
  const profileOverride = process.env[profileEnvKey];
  const globalDefault = process.env.WEBVULKAN_BENCH_MIN_SPEEDUP || "2.0";
  return parsePositiveFloat(profileOverride || globalDefault, profileEnvKey);
}

function normalizeRequiredProfiles() {
  const rawProfiles = process.env.WEBVULKAN_BENCH_REQUIRED_PROFILES || "dispatch_overhead,balanced_grid";
  return rawProfiles
    .split(",")
    .map((value) => value.trim())
    .filter((value) => value.length > 0);
}

function pushSummaryIfComplete(block, sink) {
  if (!block) {
    return;
  }
  if (!block.mode || !block.profile || !block.avg_ms) {
    return;
  }
  const avgMs = Number.parseFloat(block.avg_ms);
  if (!Number.isFinite(avgMs) || avgMs <= 0.0) {
    throw new Error(`Invalid avg_ms in summary block: '${block.avg_ms}'`);
  }
  sink.push({
    mode: block.mode,
    profile: block.profile,
    avgMs
  });
}

function parseSummaryBlocks(logText) {
  const lines = logText.split(/\r?\n/);
  const summaries = [];
  let currentBlock = null;

  for (const line of lines) {
    if (line.trim() === "dispatch timing summary") {
      pushSummaryIfComplete(currentBlock, summaries);
      currentBlock = {};
      continue;
    }

    if (!currentBlock) {
      continue;
    }

    const keyValueMatch = line.match(/^\s+([a-z0-9_]+)=([^\s]+)\s*$/i);
    if (keyValueMatch) {
      const [, key, value] = keyValueMatch;
      currentBlock[key] = value;
      continue;
    }

    if (line.trim() !== "" && !line.startsWith(" ")) {
      pushSummaryIfComplete(currentBlock, summaries);
      currentBlock = null;
    }
  }

  pushSummaryIfComplete(currentBlock, summaries);
  return summaries;
}

function average(values) {
  if (!values.length) {
    throw new Error("Cannot average an empty value list");
  }
  return values.reduce((sum, value) => sum + value, 0.0) / values.length;
}

function ensureModeEntries(mapByProfile, profileName, modeName) {
  const byMode = mapByProfile.get(profileName);
  if (!byMode || !byMode.has(modeName)) {
    throw new Error(`Missing benchmark summary for profile='${profileName}' mode='${modeName}'`);
  }
  return byMode.get(modeName);
}

let logPath = "";
let emitJson = false;
let jsonOutPath = "";
let enforceThresholds = true;

const args = process.argv.slice(2);
for (let i = 0; i < args.length; ++i) {
  const arg = args[i];
  if (arg === "--emit-json") {
    emitJson = true;
    continue;
  }
  if (arg === "--no-validate") {
    enforceThresholds = false;
    continue;
  }
  if (arg === "--json-out") {
    const nextValue = args[i + 1];
    if (!nextValue) {
      throw new Error("--json-out requires a file path");
    }
    jsonOutPath = nextValue;
    i += 1;
    continue;
  }
  if (arg.startsWith("--")) {
    throw new Error(`Unknown option '${arg}'`);
  }
  if (!logPath) {
    logPath = arg;
    continue;
  }
  throw new Error("Only one runtime-smoke-log-path argument is supported");
}

if (!logPath) {
  throw new Error(
    "Usage: node validate_dispatch_bench.mjs <runtime-smoke-log-path> [--emit-json] [--json-out <path>] [--no-validate]"
  );
}

const logText = await readFile(logPath, "utf8");
const summaries = parseSummaryBlocks(logText);
if (!summaries.length) {
  throw new Error("No 'dispatch timing summary' blocks were found in runtime smoke log");
}

const benchmarksByProfile = new Map();
for (const summary of summaries) {
  if (!benchmarksByProfile.has(summary.profile)) {
    benchmarksByProfile.set(summary.profile, new Map());
  }
  const profileMap = benchmarksByProfile.get(summary.profile);
  if (!profileMap.has(summary.mode)) {
    profileMap.set(summary.mode, []);
  }
  profileMap.get(summary.mode).push(summary.avgMs);
}

const requiredProfiles = normalizeRequiredProfiles();
const reportProfiles = [];
for (const profileName of requiredProfiles) {
  const fastSamples = ensureModeEntries(benchmarksByProfile, profileName, "fast_wasm");
  const rawSamples = ensureModeEntries(benchmarksByProfile, profileName, "raw_llvm_ir");

  const fastAvgMs = average(fastSamples);
  const rawAvgMs = average(rawSamples);
  const speedup = rawAvgMs / fastAvgMs;
  const minSpeedup = envThresholdForProfile(profileName);
  const profilePass = speedup >= minSpeedup;

  reportProfiles.push({
    name: profileName,
    fast_wasm_avg_ms: fastAvgMs,
    raw_llvm_ir_avg_ms: rawAvgMs,
    speedup_x: speedup,
    required_min_speedup_x: minSpeedup,
    pass: profilePass
  });

  console.log(
    `[bench] profile=${profileName} fast_wasm_avg_ms=${fastAvgMs.toFixed(6)} raw_llvm_ir_avg_ms=${rawAvgMs.toFixed(6)} speedup=${speedup.toFixed(3)}x required>=${minSpeedup.toFixed(3)}x`
  );

  if (enforceThresholds && speedup < minSpeedup) {
    throw new Error(
      `Benchmark gate failed for profile='${profileName}': observed speedup ${speedup.toFixed(3)}x < required ${minSpeedup.toFixed(3)}x`
    );
  }
}

const speedupValues = reportProfiles.map((profile) => profile.speedup_x);
const minSpeedupObserved = Math.min(...speedupValues);
const maxSpeedupObserved = Math.max(...speedupValues);
const avgSpeedupObserved = average(speedupValues);
const geometricMeanSpeedupObserved = Math.exp(
  speedupValues.reduce((sum, value) => sum + Math.log(value), 0.0) / speedupValues.length
);

const report = {
  required_profiles: requiredProfiles,
  profiles: reportProfiles,
  summary: {
    min_speedup_x: minSpeedupObserved,
    max_speedup_x: maxSpeedupObserved,
    avg_speedup_x: avgSpeedupObserved,
    geomean_speedup_x: geometricMeanSpeedupObserved,
    all_profiles_pass: reportProfiles.every((profile) => profile.pass)
  }
};

if (jsonOutPath) {
  await writeFile(jsonOutPath, JSON.stringify(report, null, 2) + "\n", "utf8");
}

if (emitJson) {
  console.log("[bench] benchmark report json");
  console.log(JSON.stringify(report, null, 2));
}

if (enforceThresholds) {
  console.log("[bench] runtime benchmark gate passed");
} else {
  console.log("[bench] runtime benchmark report generated without threshold validation");
}
