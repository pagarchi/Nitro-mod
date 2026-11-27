#pragma once
// Pure math for mesh picking (no RenderWare dependency, unit-testable on the host).
#include "Math.h"
namespace nfx {
inline bool FiniteV3(const V3& v) { return finitef(v.x) && finitef(v.y) && finitef(v.z); }

// Moller-Trumbore ray/triangle test, double sided. Returns hit distance t (> tMin) and the unit face normal
// flipped to face the ray origin. Non-finite input or output (NaN / inf) never produces a hit.
inline bool RayTriangle(const V3& o, const V3& d, const V3& a, const V3& b, const V3& c, float& t, V3& n) {
    const float tMin = 1e-3f;
    if (!FiniteV3(o) || !FiniteV3(d) || !FiniteV3(a) || !FiniteV3(b) || !FiniteV3(c)) return false;
    V3 e1 = b - a, e2 = c - a, p = cross(d, e2); float det = dot(e1, p);
    if (!(fabsf(det) >= 1e-12f)) return false;                                  // parallel, degenerate or NaN
    float inv = 1.f / det; V3 s = o - a; float u = dot(s, p) * inv; if (!(u >= 0.f && u <= 1.f)) return false;
    V3 q = cross(s, e1); float v = dot(d, q) * inv; if (!(v >= 0.f && u + v <= 1.f)) return false;
    float tt = dot(e2, q) * inv; if (!(tt > tMin) || !finitef(tt)) return false;
    V3 fn = cross(e1, e2); float l = vlen(fn); if (!(l >= 1e-12f) || !finitef(l)) return false; fn = fn * (1.f / l);
    if (dot(fn, d) > 0.f) fn = fn * -1.f;
    t = tt; n = fn; return true;
}

// Nearest intersection of a ray with an indexed triangle list (vertices already in the ray's space, i.e. world space).
// TriT must expose `vertIndex[3]` (RpTriangle does). `best` is IN/OUT: only hits closer than the incoming `best` are accepted and
// `best` / `bestN` are updated; returns true when this mesh produced a closer hit. Safe against null pointers, bad counts,
// out-of-range vertex indices and non-finite vertices (those triangles are ignored, never dereferenced out of bounds).
template <class TriT>
inline bool RayMesh(const V3& o, const V3& d, const V3* wv, int nv, const TriT* tris, int nt, float& best, V3& bestN) {
    if (!wv || !tris || nv <= 0 || nt <= 0) return false;
    bool found = false;
    for (int i = 0; i < nt; ++i) {
        int a = (int)tris[i].vertIndex[0], b = (int)tris[i].vertIndex[1], c = (int)tris[i].vertIndex[2];
        if (a < 0 || b < 0 || c < 0 || a >= nv || b >= nv || c >= nv) continue;
        float t; V3 n;
        if (RayTriangle(o, d, wv[a], wv[b], wv[c], t, n) && t < best) { best = t; bestN = n; found = true; }
    }
    return found;
}

// Vehicle-local <-> world conversion for the vehicle basis (r = right, f = forward, u = up, p = position; local x right, y forward, z up).
inline bool BasisUsable(const V3& r, const V3& f, const V3& u, const V3& p) {      // finite, ~unit length, ~orthogonal
    if (!FiniteV3(r) || !FiniteV3(f) || !FiniteV3(u) || !FiniteV3(p)) return false;
    const float tol = 0.05f;
    if (fabsf(vlen(r) - 1.f) > tol || fabsf(vlen(f) - 1.f) > tol || fabsf(vlen(u) - 1.f) > tol) return false;
    return fabsf(dot(r, f)) <= tol && fabsf(dot(r, u)) <= tol && fabsf(dot(f, u)) <= tol;
}
inline V3 WorldPointToLocal(const V3& w, const V3& r, const V3& f, const V3& u, const V3& p) { V3 rel = w - p; return V3(dot(rel, r), dot(rel, f), dot(rel, u)); }
inline V3 WorldDirToLocal3(const V3& w, const V3& r, const V3& f, const V3& u) { return V3(dot(w, r), dot(w, f), dot(w, u)); }
inline V3 LocalPointToWorld3(const V3& l, const V3& r, const V3& f, const V3& u, const V3& p) { return p + r * l.x + f * l.y + u * l.z; }
}
