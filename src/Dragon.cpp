#include "nitrofx/App.h"
// Procedural, fully animated dragon drawn from smooth vector strips every frame (no image assets, no allocations).
// Metallic gold dorsal plating, platinum flanks, graphite belly, bat wings with curved finger bones, horned head with glowing eye.
// Lives entirely in the showcase area x >= 870 (the menu panel ends at x = 854), so it never covers a control.
namespace nfx {
using namespace d2;
static const int N = 34;                                   // spine samples
struct Fr { float x, y, tx, ty, nx, ny, th; };            // spine frame: position, tangent, normal ("up" on screen), half thickness
static Col A(Col c, float a) { return CA(c, a); }
static void Bone(float x1, float y1, float x2, float y2, float w, float al) {                       // metallic bone: dark core + gold edge + platinum highlight
    Line(x1, y1, x2, y2, w + 2.2f, C(40, 30, 14, (int)(255 * al))); Line(x1, y1, x2, y2, w, C(196, 158, 72, (int)(255 * al))); Line(x1, y1, x2, y2, w * .38f, C(246, 238, 214, (int)(190 * al)));
}
static void Wing(const Fr& sh, float t, float ph, float sc, float dark, float al) {
    float f = sinf(t * 1.9f + ph);
    float a1 = -1.25f + 0.55f * f, a2 = a1 + 0.95f + 0.28f * sinf(t * 1.9f + ph - 0.8f);              // upper arm / forearm angles
    float ex = sh.x + cosf(a1) * 100 * sc, ey = sh.y + sinf(a1) * 100 * sc, wx = ex + cosf(a2) * 100 * sc, wy = ey + sinf(a2) * 100 * sc;
    static const float len[4] = { 150, 156, 140, 108 }, off[4] = { -0.70f, -0.10f, 0.55f, 1.15f }; float tx[4], ty[4];
    for (int k = 0; k < 4; ++k) { float ak = a2 + off[k] + 0.22f * sinf(t * 1.9f + ph - 0.9f - 0.28f * k) + 0.30f * f; tx[k] = wx + cosf(ak) * len[k] * sc; ty[k] = wy + sinf(ak) * len[k] * sc; }
    float bx = sh.x + sh.tx * 120 * sc, by = sh.y + sh.ty * 120 * sc;                                      // trailing root on the body
    Col mi = C((int)(60 * dark + 8), (int)(46 * dark + 6), (int)(28 * dark + 6), (int)(240 * al)), mo = C((int)(150 * dark + 20), (int)(112 * dark + 14), (int)(56 * dark + 10), (int)(196 * al));
    float mx[5], my[5]; for (int k = 0; k < 4; ++k) { mx[k] = tx[k]; my[k] = ty[k]; } mx[4] = bx; my[4] = by;
    for (int k = 0; k < 4; ++k) {                                                                           // scalloped membrane panels
        float nx2 = mx[k + 1], ny2 = my[k + 1]; float cx = (mx[k] + nx2) * .5f + (wx - (mx[k] + nx2) * .5f) * 0.20f, cy = (my[k] + ny2) * .5f + (wy - (my[k] + ny2) * .5f) * 0.20f;
        Tri(wx, wy, mi, mx[k], my[k], mo, cx, cy, A(mo, 0.9f)); Tri(wx, wy, mi, cx, cy, A(mo, 0.9f), nx2, ny2, mo);
        Line(mx[k], my[k], cx, cy, 1.6f, C(224, 186, 96, (int)(170 * al * dark))); Line(cx, cy, nx2, ny2, 1.6f, C(224, 186, 96, (int)(170 * al * dark))); }
    Tri(wx, wy, mi, bx, by, mo, sh.x, sh.y, mi);
    SetAdditive(true); for (int k = 0; k < 4; ++k) Line(wx, wy, lerpf(wx, tx[k], .6f), lerpf(wy, ty[k], .6f), 5.f, C(255, 220, 140, (int)(26 * al * dark))); SetAdditive(false);   // soft sheen
    Bone(sh.x, sh.y, ex, ey, 7.0f * sc, al); Bone(ex, ey, wx, wy, 5.6f * sc, al);
    for (int k = 0; k < 4; ++k) { Bone(wx, wy, tx[k], ty[k], 3.4f * sc, al); float ux = cosf(atan2f(ty[k] - wy, tx[k] - wx)), uy = sinf(atan2f(ty[k] - wy, tx[k] - wx));
        Tri(tx[k] + ux * 12 * sc, ty[k] + uy * 12 * sc, C(246, 238, 214, (int)(255 * al)), tx[k] - uy * 3.2f, ty[k] + ux * 3.2f, C(196, 158, 72, (int)(255 * al)), tx[k] + uy * 3.2f, ty[k] - ux * 3.2f, C(196, 158, 72, (int)(255 * al))); }   // claw tips
    Circle(ex, ey, 6 * sc, C(246, 226, 160, (int)(255 * al)), C(176, 138, 58, (int)(255 * al)), 12); Circle(wx, wy, 7.5f * sc, C(250, 236, 180, (int)(255 * al)), C(176, 138, 58, (int)(255 * al)), 12);   // joints
}
static void Horn(float bx, float by, float dx, float dy, float len, float curl, float w, float al, float t) {     // curved tapered horn: gold root -> platinum tip
    float px = bx, py = by, ang = atan2f(dy, dx); const int seg = 7;
    for (int i = 0; i < seg; ++i) { float u0 = i / (float)seg, u1 = (i + 1) / (float)seg; float a0 = ang + curl * u0 * u0, a1 = ang + curl * u1 * u1;
        float x2 = px + cosf(a1) * len / seg, y2 = py + sinf(a1) * len / seg; float w0 = w * (1 - u0), w1 = w * (1 - u1);
        float n0x = -sinf(a0), n0y = cosf(a0), n1x = -sinf(a1), n1y = cosf(a1); Col c0 = CMix(C(214, 172, 80), C(246, 240, 226), u0), c1 = CMix(C(214, 172, 80), C(246, 240, 226), u1); c0.a = c1.a = (unsigned char)(255 * al);
        Quad(px + n0x * w0, py + n0y * w0, c0, x2 + n1x * w1, y2 + n1y * w1, c1, x2 - n1x * w1, y2 - n1y * w1, CMix(c1, C(60, 46, 20, (int)(255 * al)), .45f), px - n0x * w0, py - n0y * w0, CMix(c0, C(60, 46, 20, (int)(255 * al)), .45f));
        px = x2; py = y2; }
}
void DrawDragon(float t, float alpha) {
    if (alpha <= 0.01f) return; const float al = alpha;
    // soft gold ambience + rising embers (very subtle; purple is not used here)
    SetAdditive(true); Circle(1080, 380, 230, C(212, 175, 85, (int)(26 * al)), C(212, 175, 85, 0), 40);
    for (int i = 0; i < 14; ++i) { float ph = t * 0.30f + i * 0.67f; float ex = 890 + fmodf(i * 89.f + ph * 38.f, 360.f), ey = 670 - fmodf(ph * 50.f + i * 37.f, 600.f); Circle(ex, ey, 1.8f, C(255, 214, 130, (int)(120 * al)), C(255, 170, 70, 0), 8); }
    SetAdditive(false);
    // ---- spine: smooth S-curve with a travelling wave, gentle bob ----
    static Fr sp[N + 1]; float bob = sinf(t * 1.3f) * 10.f;
    for (int i = 0; i <= N; ++i) { float u = i / (float)N;
        sp[i].x = 925 + 260 * u + sinf(t * 2.0f - u * 5.2f) * (3 + 22 * u) * u + 20 * sinf(u * 3.1f);
        sp[i].y = 250 + 395 * u * (1 - 0.12f * u) + sinf(t * 2.0f - u * 5.2f - 1.2f) * (4 + 14 * u) + bob;
        sp[i].th = u < 0.30f ? 12 + 26 * smoothstepf(0, 0.30f, u) : 38 * powf(1 - (u - 0.30f) / 0.70f, 1.35f) + 2.5f; }
    for (int i = 0; i <= N; ++i) { int a = i > 0 ? i - 1 : i, b = i < N ? i + 1 : i; float dx = sp[b].x - sp[a].x, dy = sp[b].y - sp[a].y, l = sqrtf(dx * dx + dy * dy) + 1e-4f;
        sp[i].tx = dx / l; sp[i].ty = dy / l; sp[i].nx = sp[i].ty; sp[i].ny = -sp[i].tx; if (sp[i].nx > 0) { sp[i].nx = -sp[i].nx; sp[i].ny = -sp[i].ny; } }   // normal always points to the upper-left ("dorsal" side)
    Wing(sp[7], t, 0.55f, 0.62f, 0.55f, al);                                                                  // far wing (behind the body)
    // ---- body strips: gold dorsal -> platinum flank -> graphite belly, with a travelling specular band ----
    for (int i = 0; i < N; ++i) { const Fr& p = sp[i]; const Fr& q = sp[i + 1]; float u = i / (float)N;
        float sh = 0.5f + 0.5f * sinf(u * 9.0f - t * 1.5f); Col gold = CMix(C(196, 154, 64), C(255, 232, 150), sh * .75f), plat = CMix(C(150, 154, 168), C(238, 240, 248), sh * .55f), dark = C(30, 29, 36);
        gold.a = plat.a = dark.a = (unsigned char)(255 * al);
        float tp = p.th, tq = q.th;
        Quad(p.x + p.nx * tp, p.y + p.ny * tp, gold, q.x + q.nx * tq, q.y + q.ny * tq, gold, q.x + q.nx * tq * .35f, q.y + q.ny * tq * .35f, plat, p.x + p.nx * tp * .35f, p.y + p.ny * tp * .35f, plat);
        Quad(p.x + p.nx * tp * .35f, p.y + p.ny * tp * .35f, plat, q.x + q.nx * tq * .35f, q.y + q.ny * tq * .35f, plat, q.x - q.nx * tq * .4f, q.y - q.ny * tq * .4f, CMix(plat, dark, .55f), p.x - p.nx * tp * .4f, p.y - p.ny * tp * .4f, CMix(plat, dark, .55f));
        Quad(p.x - p.nx * tp * .4f, p.y - p.ny * tp * .4f, CMix(plat, dark, .55f), q.x - q.nx * tq * .4f, q.y - q.ny * tq * .4f, CMix(plat, dark, .55f), q.x - q.nx * tq, q.y - q.ny * tq, dark, p.x - p.nx * tp, p.y - p.ny * tp, dark);
        // overlapping scale chevrons
        float mx = (p.x + q.x) * .5f, my = (p.y + q.y) * .5f, mt = (tp + tq) * .5f;
        for (int k = 0; k < 3; ++k) { float o = (k - 1) * mt * .55f, cx = mx + p.nx * o, cy = my + p.ny * o, s2 = mt * .26f + 1.2f;
            Line(cx - p.tx * s2 + p.nx * s2, cy - p.ty * s2 + p.ny * s2, cx + p.tx * s2 * 1.2f, cy + p.ty * s2 * 1.2f, 1.1f, C(40, 30, 12, (int)(120 * al)));
            Line(cx - p.tx * s2 - p.nx * s2, cy - p.ty * s2 - p.ny * s2, cx + p.tx * s2 * 1.2f, cy + p.ty * s2 * 1.2f, 1.1f, C(40, 30, 12, (int)(120 * al))); }
        if (u > 0.04f && u < 0.92f && (i & 1) == 0) { float h = 6 + tp * .42f; float ax = p.x + p.nx * tp, ay = p.y + p.ny * tp, bx = q.x + q.nx * tq, by = q.y + q.ny * tq;     // dorsal spikes
            Tri(ax, ay, C(176, 138, 58, (int)(255 * al)), bx, by, C(120, 92, 36, (int)(255 * al)), (ax + bx) * .5f + p.nx * h + p.tx * h * .5f, (ay + by) * .5f + p.ny * h + p.ty * h * .5f, C(255, 238, 176, (int)(255 * al))); }
        if (i > 3) Line(p.x - p.nx * tp * .7f - p.tx * 2, p.y - p.ny * tp * .7f - p.ty * 2, p.x - p.nx * tp * .1f, p.y - p.ny * tp * .1f, 1.0f, C(210, 212, 224, (int)(60 * al)));   // belly plates
    }
    for (int i = 0; i < N; ++i) Line(sp[i].x + sp[i].nx * sp[i].th, sp[i].y + sp[i].ny * sp[i].th, sp[i + 1].x + sp[i + 1].nx * sp[i + 1].th, sp[i + 1].y + sp[i + 1].ny * sp[i + 1].th, 1.8f, C(240, 206, 112, (int)(235 * al)));   // gold rim light
    SetAdditive(true); for (int i = 3; i < N - 3; ++i) { float u = i / (float)N, k = 0.5f + 0.5f * sinf(u * 9.f - t * 1.5f); Line(sp[i].x + sp[i].nx * sp[i].th * .5f, sp[i].y + sp[i].ny * sp[i].th * .5f, sp[i + 1].x + sp[i + 1].nx * sp[i + 1].th * .5f, sp[i + 1].y + sp[i + 1].ny * sp[i + 1].th * .5f, 2.4f, C(255, 250, 230, (int)(46 * k * al))); } SetAdditive(false);
    // ---- tail blade ----
    { const Fr& e = sp[N]; float bl = 34; Tri(e.x + e.nx * 11, e.y + e.ny * 11, C(214, 172, 80, (int)(255 * al)), e.x - e.nx * 11, e.y - e.ny * 11, C(96, 74, 30, (int)(255 * al)), e.x + e.tx * bl, e.y + e.ty * bl, C(250, 244, 226, (int)(255 * al)));
      Tri(e.x + e.nx * 11, e.y + e.ny * 11, C(214, 172, 80, (int)(255 * al)), e.x + e.nx * 22 - e.tx * 6, e.y + e.ny * 22 - e.ty * 6, C(255, 232, 150, (int)(255 * al)), e.x + e.tx * bl * .55f, e.y + e.ty * bl * .55f, C(246, 238, 214, (int)(255 * al))); }
    // ---- forelegs (tucked): two jointed limbs with claws ----
    for (int k = 0; k < 2; ++k) { const Fr& c = sp[10 + k * 2]; float sx0 = c.x - c.nx * c.th * .6f, sy0 = c.y - c.ny * c.th * .6f, sw = sinf(t * 1.9f + k) * 5.f;
        float kx = sx0 - c.tx * 12 - c.nx * 14, ky = sy0 - c.ty * 12 - c.ny * 14 + sw, fx = kx + c.tx * 6 - c.nx * 18, fy = ky + c.ty * 6 - c.ny * 18; Bone(sx0, sy0, kx, ky, 6.0f - k, al * (k ? .75f : 1.f)); Bone(kx, ky, fx, fy, 4.6f - k, al * (k ? .75f : 1.f));
        for (int n = -1; n <= 1; ++n) Tri(fx + n * 3.f, fy, C(196, 158, 72, (int)(255 * al)), fx + n * 3.f + 3, fy - 1, C(196, 158, 72, (int)(255 * al)), fx + n * 4.f - 2, fy - 9 - c.ny * 4, C(250, 244, 226, (int)(255 * al))); }
    // ---- head (faces the menu / left): skull, snout, jaw, horns, brow, eye, teeth, whiskers ----
    { const Fr& h = sp[0]; float fx = -sp[0].tx, fy = -sp[0].ty; float l = sqrtf(fx * fx + fy * fy) + 1e-4f; fx /= l; fy /= l; float fl = atan2f(fy, fx);
      fx = cosf(fl - 0.18f); fy = sinf(fl - 0.18f);                                                                 // snout points slightly left-down
      float dx = fy, dy = -fx; if (dy > 0) { dx = -dx; dy = -dy; }                                                  // d = dorsal direction (up on screen)
      float cx = h.x + 10, cy = h.y - 4; float jaw = 3.f + 6.f * (0.5f + 0.5f * sinf(t * 1.7f));
      const float hs = 1.35f;
      #define HP(a, b) (cx + fx * (a) * hs + dx * (b) * hs), (cy + fy * (a) * hs + dy * (b) * hs)
      Col top = C(214, 176, 84, (int)(255 * al)), mid = C(206, 208, 220, (int)(255 * al)), low = C(60, 58, 68, (int)(255 * al)), ed = C(240, 206, 112, (int)(240 * al));
      // lower jaw first (behind the skull), opens slightly
      Tri(HP(2, -10), top, HP(56, -12 - jaw), low, HP(-14, -22 - jaw * .4f), low); Tri(HP(2, -10), mid, HP(56, -12 - jaw), low, HP(48, -6), mid);
      Col th = C(250, 246, 232, (int)(255 * al)); for (int k = 0; k < 5; ++k) { float a = 14 + k * 9.f; Tri(HP(a, -7 - jaw * .8f), th, HP(a + 4.5f, -7 - jaw * .8f), th, HP(a + 2.2f, -1 - jaw * .5f), C(206, 202, 190, (int)(255 * al))); }
      // skull + snout (fan from centre)
      float px[10] = { -18, -8, 10, 34, 60, 72, 66, 40, 12, -14 }, py[10] = { 4, 18, 22, 18, 10, 2, -6, -4, -8, -6 };
      for (int i = 0; i < 10; ++i) { int j = (i + 1) % 10; Col ci = (py[i] > 6) ? top : mid, cj = (py[j] > 6) ? top : mid; if (py[i] < -3) ci = CMix(mid, low, .6f); if (py[j] < -3) cj = CMix(mid, low, .6f);
          Tri(HP(14, 6), mid, HP(px[i], py[i]), ci, HP(px[j], py[j]), cj); }
      for (int i = 0; i < 10; ++i) { int j = (i + 1) % 10; Line(HP(px[i], py[i]), HP(px[j], py[j]), 1.6f, ed); }
      for (int k = 0; k < 4; ++k) { float a = 40 + k * 7.5f; Tri(HP(a, -3), th, HP(a + 4, -3), th, HP(a + 2, -10 - jaw * .3f), C(206, 202, 190, (int)(255 * al))); }   // upper teeth
      Circle(HP(66, 4), 1.8f, C(20, 18, 14, (int)(255 * al)), C(20, 18, 14, (int)(255 * al)), 8);                      // nostril
      Line(HP(-6, 16), HP(34, 15), 3.f, C(250, 232, 160, (int)(230 * al)));                                           // brow ridge
      Horn(cx + fx * -6 + dx * 18, cy + fy * -6 + dy * 18, -fx * 0.9f + dx * 0.5f, -fy * 0.9f + dy * 0.5f, 64, 0.9f * (dx < 0 ? -1.f : 1.f) * 0.7f, 7.f, al, t);
      Horn(cx + fx * 8 + dx * 20, cy + fy * 8 + dy * 20, -fx * 0.6f + dx * 0.8f, -fy * 0.6f + dy * 0.8f, 46, 0.7f * (dx < 0 ? -1.f : 1.f) * 0.7f, 5.f, al * .85f, t);
      for (int k = 0; k < 3; ++k) Tri(HP(-14 - k * 5, -4 - k * 6), C(176, 138, 58, (int)(255 * al)), HP(-4 - k * 4, -14 - k * 6), C(110, 84, 34, (int)(255 * al)), HP(-30 - k * 9, -14 - k * 8 + sinf(t * 2.f + k) * 2), C(250, 238, 190, (int)(255 * al)));   // cheek frill
      float wv = sinf(t * 2.2f);                                                                                       // whiskers
      for (int k = 0; k < 2; ++k) { float s0 = k ? -1.f : 1.f; float ax = cx + fx * 60 + dx * (-2), ay = cy + fy * 60 + dy * (-2); for (int i = 0; i < 6; ++i) { float u0 = i / 6.f, u1 = (i + 1) / 6.f;
          Line(ax + fx * 30 * u0 + dx * (-14 * u0 + wv * 5 * u0 * s0), ay + fy * 30 * u0 + dy * (-14 * u0 + wv * 5 * u0 * s0), ax + fx * 30 * u1 + dx * (-14 * u1 + wv * 5 * u1 * s0), ay + fy * 30 * u1 + dy * (-14 * u1 + wv * 5 * u1 * s0), 1.6f - u0, C(236, 200, 110, (int)(210 * al * (1 - u0 * .6f)))); } }
      SetAdditive(true); Circle(HP(32, 11), 9, C(255, 190, 70, (int)(170 * al)), C(255, 150, 40, 0), 14); SetAdditive(false);           // eye glow + slit pupil
      Circle(HP(32, 11), 3.6f, C(255, 226, 150, (int)(255 * al)), C(240, 170, 60, (int)(255 * al)), 10); Tri(HP(32, 8), C(20, 14, 6, (int)(255 * al)), HP(32, 14), C(20, 14, 6, (int)(255 * al)), HP(33.6f, 11), C(20, 14, 6, (int)(255 * al)));
      #undef HP
    }
    Wing(sp[8], t, 0.f, 0.80f, 1.f, al);                                                                                  // near wing on top
}
}
