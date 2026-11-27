// Runs the REAL src/MeshPick.cpp (PickVehicleMesh) against a mock RpClump built from tests/mock_sdk/Pch.h.
// Verifies: real-triangle picking, nearest surface across atomics, hidden atomics skipped, frame (LTM) transform, invalid geometry handled,
// no hit => failure (there is no bounding-box fallback). Does NOT verify the real Plugin-SDK / game.
#include "nitrofx/App.h"
#include "nitrofx/MeshMath.h"
#include <cstdio>
using namespace nfx;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { ++fails; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
namespace nfx {   // App.h symbols that MeshPick.cpp's translation unit references only through declarations are not needed at link time.
}
struct MockAtomic { RpAtomic a; RpGeometry g; std::vector<RwV3d> v; std::vector<RpTriangle> t; RpMorphTarget mt; RwFrame f; };
static RwMatrix Ident() { RwMatrix m; memset(&m, 0, sizeof m); m.right.x = 1; m.up.y = 1; m.at.z = 1; return m; }
static void AddQuad(MockAtomic& m, float z, float half) {
    int b = (int)m.v.size(); RwV3d p[4] = { { -half, -half, z }, { half, -half, z }, { half, half, z }, { -half, half, z } }; for (int i = 0; i < 4; ++i) m.v.push_back(p[i]);
    RpTriangle t1 = { { (unsigned short)b, (unsigned short)(b + 1), (unsigned short)(b + 2) }, 0 }, t2 = { { (unsigned short)b, (unsigned short)(b + 2), (unsigned short)(b + 3) }, 0 }; m.t.push_back(t1); m.t.push_back(t2);
}
static void Finish(MockAtomic& m, bool render = true) {
    m.mt.verts = m.v.empty() ? nullptr : &m.v[0]; m.g.numVertices = (int)m.v.size(); m.g.numTriangles = (int)m.t.size(); m.g.numMorphTargets = 1; m.g.triangles = m.t.empty() ? nullptr : &m.t[0]; m.g.morphTarget = &m.mt;
    memset(&m.a, 0, sizeof m.a); m.a.geometry = &m.g; m.a.object.object.flags = render ? rpATOMICRENDER : 0; m.a.object.object.parent = &m.f;
}
int main() {
    // bounding box would be [-2,2]^3 for everything below; picking must return the TRIANGLE surface instead
    RpClump clump; memset(&clump.object, 0, sizeof clump.object); clump.object.type = rpCLUMP; CVehicle veh; veh.m_pRwObject = &clump.object;
    MockAtomic body, hidden, wheel; body.f.ltm = Ident(); hidden.f.ltm = Ident(); wheel.f.ltm = Ident();
    AddQuad(body, 1.0f, 1.0f);                        // body roof at z = 1, 2 x 2 m
    AddQuad(hidden, 1.5f, 1.5f);                      // hidden variant ABOVE the roof (render flag off) must be ignored
    AddQuad(wheel, 0.2f, 0.3f);                       // small part below the roof
    Finish(body); Finish(hidden, false); Finish(wheel);
    clump.atomics.push_back(&hidden.a); clump.atomics.push_back(&body.a); clump.atomics.push_back(&wheel.a);
    V3 wh, wn;
    CHECK(PickVehicleMesh(&veh, V3(0, 0, 10), V3(0, 0, -1), wh, wn)); CHECK(fabsf(wh.z - 1.0f) < 1e-4f && fabsf(wh.x) < 1e-4f && wn.z > 0.99f);    // roof, not hidden atomic (z=1.5), not wheel (z=0.2)
    CHECK(PickVehicleMesh(&veh, V3(0, 0, -10), V3(0, 0, 1), wh, wn)); CHECK(fabsf(wh.z - 0.2f) < 1e-4f && wn.z < -0.99f);                         // from below: nearest surface is the wheel part, normal faces the ray
    CHECK(PickVehicleMesh(&veh, V3(0.8f, 0.8f, 10), V3(0, 0, -1), wh, wn) && fabsf(wh.z - 1.0f) < 1e-4f);                                           // beyond the wheel's footprint
    CHECK(!PickVehicleMesh(&veh, V3(1.9f, 1.9f, 10), V3(0, 0, -1), wh, wn));                                                                       // inside the bounding box but NOT on any triangle => no pick (no bbox fallback)
    CHECK(!PickVehicleMesh(&veh, V3(0, 0, 100), V3(0, 0, -1), wh, wn));                                                                            // farther than 60 m
    // frame transform (LTM): move + rotate the body atomic 90 deg about Z and lift it by 5
    body.f.ltm = Ident(); body.f.ltm.right = { 0, 1, 0 }; body.f.ltm.up = { -1, 0, 0 }; body.f.ltm.pos = { 10, 0, 5 };
    CHECK(PickVehicleMesh(&veh, V3(10, 0.5f, 50), V3(0, 0, -1), wh, wn) && fabsf(wh.z - 6.0f) < 1e-4f && fabsf(wh.x - 10.f) < 1e-4f && fabsf(wh.y - 0.5f) < 1e-4f);
    CHECK(!PickVehicleMesh(&veh, V3(0, 0, 50), V3(0, 0, -1), wh, wn) || fabsf(wh.z - 0.2f) < 1e-4f);                                               // body moved away; only the wheel part remains at the origin
    // invalid geometry / pointers are skipped safely
    MockAtomic g1, g2, g3, g4, g5; MockAtomic* gs[5] = { &g1, &g2, &g3, &g4, &g5 }; for (int i = 0; i < 5; ++i) { gs[i]->f.ltm = Ident(); AddQuad(*gs[i], 3.f, 2.f); Finish(*gs[i]); }
    g1.g.triangles = nullptr; g2.g.morphTarget = nullptr; g3.g.numMorphTargets = 0; g4.mt.verts = nullptr; g5.a.object.object.parent = nullptr;
    for (int i = 0; i < 5; ++i) clump.atomics.push_back(&gs[i]->a);
    MockAtomic badIdx; badIdx.f.ltm = Ident(); AddQuad(badIdx, 4.f, 2.f); badIdx.t[0].vertIndex[2] = 9999; Finish(badIdx); clump.atomics.push_back(&badIdx.a);       // one corrupt index, second triangle still valid
    MockAtomic huge; huge.f.ltm = Ident(); AddQuad(huge, 4.5f, 2.f); Finish(huge); huge.g.numVertices = 1000000; clump.atomics.push_back(&huge.a);              // absurd vertex count => skipped, no giant allocation
    MockAtomic nanv; nanv.f.ltm = Ident(); AddQuad(nanv, 4.7f, 2.f); nanv.v[0].x = NAN; Finish(nanv); clump.atomics.push_back(&nanv.a);
    clump.atomics.push_back(nullptr);                                                                                                                 // null atomic: the real RW iterator would never pass it, but the callback must not crash
    body.f.ltm = Ident();
    // (0.5,-0.5) lies only in the CORRUPT triangle of badIdx (skipped), in nanv's NaN-vertex triangles (skipped) and under the invalid atomics: the nearest VALID surface is the body roof (z=1)
    CHECK(PickVehicleMesh(&veh, V3(0.5f, -0.5f, 20), V3(0, 0, -1), wh, wn)); CHECK(fabsf(wh.z - 1.0f) < 1e-4f);
    // (-0.5,0.5) lies in badIdx's still-valid second triangle (z=4): invalid siblings are ignored, the valid one is picked
    CHECK(PickVehicleMesh(&veh, V3(-0.5f, 0.5f, 20), V3(0, 0, -1), wh, wn)); CHECK(fabsf(wh.z - 4.0f) < 1e-4f && nfx::finitef(wh.x) && nfx::finitef(wn.z));
    clump.atomics.pop_back();
    // top-level null / wrong-type handling and bad rays
    CVehicle none; CHECK(!PickVehicleMesh(nullptr, V3(0, 0, 10), V3(0, 0, -1), wh, wn)); CHECK(!PickVehicleMesh(&none, V3(0, 0, 10), V3(0, 0, -1), wh, wn));
    RwObject notClump = clump.object; notClump.type = 1; CVehicle wrong; wrong.m_pRwObject = &notClump; CHECK(!PickVehicleMesh(&wrong, V3(0, 0, 10), V3(0, 0, -1), wh, wn));
    CHECK(!PickVehicleMesh(&veh, V3(NAN, 0, 10), V3(0, 0, -1), wh, wn)); CHECK(!PickVehicleMesh(&veh, V3(0, 0, 10), V3(0, 0, 0), wh, wn)); CHECK(!PickVehicleMesh(&veh, V3(0, 0, 10), V3(INFINITY, 0, 0), wh, wn));
    printf(fails ? "\n%d MESHPICK MOCK FAILURES\n" : "\nMESHPICK MOCK TESTS PASSED\n", fails); return fails ? 1 : 0;
}
