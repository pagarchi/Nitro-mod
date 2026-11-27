#pragma once
#include "Pch.h"
#include "UiMath.h"
namespace nfx {
struct Col { unsigned char r, g, b, a; };
inline Col C(int r, int g, int b, int a = 255) { Col c; c.r = (unsigned char)r; c.g = (unsigned char)g; c.b = (unsigned char)b; c.a = (unsigned char)(a < 0 ? 0 : (a > 255 ? 255 : a)); return c; }
inline Col CA(Col c, float k) { c.a = (unsigned char)(c.a * (k < 0 ? 0 : (k > 1 ? 1 : k))); return c; }
inline Col CMix(Col a, Col b, float t) { return C((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t), (int)(a.a + (b.a - a.a) * t)); }
namespace d2 {
const float VW = uim::VW, VH = uim::VH;        // virtual UI resolution, scaled uniformly (letterboxed) into the game screen
void RefreshView();                            // re-reads the game screen size (resolution / fullscreen / device-reset safe)
float Scale();                                 // virtual -> game px
float VToGX(float vx); float VToGY(float vy); float GXToV(float gx); float GYToV(float gy);
void Fill(Col c);                              // covers the WHOLE game screen (incl. letterbox bars)
void Begin();                                  // call once per frame before any 2D drawing
void End();
void SetAdditive(bool on);
void Rect(float x, float y, float w, float h, Col c);
void RectV(float x, float y, float w, float h, Col top, Col bottom);
void RectH(float x, float y, float w, float h, Col left, Col right);
void Tri(float x1, float y1, Col c1, float x2, float y2, Col c2, float x3, float y3, Col c3);
void Quad(float x1, float y1, Col c1, float x2, float y2, Col c2, float x3, float y3, Col c3, float x4, float y4, Col c4);
void Line(float x1, float y1, float x2, float y2, float w, Col c);
void Circle(float cx, float cy, float r, Col center, Col edge, int seg = 20);
void TexRect(float x, float y, float w, float h, RwTexture* tex, float u0, float v0, float u1, float v1, Col tint);
void Text(const char* s, float x, float y, float px, Col c, int align = 0); // align: 0 left, 1 center, 2 right
float TextWidth(const char* s, float px);
void TextFit(const char* s, float x, float y, float px, Col c, float maxW, int align = 0); // shrinks / truncates (..) to fit maxW
void Flush();
}
}
