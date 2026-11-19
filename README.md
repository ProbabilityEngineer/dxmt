# DXMT — elite-mac branch

> **This is the `elite-mac` branch of a personal fork, not the DXMT project.**
> It carries a few fixes for Elite Dangerous: Odyssey under CrossOver, built
> and released for [ED Unofficial Patch for Mac](https://github.com/ProbabilityEngineer/ED-Unofficial-Patch-Mac).
> The DXMT project does not support it: please do not report problems with
> this branch or its builds to DXMT.
>
> On top of DXMT `main` (as of the merge below the changes):
>
> - **Partial colour write masks** are emulated in pixel shaders that discard
>   (an Apple driver loses the discard otherwise; station window decals and
>   logos disappear).
> - **MetalFX temporal scalers** that have gone idle are freed when a new one
>   is made (stock DXMT keeps every scaler, so memory grows with each new
>   render size).
> - The write-mask fix is **reported** as a private feature, so ED Unofficial
>   Patch for Mac can switch its own workaround off.
>
> Releases are tagged `elite-mac-vX.Y.Z`, matching the ED Unofficial Patch
> for Mac release (`macos-vX.Y.Z`) they were built for. Install instructions
> are in that project's README. DXMT is LGPL 2.1 or later (see `LICENSE`);
> this branch is its source.


A Metal-based translation layer for Direct3D 11 and 10 which allows running 3D applications on macOS using Wine.

For the current status of the project, please refer to the [project wiki](https://github.com/3Shain/dxmt/wiki).

The most recent development builds can be found [here](https://github.com/3Shain/dxmt/actions).


## Build

See [DEVELOPMENT.md](docs/DEVELOPMENT.md)
