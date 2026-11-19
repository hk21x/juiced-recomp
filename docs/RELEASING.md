# Releasing Juiced

The model is PGR2's (1.6): a release holds the runtime and a compiler, no game code; the player's PC builds
`Juiced.dll` from their disc (`Juiced.exe --prepare-code`, run by the launcher).

## Steps

1. `main` holds what ships; `scripts/smoke.sh` printed SMOKE OK on it.
2. Notes in `docs/releases/vX.Y.Z.md`, committed before the tag.
3. On a Windows PC with the disc extracted in `assets\`: `build.bat`, then `release-package.bat`, which makes
   `Juiced-package.zip` (about 74 MB) and `Juiced-package.zip.json` (size and SHA-256).
4. Push the tag `vX.Y.Z`: the workflow builds `JuicedLauncher.exe` and makes a **draft** release with it and the notes.
5. Attach `Juiced-package.zip` and `Juiced-package.zip.json` to the draft, then publish.
6. Test the published release: the launcher alone in an empty folder, your disc, **Get the game ready**, **Play**.

## Before the first release (v1.0.0)

- The repository must be public: the launcher reads the release (and its package) through GitHub's API, and finds
  nothing for a private repository.
- The SDK commit `sdk` points at must be on the fork (it is: branch `juiced`). Getting it onto the fork's `main`
  needs the all-games smoke gate; not needed to ship.
