#include "nitrofx/Types.h"
#include <cmath>
namespace nfx {
const char* const kCategoryNames[CAT_COUNT] = { "Neon", "Matrix", "Water", "Fire", "Magma", "Cryo / Ice", "Plasma", "Electric",
    "Toxic", "Holographic", "Cosmic", "Chrome", "Golden", "Shadow", "Sandstorm" };

void TexSettings::Clamp() {
    if (category < 0 || category >= CAT_COUNT) category = CAT_NEON;
    if (!finitef(colorVar)) colorVar = 0;
    if (!finitef(speed)) speed = 1;
    if (!finitef(detail)) detail = .5f;
    if (!finitef(glow)) glow = 1;
    if (!finitef(opacity)) opacity = 1;
    if (!finitef(power)) power = 1;
    colorVar = clampf(colorVar, 0, 1); speed = clampf(speed, kSpeedMin, kSpeedMax); detail = clampf(detail, 0, 1);
    glow = clampf(glow, 0, kGlowMax); opacity = clampf(opacity, kOpacityMin, 1); power = clampf(power, kPowerMin, kPowerMax);
}
void Profile::Sanitize() {
    if (mirrorMode < 0 || mirrorMode > 2) mirrorMode = MIRROR_SELECTIVE;
    if ((int)emitters.size() > kMaxEmitters) emitters.resize(kMaxEmitters);
    int maxId = 0;
    for (size_t i = 0; i < emitters.size(); ++i) if (emitters[i].id > maxId) maxId = emitters[i].id;
    for (size_t i = 0; i < emitters.size(); ++i) {
        Emitter& e = emitters[i];
        e.tex.Clamp();
        e.angleDeg = finitef(e.angleDeg) ? clampf(e.angleDeg, kAngleMin, kAngleMax) : 45.f;
        float l = sqrtf(e.hdgX * e.hdgX + e.hdgY * e.hdgY);
        if (!finitef(l) || l < 1e-4f) { e.hdgX = e.pos.x >= 0 ? 1.f : -1.f; e.hdgY = 0; } else if (fabsf(l - 1.f) > 1e-3f) { e.hdgX /= l; e.hdgY /= l; } // already-normalised headings stay bit-exact (lossless save/load)
        bool dup = e.id <= 0;
        for (size_t j = 0; j < i && !dup; ++j) if (emitters[j].id == e.id) dup = true;
        if (dup) e.id = ++maxId;
    }
    if (nextId <= maxId) nextId = maxId + 1;
}
void ResolveEmitters(const Profile& p, std::vector<Resolved>& out) {
    out.clear();
    for (size_t i = 0; i < p.emitters.size(); ++i) {
        const Emitter& e = p.emitters[i];
        if (!e.active) continue;
        Resolved r; r.pos = e.pos; r.hx = e.hdgX; r.hy = e.hdgY; r.angle = e.angleDeg; r.src = &e; r.mirrored = false;
        out.push_back(r);
        bool m = (p.mirrorMode == MIRROR_ALL) || (p.mirrorMode == MIRROR_SELECTIVE && e.mirror);
        if (m && fabsf(e.pos.x) > 0.05f) { r.pos.x = -e.pos.x; r.hx = -e.hdgX; r.mirrored = true; out.push_back(r); }
    }
}
int CountActive(const Profile& p) { int n = 0; for (size_t i = 0; i < p.emitters.size(); ++i) if (p.emitters[i].active) ++n; return n; }
int CountMirrored(const Profile& p) { std::vector<Resolved> r; ResolveEmitters(p, r); int n = 0; for (size_t i = 0; i < r.size(); ++i) if (r[i].mirrored) ++n; return n; }

const SlotDef kSlots[kSlotCount] = {
    { "Front Wheel / Fender", 0.98f, 0.74f, 0.40f, 1.0f, 0.0f },
    { "Rear Wheel / Fender",  0.98f, 0.24f, 0.40f, 1.0f, 0.0f },
    { "Under Hood",           0.62f, 0.84f, 0.48f, 0.8f, 0.6f },
    { "Under Side Skirt",     0.97f, 0.50f, 0.14f, 1.0f, 0.0f },
    { "Front Bumper",         0.72f, 0.99f, 0.28f, 0.5f, 0.85f },
    { "Rear Bumper",          0.72f, 0.01f, 0.28f, 0.5f, -0.85f },
    { "Door Line",            0.99f, 0.52f, 0.50f, 1.0f, 0.0f },
    { "Trunk Edge",           0.66f, 0.10f, 0.55f, 0.7f, -0.7f } };

bool AddSlotEmitter(Profile& p, int slot, const V3& mn, const V3& mx, const TexSettings& tex) {
    if (slot < 0 || slot >= kSlotCount || (int)p.emitters.size() >= kMaxEmitters) return false;
    const SlotDef& s = kSlots[slot];
    Emitter e; e.id = p.nextId++;
    e.pos = V3(s.fx * mx.x, mn.y + s.fy * (mx.y - mn.y), mn.z + s.fz * (mx.z - mn.z));
    float l = sqrtf(s.hx * s.hx + s.hy * s.hy); e.hdgX = s.hx / l; e.hdgY = s.hy / l;
    e.angleDeg = 45.f; e.tex = tex; e.mirror = true; e.active = true;
    p.emitters.push_back(e);
    return true;
}
void MakeDefaultProfile(Profile& p, const V3& mn, const V3& mx) {
    p = Profile(); TexSettings t; AddSlotEmitter(p, 0, mn, mx, t);
}
}
