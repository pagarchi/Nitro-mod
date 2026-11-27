#include "nitrofx/App.h"
namespace nfx {
AppState S; InputState In;
void SetStatus(const std::string& s) { S.status = s; S.statusTime = GetTickCount(); }
Emitter* SelectedEmitter() { for (size_t i = 0; i < S.profile.emitters.size(); ++i) if (S.profile.emitters[i].id == S.selectedId) return &S.profile.emitters[i]; return nullptr; }
void MarkDirty() { AutosaveMarkDirty(S.autosave, GetTickCount()); }
// Writes every unsaved configuration. The dirty flag is cleared ONLY by a successful write (inside AutosaveFlush); a failed write keeps the
// data dirty and schedules a back-off retry. force=true (menu close, vehicle switch, shutdown) ignores the back-off, force=false obeys it.
static void FlushAutosave(bool force) {
    int failed = AutosaveFlush(S.autosave, S.vehModel, S.profile, GetTickCount(), force, &SaveProfileFile);
    if (failed > 0) SetStatus("Save failed - changes kept in memory, retrying (folder not writable / disk full?)");
}
void SaveNow() { FlushAutosave(true); }

bool VehicleBasis(CVehicle* v, V3& r, V3& f, V3& u, V3& p) {
    if (!v || !v->m_matrix) return false; CMatrix* m = v->m_matrix;   // right / up(=forward) / at(=up) / pos
    r = V3(m->right.x, m->right.y, m->right.z); f = V3(m->up.x, m->up.y, m->up.z); u = V3(m->at.x, m->at.y, m->at.z); p = V3(m->pos.x, m->pos.y, m->pos.z); return true;
}
V3 LocalToWorld(CVehicle* v, const V3& l) { V3 r, f, u, p; if (!VehicleBasis(v, r, f, u, p)) return l; return p + r * l.x + f * l.y + u * l.z; }
V3 LocalDirToWorld(CVehicle* v, const V3& l) { V3 r, f, u, p; if (!VehicleBasis(v, r, f, u, p)) return l; return r * l.x + f * l.y + u * l.z; }
V3 WorldDirToLocal(CVehicle* v, const V3& w) { V3 r, f, u, p; if (!VehicleBasis(v, r, f, u, p)) return w; return V3(dot(w, r), dot(w, f), dot(w, u)); }
bool GetBounds(CVehicle* v, V3& mn, V3& mx) {
    if (!v) return false; CBaseModelInfo* mi = CModelInfo::GetModelInfo(v->m_nModelIndex);
    if (!mi || !mi->m_pColModel) return false; CBoundingBox& b = mi->m_pColModel->m_boundBox;
    mn = V3(b.m_vecMin.x, b.m_vecMin.y, b.m_vecMin.z); mx = V3(b.m_vecMax.x, b.m_vecMax.y, b.m_vecMax.z);
    return finitef(mn.x) && finitef(mx.x) && mx.x > mn.x && mx.y > mn.y && mx.z > mn.z;
}
bool WorldToScreen(const V3& w, float& vx, float& vy) {
    RwV3d in; in.x = w.x; in.y = w.y; in.z = w.z; RwV3d out; float sw, sh;
    if (!CSprite::CalcScreenCoors(in, &out, &sw, &sh, false, false)) return false;
    vx = out.x * d2::VW / (float)RsGlobal.maximumWidth; vy = out.y * d2::VH / (float)RsGlobal.maximumHeight; return true;
}
bool InRect(float x, float y, float w, float h) { return In.valid && In.mx >= x && In.mx <= x + w && In.my >= y && In.my <= y + h; }

static bool GameWindowActive(HWND& h) { h = GetForegroundWindow(); if (!h) return false; DWORD pid = 0; GetWindowThreadProcessId(h, &pid); return pid == GetCurrentProcessId(); }
static void PollTyping(bool act) {
    static bool prev[256]; static DWORD backHeld = 0, backRep = 0;
    static const int kKeys[] = { VK_SPACE, VK_OEM_MINUS, VK_SUBTRACT, VK_BACK, VK_RETURN };
    In.typed.clear(); In.backPressed = In.enterPressed = false;
    bool shift = act && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0, caps = (GetKeyState(VK_CAPITAL) & 1) != 0, upper = shift != caps;
    auto down = [&](int vk) { return act && (GetAsyncKeyState(vk) & 0x8000) != 0; };
    auto edge = [&](int vk) { bool d = down(vk); bool e = d && !prev[vk]; prev[vk] = d; return e; };
    for (int vk = 'A'; vk <= 'Z'; ++vk) if (edge(vk)) In.typed += (char)(upper ? vk : vk - 'A' + 'a');
    for (int vk = '0'; vk <= '9'; ++vk) if (edge(vk) && !shift) In.typed += (char)vk;
    for (int vk = VK_NUMPAD0; vk <= VK_NUMPAD9; ++vk) if (edge(vk)) In.typed += (char)('0' + (vk - VK_NUMPAD0));
    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); ++i) {
        int vk = kKeys[i]; bool e = edge(vk); if (!e) continue;
        if (vk == VK_SPACE) In.typed += ' '; else if (vk == VK_OEM_MINUS) In.typed += shift ? '_' : '-'; else if (vk == VK_SUBTRACT) In.typed += '-';
        else if (vk == VK_BACK) { In.backPressed = true; backHeld = backRep = GetTickCount(); } else if (vk == VK_RETURN) In.enterPressed = true;
    }
    if (down(VK_BACK) && !In.backPressed) { DWORD n = GetTickCount(); if (n - backHeld > 400 && n - backRep > 45) { In.backPressed = true; backRep = n; } }   // hold-to-repeat
}
static void UpdateInput() {
    HWND h; bool act = GameWindowActive(h); POINT pt; RECT rc;
    bool l = act && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; bool prev = In.lDown;
    In.lPressed = l && !prev; In.lReleased = !l && prev; In.lDown = l;
    In.altDown = act && (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    static bool pd = false, pe = false; bool d = act && (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0, e = act && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    In.delPressed = d && !pd; In.escPressed = e && !pe; pd = d; pe = e;
    PollTyping(act);
    In.valid = false;
    if (act && GetCursorPos(&pt) && ScreenToClient(h, &pt) && GetClientRect(h, &rc) && rc.right > 0 && rc.bottom > 0) {
        In.mx = pt.x * d2::VW / (float)rc.right; In.my = pt.y * d2::VH / (float)rc.bottom; In.valid = true;
    }
}
void EnterEditMode() {
    if (!S.veh) { SetStatus("Enter a vehicle first"); return; }
    S.backup = S.profile; S.editMode = true; S.menuOpen = false; S.selectedId = -1; S.dragging = false;
}
static void LoadForVehicle(CVehicle* v) {
    SaveNow();                                                        // forced attempt; on failure the data stays dirty
    const int oldModel = S.vehModel; const int newModel = v ? (int)v->m_nModelIndex : -1;
    bool keepInMemory = false;                                        // true = S.profile is still the authoritative (unsaved) copy for the new vehicle
    if (S.autosave.dirty) {
        if (oldModel < 0) S.autosave.dirty = false;                   // edits made with no vehicle have no file to go to (nothing was ever bound to a model)
        else if (newModel == oldModel) keepInMemory = true;           // same model, new vehicle object: do NOT reload the stale file over unsaved edits
        else {                                                        // different model: park the unsaved profile and keep retrying ITS OWN file
            AutosavePark(S.autosave, oldModel, S.profile, S.autosave.failures, S.autosave.retryAt);
            S.autosave.dirty = false; S.autosave.failures = 0; S.autosave.retryAt = 0;
        }
    }
    S.veh = v; S.selectedId = -1; S.dragging = false; S.emitterPage = 0; CloseDialog();
    if (!v) { S.vehModel = -1; S.haveBounds = false; return; }
    S.vehModel = newModel; S.haveBounds = GetBounds(v, S.bbMin, S.bbMax);
    if (keepInMemory) return;
    Profile p;
    if (AutosaveTakePending(S.autosave, S.vehModel, p)) { S.profile = p; MarkDirty(); }   // unsaved data of this model that could not be written earlier
    else if (LoadProfileFile(VehiclePath(S.vehModel), p)) S.profile = p;
    else if (S.haveBounds) { MakeDefaultProfile(S.profile, S.bbMin, S.bbMax); MarkDirty(); }
    else S.profile = Profile();
}
// ---- per-frame logic (hotkey, vehicle tracking, input lock, autosave) ----
static void Process() {
    S.time = CTimer::m_snTimeInMilliseconds / 1000.0f;
    UpdateInput();
    HWND h; bool act = GameWindowActive(h);
    static bool prevCombo = false;                                    // R + S + I must be down simultaneously; edge-triggered
    bool combo = act && (GetAsyncKeyState('R') & 0x8000) && (GetAsyncKeyState('S') & 0x8000) && (GetAsyncKeyState('I') & 0x8000);
    if (combo && !prevCombo && !S.editMode && S.dlg == DLG_NONE) { S.menuOpen = !S.menuOpen; if (S.menuOpen) { S.menuAlpha = 0; ReloadSaves(); } else SaveNow(); }
    prevCombo = combo;
    CVehicle* pv = FindPlayerVehicle(-1, false);
    if (pv != S.veh || (pv && pv->m_nModelIndex != S.vehModel)) { if (S.editMode) { S.editMode = false; S.profile = S.backup; } LoadForVehicle(pv); }
    if (S.menuOpen && In.escPressed) { if (S.dlg != DLG_NONE) CloseDialog(); else { S.menuOpen = false; SaveNow(); } }   // ESC: dialog first, then menu
    if (!S.menuOpen && S.dlg != DLG_NONE) CloseDialog();
    if (S.menuOpen || S.editMode) {                                    // lock gameplay input; ALT in edit mode releases the camera
        CPad* pad = CPad::GetPad(0); if (pad) pad->DisablePlayerControls = !(S.editMode && In.altDown);
    }
    if (AutosaveDue(S.autosave, S.vehModel, GetTickCount(), S.dragging)) FlushAutosave(false);   // quiet delay + back-off respected
    if (S.menuOpen) S.menuAlpha = std::min(1.0f, S.menuAlpha + 0.06f);
}
static void Draw() {
    DrawJets();                                                         // 3D pass (depth-tested against the vehicle)
    if (!S.menuOpen && !S.editMode) return;
    d2::Begin();
    if (S.editMode) DrawEditor(); else DrawMenu();
    d2::End();
}
static void Init() { S.newTex = TexSettings(); ReloadSaves(); }
static void Shutdown() { SaveNow(); gTex.Shutdown(); }   // final forced attempt

struct NitroFXPlugin {
    NitroFXPlugin() {
        Events::initRwEvent += Init;
        Events::gameProcessEvent += Process;
        Events::drawingEvent += Draw;
        Events::shutdownRwEvent += Shutdown;
    }
} gNitroFXPlugin;
}
