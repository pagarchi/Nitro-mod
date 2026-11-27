# NITRO FX — GTA San Andreas (PC 1.0 US) ASI mod
One unified **Jet** effect (narrow, dense, white-blue by default, ~45° out/up) emitted from vehicle-local emitters.
15 procedural texture categories only restyle that jet. Menu: **R + S + I**. Branding: `Create by : Parsa Rsi`.

## What changed in this revision
* **64 emitters per vehicle** (was 16); each has its own position, direction, angle, mirror flag and full material. Emitter list is paged (16/page); Effects tab has per-emitter Jet Angle + Direction sliders. Texture cache raised to 72 entries so 64 different materials do not thrash.
* **Multi-save system** (Saves tab): any number of named saves per vehicle (no per-model cap; limited only by free disk space - a failed write is always reported), each with its own random 64-bit Save ID (= file `NITRO_FX\saves\<id>.nfx`); the model ID is stored only for compatibility. Same name allowed on different models; duplicate name on the same model -> CANCEL / OVERWRITE dialog -> "PREVIOUS SAVE UPDATED SUCCESSFULLY". Delete and Overwrite-row and Reset-Vehicle all ask CANCEL / CONFIRM.
* **Lossless**: floats stored with 9 digits (exact round trip), headings are no longer re-normalised on load; all emitter fields + mirror mode + jet on/off restored.
* **Validation before load**: file is fully read and strictly validated (CRC, counts, ranges), then model ID is compared; on any failure nothing is applied. Corrupt files are renamed `*.corrupt`, never deleted. Writes are atomic (tmp + replace) and re-parsed before touching disk.
* **Real mesh picking**: `MeshPick.cpp` ray-casts the triangles of every visible atomic of the vehicle's RpClump (world-transformed via the frame LTM), nearest hit wins; emitter gets the exact surface point + normal in vehicle-local coordinates. The bounding box is no longer used for picking (it is still used only by the 8 quick-add slot buttons / default profile, which are fractions of the box, not picks).
* Legacy model-less presets are no longer shown in the menu (their files are untouched).

## Status (honest)
| Item | Implemented | Compiled | Runtime-tested in GTA SA |
|---|---|---|---|
| Profile/emitter/mirror logic, multi-save storage, strict validation, corruption handling, autosave retry logic, ray/triangle/mesh + world->local math | yes | yes (g++ Linux, ASan+UBSan, `sh tests/run_tests.sh` -> `tests/logic_test.cpp`, all pass) | n/a |
| `src/MeshPick.cpp` compiled + run against a **mock** RenderWare clump (`tests/meshpick_mock_test.cpp`) | yes | yes (mock layout = assumption about the SDK, **not** proof) | n/a |
| Procedural texture generator (15 categories) | yes | yes (g++, Linux) | not re-run |
| RenderWare/Plugin-SDK layer (jet, 2D UI, dragon, editor, input, **MeshPick, Saves UI/dialogs, key polling**) | yes | **NO** | **NO** |
No Plugin-SDK checkout and no network were available, so the SDK-facing code was NOT compiled. Names most likely to need a fix on the first
GitHub build: `CEntity::m_pRwObject`, `RpClumpForAllAtomics`, `RwFrameGetLTM`, `RpAtomic::object.object.flags/parent`, `RpGeometry::morphTarget[0].verts`, `RpTriangle::vertIndex`, `rpATOMICRENDER`, `rpCLUMP`.

## Save safety (this revision)
* **Autosave never discards unsaved changes**: the dirty flag is cleared only after a successful write. A failed write keeps the data dirty and is retried with back-off (2 s, 4 s, 8 s, 16 s, then every 30 s). If you switch vehicles while a write is failing, the old vehicle's unsaved configuration is kept in memory and retried for its own file; same-model vehicle swaps do not reload the stale file over unsaved edits. Edits made with no vehicle have no target file and are not persisted.
* **Saves are validated strictly** (`ParseSave`): unknown fields, missing/duplicate fields, malformed numbers (trailing characters, hex, inf/nan, overflow), out-of-range emitter/material values, bad ids/categories/counts and malformed records reject the whole file; nothing is clamped or repaired and the live configuration is untouched. CRC and atomic (tmp + replace) writes are unchanged. Autosave working copies (`vehicles\model_N.nfx`) keep the lenient "skip bad records, clamp ranges" behaviour.
* A single save file is read with a bounded reader (256 KB cap) so a huge or hostile file cannot exhaust memory; oversize files are quarantined as `*.corrupt`.

## Known limits
* Autosave retry state lives in memory: if the game process is killed while the disk is failing, unsaved edits are lost (nothing can persist them without a working disk).
* Mesh picking cannot distinguish the very-low-LOD (`*_vlo`) or other helper atomics from the visible mesh if the game leaves their render flag set; this has NOT been checked in-game. If jets snap to a coarse hull in some vehicles, that is the first thing to inspect.
* Mesh picking hits the *rendered* geometry (doors, wheels, spoilers too); hidden variants (render flag off) are skipped. Max pick distance 60 m. Screen->ray still uses the existing CSprite::CalcScreenCoors calibration.
* Typing a save name uses raw key polling (A-Z, 0-9, space, -, _): GTA's own cheat-string listener also sees those keys.
* Jets are drawn in the drawing event with real Z-test against the already rendered vehicle (no depth write).
* UI font size is controlled by `kFontUnit` in `src/Draw2D.cpp`.
See BUILD_INSTRUCTIONS.md and INSTALLATION.md.
