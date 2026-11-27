#include "nitrofx/App.h"
#include "nitrofx/TexGen.h"
namespace nfx {
#define RS(state, val) RwRenderStateSet(state, (void*)(val))
static const int kSt = 14;                           // stations along the jet
static RwIm3DVertex gV[(kSt + 1) * 2];
static RwImVertexIndex gI[kSt * 6];
static bool gIdxInit = false;

static void Ribbon(const V3* c, const V3* side, const float* wd, const float* al, float vScale, float vOff, float r, float g, float b) {
    for (int i = 0; i <= kSt; ++i) {
        float s = i / (float)kSt; V3 a = c[i] - side[i] * wd[i], bb = c[i] + side[i] * wd[i];
        int A = (int)clampf(al[i] * 255.f, 0, 255);
        RwIm3DVertexSetPos(&gV[i * 2], a.x, a.y, a.z); RwIm3DVertexSetU(&gV[i * 2], 0.f); RwIm3DVertexSetV(&gV[i * 2], s * vScale + vOff);
        RwIm3DVertexSetRGBA(&gV[i * 2], (int)(r * 255), (int)(g * 255), (int)(b * 255), A);
        RwIm3DVertexSetPos(&gV[i * 2 + 1], bb.x, bb.y, bb.z); RwIm3DVertexSetU(&gV[i * 2 + 1], 1.f); RwIm3DVertexSetV(&gV[i * 2 + 1], s * vScale + vOff);
        RwIm3DVertexSetRGBA(&gV[i * 2 + 1], (int)(r * 255), (int)(g * 255), (int)(b * 255), A);
    }
    if (RwIm3DTransform(gV, (kSt + 1) * 2, nullptr, rwIM3D_VERTEXUV)) { RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, gI, kSt * 6); RwIm3DEnd(); }
}
struct Job { float dist; int idx; };
static bool Farther(const Job& a, const Job& b) { return a.dist > b.dist; }

void DrawJets() {
    if (!S.veh || !S.profile.jetEnabled) return;
    V3 vr, vf, vu, vp; if (!VehicleBasis(S.veh, vr, vf, vu, vp)) return;
    if (!gIdxInit) { for (int i = 0; i < kSt; ++i) { int k = i * 6, a = i * 2; gI[k] = a; gI[k + 1] = a + 1; gI[k + 2] = a + 2; gI[k + 3] = a + 1; gI[k + 4] = a + 3; gI[k + 5] = a + 2; } gIdxInit = true; }
    std::vector<Resolved> res; ResolveEmitters(S.profile, res); if (res.empty()) return;
    CVector cp = TheCamera.GetPosition(); V3 cam(cp.x, cp.y, cp.z);
    std::vector<Job> jobs; std::vector<V3> origin(res.size());
    for (size_t i = 0; i < res.size(); ++i) { origin[i] = LocalToWorld(S.veh, res[i].pos); Job j; j.dist = vlen(origin[i] - cam); j.idx = (int)i; jobs.push_back(j); }
    std::sort(jobs.begin(), jobs.end(), Farther);                         // far-to-near for blended draws
    float t = S.time;
    // render state: REAL depth test against the already-rendered vehicle, no depth write -> body occludes far-side jets
    RS(rwRENDERSTATEZTESTENABLE, TRUE); RS(rwRENDERSTATEZWRITEENABLE, FALSE); RS(rwRENDERSTATEVERTEXALPHAENABLE, TRUE);
    RS(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE); RS(rwRENDERSTATEFOGENABLE, FALSE); RS(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
    RS(rwRENDERSTATETEXTUREADDRESS, rwTEXTUREADDRESSWRAP); RS(rwRENDERSTATETEXTUREFILTER, rwFILTERLINEAR);
    for (size_t jn = 0; jn < jobs.size(); ++jn) {
        const Resolved& r = res[jobs[jn].idx]; const TexSettings& ts = r.src->tex; V3 o = origin[jobs[jn].idx];
        RwTexture* tex = gTex.Get(ts); if (!tex) continue;
        RS(rwRENDERSTATEDESTBLEND, CategoryIsAdditive(ts.category) ? rwBLENDONE : rwBLENDINVSRCALPHA);
        RS(rwRENDERSTATETEXTURERASTER, RwTextureGetRaster(tex));
        float ang = r.angle * 0.0174533f; V3 dl(r.hx * cosf(ang), r.hy * cosf(ang), sinf(ang)); V3 d = vnorm(LocalDirToWorld(S.veh, dl));
        float L = 2.6f * (0.6f + 0.4f * ts.power), w0 = 0.085f * (0.75f + 0.25f * ts.power);   // narrow, elongated
        V3 c[kSt + 1], sd[kSt + 1]; float wCore[kSt + 1], aCore[kSt + 1], wGlow[kSt + 1], aGlow[kSt + 1];
        float bright = clampf(0.55f + 0.45f * ts.power, 0.3f, 1.4f);
        for (int i = 0; i <= kSt; ++i) { float s = i / (float)kSt; c[i] = o + d * (L * s) - V3(0, 0, 1) * (0.30f * L * s * s); }
        for (int i = 0; i <= kSt; ++i) {
            float s = i / (float)kSt; V3 tg = vnorm(c[i < kSt ? i + 1 : i] - c[i > 0 ? i - 1 : i]); V3 toCam = vnorm(cam - c[i]);
            V3 sv = cross(tg, toCam); sd[i] = vlen(sv) < 1e-4f ? V3(0, 0, 1) : vnorm(sv);
            float fade = smoothstepf(0.f, 0.06f, s) * powf(1.f - s, 0.7f);
            wCore[i] = w0 * (1.f - 0.55f * s) + w0 * 0.15f; aCore[i] = clampf(fade * ts.opacity * bright, 0, 1);
            wGlow[i] = wCore[i] * 3.2f;                       aGlow[i] = clampf(fade * ts.opacity * ts.glow * 0.28f * bright, 0, 1);
        }
        float scroll = -t * 1.2f * ts.speed;
        if (ts.glow > 0.02f) Ribbon(c, sd, wGlow, aGlow, 0.9f, scroll * 0.6f + 0.37f, 1, 1, 1);
        Ribbon(c, sd, wCore, aCore, 1.6f, scroll, 1, 1, 1);
    }
    RS(rwRENDERSTATEZWRITEENABLE, TRUE); RS(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA); RS(rwRENDERSTATETEXTURERASTER, nullptr);
}
}
