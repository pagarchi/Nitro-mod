#include "nitrofx/Draw2D.h"
namespace nfx { namespace d2 {
static std::vector<RwIm2DVertex> gBatch;
static bool gAdd = false;
#define RS(state, val) RwRenderStateSet(state, (void*)(val))
static float SXs() { return (float)RsGlobal.maximumWidth / VW; }
static float SYs() { return (float)RsGlobal.maximumHeight / VH; }
static unsigned Pack(Col c) { return ((unsigned)c.a << 24) | ((unsigned)c.r << 16) | ((unsigned)c.g << 8) | (unsigned)c.b; }
static void SetState(RwRaster* ras) {
    RS(rwRENDERSTATETEXTURERASTER, ras);
    RS(rwRENDERSTATEZTESTENABLE, FALSE); RS(rwRENDERSTATEZWRITEENABLE, FALSE);
    RS(rwRENDERSTATEVERTEXALPHAENABLE, TRUE); RS(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
    RS(rwRENDERSTATEDESTBLEND, gAdd ? rwBLENDONE : rwBLENDINVSRCALPHA);
    RS(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE); RS(rwRENDERSTATEFOGENABLE, FALSE);
    RS(rwRENDERSTATETEXTUREFILTER, rwFILTERLINEAR);
}
static void Vtx(RwIm2DVertex& v, float x, float y, Col c, float u = 0, float vv = 0) {
    v.x = x * SXs(); v.y = y * SYs(); v.z = RwIm2DGetNearScreenZ(); v.rhw = 1.0f / RwCameraGetNearClipPlane(*(RwCamera**)0xC1703C);
    v.emissiveColor = Pack(c); v.u = u; v.v = vv;
}
void Begin() { gBatch.clear(); gBatch.reserve(1024); gAdd = false; }
void Flush() {
    if (gBatch.empty()) return;
    SetState(nullptr);
    RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, &gBatch[0], (RwInt32)gBatch.size());
    gBatch.clear();
}
void End() { Flush(); CFont::RenderFontBuffer(); RS(rwRENDERSTATEZTESTENABLE, TRUE); RS(rwRENDERSTATEZWRITEENABLE, TRUE); RS(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA); }
void SetAdditive(bool on) { if (on != gAdd) { Flush(); gAdd = on; } }
static void PushTri(float x1, float y1, Col c1, float x2, float y2, Col c2, float x3, float y3, Col c3) {
    if (gBatch.size() + 3 > 900) Flush();
    RwIm2DVertex a, b, c; Vtx(a, x1, y1, c1); Vtx(b, x2, y2, c2); Vtx(c, x3, y3, c3);
    gBatch.push_back(a); gBatch.push_back(b); gBatch.push_back(c);
}
void Tri(float x1, float y1, Col c1, float x2, float y2, Col c2, float x3, float y3, Col c3) { PushTri(x1, y1, c1, x2, y2, c2, x3, y3, c3); }
void Quad(float x1, float y1, Col c1, float x2, float y2, Col c2, float x3, float y3, Col c3, float x4, float y4, Col c4) {
    PushTri(x1, y1, c1, x2, y2, c2, x3, y3, c3); PushTri(x1, y1, c1, x3, y3, c3, x4, y4, c4);
}
void Rect(float x, float y, float w, float h, Col c) { Quad(x, y, c, x + w, y, c, x + w, y + h, c, x, y + h, c); }
void RectV(float x, float y, float w, float h, Col t, Col b) { Quad(x, y, t, x + w, y, t, x + w, y + h, b, x, y + h, b); }
void RectH(float x, float y, float w, float h, Col l, Col r) { Quad(x, y, l, x + w, y, r, x + w, y + h, r, x, y + h, l); }
void Line(float x1, float y1, float x2, float y2, float w, Col c) {
    float dx = x2 - x1, dy = y2 - y1, l = sqrtf(dx * dx + dy * dy); if (l < 1e-4f) return;
    float nx = -dy / l * w * .5f, ny = dx / l * w * .5f;
    Quad(x1 + nx, y1 + ny, c, x2 + nx, y2 + ny, c, x2 - nx, y2 - ny, c, x1 - nx, y1 - ny, c);
}
void Circle(float cx, float cy, float r, Col cc, Col ce, int seg) {
    for (int i = 0; i < seg; ++i) {
        float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
        PushTri(cx, cy, cc, cx + cosf(a0) * r, cy + sinf(a0) * r, ce, cx + cosf(a1) * r, cy + sinf(a1) * r, ce);
    }
}
void TexRect(float x, float y, float w, float h, RwTexture* tex, float u0, float v0, float u1, float v1, Col tint) {
    if (!tex) return; Flush();
    RwIm2DVertex q[4]; Vtx(q[0], x, y, tint, u0, v0); Vtx(q[1], x + w, y, tint, u1, v0); Vtx(q[2], x, y + h, tint, u0, v1); Vtx(q[3], x + w, y + h, tint, u1, v1);
    SetState(RwTextureGetRaster(tex));
    RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, q, 4);
}
// ---- text (CFont). Virtual px -> CFont scale. kFontUnit is the one tuning constant if text looks too big/small.
static const float kFontUnit = 22.0f;
static void ToGxt(const char* s, unsigned short* out, int cap) { int i = 0; for (; s[i] && i < cap - 1; ++i) out[i] = (unsigned char)s[i]; out[i] = 0; }
static void FontSetup(float px, Col c, int align) {
    float sy = (px * SYs()) / (kFontUnit * (float)RsGlobal.maximumHeight / 448.0f);
    CFont::SetFontStyle(1); CFont::SetScale(sy * 0.5f, sy); CFont::SetColor(CRGBA(c.r, c.g, c.b, c.a));
    CFont::SetProportional(true); CFont::SetBackground(false, false); CFont::SetWrapx(100000.0f); CFont::SetEdge(0); CFont::SetDropShadowPosition(0);
    CFont::SetOrientation(align == 1 ? ALIGN_CENTER : (align == 2 ? ALIGN_RIGHT : ALIGN_LEFT));
}
float TextWidth(const char* s, float px) {
    unsigned short buf[256]; ToGxt(s, buf, 256); FontSetup(px, C(255, 255, 255), 0);
    return CFont::GetStringWidth((char*)buf, true, false) / SXs();
}
void Text(const char* s, float x, float y, float px, Col c, int align) {
    Flush(); unsigned short buf[256]; ToGxt(s, buf, 256); FontSetup(px, c, align);
    CFont::PrintString(x * SXs(), y * SYs(), (char*)buf); CFont::RenderFontBuffer();
}
} }
