#include "nitrofx/UiMath.h"
#include <cstdio>
#include <cmath>
using namespace nfx::uim;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
static bool Near(float a, float b, float e = 0.01f) { return fabsf(a - b) <= e; }
int main() {
    const float res[][2] = { {1280,720},{1920,1080},{2560,1440},{3840,2160},{1024,768},{800,600},{1680,1050},{2560,1080},{640,480},{1366,768} };
    for (size_t i = 0; i < sizeof(res) / sizeof(res[0]); ++i) {
        float W = res[i][0], H = res[i][1]; Fit f = MakeFit(W, H);
        CHECK(f.s > 0 && f.ox >= -0.01f && f.oy >= -0.01f);
        CHECK(ToGX(f, 0) >= -0.01f && ToGX(f, VW) <= W + 0.01f && ToGY(f, 0) >= -0.01f && ToGY(f, VH) <= H + 0.01f);   // whole UI inside the screen
        float pts[][2] = { {0,0},{640,360},{1280,720},{37.5f,401.25f} };
        for (int k = 0; k < 4; ++k) { float gx = ToGX(f, pts[k][0]), gy = ToGY(f, pts[k][1]); CHECK(Near(ToVX(f, gx), pts[k][0], 0.02f) && Near(ToVY(f, gy), pts[k][1], 0.02f)); }  // mouse round trip
        CHECK(Near(ToGX(f, VW / 2), W / 2, 0.05f) && Near(ToGY(f, VH / 2), H / 2, 0.05f));                       // centred
        CHECK(Near((ToGX(f, 1) - ToGX(f, 0)), (ToGY(f, 1) - ToGY(f, 0)), 1e-3f));                                 // uniform: no stretching
    }
    Fit w = MakeFit(1920, 1080); CHECK(Near(w.s, 1.5f) && Near(w.ox, 0) && Near(w.oy, 0));
    Fit u = MakeFit(2560, 1080); CHECK(Near(u.s, 1.5f) && Near(u.ox, 320));
    CHECK(Near(ClientToGame(400, 800, 1920), 960));
    CHECK(ClientToGame(5, 0, 1920) == 0.f);
    CHECK(MakeFit(0, 0).s > 0);
    // font scale: proportional to px, plausible fallback when measurement is missing/absurd
    float a = FontScale(16, 1.f, 720, 40.f), b = FontScale(32, 1.f, 720, 40.f); CHECK(Near(b, a * 2, 0.01f));
    CHECK(FontScale(16, 1.f, 720, 0.f) == FontScaleFallback(16, 1.f, 720));
    CHECK(FontScale(16, 1.f, 720, 1e6f) == FontScaleFallback(16, 1.f, 720));
    CHECK(FontScale(16, 1.5f, 1080, 60.f) > 0.15f);
    float x = -5, y = 900; ClampRect(x, y, 100, 50); CHECK(x == 0 && Near(y, VH - 50));
    if (fails) { printf("%d FAILED\n", fails); return 1; } printf("ui_math_test: all passed\n"); return 0;
}
