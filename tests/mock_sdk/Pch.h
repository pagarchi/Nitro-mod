#pragma once
// MOCK of the RenderWare / Plugin-SDK types that src/MeshPick.cpp touches. It exists ONLY so the real MeshPick.cpp logic can be compiled and
// exercised on a Linux host. It encodes the project's BELIEF about the SDK layout; it does NOT prove the real Plugin-SDK matches it.
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
struct RwV3d { float x, y, z; };
struct RwMatrix { RwV3d right; unsigned flags; RwV3d up; unsigned pad1; RwV3d at; unsigned pad2; RwV3d pos; unsigned pad3; };
struct RwObject { unsigned char type, subType, flags, privateFlags; void* parent; };
struct RwObjectHasFrame { RwObject object; };
struct RwFrame { RwMatrix ltm; };
struct RpTriangle { unsigned short vertIndex[3]; unsigned short matIndex; };
struct RpMorphTarget { RwV3d* verts; };
struct RpGeometry { int numTriangles, numVertices, numMorphTargets; RpTriangle* triangles; RpMorphTarget* morphTarget; };
struct RpAtomic { RwObjectHasFrame object; RpGeometry* geometry; };
struct RpClump { RwObject object; std::vector<RpAtomic*> atomics; };
enum { rpATOMICRENDER = 0x04, rpCLUMP = 2 };
struct CVehicle { RwObject* m_pRwObject = nullptr; };
struct RwTexture;
typedef RpAtomic* (*RpAtomicCallBack)(RpAtomic*, void*);
inline RpClump* RpClumpForAllAtomics(RpClump* c, RpAtomicCallBack cb, void* data) { for (size_t i = 0; i < c->atomics.size(); ++i) if (!cb(c->atomics[i], data)) break; return c; }
inline RwMatrix* RwFrameGetLTM(RwFrame* f) { return &f->ltm; }
