import { readFile } from "node:fs/promises";

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

const logPath = process.argv[2];
if (!logPath) {
  throw new Error("Usage: node validate_dispatch_bench.mjs <runtime-smoke-log-path>");
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
for (const profileName of requiredProfiles) {
  const fastSamples = ensureModeEntries(benchmarksByProfile, profileName, "fast_wasm");
  const rawSamples = ensureModeEntries(benchmarksByProfile, profileName, "raw_llvm_ir");

  const fastAvgMs = average(fastSamples);
  const rawAvgMs = average(rawSamples);
  const speedup = rawAvgMs / fastAvgMs;
  const minSpeedup = envThresholdForProfile(profileName);

  console.log(
    `[bench] profile=${profileName} fast_wasm_avg_ms=${fastAvgMs.toFixed(6)} raw_llvm_ir_avg_ms=${rawAvgMs.toFixed(6)} speedup=${speedup.toFixed(3)}x required>=${minSpeedup.toFixed(3)}x`
  );

  if (speedup < minSpeedup) {
    throw new Error(
      `Benchmark gate failed for profile='${profileName}': observed speedup ${speedup.toFixed(3)}x < required ${minSpeedup.toFixed(3)}x`
    );
  }
}

console.log("[bench] runtime benchmark gate passed");
