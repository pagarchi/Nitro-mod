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
    vx = d2::GXToV(out.x); vy = d2::GYToV(out.y); return true;   // game px -> letterboxed virtual coords
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

// ======================= real mouse control =======================
// ROOT CAUSE of the frozen cursor: GTA SA re-centres the OS cursor every frame (psMouseSetPos) and feeds the raw mouse movement to the
// camera, so GetCursorPos always returned the screen centre. While the menu owns the mouse we (1) disable that re-centring,
// (2) zero GTA's mouse deltas so the camera does not turn, (3) keep the cursor inside the game window, and restore all of it on close / ALT.
// If the re-centring routine is not the expected one (other exe version) we fall back to a virtual cursor driven by Raw Input deltas.
static const DWORD kPsMouseSetPos = 0x6194A0;                 // GTA SA 1.0 US: first byte 0xE9 (jmp); 0xC3 (ret) turns the re-centring off
static const DWORD kMouseStates[3] = { 0xB73418, 0xB73404, 0xB7342C };   // CPad::NewMouseControllerState / Old / PCTemp (bools[7], pad, float X, Y, Z)
static bool gUiMouse = false, gPatched = false, gClipSet = false, gSavedValid = false, gRawReg = false;
static BYTE gOrigByte = 0; static RECT gPrevClip; static POINT gSavedPos; static float gVx = 0, gVy = 0;   // gV*: fallback virtual cursor (game px)
bool UiMouseCaptured() { return gUiMouse; }
static bool WritableByte(DWORD addr, BYTE v, BYTE* old) {
    BYTE* p = (BYTE*)addr; if (IsBadReadPtr(p, 1)) return false; DWORD prot; if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &prot)) return false;
    if (old) *old = *p; *p = v; DWORD t; VirtualProtect(p, 1, prot, &t); FlushInstructionCache(GetCurrentProcess(), p, 1); return true;
}
static void SetRecenter(bool blocked) {
    if (blocked && !gPatched) { BYTE* p = (BYTE*)kPsMouseSetPos; if (!IsBadReadPtr(p, 1) && *p == 0xE9 && WritableByte(kPsMouseSetPos, 0xC3, &gOrigByte)) gPatched = true; }
    else if (!blocked && gPatched) { WritableByte(kPsMouseSetPos, gOrigByte, nullptr); gPatched = false; }
}
static void ZeroGameMouse(bool readWheel) {
    for (int i = 0; i < 3; ++i) {
        BYTE* st = (BYTE*)kMouseStates[i]; if (IsBadWritePtr(st, 20)) continue;
        if (readWheel && i == 0) { if (st[3]) In.wheel = 1; else if (st[4]) In.wheel = -1; }
        st[0] = st[1] = st[2] = st[3] = st[4] = st[5] = st[6] = 0; ((float*)(st + 8))[0] = ((float*)(st + 8))[1] = 0.f; ((float*)(st + 8))[2] = 0.f;
    }
}
static void RawPoll(HWND h, float& dx, float& dy) {
    dx = dy = 0; if (!gRawReg) { RAWINPUTDEVICE d; d.usUsagePage = 1; d.usUsage = 2; d.dwFlags = RIDEV_INPUTSINK; d.hwndTarget = h; gRawReg = RegisterRawInputDevices(&d, 1, sizeof d) != FALSE; if (!gRawReg) return; }
    alignas(8) static BYTE buf[4096];
    for (int guard = 0; guard < 8; ++guard) {
        UINT sz = sizeof buf; UINT n = GetRawInputBuffer((PRAWINPUT)buf, &sz, sizeof(RAWINPUTHEADER)); if (n == 0 || n == (UINT)-1) break;
        PRAWINPUT r = (PRAWINPUT)buf; for (UINT i = 0; i < n; ++i) { if (r->header.dwType == RIM_TYPEMOUSE && !(r->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) { dx += (float)r->data.mouse.lLastX; dy += (float)r->data.mouse.lLastY; } r = NEXTRAWINPUTBLOCK(r); }
    }
}
static bool ClientRect(HWND h, RECT& rc) { return h && GetClientRect(h, &rc) && rc.right > 0 && rc.bottom > 0; }
// ui=true: free cursor owns the mouse. fromAlt=true when the switch is caused by ALT (cursor position is kept / restored).
static void SetMouseMode(bool ui, HWND h, bool fromAlt) {
    if (ui == gUiMouse) return; gUiMouse = ui; RECT rc;
    if (ui) {
        SetRecenter(true);
        if (ClientRect(h, rc)) {
            POINT a = { rc.left, rc.top }, b = { rc.right, rc.bottom }; ClientToScreen(h, &a); ClientToScreen(h, &b); RECT clip = { a.x, a.y, b.x, b.y };
            GetClipCursor(&gPrevClip); ClipCursor(&clip); gClipSet = true;
            if (fromAlt && gSavedValid) { SetCursorPos(gSavedPos.x, gSavedPos.y); In.justRestored = true; }
            else SetCursorPos((a.x + b.x) / 2, (a.y + b.y) / 2);                     // menu opens with the cursor in the middle
            gVx = (float)RsGlobal.maximumWidth * .5f; gVy = (float)RsGlobal.maximumHeight * .5f;
        }
    } else {
        if (fromAlt) gSavedValid = GetCursorPos(&gSavedPos) != FALSE; else gSavedValid = false;
        if (gClipSet) { ClipCursor(&gPrevClip); gClipSet = false; }
        SetRecenter(false);
    }
}
static void UpdateInput() {
    d2::RefreshView();
    HWND h; bool act = GameWindowActive(h); POINT pt; RECT rc;
    bool l = act && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; bool prev = In.lDown;
    In.lPressed = l && !prev; In.lReleased = !l && prev; In.lDown = l;
    In.altDown = act && (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    static bool pd = false, pe = false; bool d = act && (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0, e = act && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    In.delPressed = d && !pd; In.escPressed = e && !pe; pd = d; pe = e;
    PollTyping(act);
    In.valid = false; In.wheel = 0; In.justRestored = false;
    if (gUiMouse) ZeroGameMouse(true);
    if (!act || !ClientRect(h, rc)) return;
    float W = (float)RsGlobal.maximumWidth, H = (float)RsGlobal.maximumHeight, gx, gy;
    if (gUiMouse && !gPatched) {                                                       // fallback: virtual cursor from Raw Input deltas
        float dx, dy; RawPoll(h, dx, dy); gVx = uim::Clamp(gVx + dx, 0.f, W); gVy = uim::Clamp(gVy + dy, 0.f, H); gx = gVx; gy = gVy;
    } else if (GetCursorPos(&pt) && ScreenToClient(h, &pt)) {                          // real OS cursor -> game px (fixes windowed / scaled client areas)
        gx = uim::ClientToGame((float)pt.x, (float)rc.right, W); gy = uim::ClientToGame((float)pt.y, (float)rc.bottom, H);
    } else return;
    In.mx = d2::GXToV(gx); In.my = d2::GYToV(gy); In.valid = true;                     // same letterbox transform as the drawing code
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
    // ---- input ownership: menu / edit mode lock GTA's controls; ALT hands the camera back (menu + cursor hidden), release restores everything ----
    const bool locked = S.menuOpen || S.editMode;
    const bool altFree = In.altDown && ((S.menuOpen && S.dlg == DLG_NONE) || S.editMode);
    S.altHidden = S.menuOpen && altFree;
    static bool prevLocked = false, prevAltFree = false;
    CPad* pad = CPad::GetPad(0);
    if (locked || prevLocked) { if (pad) pad->DisablePlayerControls = locked && !altFree; }          // released exactly once when the menu closes
    const bool wantUi = locked && !altFree && act;                                                    // alt-tab away also gives the mouse back to Windows
    SetMouseMode(wantUi, h, wantUi ? prevAltFree : altFree);
    if (gUiMouse) ZeroGameMouse(false);                                                               // camera must not turn while clicking the UI
    prevLocked = locked; prevAltFree = altFree;
    if (!locked && gUiMouse) SetMouseMode(false, h, false);
    if (AutosaveDue(S.autosave, S.vehModel, GetTickCount(), S.dragging)) FlushAutosave(false);   // quiet delay + back-off respected
    if (S.menuOpen) S.menuAlpha = std::min(1.0f, S.menuAlpha + 0.06f);
}
static void Draw() {
    DrawJets();                                                         // 3D pass (depth-tested against the vehicle)
    if (!S.menuOpen && !S.editMode) return;
    if (gUiMouse) ZeroGameMouse(false);
    if (S.menuOpen && S.altHidden) return;                              // ALT: menu and cursor temporarily hidden
    if (In.justRestored) { In.lPressed = false; In.lDown = false; }     // the ALT release must not click anything
    d2::Begin();
    if (S.editMode) DrawEditor(); else DrawMenu();
    d2::End();
}
static void Init() { S.newTex = TexSettings(); ReloadSaves(); }
static void Shutdown() { SaveNow(); gTex.Shutdown(); HWND h = GetForegroundWindow(); SetMouseMode(false, h, false); CPad* p = CPad::GetPad(0); if (p) p->DisablePlayerControls = false; }   // final forced attempt

struct NitroFXPlugin {
    NitroFXPlugin() {
        Events::initRwEvent += Init;
        Events::gameProcessEvent += Process;
        Events::drawingEvent += Draw;
        Events::shutdownRwEvent += Shutdown;
    }
} gNitroFXPlugin;
}
