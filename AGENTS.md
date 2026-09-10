# Agent Instructions

## Repository

- DXMT is a C/C++20 Metal-based translation layer for Direct3D 10/11/12 on macOS using Wine.
- Main implementation is under `src/`; shared headers and libraries are under `include/` and `libs/`.
- Native and cross builds require the documented macOS, LLVM, Wine, and Windows cross-compiler toolchains.

## Build and test

- Initialize submodules with `git submodule update --init --recursive`.
- Native build: `meson setup -Dnative_llvm_path=<path-to-llvm> --buildtype release build`, then `meson compile -C build`.
- Cross build: `meson setup --cross-file build-win64.txt -Dnative_llvm_path=<path-to-llvm> -Dwine_build_path=<path-to-wine> --buildtype release build`, then `meson compile -C build`.
- Enable optional tests with `-Denable_tests=true`; inspect relevant Meson targets before running them.
- Consult `docs/DEVELOPMENT.md`, `.github/workflows/ci.yml`, and `CONTRIBUTING.md` for CI-faithful prerequisites and commit requirements.

## Toolchain and work tracking

- Use clangd/LSP with `compile_commands.json` when available; run diagnostics before broad builds.
- Keep generated output in ignored directories such as `build/` and `toolchains/`.
- Use `clu` as the authoritative source of project tasks and work state.
- At the start of substantial work, run `clu ready`, then use `clu claim --context` or claim the specifically requested task; read inherited context before editing.
- Put newly discovered work, notes, and dependencies in `clu`, not Markdown todo lists.
- Close completed work in `clu` after validation; leave incomplete or blocked work represented there.

## Version control

- Use normal Git workflows. Inspect `git status` and branch/remote alignment before committing or pushing.

## CrossOver and deployment safety

- Do not modify modules inside the global CrossOver installation without explicit user approval.
- Prefer an isolated test bottle and preserve a timestamped backup before every deployment.
- Never replace `d3d11.dll`, `dxgi.dll`, `winemetal.dll`, or `winemetal.so` while the target application is running; changes take effect on a fresh process.
- Deploy matched artifacts: Windows DXMT modules must pair with the corresponding architecture-compatible Unix `winemetal.so`.
- Do not leave backup `.dll` or `.so` files in an active CrossOver module directory; CrossOver may scan them as loadable modules.
- Preserve logs, screenshots, hashes, and diagnostic artifacts for experimental deployments. Do not delete backups without approval.
- Keep `d3d11.hidpiNativeResolution = True` opt-in.

## Validation discipline

- Validate shader-semantic changes numerically against identical Windows/WARP-generated DXBC and exact readback data where possible; visual output alone is insufficient.
- Keep native-WARP limitations separate from native-hardware behavior and CrossOver hosting/ABI failures.
- Do not claim ARM64EC conformance until the probe reaches D3D11 execution and produces comparable output.
- Treat experimental rendering fixes as unvalidated until resolution, visual correctness, shutdown behavior, and rollback are checked.
- Profile before making performance claims; keep Metal 4 opt-in with a Metal 3.2 fallback until measured benefits are demonstrated.
