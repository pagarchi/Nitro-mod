#include "nitrofx/App.h"
namespace nfx {
using namespace d2;
// ================= widgets (premium graphite / gold / platinum) =================
static float MouseWheelOver(float x, float y, float w, float h) { return InRect(x, y, w, h) ? (float)In.wheel : 0.f; }
void UiPanel(float x, float y, float w, float h, float a) {
    SetAdditive(true); RectV(x - 6, y - 6, w + 12, 6, C(212, 175, 85, 0), C(212, 175, 85, (int)(22 * a))); RectV(x - 6, y + h, w + 12, 6, C(212, 175, 85, (int)(22 * a)), C(212, 175, 85, 0)); SetAdditive(false);   // faint outer glow
    RectV(x, y, w, h, C(30, 31, 36, (int)(244 * a)), C(9, 9, 11, (int)(248 * a)));                                                                   // graphite -> deep black
    RectH(x, y, w / 2, 2, C(212, 175, 85, 0), C(236, 200, 110, (int)(255 * a))); RectH(x + w / 2, y, w / 2, 2, C(236, 200, 110, (int)(255 * a)), C(212, 175, 85, 0));   // metallic top edge
    Col e = C(150, 122, 58, (int)(210 * a)); Rect(x, y + h - 1, w, 1, e); Rect(x, y, 1, h, e); Rect(x + w - 1, y, 1, h, e);
    Rect(x + 4, y + 4, w - 8, 1, C(255, 255, 255, (int)(14 * a))); Rect(x + 4, y + 5, 1, h - 10, C(132, 108, 190, (int)(26 * a)));                  // inner bevel + faint violet accent
    const float k = 14, t = 2; Col g = C(236, 198, 100, (int)(255 * a));                                                                           // gold corner brackets
    float cs[4][4] = { { x, y, 1, 1 }, { x + w, y, -1, 1 }, { x, y + h, 1, -1 }, { x + w, y + h, -1, -1 } };
    for (int i = 0; i < 4; ++i) { float cx = cs[i][0], cy = cs[i][1], sx = cs[i][2], sy = cs[i][3];
        Rect(sx > 0 ? cx : cx - k, sy > 0 ? cy : cy - t, k, t, g); Rect(sx > 0 ? cx : cx - t, sy > 0 ? cy : cy - k, t, k, g); }
}
bool UiButton(float x, float y, float w, float h, const char* label, bool sel, float px) {
    bool hov = InRect(x, y, w, h); bool down = hov && In.lDown; bool click = hov && In.lPressed;
    Col t, b, edge, txt;
    if (sel) { t = C(226, 190, 100, 255); b = C(160, 126, 52, 255); edge = kGoldHi; txt = kInk; }
    else if (down) { t = C(22, 22, 26, 255); b = C(36, 36, 42, 255); edge = kGold; txt = kGoldHi; }
    else if (hov) { t = C(62, 63, 72, 255); b = C(38, 39, 46, 255); edge = kGold; txt = kGoldHi; }
    else { t = C(44, 45, 52, 255); b = C(24, 25, 29, 255); edge = C(122, 100, 48, 255); txt = kWhite; }
    RectV(x, y, w, h, t, b); Rect(x, y, w, 1, edge); Rect(x, y + h - 1, w, 1, edge); Rect(x, y, 1, h, edge); Rect(x + w - 1, y, 1, h, edge);
    if (!sel) Rect(x + 1, y + 1, w - 2, 1, C(255, 255, 255, hov ? 30 : 16)); else Rect(x + 1, y + 1, w - 2, 1, C(255, 255, 255, 90));
    if (hov && !sel) { SetAdditive(true); RectV(x, y, w, h, C(212, 175, 85, 26), C(212, 175, 85, 0)); SetAdditive(false); }                // restrained hover glow
    TextFit(label, x + w / 2, y + (h - px) / 2 - 1, px, txt, w - 10, 1); return click;
}
bool UiToggle(float x, float y, float w, float h, const char* label, bool on) {
    bool hov = InRect(x, y, w, h), click = hov && In.lPressed;
    RectV(x, y, w, h, hov ? C(60, 61, 70, 255) : C(42, 43, 50, 255), hov ? C(36, 37, 44, 255) : C(23, 24, 28, 255));
    Col edge = on ? kGold : C(122, 100, 48, 255); Rect(x, y, w, 1, edge); Rect(x, y + h - 1, w, 1, edge); Rect(x, y, 1, h, edge); Rect(x + w - 1, y, 1, h, edge);
    Rect(x + 1, y + 1, w - 2, 1, C(255, 255, 255, 16));
    float pw = 46, ph = 20, px = x + w - pw - 12, py = y + (h - ph) / 2;
    TextFit(label, x + 14, y + (h - 17) / 2 - 1, 17, hov ? kGoldHi : kWhite, w - pw - 40);
    RectV(px, py, pw, ph, on ? C(226, 190, 100, 255) : C(14, 14, 17, 255), on ? C(168, 134, 56, 255) : C(32, 32, 38, 255)); Rect(px, py, pw, 1, edge); Rect(px, py + ph - 1, pw, 1, edge); Rect(px, py, 1, ph, edge); Rect(px + pw - 1, py, 1, ph, edge);
    float kx = on ? px + pw - ph + 2 : px + 2; RectV(kx, py + 2, ph - 4, ph - 4, on ? C(40, 34, 18, 255) : C(150, 152, 162, 255), on ? C(22, 19, 10, 255) : C(98, 100, 110, 255));
    Text(on ? "ON" : "OFF", on ? px + 10 : px + pw - 9, py + 1, 12, on ? kInk : kDim, 1); return click;
}
bool UiSlider(float x, float y, float w, const char* label, float& v, float lo, float hi) {
    static const void* active = nullptr; const void* id = &v; bool changed = false;
    const float lw = 176, vw = 64, tx = x + lw, tw = w - lw - vw - 14, ty = y + 11;
    Text(label, x, y, 17, kWhite);
    Rect(tx, ty, tw, 6, C(10, 10, 12, 255)); Rect(tx, ty, tw, 1, C(90, 76, 40, 255)); Rect(tx, ty + 5, tw, 1, C(255, 255, 255, 18));
    if (In.lPressed && InRect(tx - 10, y - 2, tw + 20, 30)) active = id;
    if (!In.lDown) active = nullptr;
    if (active == id && In.valid) { float nv = lo + clampf((In.mx - tx) / tw, 0, 1) * (hi - lo); if (nv != v) { v = nv; changed = true; } }
    float k = clampf((v - lo) / (hi - lo), 0, 1); RectH(tx, ty, tw * k, 6, kGoldDim, kGold);
    bool hov = active == id || InRect(tx - 10, y - 2, tw + 20, 30); float kx = tx + tw * k;
    Circle(kx, ty + 3, 10, C(212, 175, 85, hov ? 70 : 0), C(212, 175, 85, 0), 16); Circle(kx, ty + 3, 8, kGoldHi, kGold, 16); Circle(kx, ty + 3, 3.5f, C(40, 34, 18), C(40, 34, 18), 10);
    float bx = x + w - vw; Rect(bx, y - 1, vw, 24, C(10, 10, 12, 255)); Rect(bx, y - 1, vw, 1, C(122, 100, 48, 255)); Rect(bx, y + 22, vw, 1, C(122, 100, 48, 255));
    char b[16]; snprintf(b, sizeof b, "%.2f", v); Text(b, bx + vw / 2, y + 1, 16, kGoldHi, 1); return changed;
}
// cursor: a clean metallic arrow (gold fill, dark outline). The position is the REAL OS cursor, mapped through the same transform as the UI.
void UiCursor() {
    if (!In.valid) return; float x = In.mx, y = In.my;
    Tri(x - 1.5f, y - 1.5f, C(0, 0, 0, 230), x + 17.5f, y + 13.5f, C(0, 0, 0, 230), x + 5, y + 21, C(0, 0, 0, 230));
    Tri(x, y, kGoldHi, x + 15, y + 11, kGold, x + 4, y + 18, kGoldDim);
    Tri(x + 4, y + 18, kGoldDim, x + 15, y + 11, kGold, x + 11, y + 22, kGoldDim);
}

// ================= branding: "Create by : Parsa Rsi" (live text: glow, glitch, RGB split, light sweep, metallic) =================
void DrawBranding(float cx, float y, float t) {
    const char* pre = "Create by : "; const char* name = "Parsa Rsi";
    const float pp = 15, pn = 24; float wp = TextWidth(pre, pp), wn = TextWidth(name, pn); float x0 = cx - (wp + wn) / 2, nx = x0 + wp;
    bool glitch = fmodf(t, 3.2f) < 0.16f; float gx = glitch ? sinf(t * 95.f) * 5.f : 0.f;
    Text(pre, x0, y + 8, pp, C(176, 166, 196, 175));                                          // subtle prefix
    for (int i = 0; i < 4; ++i) { float ox = (i < 2 ? (i ? 2.5f : -2.5f) : 0), oy = (i >= 2 ? (i == 3 ? 2.5f : -2.5f) : 0); Text(name, nx + ox, y + oy, pn, C(150, 60, 255, 38)); } // neon glow
    Text(name, nx - 1.8f + gx, y, pn, C(255, 40, 90, 105)); Text(name, nx + 1.8f + gx, y, pn, C(40, 225, 255, 105));  // RGB split
    float sweep = fmodf(t * 0.5f, 1.7f) * wn - wn * 0.35f; int n = (int)strlen(name);
    for (int i = 0; i < n; ++i) {                                                               // metallic gold + travelling light sweep
        char pf[16]; memcpy(pf, name, i); pf[i] = 0; char ch[2] = { name[i], 0 }; float cxp = nx + TextWidth(pf, pn);
        float k = expf(-powf((cxp - (nx + sweep)) / (wn * 0.14f), 2.f)); float metal = 0.5f + 0.5f * sinf(i * 0.9f + t * 1.5f);
        Col base = CMix(C(205, 165, 70), C(250, 224, 140), metal * .6f); Col c = CMix(base, C(255, 252, 240), k);
        Text(ch, cxp, y, pn, c); if (glitch && (i & 1)) Text(ch, cxp + gx * 1.5f, y - 2, pn, C(255, 255, 255, 110)); }
}

// ================= helpers =================
static const int kPer = 16;                                  // emitters per page (2 columns x 8)
static TexSettings& CurTex() { Emitter* e = SelectedEmitter(); return e ? e->tex : S.newTex; }
static void ApplyTex(int field, float val) {
    struct F { static void Set(TexSettings& t, int f, float v) { switch (f) { case 0: t.category = (int)v; break; case 1: t.colorVar = v; break; case 2: t.speed = v; break; case 3: t.detail = v; break; case 4: t.glow = v; break; case 5: t.opacity = v; break; case 6: t.power = v; break; } t.Clamp(); } };
    Emitter* e = SelectedEmitter();
    if (S.applyAll || !e) { for (size_t i = 0; i < S.profile.emitters.size(); ++i) F::Set(S.profile.emitters[i].tex, field, val); F::Set(S.newTex, field, val); }
    else F::Set(e->tex, field, val);
    S.profile.presetName.clear(); MarkDirty();
}
static int PageCount() { int n = (int)S.profile.emitters.size(); return n < 1 ? 1 : (n + kPer - 1) / kPer; }
static void SectionTitle(float x, float y, const char* t) { Text(t, x, y, 24, kGoldHi); RectH(x, y + 32, 560, 1, C(212, 175, 85, 200), C(212, 175, 85, 0)); }
static void EmitterPager(float x, float y) {          // "Emitters n/64   Page a/b" + prev/next; mouse wheel over the list also changes the page
    int pages = PageCount(); if (S.emitterPage >= pages) S.emitterPage = pages - 1; if (S.emitterPage < 0) S.emitterPage = 0;
    char b[64]; snprintf(b, sizeof b, "Emitters %d/%d     Page %d/%d", (int)S.profile.emitters.size(), kMaxEmitters, S.emitterPage + 1, pages); Text(b, x, y + 3, 16, kDim);
    bool prev = UiButton(x + 330, y, 40, 26, "<", false, 16), next = UiButton(x + 376, y, 40, 26, ">", false, 16);
    float wh = MouseWheelOver(x, y + 30, 560, 220); if (wh > 0) prev = true; else if (wh < 0) next = true;
    if (prev && S.emitterPage > 0) --S.emitterPage; if (next && S.emitterPage < pages - 1) ++S.emitterPage;
}
static void EmitterList(float x, float y, bool full) {
    if (S.emitterPage >= PageCount()) S.emitterPage = PageCount() - 1;
    for (int k = 0; k < kPer; ++k) { size_t i = (size_t)S.emitterPage * kPer + k; if (i >= S.profile.emitters.size()) break; Emitter& e = S.profile.emitters[i];
        float cx = x + (k / 8) * 282.f, cy = y + (k % 8) * 26.f; char b[32]; snprintf(b, sizeof b, "Emitter #%d", e.id);
        if (UiButton(cx, cy, full ? 120 : 190, 24, b, e.id == S.selectedId, 14)) S.selectedId = e.id;
        float bx = cx + (full ? 124 : 194);
        if (full && UiButton(bx, cy, 46, 24, e.active ? "ON" : "OFF", e.active, 13)) { e.active = !e.active; MarkDirty(); }
        if (UiButton(full ? bx + 50 : bx, cy, 36, 24, "M", e.mirror, 14)) { e.mirror = !e.mirror; MarkDirty(); }
        if (full && UiButton(bx + 90, cy, 32, 24, "X", false, 14)) { if (S.selectedId == e.id) S.selectedId = -1; S.profile.emitters.erase(S.profile.emitters.begin() + i); MarkDirty(); break; } }
}

// ================= tabs (content area is 560 x ~430, origin = x,y) =================
static void TabEmitters(float x, float y) {
    SectionTitle(x, y, "Emitter Positions");
    if (UiButton(x, y + 44, 276, 34, "Open Smoke Edit Mode", false, 17)) EnterEditMode();
    if (UiButton(x + 284, y + 44, 276, 34, "Reset Vehicle To Default", false, 16)) { if (S.haveBounds) AskResetVehicle(); else SetStatus("Enter a vehicle first"); }
    Text("Add emitter at preset position", x, y + 88, 14, kDim);
    for (int i = 0; i < kSlotCount; ++i) if (UiButton(x + (i % 2) * 284.f, y + 108 + (i / 2) * 30.f, 276, 26, kSlots[i].name, false, 15)) {
        if (!S.veh || !S.haveBounds) SetStatus("Enter a vehicle first");
        else if (!AddSlotEmitter(S.profile, i, S.bbMin, S.bbMax, S.newTex)) SetStatus("Emitter limit reached"); else { S.selectedId = S.profile.nextId - 1; S.emitterPage = ((int)S.profile.emitters.size() - 1) / kPer; MarkDirty(); } }
    EmitterPager(x, y + 234); Text("M = mirror   X = delete", x + 430, y + 237, 13, kDim); EmitterList(x, y + 264, true);
}
static void TabTextures(float x, float y) {
    SectionTitle(x, y, "Texture Library"); int cur = CurTex().category;
    for (int i = 0; i < CAT_COUNT; ++i) if (UiButton(x + (i % 3) * 188.f, y + 44 + (i / 3) * 42.f, 184, 38, kCategoryNames[i], i == cur, 16)) ApplyTex(0, (float)i);
    float py = y + 44 + 5 * 42.f + 8; RwTexture* tx = gTex.Get(CurTex());
    Rect(x, py, 236, 102, C(0, 0, 0, 255)); Rect(x, py, 236, 1, kGoldDim); Rect(x, py + 101, 236, 1, kGoldDim); Rect(x, py, 1, 102, kGoldDim); Rect(x + 235, py, 1, 102, kGoldDim);
    SetAdditive(false); if (tx) TexRect(x + 3, py + 3, 230, 96, tx, 0.1f, S.time * -0.8f, 0.9f, S.time * -0.8f + 1.0f, C(255, 255, 255, 255));
    if (UiToggle(x + 250, py, 310, 38, "Apply to all emitters", S.applyAll)) S.applyAll = !S.applyAll;
    Emitter* e = SelectedEmitter(); char b[64]; snprintf(b, sizeof b, "Editing: %s", e ? "selected emitter" : "all emitters / new ones"); Text(b, x + 252, py + 52, 15, kDim);
}
static void TabEffects(float x, float y) {
    SectionTitle(x, y, "Effects Settings");
    if (UiToggle(x, y + 44, 276, 36, "Jet Effect", S.profile.jetEnabled)) { S.profile.jetEnabled = !S.profile.jetEnabled; MarkDirty(); }
    if (UiToggle(x + 284, y + 44, 276, 36, "Apply to all", S.applyAll)) S.applyAll = !S.applyAll;
    TexSettings t = CurTex(); const char* nm[6] = { "Color Variation", "Texture Speed", "Detail Density", "Glow Intensity", "Opacity", "Effect Power" };
    float lo[6] = { 0, 0.1f, 0, 0, 0.1f, 0.3f }, hi[6] = { 1, 4, 1, 2, 1, 2 }; float v[6] = { t.colorVar, t.speed, t.detail, t.glow, t.opacity, t.power };
    for (int i = 0; i < 6; ++i) if (UiSlider(x, y + 96 + i * 42.f, 560, nm[i], v[i], lo[i], hi[i])) ApplyTex(i + 1, v[i]);
    Emitter* e = SelectedEmitter();                                       // per-emitter geometry: always only the selected emitter
    if (e) {
        float ang = e->angleDeg, yaw = atan2f(e->hdgY, e->hdgX) * 57.29578f; if (yaw < 0) yaw += 360.f;
        if (UiSlider(x, y + 96 + 6 * 42.f, 560, "Jet Angle", ang, 5, 85)) { e->angleDeg = clampf(ang, 5.f, 85.f); S.profile.presetName.clear(); MarkDirty(); }
        if (UiSlider(x, y + 96 + 7 * 42.f, 560, "Direction", yaw, 0, 360)) { float r = yaw * 0.0174533f; e->hdgX = cosf(r); e->hdgY = sinf(r); S.profile.presetName.clear(); MarkDirty(); }
    } else Text("Select an emitter to edit its angle and direction", x, y + 96 + 6 * 42.f + 4, 16, kDim);
}
static void TabMirror(float x, float y) {
    SectionTitle(x, y, "Mirror Control");
    const char* nm[3] = { "Mirror Off", "Mirror All", "Selective Mirror" }; int md[3] = { MIRROR_OFF, MIRROR_ALL, MIRROR_SELECTIVE };
    for (int i = 0; i < 3; ++i) if (UiButton(x + i * 188.f, y + 44, 184, 40, nm[i], S.profile.mirrorMode == md[i], 16)) { S.profile.mirrorMode = md[i]; S.profile.presetName.clear(); MarkDirty(); }
    Text("Uses the vehicle's real left/right axis. Mirrored jets inherit the original's settings.", x, y + 96, 14, kWhite);
    Text("Selective mode: toggle M per emitter.", x, y + 118, 14, kWhite); EmitterPager(x, y + 148); EmitterList(x, y + 182, false);
}
static void TabAbout(float x, float y) {
    SectionTitle(x, y, "About NITRO FX");
    const char* l[] = { "One unified Jet effect for GTA San Andreas (PC 1.0 US).", "Fire, water, matrix, neon, magma... only restyle the same jet.", "All textures are generated in code: no image files.", "", "Menu: R + S + I (hold all three).   ESC closes.", "Hold ALT: menu hides and the camera is free; release to return.", "Smoke Edit Mode: click body = add, drag = move,", "hold ALT = camera, DEL = delete." };
    for (int i = 0; i < 8; ++i) Text(l[i], x, y + 50 + i * 30.f, 17, kWhite);
}
// ================= main menu =================
// Layout (virtual 1280x720, always inside the screen): panel 24..854 x 24..696; dragon showcase right of it (x >= 870).
void DrawMenu() {
    InputState real = In; if (S.dlg != DLG_NONE) { In.valid = false; In.lPressed = In.lDown = In.lReleased = false; In.wheel = 0; }   // dialog is modal: widgets underneath get no input
    float a = S.menuAlpha, t = S.time; Fill(C(0, 0, 0, (int)(150 * a)));
    DrawDragon(t, a);                                                    // right side, never overlaps the panel
    const float PX = 24, PY = 24, PW = 830, PH = 672; UiPanel(PX, PY, PW, PH, a);
    Text("NITRO FX", 52, 34, 42, kGoldHi); TextFit("UNIFIED JET SYSTEM  |  PREMIUM TUNING SUITE", 54, 90, 12, kDim, 280);
    // status block
    char b[200]; const float sx = 344, sw = 486; RectV(sx, 40, sw, 76, C(14, 14, 17, 235), C(8, 8, 10, 235)); Rect(sx, 40, sw, 1, kGoldDim); Rect(sx, 115, sw, 1, C(138, 112, 52, 120)); Rect(sx, 40, 3, 76, kGold);
    snprintf(b, sizeof b, "Active Emitters: %d     Mirrored: %d", CountActive(S.profile), CountMirrored(S.profile)); Text(b, sx + 16, 46, 16, kWhite);
    const char* mm[3] = { "Off", "All", "Selective" }; Emitter* se = SelectedEmitter(); const TexSettings& ct = se ? se->tex : S.newTex;
    snprintf(b, sizeof b, "Texture: %s     Mirror Mode: %s", kCategoryNames[ct.category], mm[S.profile.mirrorMode]); TextFit(b, sx + 16, 68, 16, kWhite, sw - 28);
    snprintf(b, sizeof b, "Effect: %s     Save: %s", S.profile.jetEnabled ? "ON" : "OFF", S.profile.presetName.empty() ? "None" : S.profile.presetName.c_str()); TextFit(b, sx + 16, 90, 16, kGold, sw - 28);
    if (!S.veh) TextFit("Enter a vehicle to place / see jets", 52, 536, 13, C(255, 176, 120), 214);
    RectH(44, 128, PW - 40, 1, C(212, 175, 85, 0), C(212, 175, 85, 160)); 
    // sidebar
    const char* tabs[6] = { "Emitter Positions", "Texture Library", "Effects Settings", "Mirror Control", "Saves", "About" };
    for (int i = 0; i < 6; ++i) if (UiButton(44, 146 + i * 54.f, 210, 46, tabs[i], S.tab == i, 17)) { S.tab = i; if (i == 4) ReloadSaves(); }
    Text("R+S+I  open / close", 52, 484, 13, kDim); Text("ESC  close   ALT  hide menu", 52, 504, 13, kDim);
    Rect(268, 146, 1, 466, C(212, 175, 85, 90));
    float cx = 282, cy = 144;
    switch (S.tab) { case 0: TabEmitters(cx, cy); break; case 1: TabTextures(cx, cy); break; case 2: TabEffects(cx, cy); break; case 3: TabMirror(cx, cy); break; case 4: DrawSavesTab(cx, cy); break; default: TabAbout(cx, cy); }
    RectH(44, 624, PW - 40, 1, C(212, 175, 85, 0), C(212, 175, 85, 120));
    if (!S.status.empty() && GetTickCount() - S.statusTime < 2500) TextFit(S.status.c_str(), PX + PW / 2, 630, 15, C(255, 196, 140), PW - 60, 1);
    DrawBranding(PX + PW / 2, 652, t);                                   // exact text: "Create by : Parsa Rsi"
    In = real; DrawDialog();                                             // real input again for the modal dialog (it is drawn above everything)
    UiCursor();
}
}
