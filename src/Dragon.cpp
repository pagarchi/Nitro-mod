#include "nitrofx/App.h"
// Procedural, fully animated 2D dragon built from vector triangles each frame (no image assets).
namespace nfx {
using namespace d2;
static const float kPi = 3.14159265f;
static float rad(float d) { return d * 0.0174533f; }
void DrawDragon(float t, float alpha) {
    if (alpha <= 0.01f) return;
    const Col chromeTop = C(122, 128, 152), belly = C(26, 20, 36), goldC = C(214, 176, 86), purp = C(128, 58, 205);
    // ambient glow + embers
    SetAdditive(true); Circle(1030, 360, 235, C(95, 35, 170, (int)(58 * alpha)), C(95, 35, 170, 0), 36);
    for (int i = 0; i < 16; ++i) { float ph = t * 0.35f + i * 0.61f; float ex = 830 + fmodf(i * 97.f + ph * 40.f, 400.f), ey = 640 - fmodf(ph * 55.f + i * 31.f, 520.f);
        Circle(ex, ey, 2.2f, C(255, 190, 90, (int)(150 * alpha)), C(255, 120, 40, 0), 8); }
    SetAdditive(false);
    // spine with traveling wave + bobbing
    const int N = 26; float px[N + 1], py[N + 1], nx[N + 1], ny[N + 1], tx[N + 1], ty[N + 1], th[N + 1];
    float bob = sinf(t * 1.3f) * 12.f;
    for (int i = 0; i <= N; ++i) { float u = i / (float)N;
        px[i] = 885 + 330 * u + cosf(t * 2.4f - u * 5.5f) * 4 * u;
        py[i] = 255 + 250 * u * (1 - 0.25f * u) + sinf(t * 2.4f - u * 5.5f) * (6 + 26 * u) + bob;
        th[i] = u < 0.35f ? 9 + (u / 0.35f) * 23 : 32 * powf(1 - (u - 0.35f) / 0.65f, 1.2f) + 2; }
    for (int i = 0; i <= N; ++i) { int a = i > 0 ? i - 1 : i, b = i < N ? i + 1 : i; float dx = px[b] - px[a], dy = py[b] - py[a], l = sqrtf(dx * dx + dy * dy) + 1e-4f;
        tx[i] = dx / l; ty[i] = dy / l; nx[i] = ty[i]; ny[i] = -tx[i]; }  // normal points "up" on screen
    // wing lambda
    auto wing = [&](float ax, float ay, float ph, float sc, float dark) {
        float f = sinf(t * 2.1f + ph);
        float a1 = rad(-95 + 38 * f), a2 = a1 + rad(60) + 0.35f * sinf(t * 2.1f + ph - 0.7f);
        float ex = ax + cosf(a1) * 105 * sc, ey = ay + sinf(a1) * 105 * sc, wx = ex + cosf(a2) * 120 * sc, wy = ey + sinf(a2) * 120 * sc;
        static const float len[4] = { 215, 200, 170, 128 }; float tipx[4], tipy[4];
        for (int k = 0; k < 4; ++k) { float ak = a2 + rad(30 + k * 38) + 0.3f * sinf(t * 2.1f + ph - 1.0f - 0.2f * k); tipx[k] = wx + cosf(ak) * len[k] * sc; tipy[k] = wy + sinf(ak) * len[k] * sc; }
        float bx = px[13] + nx[13] * th[13] * .5f, by = py[13] + ny[13] * th[13] * .5f;
        Col mi = C(78, 26, 118, (int)(235 * alpha * dark)), mo = C(128, 54, 178, (int)(205 * alpha * dark));
        for (int k = 0; k < 3; ++k) Tri(wx, wy, mi, tipx[k], tipy[k], mo, tipx[k + 1], tipy[k + 1], mo);
        Tri(wx, wy, mi, tipx[3], tipy[3], mo, bx, by, mo); Tri(wx, wy, mi, bx, by, mo, ax, ay, mi);
        Col bone = C((int)(176 * dark + 20), (int)(150 * dark + 10), (int)(80 * dark + 20), (int)(255 * alpha)), arc = C(214, 176, 86, (int)(90 * alpha * dark));
        Line(ax, ay, ex, ey, 7 * sc, bone); Line(ex, ey, wx, wy, 5.5f * sc, bone);
        for (int k = 0; k < 4; ++k) { Line(wx, wy, tipx[k], tipy[k], 3.4f * sc, bone); Tri(tipx[k], tipy[k], goldC, tipx[k] + 9 * cosf(rad(40)), tipy[k] + 9 * sinf(rad(40)), goldC, tipx[k] + 2, tipy[k] - 7, goldC); }
        for (int k = 0; k < 3; ++k) { Line(lerpf(wx, tipx[k], .55f), lerpf(wy, tipy[k], .55f), lerpf(wx, tipx[k + 1], .55f), lerpf(wy, tipy[k + 1], .55f), 1.6f, arc);
                                      Line(lerpf(wx, tipx[k], .8f), lerpf(wy, tipy[k], .8f), lerpf(wx, tipx[k + 1], .8f), lerpf(wy, tipy[k + 1], .8f), 1.4f, arc); }
    };
    wing(px[7] + nx[7] * th[7] * .5f - 14, py[7] + ny[7] * th[7] * .5f - 4, 0.45f, 0.86f, 0.55f); // far wing first
    // body segments (gradient chrome) + scales
    for (int i = 0; i < N; ++i) { float u = i / (float)N; Col top = CMix(chromeTop, C(80, 40, 130), u * .8f), bot = CMix(belly, C(40, 20, 70), u);
        top.a = bot.a = (unsigned char)(255 * alpha);
        Quad(px[i] + nx[i] * th[i], py[i] + ny[i] * th[i], top, px[i + 1] + nx[i + 1] * th[i + 1], py[i + 1] + ny[i + 1] * th[i + 1], top,
             px[i + 1] - nx[i + 1] * th[i + 1], py[i + 1] - ny[i + 1] * th[i + 1], bot, px[i] - nx[i] * th[i], py[i] - ny[i] * th[i], bot);
        float mx = (px[i] + px[i + 1]) * .5f, my = (py[i] + py[i + 1]) * .5f, mt = (th[i] + th[i + 1]) * .5f, sz = mt * .34f;
        for (int k = 0; k < 3; ++k) { float cx = mx + nx[i] * mt * .55f * (1 - k), cy = my + ny[i] * mt * .55f * (1 - k);
            Col sc = k == 1 ? C(160, 165, 190, (int)(120 * alpha)) : C(110, 115, 140, (int)(95 * alpha));
            Tri(cx - tx[i] * sz + nx[i] * sz, cy - ty[i] * sz + ny[i] * sz, sc, cx - tx[i] * sz - nx[i] * sz, cy - ty[i] * sz - ny[i] * sz, sc, cx + tx[i] * sz * 1.3f, cy + ty[i] * sz * 1.3f, C(214, 176, 86, (int)(70 * alpha)));
            Line(cx - tx[i] * sz + nx[i] * sz, cy - ty[i] * sz + ny[i] * sz, cx + tx[i] * sz * 1.3f, cy + ty[i] * sz * 1.3f, 1.0f, C(214, 176, 86, (int)(110 * alpha))); }
        if (i >= 2 && (i & 1) == 0) { float h = 8 + mt * .45f; float ax = px[i] + nx[i] * th[i], ay = py[i] + ny[i] * th[i], bx = px[i + 1] + nx[i + 1] * th[i + 1], by = py[i + 1] + ny[i + 1] * th[i + 1];
            float sx = (ax + bx) * .5f + nx[i] * h + tx[i] * h * .6f, sy = (ay + by) * .5f + ny[i] * h + ty[i] * h * .6f;
            Tri(ax, ay, C(150, 70, 225, (int)(255 * alpha)), bx, by, C(100, 40, 170, (int)(255 * alpha)), sx, sy, C(236, 198, 100, (int)(255 * alpha))); } }
    for (int i = 0; i < N; ++i) Line(px[i] + nx[i] * th[i], py[i] + ny[i] * th[i], px[i + 1] + nx[i + 1] * th[i + 1], py[i + 1] + ny[i + 1] * th[i + 1], 2.0f, C(224, 186, 96, (int)(230 * alpha)));
    // tail blade
    { float ex = px[N], ey = py[N]; Tri(ex + nx[N] * 14, ey + ny[N] * 14, purp, ex - nx[N] * 14, ey - ny[N] * 14, purp, ex + tx[N] * 40, ey + ty[N] * 40, goldC); }
    // head (faces left)
    { float hx = px[0] - 6, hy = py[0], jaw = 4 + 5 * sinf(t * 1.7f); Col hc = C(98, 104, 128, (int)(255 * alpha)), hd = C(40, 34, 56, (int)(255 * alpha));
      float snx = hx - 58, sny = hy + 8, brx = hx - 18, bry = hy - 18, bkx = hx + 20, bky = hy - 14, nkx = hx + 20, nky = hy + 16, jx = hx - 38, jy = hy + 20 + jaw;
      Tri(snx, sny, hc, brx, bry, hc, bkx, bky, hd); Tri(snx, sny, hc, bkx, bky, hd, nkx, nky, hd); Tri(snx, sny + 4, hd, nkx, nky, hd, jx, jy, hd);
      for (int k = 0; k < 4; ++k) { float tx0 = snx + 6 + k * 8; Tri(tx0, sny + 5, C(240, 238, 230, (int)(255 * alpha)), tx0 + 4, sny + 5, C(240, 238, 230, (int)(255 * alpha)), tx0 + 2, sny + 12, C(200, 198, 190, (int)(255 * alpha))); }
      Tri(bkx - 6, bky, purp, bkx + 10, bky - 4, purp, bkx + 62, bky - 38 + 5 * sinf(t * 1.1f), goldC); Tri(bkx - 14, bky + 6, purp, bkx + 4, bky + 4, purp, bkx + 48, bky - 8, goldC);
      Line(snx, sny, bkx, bky, 2, C(224, 186, 96, (int)(220 * alpha)));
      SetAdditive(true); Circle(hx - 14, hy - 4, 8, C(220, 120, 255, (int)(210 * alpha)), C(150, 60, 255, 0), 12); SetAdditive(false);
      Circle(hx - 14, hy - 4, 2.6f, C(255, 245, 255, (int)(255 * alpha)), C(255, 245, 255, (int)(255 * alpha)), 8); }
    wing(px[8] + nx[8] * th[8] * .6f, py[8] + ny[8] * th[8] * .6f, 0.f, 1.f, 1.f);               // near wing on top
}
}
