#pragma once
#include "Pch.h"
#include "Types.h"
#include "Storage.h"
#include "TextureEngine.h"
#include "Draw2D.h"
namespace nfx {
struct InputState {
    bool valid = false; float mx = 0, my = 0;           // mouse in virtual UI coords (1280x720)
    bool lDown = false, lPressed = false, lReleased = false;
    bool altDown = false, delPressed = false, escPressed = false;
    int wheel = 0; bool justRestored = false;            // wheel: +1 up / -1 down this frame; justRestored: ALT was just released (ignore that frame's click)
    std::string typed; bool backPressed = false, enterPressed = false;   // text entry (save-name dialog), polled per frame
};
enum Dialog { DLG_NONE = 0, DLG_NAME, DLG_OVERWRITE, DLG_CONFIRM, DLG_MESSAGE };
enum ConfirmAction { ACT_NONE = 0, ACT_DELETE_SAVE, ACT_OVERWRITE_ROW, ACT_RESET_VEHICLE };
struct AppState {
    Profile profile, backup;
    int vehModel = -1; CVehicle* veh = nullptr;
    V3 bbMin, bbMax; bool haveBounds = false;
    bool menuOpen = false, editMode = false, dragging = false, altHidden = false;   // altHidden: ALT held -> menu + cursor hidden, game camera free
    int selectedId = -1, tab = 0; bool applyAll = false; TexSettings newTex;
    AutosaveState autosave;                              // dirty flag + retry/back-off + parked unsaved profiles (see Storage.h)
    float menuAlpha = 0.f, time = 0.f;
    std::vector<SaveMeta> saves; int savePage = 0; bool showAllSaves = false; int emitterPage = 0;
    int dlg = DLG_NONE, dlgAction = ACT_NONE; bool dlgWarn = false;
    std::string dlgName, dlgTarget, dlgTitle, dlgMsg, dlgErr;   // dlgTarget = Save ID the dialog acts on
    std::string status; unsigned statusTime = 0;
};
extern AppState S; extern InputState In;

// app / helpers
void SetStatus(const std::string& s);
void MarkDirty(); void SaveNow();
Emitter* SelectedEmitter();
bool GetBounds(CVehicle* v, V3& mn, V3& mx);
bool VehicleBasis(CVehicle* v, V3& r, V3& f, V3& u, V3& p);
V3 LocalToWorld(CVehicle* v, const V3& l);
V3 LocalDirToWorld(CVehicle* v, const V3& l);
V3 WorldDirToLocal(CVehicle* v, const V3& w);
bool WorldToScreen(const V3& w, float& vx, float& vy);   // virtual coords
void EnterEditMode();
bool PickVehicleMesh(CVehicle* v, const V3& origin, const V3& dir, V3& worldHit, V3& worldNormal);   // MeshPick.cpp: real render-mesh ray cast
// saves (Saves.cpp)
void ReloadSaves();
void OpenSaveDialog();
void AskResetVehicle();
void DrawSavesTab(float x, float y);
void DrawDialog();
void CloseDialog();
// modules
void DrawJets();
void DrawMenu();
void DrawEditor();
void DrawDragon(float t, float alpha);
void DrawBranding(float cx, float y, float t);
// ui widgets (Menu.cpp)
bool UiButton(float x, float y, float w, float h, const char* label, bool selected = false, float px = 17);
bool UiToggle(float x, float y, float w, float h, const char* label, bool on);
bool UiSlider(float x, float y, float w, const char* label, float& v, float lo, float hi);
void UiPanel(float x, float y, float w, float h, float alpha);
void UiCursor();
bool InRect(float x, float y, float w, float h);
// premium palette: deep black, graphite, metallic gold, platinum white; purple only as a faint secondary accent
const Col kGold = { 212, 175, 85, 255 }, kGoldHi = { 255, 226, 140, 255 }, kGoldDim = { 138, 112, 52, 255 }, kWhite = { 238, 239, 244, 255 },
          kDim = { 168, 171, 182, 255 }, kPurple = { 132, 108, 190, 255 }, kInk = { 22, 19, 12, 255 };
bool UiMouseCaptured();      // App.cpp: true while the free OS cursor drives the menu
}
