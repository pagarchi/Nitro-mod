#pragma once
#include <cmath>
namespace nfx {
struct V3 { float x, y, z; V3() : x(0), y(0), z(0) {} V3(float a, float b, float c) : x(a), y(b), z(c) {} };
inline V3 operator+(const V3& a, const V3& b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline V3 operator-(const V3& a, const V3& b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline V3 operator*(const V3& a, float s) { return V3(a.x * s, a.y * s, a.z * s); }
inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3& a, const V3& b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float vlen(const V3& a) { return sqrtf(dot(a, a)); }
inline V3 vnorm(const V3& a) { float l = vlen(a); return l > 1e-6f ? a * (1.0f / l) : V3(0, 0, 1); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }
inline bool finitef(float v) { return v == v && (v - v) == 0.f; }
}
