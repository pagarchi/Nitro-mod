#include "nitrofx/TexGen.h"
#include "nitrofx/Types.h"
#include <cmath>
namespace nfx {
// ---- periodic gradient (Perlin) noise ----
static float hash2(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16;
    return (h & 0xFFFFu) / 65535.f;
}
static float fadef(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static float gradDot(int ix, int iy, float dx, float dy) { float a = hash2(ix, iy) * 6.2831853f; return cosf(a) * dx + sinf(a) * dy; }
static int wrapi(int v, int p) { v %= p; return v < 0 ? v + p : v; }
static float perlin(float x, float y, int px, int py) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y); float fx = x - x0, fy = y - y0;
    int a = wrapi(x0, px), b = wrapi(x0 + 1, px), c = wrapi(y0, py), d = wrapi(y0 + 1, py);
    float n00 = gradDot(a, c, fx, fy), n10 = gradDot(b, c, fx - 1, fy), n01 = gradDot(a, d, fx, fy - 1), n11 = gradDot(b, d, fx - 1, fy - 1);
    float u = fadef(fx), v = fadef(fy);
    return lerpf(lerpf(n00, n10, u), lerpf(n01, n11, u), v) * 1.41421f; // ~[-1,1]
}
static float fbm(float x, float y, int px, int py, int oct) { // 0..1
    float s = 0, a = 0.5f, n = 0;
    for (int i = 0; i < oct; ++i) { s += a * perlin(x, y, px, py); n += a; x *= 2; y *= 2; px *= 2; py *= 2; a *= 0.5f; }
    return clampf(s / n * 0.9f + 0.5f, 0, 1);
}
static float turb(float x, float y, int px, int py, int oct) { // 0..1 turbulence
    float s = 0, a = 0.5f, n = 0;
    for (int i = 0; i < oct; ++i) { s += a * fabsf(perlin(x, y, px, py)); n += a; x *= 2; y *= 2; px *= 2; py *= 2; a *= 0.5f; }
    return clampf(s / n * 1.6f, 0, 1);
}
static float ridged(float x, float y, int px, int py, int oct) { return 1.f - clampf(turb(x, y, px, py, oct) * 1.2f, 0, 1); }
static float fract1(float v) { return v - floorf(v); }

struct Stop { float t, r, g, b; };
static void Grad(const Stop* s, int n, float t, float& r, float& g, float& b) {
    t = clampf(t, 0, 1);
    for (int i = 0; i < n - 1; ++i) if (t <= s[i + 1].t) {
        float k = (t - s[i].t) / (s[i + 1].t - s[i].t + 1e-6f);
        r = lerpf(s[i].r, s[i + 1].r, k); g = lerpf(s[i].g, s[i + 1].g, k); b = lerpf(s[i].b, s[i + 1].b, k); return;
    }
    r = s[n - 1].r; g = s[n - 1].g; b = s[n - 1].b;
}
#define PAL(name, ...) static const Stop name[] = { __VA_ARGS__ }
PAL(pNeon, {0,.02f,.05f,.35f}, {.35f,.1f,.35f,1}, {.7f,.45f,.8f,1}, {1,1,1,1});
PAL(pMatrix, {0,0,.1f,0}, {.5f,0,.8f,.2f}, {.85f,.3f,1,.4f}, {1,.9f,1,.9f});
PAL(pWater, {0,0,.15f,.5f}, {.5f,.1f,.6f,.9f}, {.8f,.5f,.9f,1}, {1,1,1,1});
PAL(pFire, {0,.4f,0,0}, {.4f,1,.3f,0}, {.75f,1,.8f,.1f}, {1,1,1,.8f});
PAL(pMagma, {0,.1f,0,0}, {.4f,.6f,.05f,0}, {.7f,1,.4f,0}, {1,1,.85f,.3f});
PAL(pCryo, {0,.3f,.6f,.9f}, {.6f,.7f,.9f,1}, {1,1,1,1});
PAL(pPlasma, {0,.2f,0,.4f}, {.4f,.7f,.1f,.9f}, {.75f,1,.4f,1}, {1,1,.9f,1});
PAL(pElec, {0,.1f,.15f,.5f}, {.5f,.4f,.6f,1}, {1,1,1,1});
PAL(pToxic, {0,0,.2f,0}, {.5f,.3f,.9f,0}, {.8f,.8f,1,.2f}, {1,1,1,.7f});
PAL(pCosmic, {0,.02f,0,.1f}, {.4f,.25f,.05f,.5f}, {.7f,.5f,.2f,.9f}, {1,1,.9f,1});
PAL(pChrome, {0,.15f,.17f,.2f}, {.5f,.6f,.65f,.7f}, {.8f,.95f,.97f,1}, {1,1,1,1});
PAL(pGold, {0,.3f,.15f,0}, {.5f,.8f,.55f,.1f}, {.8f,1,.85f,.3f}, {1,1,1,.8f});
PAL(pShadow, {0,0,0,.02f}, {.5f,.15f,0,.25f}, {1,.45f,.1f,.6f});
PAL(pSand, {0,.35f,.25f,.12f}, {.6f,.75f,.6f,.35f}, {1,.95f,.85f,.6f});
#define G(p, t, r, g, b) Grad(p, (int)(sizeof(p) / sizeof(p[0])), t, r, g, b)

static void HsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    h = fract1(h) * 6; int i = (int)h; float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i % 6) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
                     case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
}
static void HueRot(float& r, float& g, float& b, float deg) {
    if (deg == 0) return;
    float a = deg * 0.0174533f, c = cosf(a), s = sinf(a);
    float nr = (.299f + .701f * c + .168f * s) * r + (.587f - .587f * c + .330f * s) * g + (.114f - .114f * c - .497f * s) * b;
    float ng = (.299f - .299f * c - .328f * s) * r + (.587f + .413f * c + .035f * s) * g + (.114f - .114f * c + .292f * s) * b;
    float nb = (.299f - .300f * c + 1.25f * s) * r + (.587f - .588f * c - 1.05f * s) * g + (.114f + .886f * c - .203f * s) * b;
    r = clampf(nr, 0, 1); g = clampf(ng, 0, 1); b = clampf(nb, 0, 1);
}
bool CategoryIsAdditive(int c) { return !(c == CAT_MAGMA || c == CAT_CHROME || c == CAT_SHADOW || c == CAT_SANDSTORM); }

void GenerateJetTexture(int cat, int cvq, int dq, uint32_t* out) {
    const int S = kTexSize; const int oct = 2 + (dq < 0 ? 0 : (dq > 4 ? 4 : dq));
    const float hue = cvq * (360.f / 12.f); const int FX = 3, FY = 6;
    for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) {
        float u = (x + .5f) / S, v = (y + .5f) / S;
        float m = smoothstepf(0, 0.85f, 1.f - fabsf(2 * u - 1));   // width mask: bright core, soft edge
        float n = fbm(u * FX, v * FY, FX, FY, oct);
        float I = 0, r = 1, g = 1, b = 1;
        switch (cat) {
        case CAT_NEON: I = clampf(.45f + .5f * n + .5f * m * m, 0, 1); G(pNeon, I, r, g, b); break;
        case CAT_MATRIX: { float h = hash2((int)(u * 6), (int)(v * 48)); I = .15f + .55f * h * h + .3f * n; if (fract1(v * 6 + h) < .12f) I = 1; I = clampf(I, 0, 1); G(pMatrix, I, r, g, b); break; }
        case CAT_WATER: I = clampf(.25f + .45f * n + .3f * (.5f + .5f * sinf((v * 6 + n * 1.6f) * 6.2831853f)) + .25f * m, 0, 1); G(pWater, I, r, g, b); break;
        case CAT_FIRE: I = clampf(turb(u * FX, v * FY, FX, FY, oct) * 1.3f + .45f * m, 0, 1); G(pFire, I, r, g, b); break;
        case CAT_MAGMA: { float rd = ridged(u * FX, v * FY, FX, FY, oct); I = clampf(rd * rd * .9f + .2f * n + .2f * m, 0, 1); G(pMagma, I, r, g, b); break; }
        case CAT_CRYO: { float rd = ridged(u * 4, v * 8, 4, 8, oct); float sp = hash2((int)(u * 32), (int)(v * 64)); I = clampf(.4f + .3f * n + .3f * rd * rd + (sp > .985f ? .6f : 0), 0, 1); G(pCryo, I, r, g, b); break; }
        case CAT_PLASMA: I = clampf(.5f + .5f * sinf((n * 4 + v * 12 + u * 3) * 3.14159f) * (.6f + .4f * m), 0, 1); G(pPlasma, I, r, g, b); break;
        case CAT_ELECTRIC: { float rd = ridged(u * 3, v * 6, 3, 6, oct); I = clampf(powf(rd, 6.f) * 1.4f + .2f * m, 0, 1); G(pElec, I, r, g, b); break; }
        case CAT_TOXIC: { float bub = hash2((int)(u * 10), (int)(v * 24)); I = clampf(.35f + .45f * n + (bub > .9f ? .35f : 0), 0, 1); G(pToxic, I, r, g, b); break; }
        case CAT_HOLO: I = clampf(.55f + .4f * n, 0, 1); HsvToRgb(v * 2.f + n * .5f + u * .2f, .65f, 1.f, r, g, b); break;
        case CAT_COSMIC: { float st = hash2((int)(u * 40), (int)(v * 80)); I = clampf(.2f + .6f * n * n + (st > .975f ? 1.f : 0), 0, 1); G(pCosmic, I, r, g, b); break; }
        case CAT_CHROME: I = clampf(.5f + .5f * sinf((v * 5 + n) * 6.2831853f) * (.5f + .5f * (1 - fabsf(2 * u - 1))), 0, 1); I = powf(I, 1.6f); G(pChrome, I, r, g, b); break;
        case CAT_GOLDEN: { float sp = hash2((int)(u * 24), (int)(v * 48)); I = clampf(.45f + .45f * n + (sp > .97f ? .5f : 0), 0, 1); G(pGold, I, r, g, b); break; }
        case CAT_SHADOW: I = clampf(.2f + .5f * n + .3f * turb(u * FX, v * FY, FX, FY, oct), 0, 1); G(pShadow, I, r, g, b); break;
        case CAT_SANDSTORM: { float t = turb(u * 4, v * 10, 4, 10, oct); I = clampf(.3f + .7f * t, 0, 1); G(pSand, I, r, g, b); break; }
        default: G(pNeon, n, r, g, b); I = n; }
        HueRot(r, g, b, hue);
        float A = clampf(powf(m, 1.1f) * (.25f + .95f * I), 0, 1);
        uint32_t ia = (uint32_t)(A * 255.f), ir = (uint32_t)(r * 255.f), ig = (uint32_t)(g * 255.f), ib = (uint32_t)(b * 255.f);
        out[y * S + x] = (ia << 24) | (ir << 16) | (ig << 8) | ib;
    }
}
}
