// Host-side logic tests (no GTA / RenderWare needed). Build & run (Linux/macOS/MinGW):
//   g++ -std=c++14 -Wall -Iinclude -Iinclude/nitrofx tests/logic_test.cpp src/Types.cpp src/Storage.cpp -o logic_test && ./logic_test
// (tests/run_tests.sh does this in a scratch directory, with sanitizers, and also runs the MeshPick.cpp mock-clump test.)
// NOTE: the tests write into ./NITRO_FX relative to the current directory - run them from a scratch directory.
#include "nitrofx/Storage.h"
#include "nitrofx/MeshMath.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>
using namespace nfx;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { ++fails; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static Profile MakeBig(int n, unsigned seed) {
    Profile p; p.mirrorMode = MIRROR_SELECTIVE; p.jetEnabled = false;
    for (int i = 0; i < n; ++i) {
        Emitter e; e.id = p.nextId++; seed = seed * 1664525u + 1013904223u;
        e.pos = V3(((seed >> 8) % 1000) / 250.f - 2.f, ((seed >> 4) % 1000) / 200.f - 2.5f, ((seed >> 2) % 1000) / 500.f);
        float a = (seed % 628) / 100.f; e.hdgX = cosf(a); e.hdgY = sinf(a); e.angleDeg = 5.f + (seed % 800) / 10.f;
        e.active = (i % 7) != 0; e.mirror = (i % 3) != 0;
        e.tex.category = i % CAT_COUNT; e.tex.colorVar = (i % 11) / 11.f; e.tex.speed = 0.1f + (i % 39) / 10.f; e.tex.detail = (i % 5) / 4.f;
        e.tex.glow = (i % 21) / 10.f; e.tex.opacity = 0.1f + (i % 10) / 11.f; e.tex.power = 0.3f + (i % 17) / 10.f;
        p.emitters.push_back(e);
    }
    p.Sanitize(); return p;
}
static bool SameE(const Emitter& a, const Emitter& b) {
    return a.id == b.id && a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z && a.hdgX == b.hdgX && a.hdgY == b.hdgY &&
           a.angleDeg == b.angleDeg && a.active == b.active && a.mirror == b.mirror && a.tex.category == b.tex.category &&
           a.tex.colorVar == b.tex.colorVar && a.tex.speed == b.tex.speed && a.tex.detail == b.tex.detail && a.tex.glow == b.tex.glow &&
           a.tex.opacity == b.tex.opacity && a.tex.power == b.tex.power;
}
static std::string ReadAll(const std::string& p) { std::ifstream f(p.c_str(), std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
static void WriteAll(const std::string& p, const std::string& d) { std::ofstream f(p.c_str(), std::ios::binary | std::ios::trunc); f << d; }

// ---------------------------------------------------------------------------------------------------------
// helpers for the added tests
// ---------------------------------------------------------------------------------------------------------
static unsigned FnvT(const std::string& s) { unsigned h = 2166136261u; for (size_t i = 0; i < s.size(); ++i) { h ^= (unsigned char)s[i]; h *= 16777619u; } return h; }   // independent copy of the CRC oracle
static std::string BodyOf(const std::string& full) { size_t cp = full.rfind("\ncrc="); return full.substr(0, cp + 1); }
static std::string Seal(const std::string& body) { char b[32]; snprintf(b, sizeof b, "crc=%08x\n", FnvT(body)); return body + b; }   // re-signs a TAMPERED body so only the field validation can reject it
static std::vector<std::string> SplitS(const std::string& s, char sep) { std::vector<std::string> r; size_t a = 0; for (;;) { size_t c = s.find(sep, a); if (c == std::string::npos) { r.push_back(s.substr(a)); break; } r.push_back(s.substr(a, c - a)); a = c + 1; } return r; }
static std::string JoinS(const std::vector<std::string>& v, char sep) { std::string r; for (size_t i = 0; i < v.size(); ++i) { if (i) r += sep; r += v[i]; } return r; }
static std::vector<std::string> BodyLines(const std::string& body) { std::vector<std::string> v = SplitS(body, '\n'); if (!v.empty() && v.back().empty()) v.pop_back(); return v; }
static std::string JoinLines(const std::vector<std::string>& v) { std::string r; for (size_t i = 0; i < v.size(); ++i) r += v[i] + "\n"; return r; }
static const char* kTId = "0123456789abcdef";
static SaveMeta TMeta() { SaveMeta m; m.id = kTId; m.name = "Base Save"; m.modelId = 541; m.created = 1700000000LL; return m; }
static std::string BaseSaveText(const Profile& p) { return SerializeSave(TMeta(), p); }
static int FirstLineWith(const std::vector<std::string>& v, const char* prefix) { for (size_t i = 0; i < v.size(); ++i) if (v[i].compare(0, strlen(prefix), prefix) == 0) return (int)i; return -1; }
static void ExpectReject(const char* what, const std::string& body) {
    SaveMeta sm; sm.name = "sentinel"; sm.modelId = 7; Profile sp = MakeBig(2, 5); std::string before = SerializeProfile(sp);
    SaveStatus st = ParseSave(Seal(body), kTId, sm, sp);
    if (st != SAVE_CORRUPT) { ++fails; printf("FAIL: corrupt save accepted: %s\n", what); }
    if (sm.name != "sentinel" || sm.modelId != 7 || SerializeProfile(sp) != before) { ++fails; printf("FAIL: output modified although rejected: %s\n", what); }
}
static void ExpectAccept(const char* what, const std::string& body) {
    SaveMeta sm; Profile sp; if (ParseSave(Seal(body), kTId, sm, sp) != SAVE_OK) { ++fails; printf("FAIL: valid save rejected: %s\n", what); }
}
// replace one comma-separated field of the first emitter record
static std::string WithEField(const std::string& body, int field, const std::string& value) {
    std::vector<std::string> L = BodyLines(body); int li = FirstLineWith(L, "e="); if (li < 0) return body;
    std::vector<std::string> f = SplitS(L[li].substr(2), ','); if (field >= 0 && field < (int)f.size()) f[field] = value; L[li] = "e=" + JoinS(f, ','); return JoinLines(L);
}
static std::string WithoutLine(const std::string& body, const char* prefix) { std::vector<std::string> L = BodyLines(body); int i = FirstLineWith(L, prefix); if (i >= 0) L.erase(L.begin() + i); return JoinLines(L); }
static std::string DupLine(const std::string& body, const char* prefix) { std::vector<std::string> L = BodyLines(body); int i = FirstLineWith(L, prefix); if (i >= 0) L.insert(L.begin() + i, L[i]); return JoinLines(L); }
static std::string ReplaceLine(const std::string& body, const char* prefix, const std::string& line) { std::vector<std::string> L = BodyLines(body); int i = FirstLineWith(L, prefix); if (i >= 0) L[i] = line; return JoinLines(L); }
static void WipeSaves() { std::vector<SaveMeta> L = ListSaves(nullptr); for (size_t i = 0; i < L.size(); ++i) DeleteSaveFile(L[i].id); if (system("rm -f NITRO_FX/saves/*.corrupt NITRO_FX/saves/*.tmp NITRO_FX/saves/*.nfx") != 0) {} }

// 2. strict validation --------------------------------------------------------------------------------------
static void TestStrictValidation() {
    Profile base = MakeBig(5, 11); std::string body = BodyOf(BaseSaveText(base));
    ExpectAccept("control: untouched body re-signed", body);
    // unknown / unsupported fields
    ExpectReject("unknown field", body + "color=red\n");
    ExpectReject("unknown field (typo of a real key)", body + "mirorr=1\n");
    ExpectReject("unknown field before records", ReplaceLine(body, "enabled=", "enabled=1\nfoo=bar"));
    ExpectReject("blank line", body + "\n");
    ExpectReject("junk line", body + "garbage\n");
    ExpectReject("second header-like line", body + "NITRO_FX_SAVE 1\n");
    // required metadata / profile fields (each one missing, each one duplicated)
    const char* req[] = { "id=", "name=", "model=", "created=", "count=", "mirror=", "enabled=", "preset=" };
    for (int i = 0; i < 8; ++i) {
        std::string w = std::string("missing ") + req[i]; ExpectReject(w.c_str(), WithoutLine(body, req[i]));
        std::string d = std::string("duplicate ") + req[i]; ExpectReject(d.c_str(), DupLine(body, req[i]));
    }
    // malformed numbers / trailing characters in metadata
    struct NC { const char* what; const char* line; const char* prefix; } nc[] = {
        { "model trailing x", "model=541x", "model=" }, { "model hex", "model=0x21d", "model=" }, { "model empty", "model=", "model=" }, { "model plus sign", "model=+541", "model=" },
        { "model inner space", "model=5 41", "model=" }, { "model trailing space", "model=541 ", "model=" }, { "model negative", "model=-1", "model=" }, { "model too big", "model=65536", "model=" },
        { "model float", "model=541.0", "model=" }, { "model overflow", "model=99999999999999999999", "model=" },
        { "created trailing x", "created=1700000000x", "created=" }, { "created negative", "created=-5", "created=" }, { "created empty", "created=", "created=" }, { "created overflow", "created=99999999999999999999999", "created=" },
        { "count trailing x", "count=5x", "count=" }, { "count empty", "count=", "count=" }, { "count negative", "count=-1", "count=" }, { "count mismatch (6 vs 5 records)", "count=6", "count=" }, { "count mismatch (4 vs 5 records)", "count=4", "count=" }, { "count above capacity", "count=65", "count=" },
        { "mirror trailing x", "mirror=1x", "mirror=" }, { "mirror out of range", "mirror=3", "mirror=" }, { "mirror negative", "mirror=-1", "mirror=" }, { "mirror empty", "mirror=", "mirror=" },
        { "enabled=2", "enabled=2", "enabled=" }, { "enabled=true", "enabled=true", "enabled=" }, { "enabled empty", "enabled=", "enabled=" },
        { "preset with illegal chars", "preset=bad<>name", "preset=" }, { "name empty", "name=", "name=" }, { "name unsanitised (leading space)", "name= Base Save", "name=" },
        { "name too long", "name=abcdefghijklmnopqrstuvwxyz", "name=" }, { "name illegal char", "name=Bad!Name", "name=" },
        { "id wrong length", "id=0123", "id=" }, { "id uppercase hex", "id=0123456789ABCDEF", "id=" }, { "id different from file name", "id=fedcba9876543210", "id=" } };
    for (size_t i = 0; i < sizeof(nc) / sizeof(nc[0]); ++i) ExpectReject(nc[i].what, ReplaceLine(body, nc[i].prefix, nc[i].line));
    // emitter records: field indices 0 id,1-3 pos,4-5 heading,6 angle,7 active,8 mirror,9 category,10 colorVar,11 speed,12 detail,13 glow,14 opacity,15 power
    struct EC { const char* what; int field; const char* value; } ec[] = {
        { "id 0", 0, "0" }, { "id negative", 0, "-3" }, { "id above limit", 0, "1073741825" }, { "id trailing x", 0, "7x" }, { "id empty", 0, "" }, { "id float", 0, "7.0" }, { "id hex", 0, "0x7" },
        { "pos.x trailing x", 1, "1.5x" }, { "pos.y empty", 2, "" }, { "pos.z nan", 3, "nan" }, { "pos.x inf", 1, "inf" }, { "pos.x -inf", 1, "-inf" }, { "pos.x hex float", 1, "0x1p3" }, { "pos.x exponent overflow", 1, "1e999" },
        { "pos.x leading space", 1, " 1.5" }, { "pos.x trailing space", 1, "1.5 " }, { "pos.x plus", 1, "+1.5" }, { "pos.x double dot", 1, "1.2.3" }, { "pos.x just dot", 1, "." }, { "pos.x just minus", 1, "-" }, { "pos.x bad exponent", 1, "1e" },
        { "pos.x out of range", 1, "9" }, { "pos.y out of range", 2, "-12.5" }, { "pos.z out of range", 3, "8.5" },
        { "heading zero", 4, "0" }, { "heading non-unit", 4, "2" }, { "heading nan", 4, "nan" },
        { "angle below range", 6, "4.9" }, { "angle above range", 6, "85.5" }, { "angle nan", 6, "nan" },
        { "active=2", 7, "2" }, { "active=-1", 7, "-1" }, { "active trailing x", 7, "1x" }, { "mirror=2", 8, "2" },
        { "category -1", 9, "-1" }, { "category = CAT_COUNT", 9, "15" }, { "category huge", 9, "99999999999" }, { "category trailing x", 9, "3x" },
        { "colorVar >1", 10, "1.5" }, { "colorVar <0", 10, "-0.1" }, { "speed below range", 11, "0.05" }, { "speed above range", 11, "4.5" },
        { "detail >1", 12, "1.2" }, { "detail <0", 12, "-0.5" }, { "glow >2", 13, "2.5" }, { "glow <0", 13, "-1" },
        { "opacity below range", 14, "0.05" }, { "opacity >1", 14, "1.5" }, { "power below range", 15, "0.2" }, { "power above range", 15, "2.5" }, { "power empty", 15, "" }, { "power trailing x", 15, "1abc" } };
    for (size_t i = 0; i < sizeof(ec) / sizeof(ec[0]); ++i) ExpectReject(ec[i].what, WithEField(body, ec[i].field, ec[i].value));
    // malformed records
    { std::vector<std::string> L = BodyLines(body); int li = FirstLineWith(L, "e="); std::vector<std::string> f = SplitS(L[li].substr(2), ',');
      std::vector<std::string> f15(f.begin(), f.begin() + 15), f17 = f; f17.push_back("1");
      ExpectReject("record with 15 fields", ReplaceLine(body, "e=", "e=" + JoinS(f15, ',')));
      ExpectReject("record with 17 fields", ReplaceLine(body, "e=", "e=" + JoinS(f17, ',')));
      ExpectReject("record with trailing comma", ReplaceLine(body, "e=", L[li] + ","));
      ExpectReject("record with leading space after e=", ReplaceLine(body, "e=", "e= " + L[li].substr(2)));
      ExpectReject("record with space after a comma", ReplaceLine(body, "e=", "e=" + f[0] + ", " + JoinS(std::vector<std::string>(f.begin() + 1, f.end()), ',')));
      ExpectReject("empty record", ReplaceLine(body, "e=", "e="));
      ExpectReject("record separated by semicolons", ReplaceLine(body, "e=", "e=" + JoinS(f, ';')));
      // duplicate emitter IDs: copy the first record's id into the second record
      std::vector<std::string> L2 = L; int second = -1; for (size_t i = li + 1; i < L2.size(); ++i) if (L2[i].compare(0, 2, "e=") == 0) { second = (int)i; break; }
      std::vector<std::string> g = SplitS(L2[second].substr(2), ','); g[0] = f[0]; L2[second] = "e=" + JoinS(g, ',');
      ExpectReject("duplicate emitter id", JoinLines(L2));
      ExpectReject("duplicate whole record (and count mismatch fixed)", ReplaceLine(DupLine(body, "e="), "count=", "count=6")); }
    // capacity: 65 records with a matching count must still be rejected
    { Profile p65 = MakeBig(kMaxEmitters, 3); std::string b = BodyOf(BaseSaveText(p65)); std::vector<std::string> L = BodyLines(b); int li = FirstLineWith(L, "e=");
      std::vector<std::string> f = SplitS(L[li].substr(2), ','); f[0] = "999"; L.push_back("e=" + JoinS(f, ',')); L[FirstLineWith(L, "count=")] = "count=65";
      ExpectReject("65 emitters", JoinLines(L)); }
    // raw-text level defects
    { SaveMeta sm; Profile sp = MakeBig(1, 1); std::string full = BaseSaveText(base);
      CHECK(ParseSave(full + "x", kTId, sm, sp) == SAVE_CORRUPT);                        // trailing bytes after the crc line
      CHECK(ParseSave(full + "\n", kTId, sm, sp) == SAVE_CORRUPT);
      std::string nul = body; nul[nul.size() / 2] = 0; CHECK(ParseSave(Seal(nul), kTId, sm, sp) == SAVE_CORRUPT);   // embedded NUL, correct CRC
      std::string hi = body; hi[hi.size() / 2] = (char)0xE9; CHECK(ParseSave(Seal(hi), kTId, sm, sp) == SAVE_CORRUPT);
      std::string lone = body; lone.insert(lone.size() / 2, "\r"); CHECK(ParseSave(Seal(lone), kTId, sm, sp) == SAVE_CORRUPT);   // lone CR
      std::string badcrc = full; badcrc.replace(badcrc.rfind("crc=") + 4, 8, "zzzzzzzz"); CHECK(ParseSave(badcrc, kTId, sm, sp) == SAVE_CORRUPT);
      std::string nocrc = body; CHECK(ParseSave(nocrc, kTId, sm, sp) == SAVE_CORRUPT);
      CHECK(ParseSave(std::string(), kTId, sm, sp) == SAVE_CORRUPT); CHECK(ParseSave(std::string(300 * 1024, 'a'), kTId, sm, sp) == SAVE_CORRUPT); }
    // a valid save with ZERO emitters and the CRLF variant of a valid file are still accepted
    { Profile empty; ExpectAccept("zero emitters", BodyOf(BaseSaveText(empty)).replace(0, 0, "")); }
}

// output never modified on failure (also through ReadSaveFile / ListSaves) -------------------------------------
static void TestParseNeverTouchesOutput() {
    WipeSaves();
    SaveMeta m; m.id = NewSaveId(); m.name = "Keep Me"; m.modelId = 560; m.created = 5; CHECK(WriteSaveFile(m, MakeBig(8, 21)));
    std::string good = ReadAll(SavePath(m.id)); std::string bad = good; bad[bad.find("speed") == std::string::npos ? 30 : 30] ^= 0x20;
    WriteAll(SavePath(m.id), bad);
    SaveMeta sm; sm.name = "live"; sm.modelId = 123; Profile live = MakeBig(3, 77); std::string before = SerializeProfile(live);
    CHECK(ReadSaveFile(m.id, sm, live) == SAVE_CORRUPT); CHECK(sm.name == "live" && sm.modelId == 123 && SerializeProfile(live) == before);
    CHECK(!ReadAll(SavePath(m.id) + ".corrupt").empty());                                   // quarantined, never deleted
    // WriteSaveFile refuses to write anything that would not read back (unsanitised data, duplicate ids, >64 emitters)
    Profile p = MakeBig(3, 5); p.emitters[1].tex.speed = 9.f; SaveMeta w; w.id = NewSaveId(); w.name = "Bad"; w.modelId = 1; CHECK(!WriteSaveFile(w, p)); CHECK(ReadAll(SavePath(w.id)).empty());
    Profile d = MakeBig(3, 5); d.emitters[2].id = d.emitters[0].id; CHECK(!WriteSaveFile(w, d));
    Profile big = MakeBig(kMaxEmitters, 5); big.emitters.push_back(big.emitters[0]); big.emitters.back().id = 5000; CHECK((int)big.emitters.size() == kMaxEmitters + 1); CHECK(!WriteSaveFile(w, big)); CHECK(ReadAll(SavePath(w.id)).empty());
    WipeSaves();
}

// valid extremes are accepted (validation is strict but not too strict) + 64 emitters ------------------------------
static void TestBoundariesAndEmitterLimit() {
    WipeSaves();
    Profile p; p.mirrorMode = MIRROR_ALL; p.jetEnabled = true;
    for (int k = 0; k < 2; ++k) {
        Emitter e; e.id = p.nextId++; bool hi = k == 1;
        e.pos = hi ? V3(kMaxLocalX, kMaxLocalY, kMaxLocalZ) : V3(-kMaxLocalX, -kMaxLocalY, -kMaxLocalZ); e.hdgX = hi ? 0.f : 1.f; e.hdgY = hi ? 1.f : 0.f;
        e.angleDeg = hi ? kAngleMax : kAngleMin; e.tex.category = hi ? CAT_COUNT - 1 : 0; e.tex.colorVar = hi ? 1.f : 0.f; e.tex.speed = hi ? kSpeedMax : kSpeedMin;
        e.tex.detail = hi ? 1.f : 0.f; e.tex.glow = hi ? kGlowMax : 0.f; e.tex.opacity = hi ? 1.f : kOpacityMin; e.tex.power = hi ? kPowerMax : kPowerMin; e.active = hi; e.mirror = !hi;
        p.emitters.push_back(e);
    }
    p.emitters.push_back(p.emitters[0]); p.emitters.back().id = kMaxEmitterId;           // largest legal id
    SaveMeta m; m.id = NewSaveId(); m.name = "Extremes"; m.modelId = 0; m.created = 0; CHECK(WriteSaveFile(m, p));      // model 0 / created 0 are legal
    SaveMeta r; Profile b; CHECK(ReadSaveFile(m.id, r, b) == SAVE_OK && b.emitters.size() == 3);
    bool same = b.emitters.size() == p.emitters.size(); for (size_t i = 0; same && i < p.emitters.size(); ++i) same = SameE(p.emitters[i], b.emitters[i]); CHECK(same);
    // exactly 64 emitters, every category, survives save -> load -> save -> load bit-exactly; the 65th is refused by the editor-level capacity rule
    Profile big = MakeBig(kMaxEmitters, 99); CHECK((int)big.emitters.size() == 64);
    SaveMeta m64; m64.id = NewSaveId(); m64.name = "Sixty Four"; m64.modelId = 596; m64.created = 9; CHECK(WriteSaveFile(m64, big));
    Profile b1, b2; SaveMeta r1; CHECK(ReadSaveFile(m64.id, r1, b1) == SAVE_OK); CHECK(WriteSaveFile(r1, b1)); CHECK(ReadSaveFile(m64.id, r1, b2) == SAVE_OK);
    bool eq = b1.emitters.size() == 64 && b2.emitters.size() == 64; for (int i = 0; eq && i < 64; ++i) eq = SameE(big.emitters[i], b1.emitters[i]) && SameE(b1.emitters[i], b2.emitters[i]); CHECK(eq);
    std::set<int> cats; for (int i = 0; i < 64; ++i) cats.insert(b2.emitters[i].tex.category); CHECK((int)cats.size() == CAT_COUNT);
    Profile cap; TexSettings t; V3 mn(-1, -2, 0), mx(1, 2, 1); for (int i = 0; i < kMaxEmitters; ++i) CHECK(AddSlotEmitter(cap, i % kSlotCount, mn, mx, t)); CHECK(!AddSlotEmitter(cap, 0, mn, mx, t)); CHECK((int)cap.emitters.size() == kMaxEmitters);
    WipeSaves();
}

// 3. no per-model limit -------------------------------------------------------------------------------------------
static void TestUnlimitedSaves() {
    WipeSaves();
    const int N = 650;                                                                     // far above the old cap of 500 for ONE model
    std::set<std::string> ids; std::vector<std::string> order; Profile p = MakeBig(2, 4); bool allWritten = true;
    for (int i = 0; i < N; ++i) {
        SaveMeta s; s.id = NewSaveId(); char nm[32]; snprintf(nm, sizeof nm, "Style %d", i); s.name = nm; s.modelId = 541; s.created = 1700000000LL + i;
        if (!WriteSaveFile(s, p)) { allWritten = false; }
        ids.insert(s.id); order.push_back(s.id);
    }
    CHECK(allWritten); CHECK((int)ids.size() == N);                                        // unique ids, every write succeeded
    int bad = -1; std::vector<SaveMeta> L = ListSaves(&bad); CHECK(bad == 0); CHECK((int)L.size() == N);
    int forModel = 0; std::set<std::string> listed; for (size_t i = 0; i < L.size(); ++i) { if (L[i].modelId == 541) ++forModel; listed.insert(L[i].id); } CHECK(forModel == N); CHECK(listed == ids);
    // same-name behaviour is unchanged: duplicate within a model detected, same name on another model is a different save
    int dup = 0; for (size_t i = 0; i < L.size(); ++i) if (L[i].modelId == 541 && SameSaveName(L[i].name, "STYLE 649")) ++dup; CHECK(dup == 1);
    // delete a few in the middle; the rest stays intact
    for (int i = 100; i < 110; ++i) { CHECK(DeleteSaveFile(order[i])); }
    L = ListSaves(&bad); CHECK((int)L.size() == N - 10 && bad == 0);
    WipeSaves();
}

// save / load / overwrite / delete / duplicate names / many vehicle models ---------------------------------------
static void TestModelsAndLifecycle() {
    WipeSaves();
    const int models[] = { 400, 401, 411, 429, 451, 487, 520, 541, 560, 562, 596, 601 }; const int NM = sizeof(models) / sizeof(models[0]);
    std::vector<SaveMeta> metas; std::vector<Profile> profs;
    for (int i = 0; i < NM; ++i) for (int k = 0; k < 3; ++k) {                              // 3 saves per model, "Style A" exists on EVERY model
        SaveMeta s; s.id = NewSaveId(); char nm[24]; snprintf(nm, sizeof nm, k == 0 ? "Style A" : (k == 1 ? "Style B %d" : "Extra %d"), models[i]); s.name = nm; s.modelId = models[i]; s.created = 1000 + i * 10 + k;
        Profile p = MakeBig(1 + (i * 5 + k * 17) % kMaxEmitters, 500u + i * 7 + k); CHECK(WriteSaveFile(s, p)); metas.push_back(s); profs.push_back(p);
    }
    int bad = -1; std::vector<SaveMeta> L = ListSaves(&bad); CHECK(bad == 0 && (int)L.size() == NM * 3);
    int styleA = 0; std::set<int> modelsWithA; for (size_t i = 0; i < L.size(); ++i) if (SameSaveName(L[i].name, "style a")) { ++styleA; modelsWithA.insert(L[i].modelId); } CHECK(styleA == NM && (int)modelsWithA.size() == NM);
    // load: every save returns exactly what was stored, for its own model only
    for (size_t i = 0; i < metas.size(); ++i) {
        SaveMeta r; Profile b; CHECK(ReadSaveFile(metas[i].id, r, b) == SAVE_OK); CHECK(r.modelId == metas[i].modelId && r.name == metas[i].name && r.id == metas[i].id);
        bool eq = b.emitters.size() == profs[i].emitters.size(); for (size_t k = 0; eq && k < b.emitters.size(); ++k) eq = SameE(b.emitters[k], profs[i].emitters[k]); CHECK(eq);
        CHECK(ModelCompatible(r.modelId, r.modelId)); CHECK(!ModelCompatible(r.modelId, r.modelId + 1));
    }
    // overwrite one save: Save ID, name and model stay, content replaced, nothing else touched
    Profile repl = MakeBig(64, 4242); SaveMeta ow = metas[7]; ow.created = 2000; CHECK(WriteSaveFile(ow, repl));
    { SaveMeta r; Profile b; CHECK(ReadSaveFile(ow.id, r, b) == SAVE_OK && (int)b.emitters.size() == 64 && r.id == metas[7].id && r.name == metas[7].name && r.modelId == metas[7].modelId && r.created == 2000);
      for (size_t i = 0; i < metas.size(); ++i) if (i != 7) { SaveMeta r2; Profile b2; CHECK(ReadSaveFile(metas[i].id, r2, b2) == SAVE_OK && b2.emitters.size() == profs[i].emitters.size()); } }
    // a failed overwrite (invalid data) leaves the previous save intact
    { Profile badp = MakeBig(4, 1); badp.emitters[0].tex.glow = 99.f; CHECK(!WriteSaveFile(ow, badp)); SaveMeta r; Profile b; CHECK(ReadSaveFile(ow.id, r, b) == SAVE_OK && b.emitters.size() == 64); }
    // delete
    CHECK(DeleteSaveFile(metas[0].id)); { SaveMeta r; Profile b; CHECK(ReadSaveFile(metas[0].id, r, b) == SAVE_IO); } CHECK(!DeleteSaveFile(metas[0].id)); CHECK((int)ListSaves(nullptr).size() == NM * 3 - 1);
    WipeSaves();
}

// memory safety: an oversized file is never loaded completely -------------------------------------------------------
static void TestOversizedFile() {
    WipeSaves();
    std::string id = NewSaveId(); { std::ofstream f(SavePath(id).c_str(), std::ios::binary | std::ios::trunc); std::string chunk(1 << 20, 'x'); for (int i = 0; i < 8; ++i) f << chunk; }   // 8 MB
    SaveMeta m; Profile p; CHECK(ReadSaveFile(id, m, p) == SAVE_CORRUPT); CHECK(ReadSaveFile(id, m, p) == SAVE_IO);       // quarantined on first read
    { std::ifstream q((SavePath(id) + ".corrupt").c_str(), std::ios::binary); CHECK((bool)q); }
    Profile lp; std::string ap = "NITRO_FX/vehicles/model_777.nfx"; { std::ofstream f(ap.c_str(), std::ios::binary | std::ios::trunc); std::string chunk(1 << 20, 'y'); for (int i = 0; i < 3; ++i) f << chunk; }
    CHECK(!LoadProfileFile(ap, lp)); CHECK(!LoadProfileFile(ap, lp));                                                    // quarantined, no crash, no huge allocation
    WipeSaves(); if (system("rm -f NITRO_FX/vehicles/model_777.nfx*") != 0) {}
}

// lenient autosave-file parsing keeps working (and is stricter about numbers than before) ---------------------------
static void TestLenientAutosaveFile() {
    Profile p = MakeBig(4, 31); std::string t = SerializeProfile(p); Profile r;
    CHECK(ParseProfile(t, r) && r.emitters.size() == 4);
    std::string t2 = t + "unknown=1\ne=1.5x,0,0,0,1,0,45,1,1,0,0,1,0.5,1,1,1\ne=,,,\n"; CHECK(ParseProfile(t2, r) && r.emitters.size() == 4);   // unknown ignored, malformed records skipped (not half-parsed)
    std::vector<std::string> L = BodyLines(t); int li = FirstLineWith(L, "e="); std::vector<std::string> f = SplitS(L[li].substr(2), ','); f[11] = "9.5"; f[13] = "7"; L[li] = "e=" + JoinS(f, ',');
    CHECK(ParseProfile(JoinLines(L), r) && r.emitters.size() == 4 && r.emitters[0].tex.speed == kSpeedMax && r.emitters[0].tex.glow == kGlowMax);    // working copies are repaired by clamping, as before
    CHECK(!ParseProfile("garbage", r)); CHECK(!ParseProfile("", r));
}

// 1. autosave: dirty flag only cleared after a successful write ------------------------------------------------------------
static bool g_fail = false; static int g_writes = 0; static std::string g_lastPath; static Profile g_lastProfile; static std::vector<std::string> g_paths;
static bool FakeWrite(const std::string& path, const Profile& p) { ++g_writes; g_paths.push_back(path); if (g_fail) return false; g_lastPath = path; g_lastProfile = p; return true; }
static void TestAutosaveLogic() {
    // (a) failed write keeps the dirty flag, successful write clears it
    { AutosaveState a; Profile p = MakeBig(3, 1); g_fail = true; g_writes = 0; AutosaveMarkDirty(a, 1000);
      CHECK(AutosaveFlush(a, 541, p, 5000, true, &FakeWrite) == 1); CHECK(a.dirty); CHECK(a.failures == 1); CHECK(AutosaveHasUnsaved(a));
      g_fail = false; CHECK(AutosaveFlush(a, 541, p, 5100, true, &FakeWrite) == 0); CHECK(!a.dirty); CHECK(a.failures == 0); CHECK(!AutosaveHasUnsaved(a));
      CHECK(g_lastPath.find("model_541.nfx") != std::string::npos); CHECK(SerializeProfile(g_lastProfile) == SerializeProfile(p));
      CHECK(AutosaveFlush(a, 541, p, 5200, true, &FakeWrite) == 0 && g_writes == 2); }                // clean state => no further disk operations
    // (b) edits made while the disk is failing are never lost: the eventual successful write contains the LATEST data
    { AutosaveState a; Profile p = MakeBig(2, 2); g_fail = true; g_writes = 0; AutosaveMarkDirty(a, 0); AutosaveFlush(a, 560, p, 2500, false, &FakeWrite);
      p = MakeBig(9, 3); AutosaveMarkDirty(a, 3000); p.mirrorMode = MIRROR_OFF; AutosaveFlush(a, 560, p, 3000, true, &FakeWrite); CHECK(a.dirty);
      g_fail = false; AutosaveFlush(a, 560, p, 9000, false, &FakeWrite); CHECK(!a.dirty); CHECK(SerializeProfile(g_lastProfile) == SerializeProfile(p)); CHECK(g_lastProfile.emitters.size() == 9); }
    // (c) back-off: a permanently failing disk is not hammered (simulate 2 minutes in 10 ms frames)
    { AutosaveState a; Profile p = MakeBig(1, 1); g_fail = true; g_writes = 0; AutosaveMarkDirty(a, 0); int attempts = 0;
      for (unsigned now = 0; now <= 120000; now += 10) { if (AutosaveDue(a, 541, now, false)) { ++attempts; AutosaveFlush(a, 541, p, now, false, &FakeWrite); } }
      CHECK(a.dirty); CHECK(attempts == g_writes); CHECK(attempts >= 4 && attempts <= 10);        // ~ 2s,4s,8s,16s,30s,30s,... instead of 12000 frames of retries
      CHECK(AutosaveBackoffMs(1) == 2000 && AutosaveBackoffMs(2) == 4000 && AutosaveBackoffMs(3) == 8000 && AutosaveBackoffMs(4) == 16000 && AutosaveBackoffMs(5) == 30000 && AutosaveBackoffMs(1000) == 30000 && AutosaveBackoffMs(0) == 0);
      g_fail = false; unsigned now = 200000; CHECK(AutosaveDue(a, 541, now, false)); AutosaveFlush(a, 541, p, now, false, &FakeWrite); CHECK(!a.dirty); CHECK(!AutosaveDue(a, 541, now + 100000, false)); }   // recovers by itself, then goes quiet
    // (d) quiet delay, dragging, and "no vehicle" rules
    { AutosaveState a; AutosaveMarkDirty(a, 10000); CHECK(!AutosaveDue(a, 541, 11000, false)); CHECK(AutosaveDue(a, 541, 12500, false)); CHECK(!AutosaveDue(a, 541, 12500, true)); CHECK(!AutosaveDue(a, -1, 12500, false));
      Profile p; g_writes = 0; CHECK(AutosaveFlush(a, -1, p, 20000, true, &FakeWrite) == 0 && g_writes == 0 && a.dirty); }                       // nothing to write to, nothing silently dropped
    // (e) GetTickCount wrap-around (49.7 days)
    { AutosaveState a; AutosaveMarkDirty(a, 0xFFFFFF00u); CHECK(AutosaveDue(a, 541, 0x00001000u, false)); CHECK(!AutosaveDue(a, 541, 0x00000100u, false));
      Profile p = MakeBig(1, 1); g_fail = true; AutosaveFlush(a, 541, p, 0xFFFFF000u, true, &FakeWrite); CHECK(a.retryAt == 0xFFFFF000u + 2000u);
      AutosaveState w; AutosaveMarkDirty(w, 0xFFFFE000u); g_fail = true; AutosaveFlush(w, 541, p, 0xFFFFF800u, true, &FakeWrite); CHECK(w.retryAt == 0xFFFFF800u + 2000u);   // retryAt itself wraps past 0
      CHECK(!AutosaveDue(w, 541, 0xFFFFFF00u, false)); CHECK(AutosaveDue(w, 541, 0x00000A00u, false)); g_fail = false; }
    // (f) vehicle switch while the write fails: the old model's data is parked and written to ITS OWN file; coming back restores the unsaved copy
    { AutosaveState a; Profile pa = MakeBig(5, 8), pb = MakeBig(2, 9); g_fail = true; g_paths.clear(); AutosaveMarkDirty(a, 0);
      AutosaveFlush(a, 411, pa, 3000, true, &FakeWrite); CHECK(a.dirty);
      AutosavePark(a, 411, pa, a.failures, a.retryAt); a.dirty = false; a.failures = 0; a.retryAt = 0; CHECK(AutosaveHasUnsaved(a) && a.pending.size() == 1);
      Profile pa2 = MakeBig(6, 8); AutosavePark(a, 411, pa2, 1, 0); CHECK(a.pending.size() == 1 && a.pending[0].profile.emitters.size() == 6);   // newer data replaces older, still one entry
      AutosavePark(a, 520, pb, 1, 0); CHECK(a.pending.size() == 2); AutosavePark(a, -1, pb, 1, 0); CHECK(a.pending.size() == 2);
      g_paths.clear(); g_fail = true; CHECK(AutosaveFlush(a, 560, MakeBig(1, 1), 4000, true, &FakeWrite) == 2); CHECK(a.pending.size() == 2);       // both kept while failing
      CHECK(g_paths.size() == 2 && g_paths[0].find("model_411.nfx") != std::string::npos && g_paths[1].find("model_520.nfx") != std::string::npos);
      g_fail = false; g_paths.clear(); CHECK(AutosaveFlush(a, 560, MakeBig(1, 1), 50000, false, &FakeWrite) == 0); CHECK(a.pending.empty()); CHECK(g_paths.size() == 2); }
    { AutosaveState a; Profile pa = MakeBig(5, 8), got; AutosavePark(a, 411, pa, 1, 0); CHECK(!AutosaveTakePending(a, 520, got)); CHECK(AutosaveTakePending(a, 411, got)); CHECK(SerializeProfile(got) == SerializeProfile(pa)); CHECK(a.pending.empty()); CHECK(!AutosaveTakePending(a, 411, got)); }
    // (g) pending entries respect the back-off too (no hammering of a dead disk while driving other cars)
    { AutosaveState a; Profile p = MakeBig(1, 1); g_fail = true; g_writes = 0; AutosavePark(a, 411, p, 0, 0); int attempts = 0;
      for (unsigned now = 1000; now <= 121000; now += 10) { if (AutosaveDue(a, 560, now, false)) { ++attempts; AutosaveFlush(a, 560, p, now, false, &FakeWrite); } }
      CHECK(!a.pending.empty()); CHECK(attempts >= 4 && attempts <= 10); g_fail = false; }
}

// real disk: SaveProfileFile failure paths + the same state machine with the real writer ---------------------------------------
static void TestAutosaveRealDisk() {
    Profile p = MakeBig(6, 123);
    CHECK(!SaveProfileFile("/nonexistent_dir_nfx/sub/model_1.nfx", p));                                 // cannot write -> false, no crash
    // destination is a directory: the final replace fails; the .tmp file must not be left behind and the flag must stay dirty
    if (system("mkdir -p NITRO_FX/vehicles/model_900.nfx") != 0) {}
    AutosaveState a; AutosaveMarkDirty(a, 0);
    CHECK(AutosaveFlush(a, 900, p, 5000, true, &SaveProfileFile) == 1); CHECK(a.dirty); CHECK(ReadAll(VehiclePath(900) + ".tmp").empty());
    // the obstacle disappears: the SAME dirty data is now written and the flag is cleared
    if (system("rmdir NITRO_FX/vehicles/model_900.nfx") != 0) {}
    CHECK(AutosaveFlush(a, 900, p, 5100, true, &SaveProfileFile) == 0); CHECK(!a.dirty);
    Profile back; CHECK(LoadProfileFile(VehiclePath(900), back)); CHECK(back.emitters.size() == p.emitters.size() && SerializeProfile(back) == SerializeProfile(p));
    // a folder deleted while running is recreated by the next write (retry can succeed)
    if (system("rm -rf NITRO_FX/vehicles") != 0) {}
    AutosaveState b; AutosaveMarkDirty(b, 0); CHECK(AutosaveFlush(b, 901, p, 1, true, &SaveProfileFile) == 0); CHECK(!b.dirty); CHECK(LoadProfileFile(VehiclePath(901), back));
    if (system("rm -f NITRO_FX/vehicles/model_90*.nfx") != 0) {}
}

// 4. mesh picking math ---------------------------------------------------------------------------------------------------------------
struct TriT { unsigned short vertIndex[3]; unsigned short matIndex; };
static TriT Tri3(int a, int b, int c) { TriT t; t.vertIndex[0] = (unsigned short)a; t.vertIndex[1] = (unsigned short)b; t.vertIndex[2] = (unsigned short)c; t.matIndex = 0; return t; }
static void TestMeshMath() {
    const V3 o(0, 0, 10), d(0, 0, -1); const float nan = std::nanf(""), inf = INFINITY;
    // RayTriangle rejects non-finite input
    float t; V3 n; V3 a(-1, -1, 0), b(1, -1, 0), c(0, 1, 0);
    CHECK(!RayTriangle(V3(nan, 0, 5), d, a, b, c, t, n)); CHECK(!RayTriangle(o, V3(0, nan, -1), a, b, c, t, n)); CHECK(!RayTriangle(o, d, V3(nan, -1, 0), b, c, t, n)); CHECK(!RayTriangle(o, d, a, V3(inf, 0, 0), c, t, n));
    CHECK(!RayTriangle(o, d, a, a, a, t, n));                                                          // degenerate triangle
    // two stacked quads (z = 0 and z = 3) + one behind the ray origin: nearest surface along the ray is z = 3
    std::vector<V3> v; std::vector<TriT> tr;
    const float zs[3] = { 0.f, 3.f, 20.f };
    for (int k = 0; k < 3; ++k) { int b0 = (int)v.size(); v.push_back(V3(-2, -2, zs[k])); v.push_back(V3(2, -2, zs[k])); v.push_back(V3(2, 2, zs[k])); v.push_back(V3(-2, 2, zs[k])); tr.push_back(Tri3(b0, b0 + 1, b0 + 2)); tr.push_back(Tri3(b0, b0 + 2, b0 + 3)); }
    float best = 60.f; V3 bn; CHECK(RayMesh(o, d, v.data(), (int)v.size(), tr.data(), (int)tr.size(), best, bn)); CHECK(fabsf(best - 7.f) < 1e-4f); CHECK(bn.z > 0.99f);   // z=20 plane is behind the origin, z=3 beats z=0
    // triangle order must not matter
    std::vector<TriT> rev(tr.rbegin(), tr.rend()); best = 60.f; CHECK(RayMesh(o, d, v.data(), (int)v.size(), rev.data(), (int)rev.size(), best, bn) && fabsf(best - 7.f) < 1e-4f);
    // `best` is a limit: a farther mesh cannot override a nearer hit already found; max distance is honoured
    best = 5.f; V3 keep(9, 9, 9); bn = keep; CHECK(!RayMesh(o, d, v.data(), (int)v.size(), tr.data(), (int)tr.size(), best, bn)); CHECK(best == 5.f && bn.x == 9.f);
    best = 6.9f; CHECK(!RayMesh(o, d, v.data(), (int)v.size(), tr.data(), (int)tr.size(), best, bn));
    // miss
    best = 60.f; CHECK(!RayMesh(V3(50, 50, 10), d, v.data(), (int)v.size(), tr.data(), (int)tr.size(), best, bn)); CHECK(best == 60.f);
    // invalid geometry: null pointers, zero counts, out-of-range indices, non-finite vertices -> ignored, never out-of-bounds
    best = 60.f; CHECK(!RayMesh(o, d, (const V3*)nullptr, 4, tr.data(), 2, best, bn)); CHECK(!RayMesh(o, d, v.data(), 4, (const TriT*)nullptr, 2, best, bn));
    CHECK(!RayMesh(o, d, v.data(), 0, tr.data(), 2, best, bn)); CHECK(!RayMesh(o, d, v.data(), 4, tr.data(), 0, best, bn)); CHECK(!RayMesh(o, d, v.data(), -3, tr.data(), 2, best, bn)); CHECK(!RayMesh(o, d, v.data(), 4, tr.data(), -3, best, bn));
    { std::vector<V3> v4(v.begin(), v.begin() + 4); std::vector<TriT> bad; bad.push_back(Tri3(0, 1, 4)); bad.push_back(Tri3(0, 1, 65535)); bad.push_back(Tri3(7, 8, 9)); best = 60.f; CHECK(!RayMesh(o, d, v4.data(), 4, bad.data(), (int)bad.size(), best, bn));     // all indices >= nv
      bad.push_back(Tri3(0, 1, 2)); best = 60.f; CHECK(RayMesh(o, d, v4.data(), 4, bad.data(), (int)bad.size(), best, bn) && fabsf(best - 10.f) < 1e-4f);                                                           // valid triangle among invalid ones still hits
      std::vector<V3> vn = v4; vn[1] = V3(nan, 0, 0); std::vector<TriT> ok; ok.push_back(Tri3(0, 1, 2)); ok.push_back(Tri3(0, 2, 3)); best = 60.f; RayMesh(o, d, vn.data(), 4, ok.data(), 2, best, bn); CHECK(nfx::finitef(best) && best <= 60.f);   // NaN vertex: no NaN result
      vn[1] = V3(inf, inf, inf); best = 60.f; bool h = RayMesh(o, d, vn.data(), 4, ok.data(), 2, best, bn); CHECK(!h || nfx::finitef(best)); }
    // world <-> vehicle-local conversion. Vehicle at (100,200,10) facing world +X: right = -Y, forward = +X, up = +Z
    { V3 r(0, -1, 0), f(1, 0, 0), u(0, 0, 1), p(100, 200, 10); CHECK(BasisUsable(r, f, u, p));
      V3 l = WorldPointToLocal(V3(105, 200, 11), r, f, u, p); CHECK(fabsf(l.x) < 1e-5f && fabsf(l.y - 5.f) < 1e-5f && fabsf(l.z - 1.f) < 1e-5f);                          // 5 m ahead, 1 m up
      V3 l2 = WorldPointToLocal(V3(100, 198, 10), r, f, u, p); CHECK(fabsf(l2.x - 2.f) < 1e-5f && fabsf(l2.y) < 1e-5f && fabsf(l2.z) < 1e-5f);                             // 2 m to the right (world -Y)
      V3 dl = WorldDirToLocal3(V3(0, 0, 1), r, f, u); CHECK(fabsf(dl.z - 1.f) < 1e-6f && fabsf(dl.x) < 1e-6f); }
    { float yaw = 0.7f, pitch = 0.3f, roll = -0.4f;                                                        // arbitrary orientation: round trip is exact
      V3 f(cosf(pitch) * -sinf(yaw), cosf(pitch) * cosf(yaw), sinf(pitch)), upw(0, 0, 1); V3 r = vnorm(cross(f, upw)); V3 u = vnorm(cross(r, f));
      V3 r2 = r * cosf(roll) + u * sinf(roll), u2 = u * cosf(roll) - r * sinf(roll); V3 p(-250.5f, 1033.25f, 41.f); CHECK(BasisUsable(r2, f, u2, p));
      for (int i = 0; i < 20; ++i) { V3 l(((i * 37) % 17) / 4.f - 2.f, ((i * 53) % 23) / 4.f - 3.f, ((i * 11) % 9) / 4.f - 1.f); V3 w = LocalPointToWorld3(l, r2, f, u2, p); V3 b2 = WorldPointToLocal(w, r2, f, u2, p);
        CHECK(fabsf(b2.x - l.x) < 1e-3f && fabsf(b2.y - l.y) < 1e-3f && fabsf(b2.z - l.z) < 1e-3f); } }
    // unusable bases are rejected (no silent garbage conversion)
    { V3 r(1, 0, 0), f(0, 1, 0), u(0, 0, 1), p(0, 0, 0); CHECK(BasisUsable(r, f, u, p)); CHECK(!BasisUsable(r * 2.f, f, u, p)); CHECK(!BasisUsable(r, r, u, p)); CHECK(!BasisUsable(V3(nan, 0, 0), f, u, p)); CHECK(!BasisUsable(r, f, u, V3(0, inf, 0))); CHECK(!BasisUsable(V3(), V3(), V3(), p)); }
}

int main() {
    CHECK(kMaxEmitters >= 50);
    // 1. 50+ emitters, exact lossless round trip
    Profile big = MakeBig(kMaxEmitters, 7); CHECK((int)big.emitters.size() == kMaxEmitters);
    SaveMeta m; m.id = NewSaveId(); m.name = "Purple Jet"; m.modelId = 541; m.created = 1700000000LL;
    CHECK(IsValidSaveId(m.id)); CHECK(WriteSaveFile(m, big));
    SaveMeta r; Profile back; CHECK(ReadSaveFile(m.id, r, back) == SAVE_OK);
    CHECK(r.modelId == 541 && r.name == "Purple Jet" && r.id == m.id && (int)back.emitters.size() == kMaxEmitters);
    CHECK(back.mirrorMode == big.mirrorMode && back.jetEnabled == big.jetEnabled);
    bool all = back.emitters.size() == big.emitters.size(); for (size_t i = 0; all && i < big.emitters.size(); ++i) all = SameE(big.emitters[i], back.emitters[i]);
    CHECK(all);
    // 2. many saves for the same model + same name on another model; unique IDs
    std::set<std::string> ids; ids.insert(m.id);
    const char* names[] = { "Golden Fire", "Neon Matrix", "Water Style", "Dark Smoke" };
    for (int i = 0; i < 4; ++i) { SaveMeta s; s.id = NewSaveId(); s.name = names[i]; s.modelId = 541; s.created = 1700000100LL + i; CHECK(WriteSaveFile(s, MakeBig(10 + i * 10, 100 + i))); ids.insert(s.id); }
    SaveMeta o; o.id = NewSaveId(); o.name = "purple jet"; o.modelId = 560; o.created = 1700000500LL; CHECK(WriteSaveFile(o, MakeBig(5, 3))); ids.insert(o.id);
    CHECK(ids.size() == 6);
    int bad = -1; std::vector<SaveMeta> L = ListSaves(&bad); CHECK(bad == 0); CHECK(L.size() == 6);
    int dupSameModel = 0, dupOtherModel = 0; for (size_t i = 0; i < L.size(); ++i) if (SameSaveName(L[i].name, "PURPLE JET")) { if (L[i].modelId == 541) ++dupSameModel; else ++dupOtherModel; }
    CHECK(dupSameModel == 1 && dupOtherModel == 1);      // dup detection is per model (same name allowed on another model)
    // 3. overwrite keeps the Save ID, replaces content
    Profile small = MakeBig(3, 9); SaveMeta ow = m; ow.created = 1700009999LL; CHECK(WriteSaveFile(ow, small));
    CHECK(ReadSaveFile(m.id, r, back) == SAVE_OK && back.emitters.size() == 3 && r.id == m.id && ListSaves(nullptr).size() == 6);
    // 4. compatibility rule
    CHECK(ModelCompatible(541, 541)); CHECK(!ModelCompatible(541, 560)); CHECK(!ModelCompatible(-1, -1));
    // 5. corruption: bit flip, truncation, edited value, wrong name-id, garbage -> SAVE_CORRUPT + quarantined, never partially applied
    std::string base = ReadAll(SavePath(m.id)); Profile sentinel = MakeBig(2, 1); Profile keep = sentinel;
    struct Case { const char* what; std::string data; } cases[4] = { { "flip", base }, { "trunc", base.substr(0, base.size() / 2) }, { "edit", base }, { "garbage", "hello\nworld\n" } };
    cases[0].data[base.size() / 3] ^= 0x01;
    { size_t k = cases[2].data.find("model="); cases[2].data.replace(k, 9, "model=999"); }
    for (int i = 0; i < 4; ++i) {
        SaveMeta mm; CHECK(ParseSave(cases[i].data, m.id, mm, sentinel) == SAVE_CORRUPT);
        CHECK(sentinel.emitters.size() == keep.emitters.size());   // output untouched on failure
    }
    WriteAll(SavePath(m.id), cases[0].data); CHECK(ReadSaveFile(m.id, r, back) == SAVE_CORRUPT);
    CHECK(ReadAll(SavePath(m.id)).empty()); CHECK(!ReadAll(SavePath(m.id) + ".corrupt").empty());
    CHECK(ReadSaveFile(m.id, r, back) == SAVE_IO);
    WriteAll(SavePath(NewSaveId()), "not a save"); bad = 0; L = ListSaves(&bad); CHECK(bad == 1); CHECK(L.size() == 5);
    // 6. CRLF (notepad) tolerated; delete
    std::string ok2 = ReadAll(SavePath(o.id)), crlf; for (size_t i = 0; i < ok2.size(); ++i) { if (ok2[i] == '\n') crlf += '\r'; crlf += ok2[i]; }
    CHECK(ParseSave(crlf, o.id, r, back) == SAVE_OK);
    CHECK(DeleteSaveFile(o.id)); CHECK(ReadSaveFile(o.id, r, back) == SAVE_IO); CHECK(!DeleteSaveFile("../../etc/passwd"));
    // 7. name rules
    CHECK(SanitizeSaveName("  Neon <Matrix>!! ") == "Neon Matrix"); CHECK(SanitizeSaveName("!!!").empty()); CHECK((int)SanitizeSaveName(std::string(60, 'a')).size() == kMaxSaveName);
    // 8. ray / triangle (mesh picking math)
    float t; V3 n; V3 a(-1, -1, 0), b(1, -1, 0), c(0, 1, 0);
    CHECK(RayTriangle(V3(0, 0, 5), V3(0, 0, -1), a, b, c, t, n) && fabsf(t - 5) < 1e-4f && n.z > 0.99f);
    CHECK(RayTriangle(V3(0, 0, -5), V3(0, 0, 1), a, b, c, t, n) && fabsf(t - 5) < 1e-4f && n.z < -0.99f);   // back face: normal faces ray origin
    CHECK(!RayTriangle(V3(3, 0, 5), V3(0, 0, -1), a, b, c, t, n)); CHECK(!RayTriangle(V3(0, 0, 5), V3(0, 0, 1), a, b, c, t, n)); CHECK(!RayTriangle(V3(0, 0, 5), V3(1, 0, 0), a, b, c, t, n));
    // ================= additional tests: final save-safety fixes =================
    TestStrictValidation(); TestParseNeverTouchesOutput(); TestBoundariesAndEmitterLimit(); TestUnlimitedSaves(); TestModelsAndLifecycle();
    TestOversizedFile(); TestLenientAutosaveFile(); TestAutosaveLogic(); TestAutosaveRealDisk(); TestMeshMath();
    printf(fails ? "\n%d FAILURES\n" : "\nALL TESTS PASSED\n", fails); return fails ? 1 : 0;
}
