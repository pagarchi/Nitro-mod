#include "nitrofx/App.h"
namespace nfx {
using namespace d2;
// ================= widgets =================
void UiPanel(float x, float y, float w, float h, float a) {
    RectV(x, y, w, h, C(24, 20, 34, (int)(238 * a)), C(8, 7, 12, (int)(240 * a)));
    RectH(x, y, w, 2, C(212, 175, 85, 0), C(212, 175, 85, (int)(230 * a))); RectH(x + w / 2, y, w / 2, 2, C(212, 175, 85, (int)(230 * a)), C(212, 175, 85, 0)); // metallic top highlight
    Rect(x, y + h - 1, w, 1, C(212, 175, 85, (int)(150 * a))); Rect(x, y, 1, h, C(212, 175, 85, (int)(150 * a))); Rect(x + w - 1, y, 1, h, C(212, 175, 85, (int)(150 * a)));
    Rect(x + 3, y + 3, w - 6, 1, C(140, 70, 230, (int)(70 * a)));
    float cs[4][2] = { { x, y }, { x + w, y }, { x, y + h }, { x + w, y + h } };               // gold corner diamonds
    for (int i = 0; i < 4; ++i) { float cx = cs[i][0], cy = cs[i][1]; Col g = C(236, 198, 100, (int)(255 * a)); Tri(cx - 6, cy, g, cx, cy - 6, g, cx + 6, cy, g); Tri(cx - 6, cy, g, cx + 6, cy, g, cx, cy + 6, g); }
}
bool UiButton(float x, float y, float w, float h, const char* label, bool sel, float px) {
    bool hov = InRect(x, y, w, h); bool click = hov && In.lPressed;
    Col t = sel ? C(58, 40, 86, 245) : (hov ? C(44, 36, 62, 245) : C(26, 22, 38, 235)), b = sel ? C(30, 18, 50, 245) : C(14, 12, 22, 240);
    RectV(x, y, w, h, t, b); Rect(x, y, w, 1, sel || hov ? kGold : C(212, 175, 85, 110)); Rect(x, y + h - 1, w, 1, C(212, 175, 85, 70));
    if (sel) { Rect(x, y, 4, h, kGoldHi); SetAdditive(true); RectH(x, y, w * .6f, h, C(150, 80, 235, 40), C(150, 80, 235, 0)); SetAdditive(false); }
    Text(label, x + w / 2, y + (h - px) / 2 - 1, px, sel ? kGoldHi : kWhite, 1); return click;
}
bool UiToggle(float x, float y, float w, float h, const char* label, bool on) {
    char b[96]; snprintf(b, sizeof b, "%s: %s", label, on ? "ON" : "OFF"); return UiButton(x, y, w, h, b, on, 16);
}
bool UiSlider(float x, float y, float w, const char* label, float& v, float lo, float hi) {
    static const void* active = nullptr; const void* id = &v; bool changed = false;
    float tx = x + 170, tw = w - 170 - 56;
    Text(label, x, y, 18, kWhite); Rect(tx, y + 11, tw, 4, C(60, 52, 80, 255));
    float k = clampf((v - lo) / (hi - lo), 0, 1); Rect(tx, y + 11, tw * k, 4, kGold);
    if (In.lPressed && InRect(tx - 8, y, tw + 16, 26)) active = id;
    if (!In.lDown) active = nullptr;
    if (active == id && In.valid) { float nv = lo + clampf((In.mx - tx) / tw, 0, 1) * (hi - lo); if (nv != v) { v = nv; changed = true; } }
    Circle(tx + tw * k, y + 13, 8, kGoldHi, kGold, 14); char b[16]; snprintf(b, sizeof b, "%.2f", v); Text(b, x + w - 48, y, 17, kGoldHi); return changed;
}
void UiCursor() { if (!In.valid) return; float x = In.mx, y = In.my; Tri(x, y, kGoldHi, x + 15, y + 11, kGold, x + 4, y + 17, kGold); Tri(x, y, C(0, 0, 0, 160), x + 17, y + 12, C(0, 0, 0, 0), x + 5, y + 19, C(0, 0, 0, 0)); }

// ================= branding: "Create by : Parsa Rsi" (live text: glow, glitch, RGB split, light sweep, metallic) =================
void DrawBranding(float cx, float y, float t) {
    const char* pre = "Create by : "; const char* name = "Parsa Rsi";
    const float pp = 17, pn = 27; float wp = TextWidth(pre, pp), wn = TextWidth(name, pn); float x0 = cx - (wp + wn) / 2, nx = x0 + wp;
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
static TexSettings& CurTex() { Emitter* e = SelectedEmitter(); return e ? e->tex : S.newTex; }
static void ApplyTex(int field, float val) {
    struct F { static void Set(TexSettings& t, int f, float v) { switch (f) { case 0: t.category = (int)v; break; case 1: t.colorVar = v; break; case 2: t.speed = v; break; case 3: t.detail = v; break; case 4: t.glow = v; break; case 5: t.opacity = v; break; case 6: t.power = v; break; } t.Clamp(); } };
    Emitter* e = SelectedEmitter();
    if (S.applyAll || !e) { for (size_t i = 0; i < S.profile.emitters.size(); ++i) F::Set(S.profile.emitters[i].tex, field, val); F::Set(S.newTex, field, val); }
    else F::Set(e->tex, field, val);
    S.profile.presetName.clear(); MarkDirty();
}
static int PageCount() { int n = (int)S.profile.emitters.size(); return n < 1 ? 1 : (n + 15) / 16; }
static void EmitterPager(float x, float y) {          // "Emitters n/64  Page a/b" + prev/next (16 emitters per page)
    int pages = PageCount(); if (S.emitterPage >= pages) S.emitterPage = pages - 1; if (S.emitterPage < 0) S.emitterPage = 0;
    char b[64]; snprintf(b, sizeof b, "Emitters %d/%d   Page %d/%d", (int)S.profile.emitters.size(), kMaxEmitters, S.emitterPage + 1, pages); Text(b, x, y + 2, 16, kPurple);
    if (UiButton(x + 250, y, 34, 24, "<", false, 14) && S.emitterPage > 0) --S.emitterPage;
    if (UiButton(x + 290, y, 34, 24, ">", false, 14) && S.emitterPage < pages - 1) ++S.emitterPage;
}
static void EmitterList(float x, float y, bool full) {
    const int per = 16; if (S.emitterPage >= PageCount()) S.emitterPage = PageCount() - 1;
    for (int k = 0; k < per; ++k) { size_t i = (size_t)S.emitterPage * per + k; if (i >= S.profile.emitters.size()) break; Emitter& e = S.profile.emitters[i];
        float cx = x + (k / 8) * 255.f, cy = y + (k % 8) * 26.f; char b[32]; snprintf(b, sizeof b, "Emitter #%d", e.id);
        if (UiButton(cx, cy, full ? 108 : 150, 24, b, e.id == S.selectedId, 14)) S.selectedId = e.id;
        float bx = cx + (full ? 112 : 154);
        if (full && UiButton(bx, cy, 40, 24, e.active ? "ON" : "OFF", e.active, 13)) { e.active = !e.active; MarkDirty(); }
        if (UiButton(full ? bx + 44 : bx, cy, 34, 24, "M", e.mirror, 14)) { e.mirror = !e.mirror; MarkDirty(); }
        if (full && UiButton(bx + 82, cy, 28, 24, "X", false, 14)) { if (S.selectedId == e.id) S.selectedId = -1; S.profile.emitters.erase(S.profile.emitters.begin() + i); MarkDirty(); break; } }
}

// ================= tabs =================
static void TabEmitters(float x, float y) {
    Text("Emitter Positions", x, y - 4, 26, kGold);
    if (UiButton(x, y + 32, 250, 36, "Open Smoke Edit Mode", false, 18)) EnterEditMode();
    if (UiButton(x + 255, y + 32, 250, 36, "Reset Vehicle To Default", false, 17)) { if (S.haveBounds) AskResetVehicle(); else SetStatus("Enter a vehicle first"); }
    for (int i = 0; i < kSlotCount; ++i) if (UiButton(x + (i % 2) * 255.f, y + 78 + (i / 2) * 38.f, 250, 34, kSlots[i].name, false, 16)) {
        if (!S.veh || !S.haveBounds) SetStatus("Enter a vehicle first");
        else if (!AddSlotEmitter(S.profile, i, S.bbMin, S.bbMax, S.newTex)) SetStatus("Emitter limit reached"); else { S.selectedId = S.profile.nextId - 1; S.emitterPage = ((int)S.profile.emitters.size() - 1) / 16; MarkDirty(); } }
    EmitterPager(x, y + 234); Text("M = mirror, X = delete", x + 335, y + 237, 14, kWhite); EmitterList(x, y + 260, true);
}
static void TabTextures(float x, float y) {
    Text("Texture Library", x, y - 4, 26, kGold); int cur = CurTex().category;
    for (int i = 0; i < CAT_COUNT; ++i) if (UiButton(x + (i % 3) * 170.f, y + 34 + (i / 3) * 46.f, 164, 40, kCategoryNames[i], i == cur, 16)) ApplyTex(0, (float)i);
    float py = y + 34 + 5 * 46.f + 6; RwTexture* tx = gTex.Get(CurTex());
    Rect(x, py, 230, 96, C(0, 0, 0, 220)); SetAdditive(false); if (tx) TexRect(x + 2, py + 2, 226, 92, tx, 0.1f, S.time * -0.8f, 0.9f, S.time * -0.8f + 1.0f, C(255, 255, 255, 255));
    if (UiToggle(x + 245, py, 255, 36, "Apply to all emitters", S.applyAll)) S.applyAll = !S.applyAll;
    Emitter* e = SelectedEmitter(); char b[64]; snprintf(b, sizeof b, "Editing: %s", e ? "selected emitter" : "all emitters / new ones"); Text(b, x + 245, py + 48, 15, kWhite);
}
static void TabEffects(float x, float y) {
    Text("Effects Settings", x, y - 4, 26, kGold);
    if (UiToggle(x, y + 30, 250, 34, "Jet Effect", S.profile.jetEnabled)) { S.profile.jetEnabled = !S.profile.jetEnabled; MarkDirty(); }
    if (UiToggle(x + 258, y + 30, 250, 34, "Apply to all", S.applyAll)) S.applyAll = !S.applyAll;
    TexSettings t = CurTex(); const char* nm[6] = { "Color Variation", "Texture Speed", "Detail Density", "Glow Intensity", "Opacity", "Effect Power" };
    float lo[6] = { 0, 0.1f, 0, 0, 0.1f, 0.3f }, hi[6] = { 1, 4, 1, 2, 1, 2 }; float v[6] = { t.colorVar, t.speed, t.detail, t.glow, t.opacity, t.power };
    for (int i = 0; i < 6; ++i) if (UiSlider(x, y + 84 + i * 46.f, 500, nm[i], v[i], lo[i], hi[i])) ApplyTex(i + 1, v[i]);
    Emitter* e = SelectedEmitter();                                       // per-emitter geometry: always only the selected emitter
    if (e) {
        float ang = e->angleDeg, yaw = atan2f(e->hdgY, e->hdgX) * 57.29578f; if (yaw < 0) yaw += 360.f;
        if (UiSlider(x, y + 84 + 6 * 46.f, 500, "Jet Angle", ang, 5, 85)) { e->angleDeg = clampf(ang, 5.f, 85.f); S.profile.presetName.clear(); MarkDirty(); }
        if (UiSlider(x, y + 84 + 7 * 46.f, 500, "Direction", yaw, 0, 360)) { float r = yaw * 0.0174533f; e->hdgX = cosf(r); e->hdgY = sinf(r); S.profile.presetName.clear(); MarkDirty(); }
    } else Text("Select an emitter to edit its angle and direction", x, y + 84 + 6 * 46.f + 4, 16, kPurple);
}
static void TabMirror(float x, float y) {
    Text("Mirror Control", x, y - 4, 26, kGold);
    const char* nm[3] = { "Mirror Off", "Mirror All", "Selective Mirror" }; int md[3] = { MIRROR_OFF, MIRROR_ALL, MIRROR_SELECTIVE };
    for (int i = 0; i < 3; ++i) if (UiButton(x + i * 170.f, y + 34, 164, 38, nm[i], S.profile.mirrorMode == md[i], 16)) { S.profile.mirrorMode = md[i]; S.profile.presetName.clear(); MarkDirty(); }
    Text("Uses the vehicle's real left/right axis. Mirrored jets inherit the original's settings.", x, y + 82, 15, kWhite);
    Text("Selective mode: toggle M per emitter.", x, y + 104, 15, kWhite); EmitterPager(x, y + 128); EmitterList(x, y + 160, false);
}
static void TabAbout(float x, float y) {
    Text("About NITRO FX", x, y - 4, 26, kGold);
    const char* l[] = { "One unified Jet effect for GTA San Andreas (PC 1.0 US).", "Fire, water, matrix, neon, magma... only restyle the same jet.", "All textures are generated in code: no image files.", "", "Menu: R + S + I (hold all three).   ESC closes.", "Smoke Edit Mode: click body = add, drag = move,", "hold ALT = camera, DEL = delete." };
    for (int i = 0; i < 7; ++i) Text(l[i], x, y + 36 + i * 28.f, 17, kWhite);
}
// ================= main menu =================
void DrawMenu() {
    InputState real = In; if (S.dlg != DLG_NONE) { In.valid = false; In.lPressed = In.lDown = In.lReleased = false; }   // dialog is modal: widgets underneath get no input
    float a = S.menuAlpha, t = S.time; Rect(0, 0, VW, VH, C(0, 0, 0, (int)(130 * a)));
    DrawDragon(t, a);                                                    // right side, never overlaps the panel (x >= 790)
    UiPanel(40, 40, 760, 640, a);
    Text("NITRO FX", 66, 52, 38, kGoldHi); Text("UNIFIED JET SYSTEM", 70, 94, 14, kPurple);
    // status block
    char b[160]; Rect(300, 52, 480, 82, C(10, 8, 16, 200)); Rect(300, 52, 480, 1, C(212, 175, 85, 120));
    snprintf(b, sizeof b, "Active Emitters: %d     Mirrored: %d", CountActive(S.profile), CountMirrored(S.profile)); Text(b, 312, 58, 16, kWhite);
    const char* mm[3] = { "Off", "All", "Selective" }; Emitter* se = SelectedEmitter(); const TexSettings& ct = se ? se->tex : S.newTex;
    snprintf(b, sizeof b, "Texture: %s     Mirror Mode: %s", kCategoryNames[ct.category], mm[S.profile.mirrorMode]); Text(b, 312, 80, 16, kWhite);
    snprintf(b, sizeof b, "Effect: %s     Save: %s", S.profile.jetEnabled ? "ON" : "OFF", S.profile.presetName.empty() ? "None" : S.profile.presetName.c_str()); Text(b, 312, 102, 16, kGold);
    if (!S.veh) Text("Enter a vehicle to place / see jets", 66, 118, 14, C(255, 170, 120));
    const char* tabs[6] = { "Emitter Positions", "Texture Library", "Effects Settings", "Mirror Control", "Saves", "About" };
    for (int i = 0; i < 6; ++i) if (UiButton(62, 150 + i * 54.f, 190, 46, tabs[i], S.tab == i, 16)) { S.tab = i; if (i == 4) ReloadSaves(); }
    Rect(262, 150, 1, 460, C(212, 175, 85, 90));
    float cx = 280, cy = 160;
    switch (S.tab) { case 0: TabEmitters(cx, cy); break; case 1: TabTextures(cx, cy); break; case 2: TabEffects(cx, cy); break; case 3: TabMirror(cx, cy); break; case 4: DrawSavesTab(cx, cy); break; default: TabAbout(cx, cy); }
    if (!S.status.empty() && GetTickCount() - S.statusTime < 2500) Text(S.status.c_str(), 420, 612, 15, C(255, 190, 140), 1);
    DrawBranding(420, 636, t);                                           // exact text: "Create by : Parsa Rsi"
    In = real; DrawDialog();                                             // real input again for the modal dialog (it is drawn above everything)
    UiCursor();
}
}
