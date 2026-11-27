#pragma once
#include "Types.h"
namespace nfx {
std::string BaseDir();                                   // <asi folder>\NITRO_FX (created on demand)
std::string VehiclePath(int modelId);                    // autosave of the ACTIVE configuration for a model (working copy)
std::string PresetPath(const std::string& name);         // legacy model-less presets (no longer used by the UI)
std::string SanitizePresetName(const std::string& n);
std::string SerializeProfile(const Profile& p);
bool ParseProfile(const std::string& text, Profile& out); // false = corrupted header / empty
bool SaveProfileFile(const std::string& path, const Profile& p);
bool LoadProfileFile(const std::string& path, Profile& out); // quarantines corrupted files as *.corrupt
std::vector<std::string> ListPresets();
bool DeletePreset(const std::string& name);

// ---------------- multi-save system ----------------
// One file per configuration: NITRO_FX\saves\<SaveID>.nfx. SaveID is unique (64-bit, 16 hex chars) and is the ONLY thing that
// identifies a save. The vehicle model ID is stored inside the file and only decides compatibility.
// There is deliberately NO per-model save-count limit: the only bounds are free disk space (a failed write is reported, never ignored)
// and memory safety (a single save file is capped at kMaxSaveFileBytes and read with a bounded reader; the in-memory list holds metadata only).
const int kMaxSaveName = 24;
const unsigned kMaxSaveFileBytes = 256u * 1024u;            // 64 emitters serialise to ~16 KB; anything above this cap is not a valid save
struct SaveMeta { std::string id, name; int modelId = -1; long long created = 0; int emitterCount = 0; };
enum SaveStatus { SAVE_OK = 0, SAVE_IO = 1, SAVE_CORRUPT = 2 };

std::string SavesDir();
std::string SavePath(const std::string& id);
std::string SanitizeSaveName(const std::string& n);       // letters, digits, space, _ - ; trimmed; max kMaxSaveName
bool SameSaveName(const std::string& a, const std::string& b); // case-insensitive, after sanitising
bool IsValidSaveId(const std::string& id);
std::string NewSaveId();                                   // guaranteed not to collide with an existing file
std::string SerializeSave(const SaveMeta& m, const Profile& p);
SaveStatus ParseSave(const std::string& text, const std::string& expectId, SaveMeta& meta, Profile& out); // strict: any defect => SAVE_CORRUPT; meta/out are written ONLY on SAVE_OK
bool WriteSaveFile(const SaveMeta& m, const Profile& p);   // atomic (tmp + replace); verifies the data re-parses before touching disk
SaveStatus ReadSaveFile(const std::string& id, SaveMeta& meta, Profile& out); // corrupt files are renamed *.corrupt
std::vector<SaveMeta> ListSaves(int* corruptQuarantined);  // valid saves only, sorted by name
bool DeleteSaveFile(const std::string& id);

// ---------------- autosave of the active configuration (dirty tracking + safe retry) ----------------
// Rules: the dirty flag is cleared ONLY after a write succeeded. A failed write keeps the data dirty and is retried with exponential
// back-off (2 s, 4 s, 8 s, 16 s, then every 30 s) so a full / read-only disk is not hammered. A profile that is replaced in memory
// (vehicle switch) while its write is failing is parked in `pending` and keeps being retried for ITS OWN model file.
typedef bool (*ProfileWriteFn)(const std::string& path, const Profile& p);
struct PendingSave { int modelId = -1; Profile profile; unsigned retryAt = 0; int failures = 0; };
struct AutosaveState {
    bool dirty = false; unsigned dirtyTime = 0;             // active configuration has unsaved changes
    unsigned retryAt = 0; int failures = 0;                 // back-off for the active configuration
    std::vector<PendingSave> pending;                       // unsaved configurations of vehicles that are no longer active
};
const unsigned kAutosaveDelayMs = 2000, kAutosaveMaxBackoffMs = 30000;
unsigned AutosaveBackoffMs(int failures);                   // delay before the next retry after `failures` consecutive failures
void AutosaveMarkDirty(AutosaveState& a, unsigned now);
bool AutosaveHasUnsaved(const AutosaveState& a);
bool AutosaveDue(const AutosaveState& a, int activeModel, unsigned now, bool busy);   // periodic check (busy = user is dragging)
// Writes everything that is unsaved. force=true (menu close, vehicle switch, shutdown) ignores back-off and the quiet delay; force=false obeys them.
// Returns the number of writes that FAILED in this call. Afterwards AutosaveHasUnsaved() tells whether anything is still dirty.
int AutosaveFlush(AutosaveState& a, int activeModel, const Profile& active, unsigned now, bool force, ProfileWriteFn write);
void AutosavePark(AutosaveState& a, int model, const Profile& p, int failures, unsigned retryAt);   // keep an unsaved profile of a model that is being left (carries its back-off)
bool AutosaveTakePending(AutosaveState& a, int model, Profile& out);       // newest unsaved profile for `model` (removed from the queue)
}
