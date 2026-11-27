# Build (GitHub Actions - no local setup)
1. Create a GitHub repo, push this folder (keep `.github/workflows/build.yml`).
2. Actions tab -> "Build NITRO FX" -> Run. The workflow clones `DK22Pac/plugin-sdk`, configures CMake for **Win32**
   (`-A Win32`, defines `GTASA`, `PLUGIN_SGV_10US`), builds Release and uploads `NITRO_FX.asi` + `NITRO_FX_release.zip`.
3. If it fails, open the log: the first errors are most likely Plugin-SDK name/signature differences (see README "Status").

# Build locally (optional)
VS 2022 + CMake: `cmake -S . -B build -A Win32 -DPLUGIN_SDK_DIR=C:\plugin-sdk` then `cmake --build build --config Release`.

# Logic tests (no GTA / SDK needed, Linux/macOS/MinGW)
`sh tests/run_tests.sh` (builds with ASan/UBSan and runs in a scratch directory; also runs `src/MeshPick.cpp` against a mock RenderWare clump).
Manual: `g++ -std=c++14 -Wall -Iinclude -Iinclude/nitrofx tests/logic_test.cpp src/Types.cpp src/Storage.cpp -o logic_test` and run `./logic_test` from an empty scratch folder (it writes `./NITRO_FX/...`).
