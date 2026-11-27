// Multi-save UI + logic: create / load / overwrite / delete named configurations, per-vehicle-model compatibility, modal dialogs.
#include "nitrofx/App.h"
#include <ctime>
#include <cctype>
namespace nfx {
using namespace d2;

static void Msg(const std::string& title, const std::string& body, bool warn) { S.dlg = DLG_MESSAGE; S.dlgTitle = title; S.dlgMsg = body; S.dlgWarn = warn; }
void CloseDialog() { S.dlg = DLG_NONE; S.dlgAction = ACT_NONE; S.dlgErr.clear(); S.dlgTarget.clear(); }
void ReloadSaves() {
    int bad = 0; S.saves = ListSaves(&bad);
    if (bad > 0) { char b[96]; snprintf(b, sizeof b, "%d corrupted save file(s) moved to *.corrupt", bad); SetStatus(b); }
    int per = 6, n = (int)S.saves.size(), pages = (n + per - 1) / per; if (S.savePage >= pages) S.savePage = pages > 0 ? pages - 1 : 0;
}
static const SaveMeta* FindMeta(const std::string& id) { for (size_t i = 0; i < S.saves.size(); ++i) if (S.saves[i].id == id) return &S.saves[i]; return nullptr; }
static int CountForModel(int model) { int n = 0; for (size_t i = 0; i < S.saves.size(); ++i) if (S.saves[i].modelId == model) ++n; return n; }
static std::string Num(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); return b; }
static void Incompatible(int savedModel) {
    Msg("INCOMPATIBLE SAVE", "This save was made for vehicle model ID " + Num(savedModel) + ".\nThe current vehicle is model ID " + Num(S.vehModel) + ".\nNothing was loaded or changed.", true);
}
static bool WriteCurrent(SaveMeta m) { Profile p = S.profile; p.presetName = m.name; p.Sanitize(); return WriteSaveFile(m, p); }

void OpenSaveDialog() {
    if (!S.veh || S.vehModel < 0) { SetStatus("Enter a vehicle first"); return; }
    S.dlg = DLG_NAME; S.dlgName.clear(); S.dlgErr.clear();
}
// name dialog -> SAVE
static void DoSaveCurrent() {
    if (!S.veh || S.vehModel < 0) { Msg("NO VEHICLE", "Enter a vehicle before saving.", true); return; }
    std::string name = SanitizeSaveName(S.dlgName);
    if (name.empty()) { S.dlgErr = "Enter a name (letters, numbers, space, - _)"; return; }
    S.dlgName = name; ReloadSaves();
    for (size_t i = 0; i < S.saves.size(); ++i) if (S.saves[i].modelId == S.vehModel && SameSaveName(S.saves[i].name, name)) {   // duplicate name for THIS model only
        S.dlgTarget = S.saves[i].id; S.dlgAction = ACT_NONE; S.dlg = DLG_OVERWRITE; return; }
    SaveMeta m; m.id = NewSaveId(); m.name = name; m.modelId = S.vehModel; m.created = (long long)time(nullptr);
    if (m.id.empty()) { S.dlgErr = "Could not allocate a unique Save ID"; return; }
    if (!WriteCurrent(m)) { S.dlgErr = "Could not write the save file (disk full / folder not writable?)"; return; }   // no count limit: only real storage errors stop a save
    S.profile.presetName = name; MarkDirty(); ReloadSaves(); CloseDialog(); SetStatus("Saved: " + name);
}
// overwrite an existing save (same Save ID) with the current configuration; only for the same vehicle model
static void DoOverwrite(const std::string& id) {
    ReloadSaves(); const SaveMeta* cur = FindMeta(id);
    if (!cur) { Msg("SAVE NOT FOUND", "That save no longer exists.", true); return; }
    if (!S.veh || !ModelCompatible(cur->modelId, S.vehModel)) { Incompatible(cur->modelId); return; }
    SaveMeta m = *cur; m.created = (long long)time(nullptr);
    if (!WriteCurrent(m)) { Msg("OVERWRITE FAILED", "The existing save was NOT changed.", true); return; }   // atomic write: old data intact on failure
    S.profile.presetName = m.name; MarkDirty(); ReloadSaves();
    Msg("PREVIOUS SAVE UPDATED SUCCESSFULLY", m.name, false);
}
// LOAD: everything is read + validated first; the live configuration is touched only after every check passed
static void DoLoad(const std::string& id) {
    SaveMeta m; Profile p; SaveStatus st = ReadSaveFile(id, m, p);
    if (st == SAVE_IO) { Msg("LOAD FAILED", "The save file could not be read.\nNothing was changed.", true); ReloadSaves(); return; }
    if (st == SAVE_CORRUPT) { Msg("CORRUPTED SAVE", "This save file is damaged or invalid.\nIt was moved to *.corrupt. Nothing was changed.", true); ReloadSaves(); return; }
    if (!S.veh || S.vehModel < 0) { Msg("NO VEHICLE", "Enter a vehicle first.\nNothing was loaded.", true); return; }
    if (!ModelCompatible(m.modelId, S.vehModel)) { Incompatible(m.modelId); return; }
    S.profile = p; S.profile.presetName = m.name; S.selectedId = -1; S.emitterPage = 0; MarkDirty();   // exact restore, no resets
    SetStatus("Loaded: " + m.name);
}
static void AskConfirm(int action, const std::string& id, const std::string& title, const std::string& body) {
    S.dlg = DLG_CONFIRM; S.dlgAction = action; S.dlgTarget = id; S.dlgTitle = title; S.dlgMsg = body;
}
void AskResetVehicle() { AskConfirm(ACT_RESET_VEHICLE, "", "RESET VEHICLE", "Replace the current configuration with the default?\nSaved configurations are not affected."); }
static void RunConfirmed() {
    int a = S.dlgAction; std::string id = S.dlgTarget; CloseDialog();
    if (a == ACT_DELETE_SAVE) {
        const SaveMeta* m = FindMeta(id); std::string nm = m ? m->name : std::string();
        if (DeleteSaveFile(id)) { if (!nm.empty() && nm == S.profile.presetName) S.profile.presetName.clear(); SetStatus("Deleted: " + nm); } else SetStatus("Delete failed");
        ReloadSaves();
    } else if (a == ACT_OVERWRITE_ROW) DoOverwrite(id);
    else if (a == ACT_RESET_VEHICLE) { if (S.haveBounds) { MakeDefaultProfile(S.profile, S.bbMin, S.bbMax); S.selectedId = -1; S.emitterPage = 0; S.profile.presetName.clear(); MarkDirty(); SetStatus("Vehicle reset to default"); } }
}

// ===================== Saves tab =====================
void DrawSavesTab(float x, float y) {
    Text("Saves", x, y, 24, kGoldHi); RectH(x, y + 32, 560, 1, C(212, 175, 85, 200), C(212, 175, 85, 0)); char b[128];
    snprintf(b, sizeof b, "Vehicle model ID: %d     Saves for this vehicle: %d", S.vehModel, CountForModel(S.vehModel)); TextFit(S.veh ? b : "Enter a vehicle to create / load saves", x + 560, y + 6, 14, S.veh ? kDim : C(255, 176, 120), 450, 2);
    if (UiButton(x, y + 40, 276, 34, "Save Current As...", false, 17)) OpenSaveDialog();
    if (UiToggle(x + 284, y + 40, 276, 34, "Show all vehicles", S.showAllSaves)) { S.showAllSaves = !S.showAllSaves; S.savePage = 0; }
    std::vector<int> vis; for (size_t i = 0; i < S.saves.size(); ++i) if (S.showAllSaves || S.saves[i].modelId == S.vehModel) vis.push_back((int)i);
    const int per = 6; int pages = ((int)vis.size() + per - 1) / per; if (S.savePage >= pages) S.savePage = pages > 0 ? pages - 1 : 0;
    for (int k = 0; k < per; ++k) {
        int vi = S.savePage * per + k; if (vi >= (int)vis.size()) break; const SaveMeta& m = S.saves[vis[vi]]; float ry = y + 84 + k * 52.f;
        bool compat = ModelCompatible(m.modelId, S.vehModel), act = compat && m.name == S.profile.presetName;
        RectV(x, ry, 560, 48, C(34, 35, 41, 250), C(18, 19, 23, 250)); Rect(x, ry + 47, 560, 1, C(138, 112, 52, 150)); Rect(x, ry, 4, 48, act ? kGoldHi : (compat ? kGoldDim : C(255, 150, 90, 220)));
        TextFit(m.name.c_str(), x + 14, ry + 3, 18, act ? kGoldHi : kWhite, 296);
        snprintf(b, sizeof b, "Model %d%s  |  %d emitters  |  ID %.8s", m.modelId, compat ? "" : " (other vehicle)", m.emitterCount, m.id.c_str());
        TextFit(b, x + 14, ry + 28, 13, compat ? kDim : C(255, 176, 120), 296);
        if (UiButton(x + 318, ry + 8, 72, 32, "Load", false, 15)) DoLoad(m.id);
        if (UiButton(x + 394, ry + 8, 92, 32, "Overwrite", false, 14)) { if (!S.veh || !compat) Incompatible(m.modelId); else AskConfirm(ACT_OVERWRITE_ROW, m.id, "OVERWRITE SAVE", "Replace \"" + m.name + "\" with the current configuration?"); }
        if (UiButton(x + 490, ry + 8, 68, 32, "Delete", false, 15)) AskConfirm(ACT_DELETE_SAVE, m.id, "DELETE SAVE", "Delete \"" + m.name + "\"?\nThis cannot be undone.");
    }
    if (vis.empty()) Text(S.showAllSaves ? "No saves yet." : "No saves for this vehicle yet.", x, y + 94, 17, kWhite);
    if (pages > 1) {
        bool pv = UiButton(x, y + 402, 96, 28, "< Prev", false, 15), nx = UiButton(x + 104, y + 402, 96, 28, "Next >", false, 15);
        if (In.valid && InRect(x, y + 84, 560, 312)) { if (In.wheel > 0) pv = true; else if (In.wheel < 0) nx = true; }   // mouse wheel scrolls the list
        if (pv && S.savePage > 0) --S.savePage; if (nx && S.savePage < pages - 1) ++S.savePage;
        snprintf(b, sizeof b, "Page %d / %d", S.savePage + 1, pages); Text(b, x + 216, y + 406, 15, kWhite);
    }
}

// ===================== modal dialogs =====================
static void Lines(const std::string& s, float cx, float y, float px, Col c, float step) {
    size_t a = 0; while (a <= s.size()) { size_t e = s.find('\n', a); if (e == std::string::npos) e = s.size(); Text(s.substr(a, e - a).c_str(), cx, y, px, c, 1); y += step; a = e + 1; }
}
void DrawDialog() {
    static int last = DLG_NONE; int d = S.dlg;
    if (d == DLG_NONE) { last = DLG_NONE; return; }
    if (d != last) { last = d; In.lPressed = false; In.enterPressed = false; }   // the click/Enter that OPENED this dialog must not also hit one of its buttons
    Fill(C(0, 0, 0, 175));
    const float w = 560, h = (d == DLG_NAME) ? 250.f : 230.f, x = (VW - w) / 2, y = (VH - h) / 2, cx = VW / 2;
    UiPanel(x, y, w, h, 1.f);
    const float bw = 240, bh = 42, by = y + h - bh - 22, lx = x + 30, rx = x + w - 30 - bw;
    if (d == DLG_NAME) {
        for (size_t i = 0; i < In.typed.size(); ++i) { char c = In.typed[i]; if ((int)S.dlgName.size() < kMaxSaveName && (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-')) { S.dlgName += c; S.dlgErr.clear(); } }
        if (In.backPressed && !S.dlgName.empty()) { S.dlgName.erase(S.dlgName.size() - 1); S.dlgErr.clear(); }
        Text("SAVE CONFIGURATION", cx, y + 16, 28, kGoldHi, 1);
        char b[96]; snprintf(b, sizeof b, "Name this configuration for model ID %d  (max %d characters)", S.vehModel, kMaxSaveName); Text(b, cx, y + 58, 15, kWhite, 1);
        Rect(lx, y + 92, w - 60, 44, C(6, 6, 8, 255)); Rect(lx, y + 92, w - 60, 1, kGold); Rect(lx, y + 135, w - 60, 1, kGold); Rect(lx, y + 92, 1, 44, kGold); Rect(lx + w - 61, y + 92, 1, 44, kGold);
        std::string shown = S.dlgName + ((int)(S.time * 2.f) % 2 == 0 ? "|" : " "); Text(shown.c_str(), lx + 12, y + 99, 24, kWhite);
        if (!S.dlgErr.empty()) Text(S.dlgErr.c_str(), cx, y + 144, 15, C(255, 160, 120), 1);
        bool enter = In.enterPressed; In.typed.clear(); In.backPressed = In.enterPressed = false;   // consume text input exactly once
        if (UiButton(lx, by, bw, bh, "SAVE", false, 19) || enter) DoSaveCurrent();
        else if (UiButton(rx, by, bw, bh, "CANCEL", false, 19)) CloseDialog();
    } else if (d == DLG_OVERWRITE) {
        Text("SAVE NAME ALREADY EXISTS", cx, y + 16, 28, C(255, 190, 120), 1);
        Lines("A save with this name already exists for this vehicle model.\nOverwrite it with the current configuration?", cx, y + 66, 16, kWhite, 26);
        Text(("\"" + S.dlgName + "\"").c_str(), cx, y + 126, 22, kGoldHi, 1);
        if (UiButton(lx, by, bw, bh, "CANCEL", false, 19)) { S.dlg = DLG_NAME; S.dlgTarget.clear(); }                // back to the name box, nothing written
        else if (UiButton(rx, by, bw, bh, "OVERWRITE", true, 19)) { std::string id = S.dlgTarget; CloseDialog(); DoOverwrite(id); }
    } else if (d == DLG_CONFIRM) {
        Text(S.dlgTitle.c_str(), cx, y + 16, 28, C(255, 190, 120), 1);
        Lines(S.dlgMsg, cx, y + 78, 17, kWhite, 28);
        if (UiButton(lx, by, bw, bh, "CANCEL", false, 19)) CloseDialog();
        else if (UiButton(rx, by, bw, bh, "CONFIRM", true, 19)) RunConfirmed();
    } else {   // DLG_MESSAGE
        Text(S.dlgTitle.c_str(), cx, y + 16, S.dlgTitle.size() > 22 ? 24.f : 28.f, S.dlgWarn ? C(255, 150, 110) : kGoldHi, 1);
        Lines(S.dlgMsg, cx, y + 78, 17, kWhite, 28);
        if (UiButton(cx - bw / 2, by, bw, bh, "OK", true, 19) || In.enterPressed) CloseDialog();
    }
}
}
