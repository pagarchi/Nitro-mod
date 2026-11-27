#include "nitrofx/Storage.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cctype>
#include <chrono>
#include <set>
#include <cerrno>
#include <climits>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace nfx {
static const char* kHeader = "NITRO_FX_PROFILE 1";
static const char* kSaveHeader = "NITRO_FX_SAVE 1";
#ifdef _WIN32
static const char kSep = '\\';
#else
static const char kSep = '/';
#endif
static void MakeDir(const std::string& d) {
#ifdef _WIN32
    CreateDirectoryA(d.c_str(), nullptr);
#else
    mkdir(d.c_str(), 0755);
#endif
}

static bool FileExists(const std::string& p) { std::ifstream f(p.c_str(), std::ios::binary); return (bool)f; }
static bool ReplaceFile(const std::string& from, const std::string& to) {
#ifdef _WIN32
    return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(from.c_str(), to.c_str()) == 0;
#endif
}
std::string BaseDir() {
    static std::string dir;
    if (dir.empty()) {
#ifdef _WIN32
        char p[MAX_PATH] = { 0 }; HMODULE h = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&BaseDir, &h);
        GetModuleFileNameA(h, p, MAX_PATH);
        std::string s = p; size_t k = s.find_last_of("\\/");
        s = (k == std::string::npos) ? std::string(".") : s.substr(0, k);
        dir = s + kSep + "NITRO_FX";
#else
        dir = "./NITRO_FX";
#endif
        MakeDir(dir); MakeDir(dir + kSep + "vehicles"); MakeDir(dir + kSep + "presets"); MakeDir(dir + kSep + "saves");
    }
    return dir;
}
// Re-creates the folder tree. Called before every WRITE (not on every path lookup) so a folder the user deleted while the game runs
// does not turn into a permanent write failure; mkdir on an existing folder is a harmless no-op.
static void EnsureDirs() {
    std::string d = BaseDir(); MakeDir(d); MakeDir(d + kSep + "vehicles"); MakeDir(d + kSep + "presets"); MakeDir(d + kSep + "saves");
}
std::string VehiclePath(int modelId) { char b[64]; snprintf(b, sizeof b, "model_%d.nfx", modelId); return BaseDir() + kSep + "vehicles" + kSep + b; }
std::string SanitizePresetName(const std::string& n) {
    std::string r;
    for (size_t i = 0; i < n.size() && r.size() < 32; ++i) { char c = n[i]; if (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-') r += c; }
    return r;
}
std::string PresetPath(const std::string& name) { return BaseDir() + kSep + "presets" + kSep + SanitizePresetName(name) + ".nfx"; }

std::string SerializeProfile(const Profile& p) {
    std::ostringstream o; char b[512];
    o << kHeader << "\n";
    o << "mirror=" << p.mirrorMode << "\nenabled=" << (p.jetEnabled ? 1 : 0) << "\npreset=" << SanitizePresetName(p.presetName) << "\n";
    for (size_t i = 0; i < p.emitters.size(); ++i) {
        const Emitter& e = p.emitters[i]; const TexSettings& t = e.tex;
        snprintf(b, sizeof b, "e=%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
                 e.id, e.pos.x, e.pos.y, e.pos.z, e.hdgX, e.hdgY, e.angleDeg, e.active ? 1 : 0, e.mirror ? 1 : 0,
                 t.category, t.colorVar, t.speed, t.detail, t.glow, t.opacity, t.power);
        o << b;
    }
    return o.str();
}
// ---------------- strict number / record parsing ----------------
// Every number must be consumed COMPLETELY: no empty field, no spaces, no trailing characters, no hex, no inf/nan, no overflow.
static bool IsDig(char c) { return c >= '0' && c <= '9'; }
static bool ParseIntStrict(const std::string& s, int& out) {              // -?[0-9]+  (at most 11 chars: cannot overflow long long)
    size_t n = s.size(), i = 0; if (n == 0 || n > 11) return false;
    if (s[0] == '-') i = 1;
    if (i >= n) return false;
    for (size_t k = i; k < n; ++k) if (!IsDig(s[k])) return false;
    long long v = strtoll(s.c_str(), nullptr, 10);
    if (v < INT_MIN || v > INT_MAX) return false;
    out = (int)v; return true;
}
static bool ParseLLStrict(const std::string& s, long long& out) {          // -?[0-9]+ that fits in long long
    size_t n = s.size(), i = 0; if (n == 0 || n > 20) return false;
    if (s[0] == '-') i = 1;
    if (i >= n) return false;
    for (size_t k = i; k < n; ++k) if (!IsDig(s[k])) return false;
    errno = 0; long long v = strtoll(s.c_str(), nullptr, 10);
    if (errno == ERANGE) return false;
    out = v; return true;
}
static bool ParseFloatStrict(const std::string& s, float& out) {           // -?(d+(.d*)?|.d+)([eE][+-]?d+)?  exactly what printf("%.9g") produces
    size_t n = s.size(), i = 0; if (n == 0 || n > 40) return false;
    if (s[i] == '-') ++i;
    size_t d0 = i; while (i < n && IsDig(s[i])) ++i; size_t intDigits = i - d0, fracDigits = 0;
    if (i < n && s[i] == '.') { ++i; size_t f0 = i; while (i < n && IsDig(s[i])) ++i; fracDigits = i - f0; }
    if (intDigits + fracDigits == 0) return false;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) { ++i; if (i < n && (s[i] == '+' || s[i] == '-')) ++i; size_t e0 = i; while (i < n && IsDig(s[i])) ++i; if (i == e0) return false; }
    if (i != n) return false;                                              // trailing garbage / spaces / "inf" / "nan" / hex floats
    char* end = nullptr; float v = strtof(s.c_str(), &end);
    if (end != s.c_str() + n || !finitef(v)) return false;                 // overflow to inf is rejected (underflow to 0 is harmless)
    out = v; return true;
}
static void SplitCsv(const std::string& s, std::vector<std::string>& f) {   // keeps empty fields so "1,,2" is detected as malformed
    f.clear(); size_t a = 0;
    for (;;) { size_t c = s.find(',', a); if (c == std::string::npos) { f.push_back(s.substr(a)); break; } f.push_back(s.substr(a, c - a)); a = c + 1; if (f.size() > 64) break; }
}
// "e=" record: exactly 16 comma separated fields, each fully parsed. Does not range-check.
static bool ParseEmitterFields(const std::string& rec, Emitter& e, int& act, int& mir) {
    std::vector<std::string> f; SplitCsv(rec, f); if (f.size() != 16) return false;
    return ParseIntStrict(f[0], e.id) && ParseFloatStrict(f[1], e.pos.x) && ParseFloatStrict(f[2], e.pos.y) && ParseFloatStrict(f[3], e.pos.z) &&
           ParseFloatStrict(f[4], e.hdgX) && ParseFloatStrict(f[5], e.hdgY) && ParseFloatStrict(f[6], e.angleDeg) &&
           ParseIntStrict(f[7], act) && ParseIntStrict(f[8], mir) && ParseIntStrict(f[9], e.tex.category) &&
           ParseFloatStrict(f[10], e.tex.colorVar) && ParseFloatStrict(f[11], e.tex.speed) && ParseFloatStrict(f[12], e.tex.detail) &&
           ParseFloatStrict(f[13], e.tex.glow) && ParseFloatStrict(f[14], e.tex.opacity) && ParseFloatStrict(f[15], e.tex.power);
}
static bool PositionPlausible(const Emitter& e) { return fabsf(e.pos.x) <= kMaxLocalX && fabsf(e.pos.y) <= kMaxLocalY && fabsf(e.pos.z) <= kMaxLocalZ; }
// Strict semantic validation: every value must already be inside its legal range (nothing is clamped / repaired).
static bool EmitterValidStrict(const Emitter& e, int act, int mir) {
    if (e.id <= 0 || e.id > kMaxEmitterId) return false;
    if ((act != 0 && act != 1) || (mir != 0 && mir != 1)) return false;
    if (e.tex.category < 0 || e.tex.category >= CAT_COUNT) return false;
    if (!PositionPlausible(e)) return false;
    float l = sqrtf(e.hdgX * e.hdgX + e.hdgY * e.hdgY); if (!(fabsf(l - 1.f) <= 1e-3f + 1e-6f)) return false;   // heading must be a unit vector (zero heading is invalid)
    const TexSettings& t = e.tex;
    if (!(e.angleDeg >= kAngleMin && e.angleDeg <= kAngleMax)) return false;
    if (!(t.colorVar >= 0.f && t.colorVar <= 1.f) || !(t.speed >= kSpeedMin && t.speed <= kSpeedMax) || !(t.detail >= 0.f && t.detail <= 1.f)) return false;
    if (!(t.glow >= 0.f && t.glow <= kGlowMax) || !(t.opacity >= kOpacityMin && t.opacity <= 1.f) || !(t.power >= kPowerMin && t.power <= kPowerMax)) return false;
    return true;
}
enum { SEEN_MIRROR = 1, SEEN_ENABLED = 2, SEEN_PRESET = 4, SEEN_ALL_PROFILE = 7 };
// Applies one "key=value" profile line. Returns 1 = handled, 0 = not a profile line (UNKNOWN field), -1 = defective (strict mode only).
// Lenient mode (autosave working copies): defective emitter records are skipped, ranges are clamped later by Profile::Sanitize().
// Strict mode (saves): any defect, duplicate field or out-of-range value rejects the whole file; `seen` tracks duplicate/missing fields.
static int ApplyLine(const std::string& line, Profile& p, bool strict, unsigned* seen) {
    if (line.compare(0, 7, "mirror=") == 0) {
        int v = 0; bool ok = ParseIntStrict(line.substr(7), v) && v >= 0 && v <= 2;
        if (strict) { if (!ok || (*seen & SEEN_MIRROR)) return -1; *seen |= SEEN_MIRROR; }
        if (ok) { p.mirrorMode = v; }
        return 1;
    }
    if (line.compare(0, 8, "enabled=") == 0) {
        std::string v = line.substr(8); bool ok = (v == "0" || v == "1");
        if (strict) { if (!ok || (*seen & SEEN_ENABLED)) return -1; *seen |= SEEN_ENABLED; }
        if (ok) { p.jetEnabled = (v == "1"); }
        return 1;
    }
    if (line.compare(0, 7, "preset=") == 0) {
        std::string v = line.substr(7);
        if (strict) { if (v != SanitizePresetName(v) || (*seen & SEEN_PRESET)) return -1; *seen |= SEEN_PRESET; p.presetName = v; return 1; }
        p.presetName = SanitizePresetName(v); return 1;
    }
    if (line.compare(0, 2, "e=") == 0) {
        if ((int)p.emitters.size() >= kMaxEmitters) return strict ? -1 : 1;     // capacity exceeded
        Emitter e; int act = 1, mir = 1;
        if (!ParseEmitterFields(line.substr(2), e, act, mir)) return strict ? -1 : 1;   // malformed record / bad number / wrong field count
        if (strict) {
            if (!EmitterValidStrict(e, act, mir)) return -1;
            for (size_t i = 0; i < p.emitters.size(); ++i) if (p.emitters[i].id == e.id) return -1;   // duplicate emitter id
        } else if (!PositionPlausible(e)) return 1;                              // implausible: skip
        e.active = act != 0; e.mirror = mir != 0;
        p.emitters.push_back(e); return 1;
    }
    return 0;
}
bool ParseProfile(const std::string& text, Profile& out) {
    std::istringstream in(text); std::string line;
    if (!std::getline(in, line)) return false;
    while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == ' ')) line.erase(line.size() - 1);
    if (line != kHeader) return false;
    Profile p;
    while (std::getline(in, line)) { while (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1); ApplyLine(line, p, false, nullptr); }
    p.Sanitize(); out = p; return true;
}
bool SaveProfileFile(const std::string& path, const Profile& p) {
    EnsureDirs();
    std::string tmp = path + ".tmp"; std::string data = SerializeProfile(p);
    {
        std::ofstream f(tmp.c_str(), std::ios::binary | std::ios::trunc); if (!f) return false;
        f << data; f.flush(); bool good = f.good(); f.close();
        if (!good || f.fail()) { remove(tmp.c_str()); return false; }      // never leave a half-written temp file behind
    }
    if (!ReplaceFile(tmp, path)) { remove(tmp.c_str()); return false; }
    return true;
}
// Bounded reader: never loads more than maxBytes (+1 chunk) into memory. 0 = ok, 1 = I/O error / missing, 2 = larger than maxBytes.
static int ReadFileBounded(const std::string& path, unsigned maxBytes, std::string& out) {
    out.clear(); std::ifstream f(path.c_str(), std::ios::binary); if (!f) return 1;
    char buf[4096];
    for (;;) {
        f.read(buf, sizeof buf); std::streamsize n = f.gcount();
        if (n > 0) { out.append(buf, (size_t)n); if (out.size() > maxBytes) { out.clear(); return 2; } }
        if (!f) break;
    }
    if (f.bad()) { out.clear(); return 1; }
    return 0;
}
bool LoadProfileFile(const std::string& path, Profile& out) {
    std::string text; int rd = ReadFileBounded(path, kMaxSaveFileBytes, text);
    if (rd == 1) return false;
    if (rd == 0 && ParseProfile(text, out)) return true;
    ReplaceFile(path, path + ".corrupt");                                  // quarantine (never delete user data)
    return false;
}
std::vector<std::string> ListPresets() {
    std::vector<std::string> r;
#ifdef _WIN32
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((BaseDir() + kSep + "presets" + kSep + "*.nfx").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { std::string n = fd.cFileName; if (n.size() > 4) r.push_back(n.substr(0, n.size() - 4)); } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#endif
    std::sort(r.begin(), r.end());
    return r;
}
bool DeletePreset(const std::string& name) {
#ifdef _WIN32
    return DeleteFileA(PresetPath(name).c_str()) != 0;
#else
    return remove(PresetPath(name).c_str()) == 0;
#endif
}

// =====================================================================================================
// Multi-save system
// =====================================================================================================
static unsigned Fnv1a(const std::string& s) { unsigned h = 2166136261u; for (size_t i = 0; i < s.size(); ++i) { h ^= (unsigned char)s[i]; h *= 16777619u; } return h; }
static std::vector<std::string> ListFilesExt(const std::string& dir, const std::string& ext) {   // file names (not paths) ending exactly in ext
    std::vector<std::string> r;
#ifdef _WIN32
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((dir + kSep + "*" + ext).c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { std::string n = fd.cFileName; if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n.size() > ext.size() && n.compare(n.size() - ext.size(), ext.size(), ext) == 0) r.push_back(n); } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR* d = opendir(dir.c_str());
    if (d) { while (dirent* e = readdir(d)) { std::string n = e->d_name; if (n.size() > ext.size() && n.compare(n.size() - ext.size(), ext.size(), ext) == 0) r.push_back(n); } closedir(d); }
#endif
    return r;
}
std::string SavesDir() { return BaseDir() + kSep + "saves"; }
std::string SavePath(const std::string& id) { return SavesDir() + kSep + id + ".nfx"; }
bool IsValidSaveId(const std::string& id) {
    if (id.size() != 16) return false;
    for (size_t i = 0; i < id.size(); ++i) { char c = id[i]; if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false; }
    return true;
}
std::string SanitizeSaveName(const std::string& n) {
    std::string r;
    for (size_t i = 0; i < n.size(); ++i) { char c = n[i]; if (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-') r += c; }
    size_t a = r.find_first_not_of(' '); if (a == std::string::npos) return std::string();
    size_t b = r.find_last_not_of(' '); r = r.substr(a, b - a + 1);
    if ((int)r.size() > kMaxSaveName) { r.resize(kMaxSaveName); size_t e = r.find_last_not_of(' '); r = r.substr(0, e + 1); }
    return r;
}
bool SameSaveName(const std::string& a, const std::string& b) {
    std::string x = SanitizeSaveName(a), y = SanitizeSaveName(b); if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i) if (tolower((unsigned char)x[i]) != tolower((unsigned char)y[i])) return false;
    return true;
}
static unsigned long long Mix64(unsigned long long x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33; return x; }
std::string NewSaveId() {
    static unsigned long long ctr = 0;
    for (int tries = 0; tries < 1000; ++tries) {
        unsigned long long seed = (unsigned long long)std::chrono::high_resolution_clock::now().time_since_epoch().count()
                                ^ ((unsigned long long)time(nullptr) << 20) ^ (++ctr * 0x9E3779B97F4A7C15ULL) ^ (unsigned long long)(size_t)&seed;
        char b[24]; snprintf(b, sizeof b, "%016llx", Mix64(seed)); std::string id = b;
        if (!FileExists(SavePath(id)) && !FileExists(SavePath(id) + ".corrupt")) return id;
    }
    return std::string();
}
std::string SerializeSave(const SaveMeta& m, const Profile& p) {
    std::ostringstream o; char b[64];
    o << kSaveHeader << "\n" << "id=" << m.id << "\n" << "name=" << SanitizeSaveName(m.name) << "\n";
    snprintf(b, sizeof b, "model=%d\ncreated=%lld\ncount=%d\n", m.modelId, m.created, (int)p.emitters.size()); o << b;
    std::string prof = SerializeProfile(p);                       // reuse the profile body (drop its own header line)
    size_t nl = prof.find('\n'); o << (nl == std::string::npos ? std::string() : prof.substr(nl + 1));
    std::string body = o.str(); snprintf(b, sizeof b, "crc=%08x\n", Fnv1a(body));
    return body + b;
}
SaveStatus ParseSave(const std::string& raw, const std::string& expectId, SaveMeta& meta, Profile& out) {
    // meta / out are assigned ONLY at the very end on success: any failure leaves the caller's live data untouched.
    if (raw.empty() || raw.size() > kMaxSaveFileBytes) return SAVE_CORRUPT;
    std::string text; text.reserve(raw.size());                            // CRLF (notepad) -> LF; a lone CR, NUL or any other control / non-ASCII byte is corruption
    for (size_t i = 0; i < raw.size(); ++i) {
        unsigned char c = (unsigned char)raw[i];
        if (c == '\r') { if (i + 1 < raw.size() && raw[i + 1] == '\n') continue; return SAVE_CORRUPT; }
        if ((c < 0x20 && c != '\n') || c >= 0x7F) return SAVE_CORRUPT;
        text += (char)c;
    }
    size_t cp = text.rfind("\ncrc="); if (cp == std::string::npos) return SAVE_CORRUPT;
    std::string body = text.substr(0, cp + 1), crc = text.substr(cp + 5);
    if (crc.size() == 9 && crc[8] == '\n') crc.erase(8);                   // exactly "crc=xxxxxxxx" + optional final newline, nothing else
    if (crc.size() != 8) return SAVE_CORRUPT;
    for (size_t i = 0; i < 8; ++i) if (!isxdigit((unsigned char)crc[i])) return SAVE_CORRUPT;
    if ((unsigned)strtoul(crc.c_str(), nullptr, 16) != Fnv1a(body)) return SAVE_CORRUPT;   // truncated / edited / bit-rot
    std::istringstream in(body); std::string line;
    if (!std::getline(in, line) || line != kSaveHeader) return SAVE_CORRUPT;
    enum { M_ID = 1, M_NAME = 2, M_MODEL = 4, M_CREATED = 8, M_COUNT = 16, M_ALL = 31 };
    SaveMeta m; Profile p; unsigned mseen = 0, pseen = 0; int count = -1;
    while (std::getline(in, line)) {
        if (line.empty()) return SAVE_CORRUPT;                             // blank lines never occur in a file we wrote
        if (line.compare(0, 3, "id=") == 0)            { if (mseen & M_ID) return SAVE_CORRUPT; mseen |= M_ID; m.id = line.substr(3); }
        else if (line.compare(0, 5, "name=") == 0)     { if (mseen & M_NAME) return SAVE_CORRUPT; mseen |= M_NAME; m.name = line.substr(5); }
        else if (line.compare(0, 6, "model=") == 0)    { if (mseen & M_MODEL) return SAVE_CORRUPT; mseen |= M_MODEL; if (!ParseIntStrict(line.substr(6), m.modelId)) return SAVE_CORRUPT; }
        else if (line.compare(0, 8, "created=") == 0)  { if (mseen & M_CREATED) return SAVE_CORRUPT; mseen |= M_CREATED; if (!ParseLLStrict(line.substr(8), m.created) || m.created < 0) return SAVE_CORRUPT; }
        else if (line.compare(0, 6, "count=") == 0)    { if (mseen & M_COUNT) return SAVE_CORRUPT; mseen |= M_COUNT; if (!ParseIntStrict(line.substr(6), count)) return SAVE_CORRUPT; }
        else if (ApplyLine(line, p, true, &pseen) <= 0) return SAVE_CORRUPT;   // -1 = defective value, 0 = unknown / unsupported field
    }
    if (mseen != M_ALL || pseen != SEEN_ALL_PROFILE) return SAVE_CORRUPT;  // every metadata + profile field must be present exactly once
    if (!IsValidSaveId(m.id) || (!expectId.empty() && m.id != expectId)) return SAVE_CORRUPT;
    if (m.name.empty() || m.name != SanitizeSaveName(m.name)) return SAVE_CORRUPT;
    if (m.modelId < 0 || m.modelId > 65535) return SAVE_CORRUPT;
    if (count < 0 || count > kMaxEmitters || count != (int)p.emitters.size()) return SAVE_CORRUPT;
    p.Sanitize();                                                          // only raises nextId: every emitter value was already validated, nothing is repaired
    m.emitterCount = (int)p.emitters.size(); meta = m; out = p; return SAVE_OK;
}
bool WriteSaveFile(const SaveMeta& m, const Profile& p) {
    if (!IsValidSaveId(m.id) || SanitizeSaveName(m.name).empty() || m.modelId < 0) return false;
    std::string data = SerializeSave(m, p);
    { SaveMeta m2; Profile p2; if (ParseSave(data, m.id, m2, p2) != SAVE_OK) return false; }   // never write a file we could not read back
    EnsureDirs();
    std::string path = SavePath(m.id), tmp = path + ".tmp";
    { std::ofstream f(tmp.c_str(), std::ios::binary | std::ios::trunc); if (!f) return false; f << data; f.flush(); if (!f.good()) { f.close(); remove(tmp.c_str()); return false; } f.close(); if (f.fail()) { remove(tmp.c_str()); return false; } }
    if (!ReplaceFile(tmp, path)) { remove(tmp.c_str()); return false; }
    return true;
}
SaveStatus ReadSaveFile(const std::string& id, SaveMeta& meta, Profile& out) {
    if (!IsValidSaveId(id)) return SAVE_CORRUPT;
    std::string path = SavePath(id), text; int rd = ReadFileBounded(path, kMaxSaveFileBytes, text);   // bounded: a huge file can never exhaust memory
    if (rd == 1) return SAVE_IO;
    SaveStatus st = (rd == 2) ? SAVE_CORRUPT : ParseSave(text, id, meta, out);
    if (st == SAVE_CORRUPT) ReplaceFile(path, path + ".corrupt");      // quarantine, never delete user data
    return st;
}
std::vector<SaveMeta> ListSaves(int* corrupt) {
    std::vector<SaveMeta> r; int bad = 0; std::vector<std::string> files = ListFilesExt(SavesDir(), ".nfx");
    for (size_t i = 0; i < files.size(); ++i) {
        std::string id = files[i].substr(0, files[i].size() - 4); if (!IsValidSaveId(id)) continue;
        SaveMeta m; Profile p; SaveStatus st = ReadSaveFile(id, m, p);
        if (st == SAVE_OK) r.push_back(m); else if (st == SAVE_CORRUPT) ++bad;
    }
    struct Cmp { static bool Lt(const SaveMeta& a, const SaveMeta& b) {
        std::string x = a.name, y = b.name; for (size_t i = 0; i < x.size(); ++i) x[i] = (char)tolower((unsigned char)x[i]); for (size_t i = 0; i < y.size(); ++i) y[i] = (char)tolower((unsigned char)y[i]);
        if (x != y) return x < y;
        if (a.created != b.created) return a.created < b.created;
        return a.id < b.id; } };
    std::sort(r.begin(), r.end(), Cmp::Lt);
    if (corrupt) *corrupt = bad;
    return r;
}
bool DeleteSaveFile(const std::string& id) {
    if (!IsValidSaveId(id)) return false;
#ifdef _WIN32
    return DeleteFileA(SavePath(id).c_str()) != 0;
#else
    return remove(SavePath(id).c_str()) == 0;
#endif
}

// =====================================================================================================
// Autosave of the active configuration: dirty flag is cleared only after a successful write
// =====================================================================================================
static bool Reached(unsigned now, unsigned t) { return (int)(now - t) >= 0; }   // wrap-safe (GetTickCount wraps every 49.7 days)
unsigned AutosaveBackoffMs(int failures) {
    if (failures <= 0) return 0;
    unsigned d = kAutosaveDelayMs;
    for (int i = 1; i < failures && d < kAutosaveMaxBackoffMs; ++i) d *= 2;
    return d > kAutosaveMaxBackoffMs ? kAutosaveMaxBackoffMs : d;
}
void AutosaveMarkDirty(AutosaveState& a, unsigned now) { a.dirty = true; a.dirtyTime = now; }   // back-off is NOT reset by new edits
bool AutosaveHasUnsaved(const AutosaveState& a) { return a.dirty || !a.pending.empty(); }
bool AutosaveDue(const AutosaveState& a, int activeModel, unsigned now, bool busy) {
    if (a.dirty && activeModel >= 0 && !busy && (unsigned)(now - a.dirtyTime) > kAutosaveDelayMs && (a.failures == 0 || Reached(now, a.retryAt))) return true;
    for (size_t i = 0; i < a.pending.size(); ++i) if (a.pending[i].failures == 0 || Reached(now, a.pending[i].retryAt)) return true;
    return false;
}
int AutosaveFlush(AutosaveState& a, int activeModel, const Profile& active, unsigned now, bool force, ProfileWriteFn write) {
    int failed = 0;
    for (size_t i = 0; i < a.pending.size();) {                            // configurations of vehicles we already left
        PendingSave& ps = a.pending[i];
        if (!force && !(ps.failures == 0 || Reached(now, ps.retryAt))) { ++i; continue; }
        if (write(VehiclePath(ps.modelId), ps.profile)) { a.pending.erase(a.pending.begin() + (long)i); continue; }
        ++ps.failures; ps.retryAt = now + AutosaveBackoffMs(ps.failures); ++failed; ++i;
    }
    if (a.dirty && activeModel >= 0 && (force || a.failures == 0 || Reached(now, a.retryAt))) {
        if (write(VehiclePath(activeModel), active)) { a.dirty = false; a.failures = 0; a.retryAt = 0; }   // the ONLY place the flag is cleared
        else { ++a.failures; a.retryAt = now + AutosaveBackoffMs(a.failures); ++failed; }                  // dirty stays set: nothing is discarded
    }
    return failed;
}
void AutosavePark(AutosaveState& a, int model, const Profile& p, int failures, unsigned retryAt) {
    if (model < 0) return;
    for (size_t i = 0; i < a.pending.size(); ++i) if (a.pending[i].modelId == model) { a.pending[i].profile = p; return; }   // newer data replaces older
    PendingSave ps; ps.modelId = model; ps.profile = p; ps.failures = failures; ps.retryAt = retryAt; a.pending.push_back(ps);
}
bool AutosaveTakePending(AutosaveState& a, int model, Profile& out) {
    for (size_t i = 0; i < a.pending.size(); ++i) if (a.pending[i].modelId == model) { out = a.pending[i].profile; a.pending.erase(a.pending.begin() + (long)i); return true; }
    return false;
}
}
