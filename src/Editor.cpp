#include "nitrofx/App.h"
#include "nitrofx/MeshMath.h"
namespace nfx {
// ---- screen -> world ray, calibrated through the game's own projection (CSprite::CalcScreenCoors) ----
static bool Project(const V3& w, float& gx, float& gy) {   // game-screen pixels
    RwV3d in; in.x = w.x; in.y = w.y; in.z = w.z; RwV3d o; float a, b; if (!CSprite::CalcScreenCoors(in, &o, &a, &b, false, false)) return false; gx = o.x; gy = o.y; return true;
}
static bool ScreenRay(float vx, float vy, V3& origin, V3& dir) {
    if (!TheCamera.m_matrix) return false; CMatrix* m = TheCamera.m_matrix;
    V3 ax[3] = { V3(m->right.x, m->right.y, m->right.z), V3(m->up.x, m->up.y, m->up.z), V3(m->at.x, m->at.y, m->at.z) };
    CVector cp = TheCamera.GetPosition(); origin = V3(cp.x, cp.y, cp.z);
    float W = (float)RsGlobal.maximumWidth, H = (float)RsGlobal.maximumHeight, cx = W * .5f, cy = H * .5f;
    int fi = -1; float best = 1e9f, fx = 0, fy = 0;                  // forward axis = the one that projects nearest the screen centre
    for (int i = 0; i < 3; ++i) { float sx, sy; if (Project(origin + ax[i] * 10.f, sx, sy)) { float d = (sx - cx) * (sx - cx) + (sy - cy) * (sy - cy); if (d < best) { best = d; fi = i; fx = sx; fy = sy; } } }
    if (fi < 0) return false;
    int a = (fi + 1) % 3, b = (fi + 2) % 3; V3 P0 = origin + ax[fi] * 10.f; float ax_, ay_, bx_, by_;
    if (!Project(P0 + ax[a], ax_, ay_) || !Project(P0 + ax[b], bx_, by_)) return false;
    float dxa = ax_ - fx, dya = ay_ - fy, dxb = bx_ - fx, dyb = by_ - fy, det = dxa * dyb - dxb * dya; if (fabsf(det) < 1e-3f) return false;
    float ex = vx * W / d2::VW - fx, ey = vy * H / d2::VH - fy;
    float al = (ex * dyb - dxb * ey) / det, be = (dxa * ey - ex * dya) / det;
    dir = vnorm((P0 + ax[a] * al + ax[b] * be) - origin); return true;
}
// ray vs the vehicle's actual rendered triangle mesh (MeshPick.cpp); returns the surface point + surface normal in VEHICLE-LOCAL space
static bool RayVehicle(float vx, float vy, V3& hit, V3& nrm) {
    if (!S.veh) return false; V3 o, d; if (!ScreenRay(vx, vy, o, d)) return false;
    V3 wh, wn; if (!PickVehicleMesh(S.veh, o, d, wh, wn)) return false;               // real triangles only; no bounding-box fallback
    V3 r, f, u, p; if (!VehicleBasis(S.veh, r, f, u, p) || !BasisUsable(r, f, u, p)) return false;
    V3 lh = WorldPointToLocal(wh, r, f, u, p);                                        // exact local coordinates of the picked surface point
    V3 ln = vnorm(WorldDirToLocal3(wn, r, f, u));
    if (!FiniteV3(lh) || !FiniteV3(ln)) return false;
    nrm = ln; hit = lh + ln * 0.02f; return true;                                     // 2 cm off the surface so the jet is not z-fighting
}
static void Heading(const V3& pos, const V3& n, float& hx, float& hy) {
    float x = n.x, y = n.y; if (fabsf(x) < 0.3f && fabsf(y) < 0.3f) { x = pos.x >= 0 ? 1.f : -1.f; y = 0; }
    float l = sqrtf(x * x + y * y); hx = x / l; hy = y / l;
}
static bool AnyUi(const float r[][4], int n) { for (int i = 0; i < n; ++i) if (InRect(r[i][0], r[i][1], r[i][2], r[i][3])) return true; return false; }
static void Exit(bool confirm) {
    if (confirm) { S.profile.presetName.clear(); MarkDirty(); SaveNow(); } else S.profile = S.backup;
    S.editMode = false; S.dragging = false; S.menuOpen = true; S.menuAlpha = 1.f; CPad* p = CPad::GetPad(0); if (p) p->DisablePlayerControls = false;
}
static void DeleteSelected() {
    for (size_t i = 0; i < S.profile.emitters.size(); ++i) if (S.profile.emitters[i].id == S.selectedId) { S.profile.emitters.erase(S.profile.emitters.begin() + i); S.selectedId = -1; return; }
}
void DrawEditor() {
    using namespace d2;
    if (!S.veh) { Exit(false); return; }
    static bool wasAlt = false, skip = false; static POINT saved;
    bool alt = In.altDown;
    if (alt && !wasAlt) GetCursorPos(&saved);                          // remember cursor so ALT camera look cannot make it jump
    if (!alt && wasAlt) { SetCursorPos(saved.x, saved.y); skip = true; }
    wasAlt = alt; S.dragging = S.dragging && !alt;
    const float R[][4] = { { 20, 20, 400, 190 }, { 440, 640, 190, 44 }, { 640, 640, 190, 44 }, { 840, 640, 190, 44 } };
    // --- editing logic (disabled while ALT is held, and for one frame after release) ---
    if (!alt && !skip && In.valid) {
        if (In.lPressed && !AnyUi(R, 4)) {
            int hitId = -1; float bd = 26 * 26;
            for (size_t i = 0; i < S.profile.emitters.size(); ++i) { float sx, sy; if (WorldToScreen(LocalToWorld(S.veh, S.profile.emitters[i].pos), sx, sy)) { float d = (sx - In.mx) * (sx - In.mx) + (sy - In.my) * (sy - In.my); if (d < bd) { bd = d; hitId = S.profile.emitters[i].id; } } }
            if (hitId >= 0) { S.selectedId = hitId; S.dragging = true; }
            else { V3 h, n; if (RayVehicle(In.mx, In.my, h, n)) {
                if ((int)S.profile.emitters.size() >= kMaxEmitters) SetStatus("Emitter limit reached (64)");
                else { Emitter e; e.id = S.profile.nextId++; e.pos = h; Heading(h, n, e.hdgX, e.hdgY); e.tex = S.newTex; S.profile.emitters.push_back(e); S.selectedId = e.id; S.dragging = true; S.emitterPage = ((int)S.profile.emitters.size() - 1) / 16; }
            } else SetStatus("No vehicle surface under cursor"); }
        }
        if (S.dragging && In.lDown) { Emitter* e = SelectedEmitter(); V3 h, n; if (e && RayVehicle(In.mx, In.my, h, n)) { e->pos = h; Heading(h, n, e->hdgX, e->hdgY); } }
        if (In.lReleased && S.dragging) { S.dragging = false; MarkDirty(); }
        if (In.delPressed) DeleteSelected();
    }
    skip = false;
    // --- drawing ---
    Rect(0, 0, VW, VH, C(0, 0, 0, 40));
    Text("SMOKE EDIT MODE", VW / 2, 14, 40, kGoldHi, 1);
    UiPanel(R[0][0], R[0][1], R[0][2], R[0][3], 1.f);
    const char* lines[5] = { "Click on vehicle body to add emitter", "Hold and drag to move emitter", "Hold ALT to control camera", "DEL deletes the selected emitter", "Confirm Position  /  Cancel Editing" };
    for (int i = 0; i < 5; ++i) Text(lines[i], 40, 36 + i * 32.f, 19, i == 2 ? kGoldHi : kWhite);
    if (alt) Text("CAMERA CONTROL (ALT)", VW / 2, 70, 24, C(190, 130, 255), 1);
    if (UiButton(R[1][0], R[1][1], R[1][2], R[1][3], "Confirm Position", false, 18)) Exit(true);
    if (UiButton(R[2][0], R[2][1], R[2][2], R[2][3], "Cancel Editing", false, 18)) Exit(false);
    if (UiButton(R[3][0], R[3][1], R[3][2], R[3][3], "Delete Emitter", false, 18)) DeleteSelected();
    std::vector<Resolved> res; ResolveEmitters(S.profile, res);
    for (size_t i = 0; i < res.size(); ++i) { float sx, sy; if (!WorldToScreen(LocalToWorld(S.veh, res[i].pos), sx, sy)) continue;
        bool sel = res[i].src->id == S.selectedId; Col c = res[i].mirrored ? C(170, 110, 255, 200) : (sel ? kGoldHi : C(255, 255, 255, 230));
        float rr = res[i].mirrored ? 6.f : 9.f; Circle(sx, sy, rr + 3, CA(c, .25f), CA(c, 0), 16); Circle(sx, sy, rr, CA(c, .35f), c, 16);
        float ang = res[i].angle * 0.0174533f; V3 dl(res[i].hx * cosf(ang), res[i].hy * cosf(ang), sinf(ang)); float ex, ey;
        if (WorldToScreen(LocalToWorld(S.veh, res[i].pos + dl * 0.6f), ex, ey)) Line(sx, sy, ex, ey, 2.f, c); }
    if (!S.status.empty() && GetTickCount() - S.statusTime < 2500) Text(S.status.c_str(), VW / 2, 600, 20, C(255, 160, 160), 1);
    UiCursor();
}
}
