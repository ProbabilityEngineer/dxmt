# Agent Instructions

## Repository

- DXMT is a C/C++20 Metal-based translation layer for Direct3D 10/11/12 on macOS using Wine.
- The main implementation is under `src/`; shared headers and libraries are under `include/` and `libs/`.
- This project targets macOS; cross builds additionally require Wine and a Windows cross-compiler.

## Build and test

- Build with Meson 1.3+; use Xcode 16+ and LLVM 15 for the project toolchain.
- Initialize submodules before configuring: `git submodule update --init --recursive`.
- Native build: `meson setup -Dnative_llvm_path=<path-to-llvm> --buildtype release build`, then `meson compile -C build`.
- Cross build: `meson setup --cross-file build-win64.txt -Dnative_llvm_path=<path-to-llvm> -Dwine_build_path=<path-to-wine> --buildtype release build`, then `meson compile -C build`.
- Enable optional tests with `-Denable_tests=true`; inspect the relevant Meson targets before running them.
- Consult `docs/DEVELOPMENT.md` and `.github/workflows/ci.yml` for complete prerequisites and CI-faithful configurations.

## Local workflow

- Use clangd/LSP with the repository's `compile_commands.json` when available; prefer diagnostics before broad builds.
- Keep generated build output in ignored directories such as `build/` and `toolchains/`.
- Follow `CONTRIBUTING.md`: use semantic commit messages and sign off commits with your legal name.
- The upstream contribution policy does not accept AI/LLM-generated or co-authored contributions.

## Version control

- Use normal Git workflows for this repository.
- Inspect `git status` before starting and verify branch/remote alignment before declaring work clean or pushed.

## CrossOver and deployment safety

- Do not modify modules inside the global CrossOver application installation without explicit user approval.
- Prefer an isolated test bottle and preserve a timestamped backup before every deployment.
- Never replace `d3d11.dll`, `dxgi.dll`, `winemetal.dll`, or `winemetal.so` while the target application is running; changes take effect on a fresh process.
- Deploy matched artifacts: Windows DXMT modules must pair with the corresponding architecture-compatible Unix `winemetal.so`.
- Do not leave backup `.dll` or `.so` files in an active CrossOver module directory; CrossOver may scan them as loadable modules.
- Preserve logs, screenshots, hashes, and diagnostic artifacts for experimental deployments. Do not delete backups without approval.
- Keep HiDPI behavior opt-in through `d3d11.hidpiNativeResolution = True`.

## Validation discipline

- Validate shader-semantic changes numerically against identical Windows/WARP-generated DXBC and exact readback data where possible; visual output alone is insufficient.
- Keep native-WARP limitations separate from native-hardware behavior and from CrossOver hosting/ABI failures.
- Do not claim ARM64EC conformance success or failure until the probe reaches D3D11 execution and produces comparable output.
- Treat experimental rendering fixes as unvalidated until resolution, visual correctness, shutdown behavior, and rollback have all been checked.
- Profile before making performance claims, and keep Metal 4 work opt-in with a Metal 3.2 fallback until measured benefits are demonstrated.
