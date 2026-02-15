# Contributing

## Pull requests

1. Open a branch from `master`.
2. Keep changes focused and incremental.
3. Run local smoke before opening a PR:

```powershell
cmake -S . -B build -G Ninja -DWEBVULKAN_BUILD_TESTS=ON
cmake --build build --target runtime_smoke
```

4. Ensure CI checks are green.
5. Describe what changed and why.

## Commit messages

- Use short factual messages in English.
- Start with a capital letter.
