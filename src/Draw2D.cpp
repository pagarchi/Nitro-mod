#include "nitrofx/Draw2D.h"
namespace nfx { namespace d2 {
static std::vector<RwIm2DVertex> gBatch;
static bool gAdd = false;
static uim::Fit gFit = { 1.f, 0.f, 0.f };
#define RS(state, val) RwRenderStateSet(state, (void*)(val))
void RefreshView() { gFit = uim::MakeFit((float)RsGlobal.maximumWidth, (float)RsGlobal.maximumHeight); }
float Scale() { return gFit.s; }
float VToGX(float v) { return uim::ToGX(gFit, v); } float VToGY(float v) { return uim::ToGY(gFit, v); }
float GXToV(float g) { return uim::ToVX(gFit, g); } float GYToV(float g) { return uim::ToVY(gFit, g); }
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
    v.x = x * gFit.s + gFit.ox; v.y = y * gFit.s + gFit.oy; v.z = RwIm2DGetNearScreenZ(); v.rhw = 1.0f / RwCameraGetNearClipPlane(*(RwCamera**)0xC1703C);
    v.emissiveColor = Pack(c); v.u = u; v.v = vv;
}
void Begin() { RefreshView(); gBatch.clear(); gBatch.reserve(1024); gAdd = false; }
void Flush() {
    if (gBatch.empty()) return;
    SetState(nullptr);
    RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, &gBatch[0], (RwInt32)gBatch.size());
    gBatch.clear();
}
void End() { Flush(); CFont::RenderFontBuffer(); RS(rwRENDERSTATEZTESTENABLE, TRUE); RS(rwRENDERSTATEZWRITEENABLE, TRUE); RS(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA); RS(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA); }
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
void Fill(Col c) { float x = GXToV(0.f), y = GYToV(0.f), w = GXToV((float)RsGlobal.maximumWidth) - x, h = GYToV((float)RsGlobal.maximumHeight) - y; Rect(x - 1, y - 1, w + 2, h + 2, c); }
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
// ---- text (CFont) ----
// ROOT CAUSE of the "one letter per label" bug: the old code converted strings to 16-bit and passed them to CFont::PrintString,
// which reads 8-bit chars, so every string ended at its first character. Strings are now sent as plain 8-bit text.
// The scale is calibrated at runtime from the real CFont::GetStringWidth (uim::FontScale), so size is right at every resolution.
// Text is always printed left-aligned; centre/right alignment is done here from the measured width (no CentreSize ambiguity).
static float gRefW10 = 0.f; static int gRefH = -1;
static void Sanitize(const char* s, char* out, int cap) { int i = 0; for (; s && s[i] && i < cap - 1; ++i) { unsigned char c = (unsigned char)s[i]; out[i] = (c < 32 || c > 126 || c == '~') ? '-' : (char)c; } out[i] = 0; }
static void FontBase() { CFont::SetFontStyle(1); CFont::SetProportional(true); CFont::SetBackground(false, false); CFont::SetWrapx(100000.0f); CFont::SetEdge(0); CFont::SetOrientation(ALIGN_LEFT); }
static float FontScaleFor(float px) {
    int H = RsGlobal.maximumHeight;
    if (gRefH != H) { FontBase(); CFont::SetScale(0.5f, 1.0f); char ref[] = "HHHHHHHHHH"; gRefW10 = CFont::GetStringWidth(ref, true, false); gRefH = H; }
    return uim::FontScale(px, gFit.s, (float)H, gRefW10);
}
static void FontSetup(float px, Col c, bool shadow) {
    float sy = FontScaleFor(px); FontBase(); CFont::SetScale(sy * 0.5f, sy); CFont::SetColor(CRGBA(c.r, c.g, c.b, c.a));
    if (shadow) { CFont::SetDropColor(CRGBA(0, 0, 0, (unsigned char)(c.a * 0.85f))); CFont::SetDropShadowPosition(1); } else CFont::SetDropShadowPosition(0);
}
float TextWidth(const char* s, float px) {
    char buf[256]; Sanitize(s, buf, 256); FontSetup(px, C(255, 255, 255), false);
    return CFont::GetStringWidth(buf, true, false) / gFit.s;
}
void Text(const char* s, float x, float y, float px, Col c, int align) {
    if (!s || !s[0]) return;
    Flush(); char buf[256]; Sanitize(s, buf, 256);
    FontSetup(px, c, c.a > 120);
    if (align) { float w = CFont::GetStringWidth(buf, true, false) / gFit.s; x -= (align == 1 ? w * .5f : w); }
    CFont::PrintString(x * gFit.s + gFit.ox, y * gFit.s + gFit.oy, buf); CFont::RenderFontBuffer();
}
void TextFit(const char* s, float x, float y, float px, Col c, float maxW, int align) {
    if (!s || !s[0]) return;
    float w = TextWidth(s, px);
    if (w > maxW && w > 1.f) { px = px * maxW / w; if (px < 11.f) px = 11.f; w = TextWidth(s, px); }
    if (w <= maxW) { Text(s, x, y, px, c, align); return; }
    char buf[256]; Sanitize(s, buf, 252); int n = (int)strlen(buf);                    // still too wide at the minimum size: truncate with ".."
    while (n > 1) { buf[n - 1] = 0; char t[260]; snprintf(t, sizeof t, "%s..", buf); if (TextWidth(t, px) <= maxW) { Text(t, x, y, px, c, align); return; } --n; }
}
} }
