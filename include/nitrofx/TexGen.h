#pragma once
#include <cstdint>
namespace nfx {
const int kTexSize = 128;
// Pure CPU procedural generator (no RenderWare): fills size*size ARGB pixels (0xAARRGGBB).
// u = across jet width, v = along jet length. Tileable along v.
void GenerateJetTexture(int category, int colorVarQ /*0..11*/, int detailQ /*0..4*/, uint32_t* argb);
bool CategoryIsAdditive(int category);
}
