#pragma once
// Pure, SDK-free UI math (testable on a Linux host: tests/ui_math_test.cpp).
// Virtual UI space is 1280x720. It is scaled UNIFORMLY (letterboxed) into the real game screen, so every aspect ratio
// (4:3, 16:10, 16:9, 21:9, windowed or fullscreen) keeps correct proportions, and the mouse maps through the same transform.
#include <cmath>
namespace nfx { namespace uim {
const float VW = 1280.f, VH = 720.f;
struct Fit { float s, ox, oy; };
inline Fit MakeFit(float W, float H) {
    Fit f; if (W < 1.f) W = 1.f; if (H < 1.f) H = 1.f;
    float sx = W / VW, sy = H / VH; f.s = sx < sy ? sx : sy; f.ox = (W - VW * f.s) * .5f; f.oy = (H - VH * f.s) * .5f; return f;
}
inline float ToVX(const Fit& f, float gx) { return (gx - f.ox) / f.s; }     // game-screen px -> virtual
inline float ToVY(const Fit& f, float gy) { return (gy - f.oy) / f.s; }
inline float ToGX(const Fit& f, float vx) { return vx * f.s + f.ox; }       // virtual -> game-screen px
inline float ToGY(const Fit& f, float vy) { return vy * f.s + f.oy; }
inline float ClientToGame(float c, float clientSize, float gameSize) { return clientSize > 0.f ? c * gameSize / clientSize : 0.f; }
inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
// Font scale. refW10 = measured width (game px) of ten 'H' at CFont scale (0.5, 1.0). Target: one 'H' is kHWidth * px wide.
const float kHWidth = 0.62f;
inline float FontScaleFallback(float px, float s, float gameH) { return Clamp(0.72f * px * s / (0.0135f * (gameH < 1.f ? 1.f : gameH)), 0.15f, 8.f); }
inline float FontScale(float px, float s, float gameH, float refW10) {
    float fb = FontScaleFallback(px, s, gameH);
    if (!(refW10 > 1e-3f)) return fb;
    float sy = Clamp(px * s * kHWidth * 10.f / refW10, 0.15f, 8.f);
    float r = sy / fb; return (r > 0.4f && r < 2.5f) ? sy : fb;             // measured value must be plausible, else use the height formula
}
// Keeps a rectangle (virtual coords) fully inside the virtual screen.
inline void ClampRect(float& x, float& y, float w, float h) { x = Clamp(x, 0.f, VW - w); y = Clamp(y, 0.f, VH - h); }
}}
