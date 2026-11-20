// OpenXR Companion - OpenXR API layer.
// Adds a configurable radial menu, chords and layers on top of any OpenXR game.
#include "panel.h"
#include "screen.h"
#include "keys.h"
#include "mirror.h"
#include "tracking_log.h"
#include "head_smoothing.h"
#include "game_offsets.h"
#include "overlay.h"
#include <openxr/openxr_loader_negotiation.h>
#include <unordered_map>
#include <map>
#include <set>
#include <climits>

#define LAYER_NAME "XR_APILAYER_NOVENDOR_openxr_companion"

// ---------------------------------------------------------------- next-layer functions
static PFN_xrGetInstanceProcAddr gNextGipa = nullptr;
static XrInstance gInst = XR_NULL_HANDLE;
static struct {
    PFN_xrDestroyInstance DestroyInstance;
    PFN_xrCreateSession CreateSession;
    PFN_xrDestroySession DestroySession;
    PFN_xrCreateActionSet CreateActionSet;
    PFN_xrCreateAction CreateAction;
    PFN_xrSuggestInteractionProfileBindings SuggestInteractionProfileBindings;
    PFN_xrAttachSessionActionSets AttachSessionActionSets;
    PFN_xrSyncActions SyncActions;
    PFN_xrGetActionStateBoolean GetActionStateBoolean;
    PFN_xrGetActionStateFloat GetActionStateFloat;
    PFN_xrGetActionStateVector2f GetActionStateVector2f;
    PFN_xrWaitFrame WaitFrame;
    PFN_xrEndFrame EndFrame;
    PFN_xrStringToPath StringToPath;
    PFN_xrPathToString PathToString;
    PFN_xrCreateActionSpace CreateActionSpace;
    PFN_xrCreateReferenceSpace CreateReferenceSpace;
    PFN_xrDestroySpace DestroySpace;
    PFN_xrLocateSpace LocateSpace;
    PFN_xrLocateViews LocateViews;
    PFN_xrLocateSpaces LocateSpaces;
    PFN_xrLocateSpacesKHR LocateSpacesKHR;
    PFN_xrApplyHapticFeedback ApplyHapticFeedback;
    PFN_xrGetCurrentInteractionProfile GetCurrentInteractionProfile;
} N{};
static XrFns gSwap;      // swapchain calls our panels use (our hooks, so the mirror sees them too)
static XrFns gSwapNext;  // the next layer's
static HeadsetMirror gMirror;
static MirrorSettings gMirrorSet;
static FILETIME gMirrorFileTime{};
static double gLastMirrorCheck = -10;

// ---------------------------------------------------------------- head smoothing (experimental)
static HeadSmoother gHeadSmooth;
static FILETIME gHeadFileTime{};

// ---------------------------------------------------------------- our action set
enum OA { A_BTN1, A_BTN1T, A_BTN2, A_BTN2T, A_TRIG, A_TRIGT, A_GRIP, A_STICK, A_STICKC, A_STICKT, A_REST, A_MENU, A_GRIPPOSE, A_AIMPOSE, A_HAPTIC, A_COUNT };
static const struct { const char* name; XrActionType type; } kActs[A_COUNT] = {
    {"vro_btn1", XR_ACTION_TYPE_BOOLEAN_INPUT}, {"vro_btn1_touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"vro_btn2", XR_ACTION_TYPE_BOOLEAN_INPUT}, {"vro_btn2_touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"vro_trigger", XR_ACTION_TYPE_FLOAT_INPUT}, {"vro_trigger_touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"vro_grip", XR_ACTION_TYPE_FLOAT_INPUT}, {"vro_stick", XR_ACTION_TYPE_VECTOR2F_INPUT},
    {"vro_stick_click", XR_ACTION_TYPE_BOOLEAN_INPUT}, {"vro_stick_touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"vro_thumbrest", XR_ACTION_TYPE_BOOLEAN_INPUT}, {"vro_menu", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"vro_grip_pose", XR_ACTION_TYPE_POSE_INPUT}, {"vro_aim_pose", XR_ACTION_TYPE_POSE_INPUT},
    {"vro_haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT},
};
struct PB { int act; const char* sub; int hands; }; // hands: 1 left, 2 right, 3 both
static const PB kCommonPoses[] = { {A_GRIPPOSE, "input/grip/pose", 3}, {A_AIMPOSE, "input/aim/pose", 3}, {A_HAPTIC, "output/haptic", 3} };
static const PB kTouch[] = {
    {A_BTN1, "input/x/click", 1}, {A_BTN1T, "input/x/touch", 1}, {A_BTN2, "input/y/click", 1}, {A_BTN2T, "input/y/touch", 1}, {A_MENU, "input/menu/click", 1},
    {A_BTN1, "input/a/click", 2}, {A_BTN1T, "input/a/touch", 2}, {A_BTN2, "input/b/click", 2}, {A_BTN2T, "input/b/touch", 2},
    {A_TRIG, "input/trigger/value", 3}, {A_TRIGT, "input/trigger/touch", 3}, {A_GRIP, "input/squeeze/value", 3},
    {A_STICK, "input/thumbstick", 3}, {A_STICKC, "input/thumbstick/click", 3}, {A_STICKT, "input/thumbstick/touch", 3},
    {A_REST, "input/thumbrest/touch", 3},
};
static const PB kIndex[] = {
    {A_BTN1, "input/a/click", 3}, {A_BTN1T, "input/a/touch", 3}, {A_BTN2, "input/b/click", 3}, {A_BTN2T, "input/b/touch", 3},
    {A_TRIG, "input/trigger/value", 3}, {A_TRIGT, "input/trigger/touch", 3}, {A_GRIP, "input/squeeze/value", 3},
    {A_STICK, "input/thumbstick", 3}, {A_STICKC, "input/thumbstick/click", 3}, {A_STICKT, "input/thumbstick/touch", 3},
};
static const PB kWmr[] = {
    {A_MENU, "input/menu/click", 3}, {A_GRIP, "input/squeeze/click", 3}, {A_TRIG, "input/trigger/value", 3},
    {A_STICK, "input/thumbstick", 3}, {A_STICKC, "input/thumbstick/click", 3},
};
static const PB kVive[] = {
    {A_MENU, "input/menu/click", 3}, {A_GRIP, "input/squeeze/click", 3}, {A_TRIG, "input/trigger/value", 3},
    {A_STICK, "input/trackpad", 3}, {A_STICKC, "input/trackpad/click", 3}, {A_STICKT, "input/trackpad/touch", 3},
};
static const PB kSimple[] = { {A_TRIG, "input/select/click", 3}, {A_MENU, "input/menu/click", 3} };

static std::vector<PB> ProfileTable(const std::string& p) {
    std::vector<PB> v;
    auto add = [&](const PB* b, size_t n) { v.insert(v.end(), b, b + n); };
    if (p == "/interaction_profiles/oculus/touch_controller" || p == "/interaction_profiles/meta/touch_controller_plus" ||
        p == "/interaction_profiles/facebook/touch_controller_pro" || p == "/interaction_profiles/meta/touch_pro_controller" ||
        p == "/interaction_profiles/meta/touch_plus_controller") add(kTouch, std::size(kTouch));
    else if (p == "/interaction_profiles/valve/index_controller") add(kIndex, std::size(kIndex));
    else if (p == "/interaction_profiles/microsoft/motion_controller" || p == "/interaction_profiles/hp/mixed_reality_controller") add(kWmr, std::size(kWmr));
    else if (p == "/interaction_profiles/htc/vive_controller") add(kVive, std::size(kVive));
    else if (p == "/interaction_profiles/khr/simple_controller") add(kSimple, std::size(kSimple));
    else return v;
    add(kCommonPoses, std::size(kCommonPoses));
    return v;
}

// ---------------------------------------------------------------- state
static std::recursive_mutex gMx;
static XrActionSet gSet = XR_NULL_HANDLE;
static XrAction gA[A_COUNT]{};
static XrPath gHandPath[2]{};
static bool gAttached = false;
static int gMinPriority = INT_MAX;
static std::set<std::string> gOurProfiles;

struct AppBind { int hand, comp, pf; }; // pf: 0 press/value, 1 touch, 2 vec2, 3 x, 4 y
static std::unordered_map<uint64_t, std::vector<AppBind>> gAppBinds;
static std::unordered_map<uint64_t, std::vector<XrPath>> gActionSubs;

static XrSession gSess = XR_NULL_HANDLE;
static Gfx gGfx;
static XrSpace gLocal = XR_NULL_HANDLE, gView = XR_NULL_HANDLE, gGripSp[2]{}, gAimSp[2]{};
static XrTime gPredTime = 0;
static uint64_t gSyncIdx = 0;

static Engine gEng;
static void MarkOwnSave();
static OverlayRenderer gOverlay(gEng);   // everything we draw into the headset

// ---------------------------------------------------------------- controller offsets for the game
// The game's hand pose actions (from its suggested bindings) and the spaces it makes from them.
// Every pose the game locates for those spaces is turned / moved by the profile's offsets.
struct PoseBind { int hand; PoseKind kind; };
static std::unordered_map<uint64_t, std::vector<PoseBind>> gPoseBinds; // action -> bindings (under gMx)
static GameHandOffsets gHandOffsets;

struct Track { uint64_t sync = ~0ull; float prev[2]{}, cur[2]{}; };
static std::unordered_map<uint64_t, Track> gTrack;

static FILETIME gCfgTimes[5]{};
static double gLastCfgCheck = 0;

static inline uint64_t H(XrAction a) { return (uint64_t)a; }

// ---------------------------------------------------------------- helpers
static XrPath P(const std::string& s) { XrPath p = XR_NULL_PATH; N.StringToPath(gInst, s.c_str(), &p); return p; }
static std::string S(XrPath p) {
    char buf[XR_MAX_PATH_LENGTH]; uint32_t n = 0;
    if (XR_FAILED(N.PathToString(gInst, p, sizeof(buf), &n, buf))) return {};
    return buf;
}

static bool ParseAppPath(const std::string& s, AppBind& b) {
    const std::string pre[2] = { "/user/hand/left/input/", "/user/hand/right/input/" };
    for (int h = 0; h < 2; h++) {
        if (s.rfind(pre[h], 0) != 0) continue;
        std::string rest = s.substr(pre[h].size());
        auto sl = rest.find('/');
        std::string comp = rest.substr(0, sl), feat = sl == std::string::npos ? "" : rest.substr(sl + 1);
        int c = comp == "select" ? C_TRIGGER : ParseCompName(comp);
        if (c < 0 || comp == "system") return false;
        b.hand = h; b.comp = c;
        if (feat == "touch") b.pf = 1;
        else if (feat.empty() && c == C_STICK) b.pf = 2;
        else if (feat == "x") b.pf = 3;
        else if (feat == "y") b.pf = 4;
        else if (feat == "click" || feat == "value" || feat == "force" || feat.empty()) b.pf = 0;
        else return false;
        return true;
    }
    return false;
}

static void EnsureActions() {
    if (gSet != XR_NULL_HANDLE || gInst == XR_NULL_HANDLE) return;
    gHandPath[0] = P("/user/hand/left");
    gHandPath[1] = P("/user/hand/right");
    XrActionSetCreateInfo si{ XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy_s(si.actionSetName, "openxr_companion_set");
    strcpy_s(si.localizedActionSetName, "OpenXR Companion");
    si.priority = gMinPriority == INT_MAX ? 0 : (uint32_t)gMinPriority;
    XrResult r = N.CreateActionSet(gInst, &si, &gSet);
    if (XR_FAILED(r)) { Log("CreateActionSet failed %d", r); gSet = XR_NULL_HANDLE; return; }
    for (int i = 0; i < A_COUNT; i++) {
        XrActionCreateInfo ci{ XR_TYPE_ACTION_CREATE_INFO };
        strcpy_s(ci.actionName, kActs[i].name);
        strcpy_s(ci.localizedActionName, kActs[i].name);
        ci.actionType = kActs[i].type;
        ci.countSubactionPaths = 2; ci.subactionPaths = gHandPath;
        r = N.CreateAction(gSet, &ci, &gA[i]);
        if (XR_FAILED(r)) Log("CreateAction %s failed %d", kActs[i].name, r);
    }
    Log("our action set created, priority %u", si.priority);
}

static void ReloadProfile(bool announce) {
    std::wstring path; std::string err;
    auto p = LoadProfile(path, err);
    if (!p) {
        Log("config error: %s", err.c_str());
        gEng.Toast("Config error: " + err, 6.0);
        return;
    }
    gEng.SetProfile(p);
    gOverlay.InvalidateAll();
    Log("loaded profile %s (%zu bindings, %zu wheels, enabled=%d)", p->sourceName.c_str(), p->bindings.size(), p->wheels.size(), p->enabled);
    if (announce) gEng.Toast("OpenXR Companion: " + p->sourceName + " loaded");
}

static FILETIME FileTime(const std::wstring& f) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &d)) return {};
    return d.ftLastWriteTime;
}
// The layer wrote the profile itself: take the new file times without reloading (a reload would close menus).
static void MarkOwnSave() {
    std::wstring dir = ProfilesDir();
    gCfgTimes[0] = FileTime(dir + L"\\" + Utf8ToW(ExeName()) + L".json");
    gCfgTimes[1] = FileTime(dir + L"\\default.json");
    gCfgTimes[2] = FileTime(dir + L"\\user_defaults.json");
    gCfgTimes[3] = FileTime(ActiveProfilePath());
    gCfgTimes[4] = gEng.prof ? FileTime(dir + L"\\" + Utf8ToW(gEng.prof->sourceName)) : FILETIME{};
}

static void CheckConfigChanged() {
    double now = NowSec();
    if (now - gLastCfgCheck < 1.0) return;
    gLastCfgCheck = now;
    std::wstring dir = ProfilesDir();
    FILETIME t[5] = { FileTime(dir + L"\\" + Utf8ToW(ExeName()) + L".json"), FileTime(dir + L"\\default.json"), FileTime(dir + L"\\user_defaults.json"),
                      FileTime(ActiveProfilePath()), gEng.prof ? FileTime(dir + L"\\" + Utf8ToW(gEng.prof->sourceName)) : FILETIME{} };
    bool changed = memcmp(t, gCfgTimes, sizeof(t)) != 0;
    bool first = gCfgTimes[1].dwLowDateTime == 0 && gCfgTimes[1].dwHighDateTime == 0;
    memcpy(gCfgTimes, t, sizeof(t));
    if (changed && !first) ReloadProfile(true);
}

static bool Locate(XrSpace sp, XrSpace base, XrPosef& out) {
    if (!sp || !base || !gPredTime) return false;
    XrSpaceLocation l{ XR_TYPE_SPACE_LOCATION };
    if (XR_FAILED(N.LocateSpace(sp, base, gPredTime, &l))) return false;
    const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((l.locationFlags & need) != need) return false;
    out = l.pose;
    return true;
}

// Which controllers are in use: checked once a second (the profile changes rarely).
static bool gTouchCtl[2]{};
static double gLastIpCheck = -10;
static void CheckInteractionProfiles(double now) {
    if (now - gLastIpCheck < 1.0 || !N.GetCurrentInteractionProfile || !gSess) return;
    gLastIpCheck = now;
    for (int h = 0; h < 2; h++) {
        XrInteractionProfileState st{ XR_TYPE_INTERACTION_PROFILE_STATE };
        if (XR_FAILED(N.GetCurrentInteractionProfile(gSess, gHandPath[h], &st))) continue;
        std::string p = st.interactionProfile ? S(st.interactionProfile) : "";
        bool touch = p.find("touch") != std::string::npos && (p.find("oculus") != std::string::npos ||
                     p.find("meta") != std::string::npos || p.find("facebook") != std::string::npos);
        if (touch != gTouchCtl[h]) Log("%s controller: %s%s", h ? "right" : "left", p.c_str(), touch ? " (Touch: aim correction available)" : "");
        gTouchCtl[h] = touch;
    }
}


// Head smoothing settings: read when the game starts, then whenever the editor changes the file.
static void CheckHeadSmoothingFile() {
    FILETIME hft = FileTime(HeadSmoothSettingsPath());
    if (memcmp(&hft, &gHeadFileTime, sizeof(hft)) == 0) return;
    gHeadFileTime = hft;
    HeadSmoothSettings hs; LoadHeadSmoothSettings(hs);
    static bool firstLoad = true;
    if (firstLoad && hs.on && !hs.keepOn) { // safety: never already on when a game starts, unless asked
        hs.on = false; SwitchOffHeadSmoothing(); gHeadFileTime = FileTime(HeadSmoothSettingsPath());
        Log("head smoothing: was on, switched off at game start (keep_on is false)");
    }
    firstLoad = false;
    bool was = gHeadSmooth.SetSettings(hs);
    Log("head smoothing: settings read: %s, mode %s, strength %.2f", hs.on ? "ON" : "off", hs.mode == 1 ? "headset" : "recording", hs.strength);
    if (hs.on != was) gEng.Toast(hs.on ? "Head smoothing ON (experimental: stop if you feel unwell)" : "Head smoothing off", 3.0);
}

static void ReadInputsAndUpdate() {
    Frame f;
    f.t = NowSec();
    CheckInteractionProfiles(f.t);
    for (int h = 0; h < 2; h++) {
        auto gb = [&](int a) {
            XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO }; gi.action = gA[a]; gi.subactionPath = gHandPath[h];
            XrActionStateBoolean s{ XR_TYPE_ACTION_STATE_BOOLEAN };
            return XR_SUCCEEDED(N.GetActionStateBoolean(gSess, &gi, &s)) && s.isActive && s.currentState;
        };
        auto gf = [&](int a) {
            XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO }; gi.action = gA[a]; gi.subactionPath = gHandPath[h];
            XrActionStateFloat s{ XR_TYPE_ACTION_STATE_FLOAT };
            return XR_SUCCEEDED(N.GetActionStateFloat(gSess, &gi, &s)) && s.isActive ? s.currentState : 0.f;
        };
        HandRaw& r = f.h[h];
        r.b1 = gb(A_BTN1); r.b1t = gb(A_BTN1T); r.b2 = gb(A_BTN2); r.b2t = gb(A_BTN2T);
        r.trig = gf(A_TRIG); r.trigT = gb(A_TRIGT); r.grip = gf(A_GRIP);
        {
            XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO }; gi.action = gA[A_STICK]; gi.subactionPath = gHandPath[h];
            XrActionStateVector2f s{ XR_TYPE_ACTION_STATE_VECTOR2F };
            if (XR_SUCCEEDED(N.GetActionStateVector2f(gSess, &gi, &s)) && s.isActive) { r.sx = s.currentState.x; r.sy = s.currentState.y; }
        }
        r.sc = gb(A_STICKC); r.st = gb(A_STICKT); r.rest = gb(A_REST); r.menu = gb(A_MENU);
        r.gripValid = Locate(gGripSp[h], gLocal, r.gripPose);
        r.aimValid = Locate(gAimSp[h], gLocal, r.aimPose);
        r.touchCtl = gTouchCtl[h];
    }
    f.headValid = Locate(gView, gLocal, f.head);
    gOverlay.SetHead(f.head, f.headValid);
    gEng.Update(f);

    for (auto& hq : gEng.haptics) {
        XrHapticActionInfo hi{ XR_TYPE_HAPTIC_ACTION_INFO }; hi.action = gA[A_HAPTIC]; hi.subactionPath = gHandPath[hq.hand];
        XrHapticVibration v{ XR_TYPE_HAPTIC_VIBRATION };
        v.amplitude = hq.amp; v.duration = (XrDuration)(hq.sec * 1e9); v.frequency = XR_FREQUENCY_UNSPECIFIED;
        N.ApplyHapticFeedback(gSess, &hi, (XrHapticBaseHeader*)&v);
    }
    gEng.haptics.clear();
    {   // what the game's controller poses are moved by, for the LocateSpace hooks
        GameHandOffsets::Settings hs;
        hs.on = gEng.Active() && gEng.offs.enabled;
        hs.steadying = hs.on ? gEng.offs.smoothing : 0.f;
        for (int h = 0; h < 2; h++)
            for (int k = 0; k < 2; k++) hs.has[h][k] = hs.on && gEng.OffsetPose(h, k, hs.pose[h][k]);
        gHandOffsets.SetSettings(hs);
    }
    if (gEng.offsSaveRequested && gEng.prof) {
        gEng.offsSaveRequested = false;
        gEng.prof->offsets = gEng.offs;
        std::string err;
        if (SaveOffsets(*gEng.prof, err)) { MarkOwnSave(); Log("controller offsets saved to %s", gEng.prof->sourceName.c_str()); }
        else { Log("offsets: save failed: %s", err.c_str()); gEng.Toast("Could not save offsets: " + err, 6.0); }
    }
    if (gEng.mirrorAsk != MirrorRequest::None) { // a wheel / wrist button: switch the headset mirror, and remember it for the editor
        MirrorRequest a = gEng.mirrorAsk; gEng.mirrorAsk = MirrorRequest::None;
        gMirrorSet.on = a == MirrorRequest::On ? true : a == MirrorRequest::Off ? false : !gMirrorSet.on;
        SaveMirrorSettings(gMirrorSet); gMirrorFileTime = FileTime(MirrorSettingsPath());
        gEng.Toast(gMirrorSet.on ? "Headset mirror ON (a window on the desktop)" : "Headset mirror off", 2.0);
    }
    if (gMirror.ClosedByUser()) { gMirrorSet.on = false; SaveMirrorSettings(gMirrorSet); gMirrorFileTime = FileTime(MirrorSettingsPath()); }
    {
        double now = NowSec();
        if (now - gLastMirrorCheck > 1.0) { // the editor's button writes the file
            gLastMirrorCheck = now;
            FILETIME ft = FileTime(MirrorSettingsPath());
            if (memcmp(&ft, &gMirrorFileTime, sizeof(ft)) != 0) { gMirrorFileTime = ft; LoadMirrorSettings(gMirrorSet); }
            CheckHeadSmoothingFile();
        }
        gMirror.eye = gMirrorSet.eye; gMirror.zoom = gMirrorSet.zoom; gMirror.fps = gMirrorSet.fps; gMirror.smoothing = gMirrorSet.smoothing; gMirror.onTop = gMirrorSet.onTop;
        bool want = gMirrorSet.on && gGfx.api == 11;
        if (gMirrorSet.on && gGfx.api == 12 && !gMirror.On()) {
            static bool told = false;
            if (!told) { told = true; gEng.Toast("Headset mirror: this game uses D3D12, not supported yet", 4.0); }
        }
        gMirror.SetOn(want);
        gEng.mirrorOn = gMirrorSet.on;
    }
    if (gEng.reloadRequested) { gEng.reloadRequested = false; ReloadProfile(true); }
    if (gEng.saveRequested && gEng.prof) { // leaving adjust mode: keep the new placement
        gEng.saveRequested = false;
        std::string err;
        if (SavePlacement(*gEng.prof, err)) { MarkOwnSave(); Log("adjust: placement saved to %s", gEng.prof->sourceName.c_str()); }
        else { Log("adjust: save failed: %s", err.c_str()); gEng.Toast("Could not save positions: " + err, 6.0); }
    }
}

// ---------------------------------------------------------------- hooks
static XrResult XRAPI_CALL Hook_xrDestroyInstance(XrInstance inst) {
    {
        std::lock_guard l(gMx);
        Keys::Shutdown();
        DrawShutdown();
        gSet = XR_NULL_HANDLE; gAttached = false; gAppBinds.clear(); gPoseBinds.clear(); gActionSubs.clear(); gOurProfiles.clear();
        gMinPriority = INT_MAX; gInst = XR_NULL_HANDLE;
    }
    return N.DestroyInstance(inst);
}

static XrResult XRAPI_CALL Hook_xrCreateActionSet(XrInstance inst, const XrActionSetCreateInfo* ci, XrActionSet* set) {
    XrResult r = N.CreateActionSet(inst, ci, set);
    if (XR_SUCCEEDED(r)) { std::lock_guard l(gMx); gMinPriority = std::min(gMinPriority, (int)ci->priority); }
    return r;
}

static XrResult XRAPI_CALL Hook_xrCreateAction(XrActionSet set, const XrActionCreateInfo* ci, XrAction* a) {
    XrResult r = N.CreateAction(set, ci, a);
    if (XR_SUCCEEDED(r)) {
        std::lock_guard l(gMx);
        gActionSubs[H(*a)].assign(ci->subactionPaths, ci->subactionPaths + ci->countSubactionPaths);
    }
    return r;
}

static XrResult XRAPI_CALL Hook_xrSuggestInteractionProfileBindings(XrInstance inst, const XrInteractionProfileSuggestedBinding* sb) {
    std::lock_guard l(gMx);
    std::string prof = S(sb->interactionProfile);
    for (uint32_t i = 0; i < sb->countSuggestedBindings; i++) {
        AppBind b;
        std::string bp = S(sb->suggestedBindings[i].binding);
        PoseBind pb;
        if (ParseHandPosePath(bp, pb.hand, pb.kind)) {
            auto& v = gPoseBinds[H(sb->suggestedBindings[i].action)];
            bool dup = false;
            for (auto& e : v) if (e.hand == pb.hand && e.kind == pb.kind) dup = true;
            if (!dup) v.push_back(pb);
            continue;
        }
        if (ParseAppPath(bp, b)) {
            auto& v = gAppBinds[H(sb->suggestedBindings[i].action)];
            bool dup = false;
            for (auto& e : v) if (e.hand == b.hand && e.comp == b.comp && e.pf == b.pf) dup = true;
            if (!dup) v.push_back(b);
        }
    }
    EnsureActions();
    auto table = ProfileTable(prof);
    if (gSet == XR_NULL_HANDLE || table.empty()) {
        Log("suggest %s: %u app bindings (not augmented)", prof.c_str(), sb->countSuggestedBindings);
        return N.SuggestInteractionProfileBindings(inst, sb);
    }
    std::vector<XrActionSuggestedBinding> all(sb->suggestedBindings, sb->suggestedBindings + sb->countSuggestedBindings);
    for (auto& e : table)
        for (int h = 0; h < 2; h++)
            if (e.hands & (1 << h)) all.push_back({ gA[e.act], P(std::string(h ? "/user/hand/right/" : "/user/hand/left/") + e.sub) });
    XrInteractionProfileSuggestedBinding copy = *sb;
    copy.countSuggestedBindings = (uint32_t)all.size();
    copy.suggestedBindings = all.data();
    XrResult r = N.SuggestInteractionProfileBindings(inst, &copy);
    if (XR_FAILED(r)) {
        Log("suggest %s with our bindings failed (%d), retrying app-only", prof.c_str(), r);
        return N.SuggestInteractionProfileBindings(inst, sb);
    }
    gOurProfiles.insert(prof);
    Log("suggest %s: %u app + %zu ours", prof.c_str(), sb->countSuggestedBindings, all.size() - sb->countSuggestedBindings);
    return r;
}

static XrResult XRAPI_CALL Hook_xrCreateSession(XrInstance inst, const XrSessionCreateInfo* ci, XrSession* s) {
    XrResult r = N.CreateSession(inst, ci, s);
    if (XR_FAILED(r)) return r;
    std::lock_guard l(gMx);
    gSess = *s;
    gGfx.Init(ci->next);
    Log("session created, graphics api %d", gGfx.api);
    if (!gGfx.api) Log("graphics API is not D3D11/D3D12: the wheel will not be drawn, bindings still work");
    XrReferenceSpaceCreateInfo rs{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    rs.poseInReferenceSpace.orientation.w = 1;
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    N.CreateReferenceSpace(gSess, &rs, &gLocal);
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    N.CreateReferenceSpace(gSess, &rs, &gView);
    ReloadProfile(true);
    return r;
}

// What the overlay needs from the session to draw.
static OverlayContext OverlayCtx() {
    OverlayContext c;
    c.session = gSess;
    c.gfx = &gGfx;
    c.swap = &gSwap;
    c.local = gLocal;
    c.view = gView;
    return c;
}

static void DestroySessionStuff() {
    gOverlay.Destroy(OverlayCtx());
    gHandOffsets.Clear();
    for (int h = 0; h < 2; h++) {
        if (gGripSp[h]) N.DestroySpace(gGripSp[h]);
        if (gAimSp[h]) N.DestroySpace(gAimSp[h]);
        gGripSp[h] = gAimSp[h] = XR_NULL_HANDLE;
    }
    if (gLocal) N.DestroySpace(gLocal);
    if (gView) N.DestroySpace(gView);
    gLocal = gView = XR_NULL_HANDLE;
    gEng.Reset();
    gMirror.Shutdown();
    gGfx.Release();
    gSess = XR_NULL_HANDLE; gAttached = false; gPredTime = 0;
}

static XrResult XRAPI_CALL Hook_xrDestroySession(XrSession s) {
    {
        std::lock_guard l(gMx);
        if (s == gSess) DestroySessionStuff();
    }
    return N.DestroySession(s);
}

static XrResult XRAPI_CALL Hook_xrAttachSessionActionSets(XrSession s, const XrSessionActionSetsAttachInfo* ai) {
    std::lock_guard l(gMx);
    EnsureActions();
    if (gSet == XR_NULL_HANDLE || s != gSess) return N.AttachSessionActionSets(s, ai);
    if (gOurProfiles.empty()) Log("warning: the game suggested no controller profile we know; our inputs will be inactive");
    std::vector<XrActionSet> sets(ai->actionSets, ai->actionSets + ai->countActionSets);
    sets.push_back(gSet);
    XrSessionActionSetsAttachInfo copy = *ai;
    copy.countActionSets = (uint32_t)sets.size(); copy.actionSets = sets.data();
    XrResult r = N.AttachSessionActionSets(s, &copy);
    if (XR_FAILED(r)) {
        Log("attach with our set failed (%d), retrying without", r);
        return N.AttachSessionActionSets(s, ai);
    }
    gAttached = true;
    for (int h = 0; h < 2; h++) {
        XrActionSpaceCreateInfo ci{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
        ci.poseInActionSpace.orientation.w = 1;
        ci.subactionPath = gHandPath[h];
        ci.action = gA[A_GRIPPOSE]; N.CreateActionSpace(s, &ci, &gGripSp[h]);
        ci.action = gA[A_AIMPOSE]; N.CreateActionSpace(s, &ci, &gAimSp[h]);
    }
    Log("action sets attached (%u app + ours)", ai->countActionSets);
    return r;
}

static XrResult XRAPI_CALL Hook_xrSyncActions(XrSession s, const XrActionsSyncInfo* si) {
    std::lock_guard l(gMx);
    if (!gAttached || s != gSess || !si) return N.SyncActions(s, si);
    std::vector<XrActiveActionSet> sets(si->activeActionSets, si->activeActionSets + si->countActiveActionSets);
    sets.push_back({ gSet, XR_NULL_PATH });
    XrActionsSyncInfo copy = *si;
    copy.countActiveActionSets = (uint32_t)sets.size(); copy.activeActionSets = sets.data();
    XrResult r = N.SyncActions(s, &copy);
    gSyncIdx++;
    if (r == XR_SUCCESS) ReadInputsAndUpdate();
    CheckConfigChanged();
    return r;
}

static XrResult XRAPI_CALL Hook_xrCreateActionSpace(XrSession s, const XrActionSpaceCreateInfo* ci, XrSpace* sp) {
    XrResult r = N.CreateActionSpace(s, ci, sp);
    if (XR_FAILED(r) || !ci || !sp) return r;
    std::lock_guard l(gMx);
    auto it = gPoseBinds.find(H(ci->action));
    if (it == gPoseBinds.end() || it->second.empty()) return r;
    int hand = -1;
    if (ci->subactionPath == gHandPath[0]) hand = 0;
    else if (ci->subactionPath == gHandPath[1]) hand = 1;
    else if (ci->subactionPath == XR_NULL_PATH) { // no hand given: only if the action is bound to one hand
        hand = it->second[0].hand;
        for (auto& b : it->second) if (b.hand != hand) hand = -1;
    }
    if (hand < 0) { Log("game pose space: hand unknown, offsets not applied"); return r; }
    const PoseBind* bind = nullptr; // the first binding of this action on that hand
    for (auto& b : it->second) if (b.hand == hand) { bind = &b; break; }
    if (!bind) return r;
    PoseKind kind = bind->kind;
    gHandOffsets.AddSpace(*sp, hand, kind);
    const XrPosef& g = ci->poseInActionSpace;
    XrQuaternionf gq = g.orientation; float ga = 2.f * std::acos(std::min(1.f, std::fabs(gq.w))) * 57.2958f;
    float gm = std::sqrt(g.position.x * g.position.x + g.position.y * g.position.y + g.position.z * g.position.z) * 100.f;
    if (ga > 0.5f || gm > 0.1f) // the game moves its hand point itself: our offsets turn about that point, not the controller's
        Log("game pose space: %s %s (the game's own offset: %.1f deg, %.1f cm)", hand ? "right" : "left", kind == PoseKind::Aim ? "aim" : "grip", ga, gm);
    else Log("game pose space: %s %s", hand ? "right" : "left", kind == PoseKind::Aim ? "aim" : "grip");
    return r;
}

static XrResult XRAPI_CALL Hook_xrDestroySpace(XrSpace sp) {
    gHandOffsets.RemoveSpace(sp);
    gHeadSmooth.RemoveViewSpace(sp);
    return N.DestroySpace(sp);
}

// Games that ask for the eye views in their world (most Unity / native games): steadied here.
static XrResult XRAPI_CALL Hook_xrLocateViews(XrSession s, const XrViewLocateInfo* li, XrViewState* vs, uint32_t cap, uint32_t* cnt, XrView* views) {
    XrResult r = N.LocateViews(s, li, vs, cap, cnt, views);
    if (XR_FAILED(r) || !li || !vs || !cnt || !views || cap == 0) return r;
    gHeadSmooth.OnLocateViews(*li, *vs, std::min(*cnt, cap), views);
    return r;
}

static XrResult XRAPI_CALL Hook_xrCreateReferenceSpace(XrSession s, const XrReferenceSpaceCreateInfo* ci, XrSpace* sp) {
    XrResult r = N.CreateReferenceSpace(s, ci, sp);
    if (XR_SUCCEEDED(r) && ci && sp && ci->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW) {
        gHeadSmooth.AddViewSpace(*sp);
        Log("game made a VIEW space (its head): head smoothing steadies it there too");
    }
    return r;
}

static XrResult XRAPI_CALL Hook_xrLocateSpace(XrSpace sp, XrSpace base, XrTime t, XrSpaceLocation* loc) {
    XrResult r = N.LocateSpace(sp, base, t, loc);
    if (XR_FAILED(r) || !loc) return r;
    gHeadSmooth.OnLocateSpace(sp, base, t, loc->locationFlags, loc->pose);
    XrSpaceVelocity* vel = nullptr;
    for (auto* n = (XrBaseOutStructure*)loc->next; n; n = n->next) if (n->type == XR_TYPE_SPACE_VELOCITY) vel = (XrSpaceVelocity*)n;
    gHandOffsets.Apply(sp, base, t, loc->locationFlags, loc->pose, vel ? &vel->velocityFlags : nullptr, vel ? &vel->linearVelocity : nullptr, vel ? &vel->angularVelocity : nullptr);
    return r;
}

static void OffsetLocations(const XrSpacesLocateInfo* li, XrSpaceLocations* sl) {
    if (!li || !sl || !sl->locations) return;
    XrSpaceVelocities* vel = nullptr;
    for (auto* n = (XrBaseOutStructure*)sl->next; n; n = n->next) if (n->type == XR_TYPE_SPACE_VELOCITIES) vel = (XrSpaceVelocities*)n;
    uint32_t n = std::min(li->spaceCount, sl->locationCount);
    for (uint32_t i = 0; i < n; i++) gHeadSmooth.OnLocateSpace(li->spaces[i], li->baseSpace, li->time, sl->locations[i].locationFlags, sl->locations[i].pose);
    for (uint32_t i = 0; i < n; i++) {
        bool hv = vel && vel->velocities && i < vel->velocityCount;
        gHandOffsets.Apply(li->spaces[i], li->baseSpace, li->time, sl->locations[i].locationFlags, sl->locations[i].pose,
            hv ? &vel->velocities[i].velocityFlags : nullptr, hv ? &vel->velocities[i].linearVelocity : nullptr, hv ? &vel->velocities[i].angularVelocity : nullptr);
    }
}
static XrResult XRAPI_CALL Hook_xrLocateSpaces(XrSession s, const XrSpacesLocateInfo* li, XrSpaceLocations* sl) {
    XrResult r = N.LocateSpaces(s, li, sl);
    if (XR_SUCCEEDED(r)) OffsetLocations(li, sl);
    return r;
}
static XrResult XRAPI_CALL Hook_xrLocateSpacesKHR(XrSession s, const XrSpacesLocateInfo* li, XrSpaceLocations* sl) {
    XrResult r = N.LocateSpacesKHR(s, li, sl);
    if (XR_SUCCEEDED(r)) OffsetLocations(li, sl);
    return r;
}

// Work out which hands a query covers: bit 0 left, bit 1 right.
static int HandsMask(XrPath sub) {
    if (sub == XR_NULL_PATH) return 3;
    if (sub == gHandPath[0]) return 1;
    if (sub == gHandPath[1]) return 2;
    return 0;
}

struct Eval { bool any = false, blockedAll = false, blockedSome = false; bool ov = false; Override o; int pf = 0; int freeHand = -1; };
static Eval Evaluate(XrAction a, XrPath sub) {
    Eval e;
    if (!gEng.Active()) return e;
    auto it = gAppBinds.find(H(a));
    if (it == gAppBinds.end()) return e;
    int mask = HandsMask(sub);
    int nb = 0, nblk = 0;
    for (auto& b : it->second) {
        if (!(mask & (1 << b.hand))) continue;
        e.any = true; nb++;
        const Override& o = gEng.Ov(b.hand, b.comp);
        if (o.on && !e.ov) { e.ov = true; e.o = o; e.pf = b.pf; }
        if (gEng.Blocked(b.hand, b.comp)) nblk++;
        else e.freeHand = b.hand;
    }
    e.blockedAll = nb > 0 && nblk == nb;
    e.blockedSome = nblk > 0 && nblk < nb;
    return e;
}

// Keep changedSinceLastSync honest when we alter values.
static bool TrackChange(XrAction a, XrPath sub, float v0, float v1) {
    uint64_t key = H(a) * 1000003ull ^ (uint64_t)sub;
    Track& t = gTrack[key];
    if (t.sync != gSyncIdx) { if (t.sync != ~0ull) { t.prev[0] = t.cur[0]; t.prev[1] = t.cur[1]; } else { t.prev[0] = v0; t.prev[1] = v1; } t.sync = gSyncIdx; }
    t.cur[0] = v0; t.cur[1] = v1;
    return t.prev[0] != v0 || t.prev[1] != v1;
}

static bool CanRequery(XrAction a, int hand) {
    auto it = gActionSubs.find(H(a));
    if (it == gActionSubs.end()) return false;
    for (auto p : it->second) if (p == gHandPath[hand]) return true;
    return false;
}

static XrResult XRAPI_CALL Hook_xrGetActionStateBoolean(XrSession s, const XrActionStateGetInfo* gi, XrActionStateBoolean* st) {
    XrResult r = N.GetActionStateBoolean(s, gi, st);
    if (XR_FAILED(r) || !gi) return r;
    std::lock_guard l(gMx);
    Eval e = Evaluate(gi->action, gi->subactionPath);
    if (!e.any) return r;
    bool v = st->currentState;
    if (e.ov) { v = e.pf == 1 ? e.o.touch : e.o.press; st->isActive = XR_TRUE; }
    else if (e.blockedAll) v = false;
    else if (e.blockedSome && gi->subactionPath == XR_NULL_PATH) {
        if (e.freeHand >= 0 && CanRequery(gi->action, e.freeHand)) {
            XrActionStateGetInfo g2 = *gi; g2.subactionPath = gHandPath[e.freeHand];
            XrActionStateBoolean s2{ XR_TYPE_ACTION_STATE_BOOLEAN };
            if (XR_SUCCEEDED(N.GetActionStateBoolean(s, &g2, &s2))) v = s2.currentState; else v = false;
        } else v = false;
    }
    st->currentState = v;
    bool ch = TrackChange(gi->action, gi->subactionPath, v ? 1.f : 0.f, 0);
    st->changedSinceLastSync = ch;
    if (ch) st->lastChangeTime = gPredTime;
    return r;
}

static XrResult XRAPI_CALL Hook_xrGetActionStateFloat(XrSession s, const XrActionStateGetInfo* gi, XrActionStateFloat* st) {
    XrResult r = N.GetActionStateFloat(s, gi, st);
    if (XR_FAILED(r) || !gi) return r;
    std::lock_guard l(gMx);
    Eval e = Evaluate(gi->action, gi->subactionPath);
    if (!e.any) return r;
    float v = st->currentState;
    if (e.ov) {
        if (e.pf == 3) v = e.o.x; else if (e.pf == 4) v = e.o.y; else if (e.pf == 1) v = e.o.touch ? 1.f : 0.f; else v = e.o.value;
        st->isActive = XR_TRUE;
    } else if (e.blockedAll) v = 0;
    else if (e.blockedSome && gi->subactionPath == XR_NULL_PATH) {
        if (e.freeHand >= 0 && CanRequery(gi->action, e.freeHand)) {
            XrActionStateGetInfo g2 = *gi; g2.subactionPath = gHandPath[e.freeHand];
            XrActionStateFloat s2{ XR_TYPE_ACTION_STATE_FLOAT };
            v = XR_SUCCEEDED(N.GetActionStateFloat(s, &g2, &s2)) ? s2.currentState : 0.f;
        } else v = 0;
    }
    st->currentState = v;
    bool ch = TrackChange(gi->action, gi->subactionPath, v, 0);
    st->changedSinceLastSync = ch;
    if (ch) st->lastChangeTime = gPredTime;
    return r;
}

static XrResult XRAPI_CALL Hook_xrGetActionStateVector2f(XrSession s, const XrActionStateGetInfo* gi, XrActionStateVector2f* st) {
    XrResult r = N.GetActionStateVector2f(s, gi, st);
    if (XR_FAILED(r) || !gi) return r;
    std::lock_guard l(gMx);
    Eval e = Evaluate(gi->action, gi->subactionPath);
    if (!e.any) return r;
    XrVector2f v = st->currentState;
    if (e.ov) { v = { e.o.x, e.o.y }; st->isActive = XR_TRUE; }
    else if (e.blockedAll) v = { 0, 0 };
    else if (e.blockedSome && gi->subactionPath == XR_NULL_PATH) {
        if (e.freeHand >= 0 && CanRequery(gi->action, e.freeHand)) {
            XrActionStateGetInfo g2 = *gi; g2.subactionPath = gHandPath[e.freeHand];
            XrActionStateVector2f s2{ XR_TYPE_ACTION_STATE_VECTOR2F };
            v = XR_SUCCEEDED(N.GetActionStateVector2f(s, &g2, &s2)) ? s2.currentState : XrVector2f{ 0, 0 };
        } else v = { 0, 0 };
    }
    st->currentState = v;
    bool ch = TrackChange(gi->action, gi->subactionPath, v.x, v.y);
    st->changedSinceLastSync = ch;
    if (ch) st->lastChangeTime = gPredTime;
    return r;
}

static XrResult XRAPI_CALL Hook_xrWaitFrame(XrSession s, const XrFrameWaitInfo* wi, XrFrameState* fs) {
    XrResult r = N.WaitFrame(s, wi, fs);
    if (XR_SUCCEEDED(r) && fs) { std::lock_guard l(gMx); gPredTime = fs->predictedDisplayTime; }
    return r;
}

static void MirrorFrame(const std::vector<const XrCompositionLayerBaseHeader*>& layers, XrTime t) {
    gMirror.Frame(gGfx.d11, gGfx.ctx, layers, t, [](XrSpace sp, XrSpace base, XrTime tt, XrPosef& out) {
        XrSpaceLocation l{ XR_TYPE_SPACE_LOCATION };
        if (XR_FAILED(N.LocateSpace(sp, base, tt, &l))) return false; // raw: our own spaces, never the game's offsets
        const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if ((l.locationFlags & need) != need) return false;
        out = l.pose; return true;
    });
}

// Swapchains (the game's and ours): the mirror needs their images and which one was shown last.
static XrResult XRAPI_CALL Hook_xrCreateSwapchain(XrSession s, const XrSwapchainCreateInfo* ci, XrSwapchain* sc) {
    XrResult r = gSwapNext.CreateSwapchain(s, ci, sc);
    if (XR_SUCCEEDED(r) && ci && sc) gMirror.OnCreate(*sc, ci);
    return r;
}
static XrResult XRAPI_CALL Hook_xrDestroySwapchain(XrSwapchain sc) {
    gMirror.OnDestroy(sc);
    return gSwapNext.DestroySwapchain(sc);
}
static XrResult XRAPI_CALL Hook_xrEnumerateSwapchainImages(XrSwapchain sc, uint32_t cap, uint32_t* n, XrSwapchainImageBaseHeader* imgs) {
    XrResult r = gSwapNext.EnumerateSwapchainImages(sc, cap, n, imgs);
    if (XR_SUCCEEDED(r) && imgs && n && cap) gMirror.OnImages(sc, imgs, *n);
    return r;
}
static XrResult XRAPI_CALL Hook_xrAcquireSwapchainImage(XrSwapchain sc, const XrSwapchainImageAcquireInfo* ai, uint32_t* idx) {
    XrResult r = gSwapNext.AcquireSwapchainImage(sc, ai, idx);
    if (XR_SUCCEEDED(r) && idx) gMirror.OnAcquire(sc, *idx);
    return r;
}
static XrResult XRAPI_CALL Hook_xrReleaseSwapchainImage(XrSwapchain sc, const XrSwapchainImageReleaseInfo* ri) {
    XrResult r = gSwapNext.ReleaseSwapchainImage(sc, ri);
    if (XR_SUCCEEDED(r)) gMirror.OnRelease(sc);
    return r;
}

static XrResult XRAPI_CALL Hook_xrEndFrame(XrSession s, const XrFrameEndInfo* fi) {
    // The lock covers our work only, never the runtime's xrEndFrame: that can wait for the compositor
    // for milliseconds, and the game's input thread (xrSyncActions, xrGetActionState*) needs this lock.
    std::unique_lock l(gMx);
    if (s == gSess) fi = gHeadSmooth.OnEndFrame(fi);
    if (s != gSess || !fi || !gEng.Active() || !gGfx.api) {
        if (s == gSess && fi && gMirror.On() && gGfx.api == 11) MirrorFrame(std::vector<const XrCompositionLayerBaseHeader*>(fi->layers, fi->layers + fi->layerCount), fi->displayTime);
        l.unlock();
        return N.EndFrame(s, fi);
    }
    LayerList layers(fi->layers, fi->layers + fi->layerCount);
    gOverlay.Render(OverlayCtx(), layers);
    if (gMirror.On() && gGfx.api == 11) MirrorFrame(layers, fi->displayTime);
    XrFrameEndInfo copy = *fi;
    copy.layerCount = (uint32_t)layers.size();
    copy.layers = layers.data();
    l.unlock();
    return N.EndFrame(s, &copy);
}

// ---------------------------------------------------------------- dispatch
static XrResult XRAPI_CALL Hook_xrGetInstanceProcAddr(XrInstance inst, const char* name, PFN_xrVoidFunction* fn) {
    XrResult r = gNextGipa(inst, name, fn);
    if (XR_FAILED(r) || !name) return r;
    std::string n = name;
#define HOOK(x) if (n == "xr" #x) { *fn = (PFN_xrVoidFunction)Hook_xr##x; return r; }
    HOOK(GetInstanceProcAddr) HOOK(DestroyInstance) HOOK(CreateSession) HOOK(DestroySession)
    HOOK(CreateActionSet) HOOK(CreateAction) HOOK(SuggestInteractionProfileBindings) HOOK(AttachSessionActionSets)
    HOOK(SyncActions) HOOK(GetActionStateBoolean) HOOK(GetActionStateFloat) HOOK(GetActionStateVector2f)
    HOOK(WaitFrame) HOOK(EndFrame)
    HOOK(CreateActionSpace) HOOK(CreateReferenceSpace) HOOK(DestroySpace) HOOK(LocateSpace) HOOK(LocateViews)
    HOOK(CreateSwapchain) HOOK(DestroySwapchain) HOOK(EnumerateSwapchainImages) HOOK(AcquireSwapchainImage) HOOK(ReleaseSwapchainImage)
    if (n == "xrLocateSpaces") { N.LocateSpaces = (PFN_xrLocateSpaces)*fn; *fn = (PFN_xrVoidFunction)Hook_xrLocateSpaces; return r; }
    if (n == "xrLocateSpacesKHR") { N.LocateSpacesKHR = (PFN_xrLocateSpacesKHR)*fn; *fn = (PFN_xrVoidFunction)Hook_xrLocateSpacesKHR; return r; }
#undef HOOK
    return r;
}

static XrResult XRAPI_CALL Hook_xrCreateApiLayerInstance(const XrInstanceCreateInfo* info, const XrApiLayerCreateInfo* li, XrInstance* inst) {
    LogInit();
    if (!li || !li->nextInfo) return XR_ERROR_INITIALIZATION_FAILED;
    gOverlay.onProfileSaved = MarkOwnSave;
    gNextGipa = li->nextInfo->nextGetInstanceProcAddr;
    XrApiLayerCreateInfo chain = *li;
    chain.nextInfo = li->nextInfo->next;
    XrResult r = li->nextInfo->nextCreateApiLayerInstance(info, &chain, inst);
    if (XR_FAILED(r)) return r;
    gInst = *inst;
    auto get = [&](const char* nm, auto& f) { gNextGipa(gInst, nm, (PFN_xrVoidFunction*)&f); };
    get("xrDestroyInstance", N.DestroyInstance); get("xrCreateSession", N.CreateSession); get("xrDestroySession", N.DestroySession);
    get("xrCreateActionSet", N.CreateActionSet); get("xrCreateAction", N.CreateAction);
    get("xrSuggestInteractionProfileBindings", N.SuggestInteractionProfileBindings);
    get("xrAttachSessionActionSets", N.AttachSessionActionSets); get("xrSyncActions", N.SyncActions);
    get("xrGetActionStateBoolean", N.GetActionStateBoolean); get("xrGetActionStateFloat", N.GetActionStateFloat);
    get("xrGetActionStateVector2f", N.GetActionStateVector2f);
    get("xrWaitFrame", N.WaitFrame); get("xrEndFrame", N.EndFrame);
    get("xrStringToPath", N.StringToPath); get("xrPathToString", N.PathToString);
    get("xrCreateActionSpace", N.CreateActionSpace); get("xrCreateReferenceSpace", N.CreateReferenceSpace);
    get("xrDestroySpace", N.DestroySpace); get("xrLocateSpace", N.LocateSpace); get("xrLocateViews", N.LocateViews);
    get("xrApplyHapticFeedback", N.ApplyHapticFeedback);
    get("xrGetCurrentInteractionProfile", N.GetCurrentInteractionProfile);
    get("xrEnumerateSwapchainFormats", gSwapNext.EnumerateSwapchainFormats); get("xrCreateSwapchain", gSwapNext.CreateSwapchain);
    get("xrDestroySwapchain", gSwapNext.DestroySwapchain); get("xrEnumerateSwapchainImages", gSwapNext.EnumerateSwapchainImages);
    get("xrAcquireSwapchainImage", gSwapNext.AcquireSwapchainImage); get("xrWaitSwapchainImage", gSwapNext.WaitSwapchainImage);
    get("xrReleaseSwapchainImage", gSwapNext.ReleaseSwapchainImage);
    gSwap = gSwapNext; // our panels go through the hooks too, so the mirror can draw them
    gSwap.CreateSwapchain = Hook_xrCreateSwapchain; gSwap.DestroySwapchain = Hook_xrDestroySwapchain;
    gSwap.EnumerateSwapchainImages = Hook_xrEnumerateSwapchainImages;
    gSwap.AcquireSwapchainImage = Hook_xrAcquireSwapchainImage; gSwap.ReleaseSwapchainImage = Hook_xrReleaseSwapchainImage;
    LoadMirrorSettings(gMirrorSet);
    CheckHeadSmoothingFile();
    Log("instance created for '%s' (%s), OpenXR Companion layer built %s %s", info->applicationInfo.applicationName, ExeName().c_str(), __DATE__, __TIME__);
    Log("head smoothing at start: %s", gHeadSmooth.StateText().c_str());
    return r;
}

extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* li, const char* layerName, XrNegotiateApiLayerRequest* req) {
    if (!li || !req || li->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        req->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        li->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION || li->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION)
        return XR_ERROR_INITIALIZATION_FAILED;
    if (layerName && strcmp(layerName, LAYER_NAME) != 0) return XR_ERROR_INITIALIZATION_FAILED;
    req->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    req->layerApiVersion = XR_CURRENT_API_VERSION;
    req->getInstanceProcAddr = Hook_xrGetInstanceProcAddr;
    req->createApiLayerInstance = Hook_xrCreateApiLayerInstance;
    return XR_SUCCESS;
}
