#pragma once
#include <string>
#include <vector>
#include "Math.h"
namespace nfx {
enum Category { CAT_NEON, CAT_MATRIX, CAT_WATER, CAT_FIRE, CAT_MAGMA, CAT_CRYO, CAT_PLASMA, CAT_ELECTRIC,
                CAT_TOXIC, CAT_HOLO, CAT_COSMIC, CAT_CHROME, CAT_GOLDEN, CAT_SHADOW, CAT_SANDSTORM, CAT_COUNT };
extern const char* const kCategoryNames[CAT_COUNT];
enum MirrorMode { MIRROR_OFF = 0, MIRROR_ALL = 1, MIRROR_SELECTIVE = 2 };
// Per-vehicle emitter capacity (mirrored jets are derived at draw time, never stored).
// 64 emitters (+ up to 64 derived mirrors) = 128 jets max: ~45 KB of profile data, a handful of KB per frame of vertex work.
const int kMaxEmitters = 64;
// Value ranges: used by TexSettings::Clamp()/Profile::Sanitize() (live data) AND by strict save validation (which rejects instead of clamping).
const float kAngleMin = 5.f, kAngleMax = 85.f;
const float kSpeedMin = 0.1f, kSpeedMax = 4.f, kGlowMax = 2.f, kOpacityMin = 0.1f, kPowerMin = 0.3f, kPowerMax = 2.f;
const float kMaxLocalX = 8.f, kMaxLocalY = 12.f, kMaxLocalZ = 8.f;      // plausible emitter position (vehicle-local, metres)
const int kMaxEmitterId = 1 << 30;
// A save is only valid for the exact vehicle model it was made on.
inline bool ModelCompatible(int savedModelId, int currentModelId) { return savedModelId >= 0 && savedModelId == currentModelId; }

// Every texture category only changes how the SAME jet looks.
struct TexSettings {
    int category = CAT_NEON;      // default look: white-blue
    float colorVar = 0.f;         // 0..1 hue rotation
    float speed = 1.f;            // 0.1..4
    float detail = 0.5f;          // 0..1 noise detail density
    float glow = 1.f;             // 0..2
    float opacity = 1.f;          // 0.1..1
    float power = 1.f;            // 0.3..2 (length / brightness)
    void Clamp();
};

struct Emitter {
    int id = 0;
    V3 pos;                       // vehicle-local (x right, y forward, z up)
    float hdgX = 1.f, hdgY = 0.f; // local horizontal heading (outward)
    float angleDeg = 45.f;        // elevation above horizontal
    bool active = true;
    bool mirror = true;           // used by selective mirror mode
    TexSettings tex;
};

struct Profile {
    int mirrorMode = MIRROR_SELECTIVE;
    bool jetEnabled = true;
    int nextId = 1;
    std::string presetName;       // active preset ("" = none)
    std::vector<Emitter> emitters;
    void Sanitize();
};

struct Resolved { V3 pos; float hx, hy, angle; const Emitter* src; bool mirrored; };
// Mirrored emitters are DERIVED (never stored): they always inherit their source and
// vanish automatically when the source is deleted/deactivated.
void ResolveEmitters(const Profile& p, std::vector<Resolved>& out);
int CountActive(const Profile& p);
int CountMirrored(const Profile& p);

struct SlotDef { const char* name; float fx, fy, fz, hx, hy; };
const int kSlotCount = 8;
extern const SlotDef kSlots[kSlotCount];
bool AddSlotEmitter(Profile& p, int slot, const V3& bbMin, const V3& bbMax, const TexSettings& tex);
void MakeDefaultProfile(Profile& p, const V3& bbMin, const V3& bbMax);
}
