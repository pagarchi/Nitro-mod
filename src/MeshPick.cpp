// Real vehicle mesh picking: casts a world-space ray against the triangles of the vehicle's RENDERED RenderWare geometry
// (every atomic of the vehicle's RpClump that has its render flag set: body, bumpers, doors, wheels, spoilers ...).
// The bounding box is NOT used anywhere in this file or in the mouse-picking path (Editor.cpp); if no triangle is hit, picking simply fails.
#include "nitrofx/App.h"
#include "nitrofx/MeshMath.h"
namespace nfx {
namespace {
const int kMaxPickVerts = 65535;      // RpTriangle indices are 16 bit: more vertices could not even be addressed
const int kMaxPickTris = 262144;      // sanity cap against a corrupted triangle count
struct PickCtx { V3 o, d; float best; V3 nrm; bool found; std::vector<V3> wv; };

RpAtomic* AtomicCB(RpAtomic* atomic, void* data) {
    PickCtx* c = static_cast<PickCtx*>(data);
    if (!c || !atomic || !(atomic->object.object.flags & rpATOMICRENDER)) return atomic;   // skip hidden variants (damaged parts, LODs)
    RpGeometry* g = atomic->geometry;
    if (!g || !g->triangles || !g->morphTarget || g->numMorphTargets <= 0) return atomic;
    const int nv = g->numVertices, nt = g->numTriangles;
    if (nv <= 0 || nv > kMaxPickVerts || nt <= 0 || nt > kMaxPickTris) return atomic;
    const RwV3d* verts = g->morphTarget[0].verts; if (!verts) return atomic;
    RwFrame* fr = static_cast<RwFrame*>(atomic->object.object.parent); if (!fr) return atomic;
    RwMatrix* m = RwFrameGetLTM(fr); if (!m) return atomic;                                // atomic-local -> world
    c->wv.resize((size_t)nv);
    for (int i = 0; i < nv; ++i) {                                                         // vertices to world space (handles any frame scale)
        const RwV3d& v = verts[i];
        c->wv[i] = V3(m->right.x * v.x + m->up.x * v.y + m->at.x * v.z + m->pos.x,
                      m->right.y * v.x + m->up.y * v.y + m->at.y * v.z + m->pos.y,
                      m->right.z * v.x + m->up.z * v.y + m->at.z * v.z + m->pos.z);
    }
    // Nearest-hit triangle test with bounds / finiteness checks (MeshMath.h, unit-tested on the host). `best` keeps the closest hit across ALL atomics.
    if (RayMesh(c->o, c->d, c->wv.data(), nv, g->triangles, nt, c->best, c->nrm)) c->found = true;
    return atomic;                                                                         // non-null = keep iterating
}
}

bool PickVehicleMesh(CVehicle* v, const V3& origin, const V3& dir, V3& worldHit, V3& worldNormal) {
    if (!v || !v->m_pRwObject || v->m_pRwObject->type != rpCLUMP) return false;
    if (!FiniteV3(origin) || !FiniteV3(dir) || vlen(dir) < 1e-6f) return false;
    RpClump* clump = reinterpret_cast<RpClump*>(v->m_pRwObject);
    PickCtx c; c.o = origin; c.d = vnorm(dir); c.best = 60.f; c.found = false; c.nrm = V3(0, 0, 1);   // vehicle must be within 60 m of the camera
    c.wv.reserve(8192);
    RpClumpForAllAtomics(clump, AtomicCB, &c);
    if (!c.found) return false;                                                            // no triangle hit => no pick (never a bounding-box guess)
    V3 hit = c.o + c.d * c.best; if (!FiniteV3(hit) || !FiniteV3(c.nrm)) return false;
    worldHit = hit; worldNormal = c.nrm; return true;
}
}
