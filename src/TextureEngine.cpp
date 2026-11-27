#include "nitrofx/TextureEngine.h"
#include "nitrofx/TexGen.h"
namespace nfx {
TextureEngine gTex;
static RwTexture* MakeTexture(int cat, int cvq, int dq) {
    RwRaster* r = RwRasterCreate(kTexSize, kTexSize, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    if (!r) return nullptr;
    RwUInt8* px = RwRasterLock(r, 0, rwRASTERLOCKWRITE);
    if (!px) { RwRasterDestroy(r); return nullptr; }
    int stride = RwRasterGetStride(r);
    std::vector<uint32_t> tmp(kTexSize * kTexSize);
    GenerateJetTexture(cat, cvq, dq, &tmp[0]);
    for (int y = 0; y < kTexSize; ++y) memcpy(px + y * stride, &tmp[y * kTexSize], kTexSize * 4);
    RwRasterUnlock(r);
    RwTexture* t = RwTextureCreate(r);
    if (!t) { RwRasterDestroy(r); return nullptr; }
    RwTextureSetAddressing(t, rwTEXTUREADDRESSWRAP);
    RwTextureSetFilterMode(t, rwFILTERLINEAR);
    return t;
}
RwTexture* TextureEngine::Get(const TexSettings& s) {
    int cvq = (int)(clampf(s.colorVar, 0, 1) * 11.f + .5f), dq = (int)(clampf(s.detail, 0, 1) * 4.f + .5f);
    int key = s.category | (cvq << 8) | (dq << 16);
    ++tick_;
    for (size_t i = 0; i < cache_.size(); ++i) if (cache_[i].key == key) { cache_[i].lastUse = tick_; return cache_[i].tex; }
    if (cache_.size() >= 72) { // >= 64 emitters can each use a distinct texture in one frame; 72 x 64 KB = 4.5 MB, so no per-frame regeneration thrash // evict least recently used
        size_t lru = 0; for (size_t i = 1; i < cache_.size(); ++i) if (cache_[i].lastUse < cache_[lru].lastUse) lru = i;
        if (cache_[lru].tex) RwTextureDestroy(cache_[lru].tex);
        cache_.erase(cache_.begin() + lru);
    }
    Entry e; e.key = key; e.lastUse = tick_; e.tex = MakeTexture(s.category, cvq, dq);
    cache_.push_back(e); return e.tex;
}
void TextureEngine::Shutdown() {
    for (size_t i = 0; i < cache_.size(); ++i) if (cache_[i].tex) RwTextureDestroy(cache_[i].tex);
    cache_.clear();
}
}
