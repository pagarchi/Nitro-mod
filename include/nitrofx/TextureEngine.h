#pragma once
#include "Pch.h"
#include "Types.h"
namespace nfx {
// Cached procedural textures. Generated once per (category, color-variation bucket, detail bucket);
// animation is done with UV scrolling, so nothing is regenerated per frame.
class TextureEngine {
public:
    void Shutdown();
    RwTexture* Get(const TexSettings& s);   // may return nullptr on failure
private:
    struct Entry { int key; RwTexture* tex; unsigned lastUse; };
    std::vector<Entry> cache_;
    unsigned tick_ = 0;
};
extern TextureEngine gTex;
}
