import { pathToFileURL } from "node:url";

const modulePath = process.env.SMOKE_MODULE;
if (!modulePath) {
  throw new Error("SMOKE_MODULE is not set");
}
const exportName = process.env.SMOKE_EXPORT || "_wasm_runtime_smoke";

const moduleUrl = pathToFileURL(modulePath).href;
const imported = await import(moduleUrl);
const factory = imported.default;

if (typeof factory !== "function") {
  throw new Error("Expected default export factory from Emscripten module");
}

const runtime = await factory({
  print: (text) => process.stdout.write(String(text) + "\n"),
  printErr: (text) => process.stderr.write(String(text) + "\n")
});

const smokeFn = runtime[exportName];
if (typeof smokeFn !== "function") {
  throw new Error(`Export ${exportName} not found`);
}

const rc = smokeFn();
if (rc !== 0) {
  throw new Error(`Runtime smoke failed with rc=${rc}`);
}

console.log("runtime smoke passed");
