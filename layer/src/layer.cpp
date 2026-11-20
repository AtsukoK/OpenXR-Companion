// OpenXR Companion - OpenXR API layer.
// Adds a configurable radial menu, chords and layers on top of any OpenXR game.
#include "panel.h"
#include "screen.h"
#include "keys.h"
#include "mirror.h"
#include "tracking_log.h"
#include "head_smoothing.h"
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
static Panel gWheelPanel, gToastPanel, gKbPanel, gWristPanel[2], gCursorPanel[2];
static uint64_t gDrawnWrist[2] = { ~0ull, ~0ull };
static int gDrawnCursor[2] = { -1, -1 };
static Panel gDotPanel, gBeamPanel; static bool gDrawnPointer = false;
static Panel gFingerPanel; static bool gDrawnFinger = false;
static uint64_t gWinAsk = 0; static std::string gPickedExe, gPickedTitle;
static void MarkOwnSave();
static Panel gBarPanel, gPickPanel; static uint64_t gDrawnBar = ~0ull, gDrawnPick = ~0ull;
static Panel gScreenPanel; static ScreenCapture gCap; static std::vector<uint32_t> gScreenPix; static uint64_t gScreenMsgVer = 0;
static uint64_t gDrawnKb = ~0ull;
static Panel gKbBarPanel; static uint64_t gDrawnKbBar = ~0ull;
static uint64_t gDrawnWheel = ~0ull, gDrawnToast = ~0ull;
static int gWheelTex = 0;
static std::vector<uint32_t> gPix;

// ---------------------------------------------------------------- controller offsets for the game
// The game's hand pose actions (from its suggested bindings) and the spaces it makes from them.
// Every pose the game locates for those spaces is turned / moved by the profile's offsets.
struct PoseBind { int hand, kind; };   // kind 0 grip, 1 aim
static std::unordered_map<uint64_t, std::vector<PoseBind>> gPoseBinds; // action -> bindings (under gMx)
struct GameSpace { int hand, kind; };
struct OffSnap { bool on = false; bool has[2][2]{}; XrPosef o[2][2]{}; float smooth = 0; };
static std::mutex gOffMx;                  // own lock: the game may locate from its render thread
static std::unordered_map<uint64_t, GameSpace> gGameSpaces;
static OffSnap gOff;
struct SpaceSmooth { SpaceSteady st; };
static std::map<std::pair<uint64_t, uint64_t>, SpaceSmooth> gSpaceSmooth;
static Panel gOffPanel; static uint64_t gDrawnOff = ~0ull;

static bool ParsePosePath(const std::string& s, PoseBind& b) {
    const std::string pre[2] = { "/user/hand/left/input/", "/user/hand/right/input/" };
    for (int h = 0; h < 2; h++) {
        if (s.rfind(pre[h], 0) != 0) continue;
        std::string rest = s.substr(pre[h].size());
        if (rest == "grip/pose" || rest == "grip_surface/pose" || rest == "palm_ext/pose") { b = { h, 0 }; return true; }
        if (rest == "aim/pose") { b = { h, 1 }; return true; }
        return false;
    }
    return false;
}

static TrackingLog gTrackLog; // under gOffMx

// Move one located pose: space' = space * Os, base' = base * Ob  ->  pose' = inv(Ob) * pose * Os.
static void OffsetLocation(XrSpace space, XrSpace base, XrTime t, XrSpaceLocationFlags& flags, XrPosef& pose, XrSpaceVelocityFlags* vflags, XrVector3f* lin, XrVector3f* ang) {
    std::lock_guard l(gOffMx);
    {
        auto is = gGameSpaces.find((uint64_t)space);
        if (is != gGameSpaces.end() && !gGameSpaces.count((uint64_t)base)) gTrackLog.Read(is->second.hand, is->second.kind, t, flags, pose);
        gTrackLog.Summary(NowSec());
    }
    if (!(flags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) return;
    if (!gOff.on || gGameSpaces.empty()) return;
    auto is = gGameSpaces.find((uint64_t)space), ib = gGameSpaces.find((uint64_t)base);
    bool hs = is != gGameSpaces.end() && gOff.has[is->second.hand][is->second.kind];
    bool hb = ib != gGameSpaces.end() && gOff.has[ib->second.hand][ib->second.kind];
    bool smooth = is != gGameSpaces.end() && ib == gGameSpaces.end() && gOff.smooth > 0.001f;
    if (!hs && !hb && !smooth) return;
    XrPosef p = pose;
    if (hs) {
        const XrPosef& o = gOff.o[is->second.hand][is->second.kind];
        // the offset point moves on a lever: its speed gains angular x lever arm
        if (!hb && lin && ang && vflags && (*vflags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) && (*vflags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT)) {
            Vec3 arm = Rotate(pose.orientation, V(o.position));
            *lin = X(V(*lin) + Cross(V(*ang), arm));
        }
        p = PMul(p, o);
    }
    if (hb) p = PMul(PInv(gOff.o[ib->second.hand][ib->second.kind]), p);
    if (smooth) {
        SpaceSmooth& ss = gSpaceSmooth[{ (uint64_t)space, (uint64_t)base }];
        p = ss.st.Step(t, p, flags, gOff.smooth); // untracked poses pass through and restart the filter
    }
    if (is != gGameSpaces.end() && ib == gGameSpaces.end()) gTrackLog.Moved(is->second.hand, is->second.kind, pose, p);
    pose = p;
}

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
    gDrawnWheel = gDrawnKb = ~0ull; gDrawnWrist[0] = gDrawnWrist[1] = ~0ull; gDrawnPointer = false; gDrawnFinger = false; gDrawnCursor[0] = gDrawnCursor[1] = -1;
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

static XrPosef gHeadNow{ {0,0,0,1}, {0,0,0} }; static bool gHeadNowValid = false; // head in gLocal, last update

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
    gHeadNow = f.head; gHeadNowValid = f.headValid;
    gEng.Update(f);

    for (auto& hq : gEng.haptics) {
        XrHapticActionInfo hi{ XR_TYPE_HAPTIC_ACTION_INFO }; hi.action = gA[A_HAPTIC]; hi.subactionPath = gHandPath[hq.hand];
        XrHapticVibration v{ XR_TYPE_HAPTIC_VIBRATION };
        v.amplitude = hq.amp; v.duration = (XrDuration)(hq.sec * 1e9); v.frequency = XR_FREQUENCY_UNSPECIFIED;
        N.ApplyHapticFeedback(gSess, &hi, (XrHapticBaseHeader*)&v);
    }
    gEng.haptics.clear();
    {   // what the game's controller poses are moved by, for the LocateSpace hooks
        OffSnap sn;
        sn.on = gEng.Active() && gEng.offs.enabled;
        sn.smooth = sn.on ? gEng.offs.smoothing : 0.f;
        for (int h = 0; h < 2; h++) for (int k = 0; k < 2; k++) sn.has[h][k] = sn.on && gEng.OffsetPose(h, k, sn.o[h][k]);
        std::lock_guard o(gOffMx);
        gOff = sn;
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
        if (ParsePosePath(bp, pb)) {
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

static void DestroySessionStuff() {
    gWheelPanel.Destroy(gSwap); gToastPanel.Destroy(gSwap); gKbPanel.Destroy(gSwap); gDrawnKb = ~0ull; gKbBarPanel.Destroy(gSwap); gDrawnKbBar = ~0ull; for (int h = 0; h < 2; h++) { gWristPanel[h].Destroy(gSwap); gDrawnWrist[h] = ~0ull; gCursorPanel[h].Destroy(gSwap); gDrawnCursor[h] = -1; } gDotPanel.Destroy(gSwap); gBeamPanel.Destroy(gSwap); gDrawnPointer = false; gFingerPanel.Destroy(gSwap); gDrawnFinger = false; gScreenPanel.Destroy(gSwap); gCap.Stop(); gBarPanel.Destroy(gSwap); gPickPanel.Destroy(gSwap); gDrawnBar = gDrawnPick = ~0ull; gOffPanel.Destroy(gSwap); gDrawnOff = ~0ull;
    { std::lock_guard o(gOffMx); gGameSpaces.clear(); gSpaceSmooth.clear(); gOff = OffSnap{}; }
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
    gDrawnWheel = gDrawnToast = ~0ull;
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
    int kind = -1;
    for (auto& b : it->second) if (b.hand == hand && kind < 0) kind = b.kind;
    if (kind < 0) return r;
    { std::lock_guard o(gOffMx); gGameSpaces[(uint64_t)*sp] = { hand, kind }; }
    const XrPosef& g = ci->poseInActionSpace;
    XrQuaternionf gq = g.orientation; float ga = 2.f * std::acos(std::min(1.f, std::fabs(gq.w))) * 57.2958f;
    float gm = std::sqrt(g.position.x * g.position.x + g.position.y * g.position.y + g.position.z * g.position.z) * 100.f;
    if (ga > 0.5f || gm > 0.1f) // the game moves its hand point itself: our offsets turn about that point, not the controller's
        Log("game pose space: %s %s (the game's own offset: %.1f deg, %.1f cm)", hand ? "right" : "left", kind ? "aim" : "grip", ga, gm);
    else Log("game pose space: %s %s", hand ? "right" : "left", kind ? "aim" : "grip");
    return r;
}

static XrResult XRAPI_CALL Hook_xrDestroySpace(XrSpace sp) {
    {
        std::lock_guard o(gOffMx);
        gGameSpaces.erase((uint64_t)sp);
        for (auto it = gSpaceSmooth.begin(); it != gSpaceSmooth.end();)
            if (it->first.first == (uint64_t)sp || it->first.second == (uint64_t)sp) it = gSpaceSmooth.erase(it); else ++it;
    }
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
    OffsetLocation(sp, base, t, loc->locationFlags, loc->pose, vel ? &vel->velocityFlags : nullptr, vel ? &vel->linearVelocity : nullptr, vel ? &vel->angularVelocity : nullptr);
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
        OffsetLocation(li->spaces[i], li->baseSpace, li->time, sl->locations[i].locationFlags, sl->locations[i].pose,
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
    std::vector<const XrCompositionLayerBaseHeader*> layers(fi->layers, fi->layers + fi->layerCount);
    XrCompositionLayerQuad kbq{ XR_TYPE_COMPOSITION_LAYER_QUAD }, wq{ XR_TYPE_COMPOSITION_LAYER_QUAD }, tq{ XR_TYPE_COMPOSITION_LAYER_QUAD }, kq{ XR_TYPE_COMPOSITION_LAYER_QUAD }, rq[2]{ { XR_TYPE_COMPOSITION_LAYER_QUAD }, { XR_TYPE_COMPOSITION_LAYER_QUAD } }, cq[2]{ { XR_TYPE_COMPOSITION_LAYER_QUAD }, { XR_TYPE_COMPOSITION_LAYER_QUAD } }, pq[4]{ { XR_TYPE_COMPOSITION_LAYER_QUAD }, { XR_TYPE_COMPOSITION_LAYER_QUAD }, { XR_TYPE_COMPOSITION_LAYER_QUAD }, { XR_TYPE_COMPOSITION_LAYER_QUAD } }, fq[2]{ { XR_TYPE_COMPOSITION_LAYER_QUAD }, { XR_TYPE_COMPOSITION_LAYER_QUAD } };
    const Profile& p = *gEng.prof;
    XrCompositionLayerQuad sq{ XR_TYPE_COMPOSITION_LAYER_QUAD };

    // Mirrored desktop monitor, under the menus. Captured on its own thread while it is on.
    if (gEng.screen.shown && gLocal) {
        const ScreenStyle& S = p.screen;
        int maxW = S.maxPx > 0 ? S.maxPx : (sizeof(void*) == 4 ? 1280 : 1920); // 32-bit games are short on memory
        ScreenView& sv = gEng.screen;
        if (sv.source == "window" && (sv.windowAsk != gWinAsk || !IsWindow((HWND)sv.hwnd) || gCap.WindowGone())) {
            bool asked = sv.windowAsk != gWinAsk; gWinAsk = sv.windowAsk;
            auto wins = ScreenCapture::Windows();
            auto low = [](std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; };
            int cur = -1;
            for (int i = 0; i < (int)wins.size(); i++) if (wins[i].hwnd == sv.hwnd) cur = i;
            int pick = -1;
            if (!sv.windowTitle.empty()) // the window picked last time, if it's still open with the same title
                for (int i = 0; i < (int)wins.size() && pick < 0; i++) if (wins[i].title == sv.windowTitle) pick = i;
            if (pick >= 0) {}
            else if (!sv.windowMatch.empty()) {
                std::string m = low(sv.windowMatch);
                for (int i = 0; i < (int)wins.size() && pick < 0; i++) if (low(wins[i].title).find(m) != std::string::npos || low(wins[i].exe).find(m) != std::string::npos) pick = i;
                if (pick < 0) gEng.Toast("Screen: no open window matches '" + sv.windowMatch + "'", 3.0);
            } else if (!wins.empty()) {
                int step = sv.windowStep ? sv.windowStep : 0;
                pick = cur < 0 ? (step < 0 ? (int)wins.size() - 1 : 0) : ((cur + step) % (int)wins.size() + (int)wins.size()) % (int)wins.size();
            }
            sv.windowStep = 0; sv.windowMatch.clear(); sv.windowTitle.clear();
            if (pick < 0) { // nothing to show: back to the monitor
                if (wins.empty()) gEng.Toast("Screen: no window to show", 2.5);
                sv.source = "monitor"; sv.hwnd = 0;
            } else {
                bool changed = wins[pick].hwnd != sv.hwnd;
                sv.hwnd = wins[pick].hwnd;
                if (asked || changed) gEng.Toast("Screen: " + (wins[pick].title.size() > 48 ? wins[pick].title.substr(0, 45) + "..." : wins[pick].title), 2.0);
                Log("screen window: %s (%s)", wins[pick].title.c_str(), wins[pick].exe.c_str());
                std::string exe = low(wins[pick].exe); size_t dot = exe.rfind('.'); if (dot != std::string::npos) exe = exe.substr(0, dot);
                gPickedExe = exe; gPickedTitle = wins[pick].title;
            }
        }
        // Picked in the headset: that monitor / window is the default from now on.
        if (sv.choiceChanged && sv.windowAsk == gWinAsk) {
            sv.choiceChanged = false;
            ScreenStyle& PS = gEng.prof->screen;
            PS.source = sv.source;
            if (sv.source == "window") { PS.window = gPickedExe; PS.windowTitle = gPickedTitle; }
            else PS.monitor = sv.monitor;
            std::string err;
            if (SaveScreenChoice(*gEng.prof, err)) { MarkOwnSave(); Log("screen: remembered %s", sv.source == "window" ? gPickedTitle.c_str() : ("monitor " + std::to_string(sv.monitor)).c_str()); }
            else Log("screen: could not remember the choice: %s", err.c_str());
        }
        if (sv.source != "window") sv.hwnd = 0;
        if (sv.wantChoices) { // picker list: monitors, then open windows
            sv.wantChoices = false;
            std::vector<ScreenChoice> ch;
            auto mons = ScreenCapture::Monitors();
            for (int i = 0; i < (int)mons.size(); i++) {
                ScreenChoice c; c.monitor = i + 1;
                c.label = "Monitor " + std::to_string(i + 1) + (mons[i].primary ? " (main)" : "") + "\n" + std::to_string(mons[i].w) + " x " + std::to_string(mons[i].h);
                int curMon = sv.monitor == 0 ? sv.primaryIndex : sv.monitor;
                c.current = sv.source != "window" && curMon == i + 1;
                ch.push_back(c);
            }
            for (auto& w : ScreenCapture::Windows()) {
                ScreenChoice c; c.hwnd = w.hwnd;
                std::string exe = w.exe; size_t dot = exe.rfind('.'); if (dot != std::string::npos) exe = exe.substr(0, dot);
                std::string t = w.title.size() > 60 ? w.title.substr(0, 57) + "..." : w.title;
                c.label = exe + "\n" + t;
                c.current = sv.source == "window" && sv.hwnd == w.hwnd;
                ch.push_back(c);
            }
            sv.choices = std::move(ch); sv.choicesVersion++;
        }
        gCap.Configure(sv.monitor, sv.hwnd, sv.fps, maxW, S.cursor, S.opacity);
        int n, prim, shown; std::string msg; uint64_t mv;
        gCap.Status(n, prim, shown, msg, mv);
        if (n > 0) { gEng.screen.monitorCount = n; gEng.screen.primaryIndex = prim; }
        gCap.Rect(gEng.screen.monX, gEng.screen.monY, gEng.screen.monW, gEng.screen.monH);
        if (gScreenPanel.w > 0) gEng.screen.aspect = (float)gScreenPanel.h / gScreenPanel.w;
        if (mv != gScreenMsgVer) {
            gScreenMsgVer = mv;
            if (msg.find("can't") != std::string::npos || msg.find("no monitor") != std::string::npos) gEng.Toast("Screen: " + msg, 4.0);
        }
        int fw, fh;
        if (gCap.TakeFrame(gScreenPix, fw, fh)) {
            if (gScreenPanel.sc && (gScreenPanel.w != fw || gScreenPanel.h != fh)) gScreenPanel.Destroy(gSwap);
            if (!gScreenPanel.sc) gScreenPanel.Create(gSess, gGfx, gSwap, fw, fh);
            if (gScreenPanel.sc) gScreenPanel.Upload(gGfx, gSwap, gScreenPix);
        }
        // a quad layer shows from both sides: hide the screen when the head is behind it
        bool screenFront = true;
        if (gHeadNowValid) {
            const XrPosef& sp = gEng.screen.pose;
            screenFront = Dot(Rotate(sp.orientation, { 0, 0, 1 }), V(gHeadNow.position) - V(sp.position)) > 0.f;
        }
        if (gScreenPanel.ready && gEng.screen.poseValid && gScreenPanel.w > 0 && screenFront) {
            sq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            sq.space = gLocal;
            sq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            sq.subImage.swapchain = gScreenPanel.sc;
            sq.subImage.imageRect = { {0, 0}, {gScreenPanel.w, gScreenPanel.h} };
            sq.pose = gEng.screen.pose;
            sq.size = { gEng.screen.widthM, gEng.screen.widthM * gScreenPanel.h / gScreenPanel.w };
            layers.push_back((const XrCompositionLayerBaseHeader*)&sq);
        }
        // button bar under the screen and the picker in front of it
        static XrCompositionLayerQuad bq{ XR_TYPE_COMPOSITION_LAYER_QUAD }, kq2{ XR_TYPE_COMPOSITION_LAYER_QUAD };
        auto drawPanel = [&](Panel& pn, uint64_t& drawn, int W, int H, const std::string& title, const std::vector<std::string>& labels, int cols, int hover, int pressed,
                             XrCompositionLayerQuad& q, const XrPosef& pose, float wM, float hM) {
            if (pn.sc && (pn.w != W || pn.h != H)) { pn.Destroy(gSwap); drawn = ~0ull; }
            if (!pn.sc) pn.Create(gSess, gGfx, gSwap, W, H);
            if (pn.sc && drawn != gEng.screen.uiVersion) { std::vector<uint32_t> px; DrawList(px, W, H, title, labels, cols, hover, pressed, p.style); if (pn.Upload(gGfx, gSwap, px)) drawn = gEng.screen.uiVersion; }
            if (!pn.ready) return;
            q = XrCompositionLayerQuad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
            q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; q.space = gLocal; q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            q.subImage.swapchain = pn.sc; q.subImage.imageRect = { {0, 0}, {W, H} };
            q.pose = pose; q.size = { wM, hM };
            layers.push_back((const XrCompositionLayerBaseHeader*)&q);
        };
        if (sv.barVisible && sv.poseValid && screenFront)
            drawPanel(gBarPanel, gDrawnBar, 1200, (int)(1200 * sv.barH / sv.barW), "", sv.barLabels, (int)sv.barLabels.size(), sv.barHover, sv.barPressed, bq, sv.barPose, sv.barW, sv.barH);
        if (sv.pickOpen && sv.poseValid && screenFront)
            drawPanel(gPickPanel, gDrawnPick, 1000, (int)(1000 * sv.pickH / sv.pickW), sv.pickTitle, sv.pickLabels, sv.pickCols, sv.pickHover, sv.pickPressed, kq2, sv.pickPose, sv.pickW, sv.pickH);
    } else if (gCap.Running() || gScreenPanel.sc) {
        gCap.Stop(); gScreenPanel.Destroy(gSwap); gScreenPix.clear(); gScreenPix.shrink_to_fit();
        gBarPanel.Destroy(gSwap); gPickPanel.Destroy(gSwap); gDrawnBar = gDrawnPick = ~0ull;
    }

    if (gEng.view.visible && gLocal) {
        int tex = p.style.texSize;
        if (gWheelPanel.sc && gWheelTex != tex) { gWheelPanel.Destroy(gSwap); gDrawnWheel = ~0ull; }
        if (!gWheelPanel.sc && gWheelPanel.Create(gSess, gGfx, gSwap, tex, tex)) gWheelTex = tex;
        if (gWheelPanel.sc && gDrawnWheel != gEng.view.version) {
            DrawWheel(gPix, tex, p, gEng.view);
            if (gEng.view.alpha < 0.999f) { // see-through (premultiplied: scale every channel)
                uint32_t a = (uint32_t)(std::min(std::max(gEng.view.alpha, 0.f), 1.f) * 256.f);
                for (auto& px : gPix) {
                    uint32_t rb = ((px & 0x00FF00FFu) * a >> 8) & 0x00FF00FFu, ga = (((px >> 8) & 0x00FF00FFu) * a) & 0xFF00FF00u;
                    px = rb | ga;
                }
            }
            if (gWheelPanel.Upload(gGfx, gSwap, gPix)) gDrawnWheel = gEng.view.version;
        }
        if (gWheelPanel.ready) {
            wq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            wq.space = gLocal;
            wq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            wq.subImage.swapchain = gWheelPanel.sc;
            wq.subImage.imageRect = { {0, 0}, {gWheelPanel.w, gWheelPanel.h} };
            wq.pose = gEng.view.pose;
            wq.size = { gEng.view.size, gEng.view.size };
            layers.push_back((const XrCompositionLayerBaseHeader*)&wq);
        }
    }
    if (gEng.kb.visible && gLocal) {
        const int KW = 1500;
        float uw, uh; KbLayout(uw, uh);
        const int KH = (int)(KW * uh / uw);
        if (!gKbPanel.sc) gKbPanel.Create(gSess, gGfx, gSwap, KW, KH);
        if (gKbPanel.sc && gDrawnKb != gEng.kb.version) {
            DrawKeyboard(gPix, KW, KH, gEng.kb, p.style);
            if (gKbPanel.Upload(gGfx, gSwap, gPix)) gDrawnKb = gEng.kb.version;
        }
        if (gKbPanel.ready) {
            kq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            kq.space = gLocal;
            kq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            kq.subImage.swapchain = gKbPanel.sc;
            kq.subImage.imageRect = { {0, 0}, {KW, KH} };
            kq.pose = gEng.kb.pose;
            kq.size = { gEng.kb.widthM, gEng.kb.heightM };
            layers.push_back((const XrCompositionLayerBaseHeader*)&kq);
        }
        { // the docking bar under it
            const KeyboardView& K = gEng.kb;
            const int BW = 1200, BH = std::max(40, (int)(BW * K.barH / K.barW));
            if (gKbBarPanel.sc && (gKbBarPanel.w != BW || gKbBarPanel.h != BH)) { gKbBarPanel.Destroy(gSwap); gDrawnKbBar = ~0ull; }
            if (!gKbBarPanel.sc) gKbBarPanel.Create(gSess, gGfx, gSwap, BW, BH);
            if (gKbBarPanel.sc && gDrawnKbBar != K.barVersion && !K.barLabels.empty()) {
                std::vector<uint32_t> px; DrawList(px, BW, BH, "", K.barLabels, (int)K.barLabels.size(), K.barHover, K.barPressed, p.style);
                if (gKbBarPanel.Upload(gGfx, gSwap, px)) gDrawnKbBar = K.barVersion;
            }
            if (gKbBarPanel.ready) {
                kbq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; kbq.space = gLocal; kbq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                kbq.subImage.swapchain = gKbBarPanel.sc; kbq.subImage.imageRect = { {0, 0}, {BW, BH} };
                kbq.pose = K.barPose; kbq.size = { K.barW, K.barH };
                layers.push_back((const XrCompositionLayerBaseHeader*)&kbq);
            }
        }
    }
    for (int h = 0; h < 2; h++) {
        const WristView& wv = gEng.wrist[h];
        if (!wv.visible || !gLocal) continue;
        const int WS = 512;
        Panel& pn = gWristPanel[h];
        if (!pn.sc) pn.Create(gSess, gGfx, gSwap, WS, WS);
        if (pn.sc && gDrawnWrist[h] != wv.version) {
            DrawWrist(gPix, WS, p, p.wrist[h], wv);
            if (pn.Upload(gGfx, gSwap, gPix)) gDrawnWrist[h] = wv.version;
        }
        if (pn.ready) {
            auto& q = rq[h];
            q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            q.space = gLocal;
            q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            q.subImage.swapchain = pn.sc;
            q.subImage.imageRect = { {0, 0}, {WS, WS} };
            q.pose = wv.pose;
            q.size = { wv.sizeM, wv.sizeM };
            layers.push_back((const XrCompositionLayerBaseHeader*)&q);
        }
        // Touch cursor on top of the panel: redrawn only when its look changes, moved every frame.
        if (pn.ready && wv.cursor) {
            const int CS = 128;
            Panel& cp = gCursorPanel[h];
            if (!cp.sc) cp.Create(gSess, gGfx, gSwap, CS, CS);
            if (cp.sc && gDrawnCursor[h] != wv.cursorLevel) {
                std::vector<uint32_t> px;
                DrawCursor(px, CS, wv.cursorLevel, p.style);
                if (cp.Upload(gGfx, gSwap, px)) gDrawnCursor[h] = wv.cursorLevel;
            }
            if (cp.ready) {
                auto& q = cq[h];
                q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                q.space = gLocal;
                q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                q.subImage.swapchain = cp.sc;
                q.subImage.imageRect = { {0, 0}, {CS, CS} };
                q.pose = wv.cursorPose;
                q.size = { wv.cursorSize, wv.cursorSize };
                layers.push_back((const XrCompositionLayerBaseHeader*)&q);
            }
        }
    }
    // Controller offsets panel
    static XrCompositionLayerQuad oq{ XR_TYPE_COMPOSITION_LAYER_QUAD };
    if (gEng.offView.open && gLocal) {
        const OffsetView& ov = gEng.offView;
        const int OW = 900, OH = (int)(OW * ov.h / ov.w);
        if (gOffPanel.sc && (gOffPanel.w != OW || gOffPanel.h != OH)) { gOffPanel.Destroy(gSwap); gDrawnOff = ~0ull; }
        if (!gOffPanel.sc) gOffPanel.Create(gSess, gGfx, gSwap, OW, OH);
        if (gOffPanel.sc && gDrawnOff != ov.version) { std::vector<uint32_t> px; DrawList(px, OW, OH, ov.title, ov.labels, ov.cols, ov.hover, ov.pressed, p.style); if (gOffPanel.Upload(gGfx, gSwap, px)) gDrawnOff = ov.version; }
        if (gOffPanel.ready) {
            oq = XrCompositionLayerQuad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
            oq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; oq.space = gLocal; oq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            oq.subImage.swapchain = gOffPanel.sc; oq.subImage.imageRect = { {0, 0}, {OW, OH} };
            oq.pose = ov.pose; oq.size = { ov.w, ov.h };
            layers.push_back((const XrCompositionLayerBaseHeader*)&oq);
        }
    } else if (gOffPanel.sc) { gOffPanel.Destroy(gSwap); gDrawnOff = ~0ull; }
    // Laser pointers, on top of everything: beam from the controller, dot where it hits.
    if (gLocal && (gEng.pointer[0].on || gEng.pointer[1].on)) {
        const int DS = 64, BW = 16, BH = 256;
        if (!gDotPanel.sc) gDotPanel.Create(gSess, gGfx, gSwap, DS, DS);
        if (!gBeamPanel.sc) gBeamPanel.Create(gSess, gGfx, gSwap, BW, BH);
        if (!gDrawnPointer && gDotPanel.sc && gBeamPanel.sc) {
            std::vector<uint32_t> px;
            DrawPointerDot(px, DS, p.style); bool a = gDotPanel.Upload(gGfx, gSwap, px);
            DrawBeam(px, BW, BH, p.style); bool b = gBeamPanel.Upload(gGfx, gSwap, px);
            gDrawnPointer = a && b;
        }
        for (int h = 0; h < 2; h++) {
            const PointerView& P = gEng.pointer[h];
            if (!P.on || !gDotPanel.ready || !gBeamPanel.ready) continue;
            auto& b = pq[h * 2]; auto& d = pq[h * 2 + 1];
            b.layerFlags = d.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            b.space = d.space = gLocal;
            b.eyeVisibility = d.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            b.subImage.swapchain = gBeamPanel.sc; b.subImage.imageRect = { {0, 0}, {BW, BH} };
            b.pose = P.beamPose; b.size = { 0.0035f, P.beamLen };
            d.subImage.swapchain = gDotPanel.sc; d.subImage.imageRect = { {0, 0}, {DS, DS} };
            d.pose = P.dotPose; d.size = { P.dotSize, P.dotSize };
            layers.push_back((const XrCompositionLayerBaseHeader*)&b);
            if (P.dot) layers.push_back((const XrCompositionLayerBaseHeader*)&d);
        }
    }
    // Fingertip silhouettes near wrist panels.
    if (gLocal && (gEng.pointer[0].finger || gEng.pointer[1].finger)) {
        const int FW = 64, FH = 256;
        if (!gFingerPanel.sc) gFingerPanel.Create(gSess, gGfx, gSwap, FW, FH);
        if (!gDrawnFinger && gFingerPanel.sc) {
            std::vector<uint32_t> px;
            DrawFinger(px, FW, FH, p.style);
            gDrawnFinger = gFingerPanel.Upload(gGfx, gSwap, px);
        }
        for (int h = 0; h < 2; h++) {
            const PointerView& P = gEng.pointer[h];
            if (!P.finger || !gFingerPanel.ready) continue;
            auto& q = fq[h];
            q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            q.space = gLocal;
            q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            q.subImage.swapchain = gFingerPanel.sc; q.subImage.imageRect = { {0, 0}, {FW, FH} };
            q.pose = P.fingerPose; q.size = { P.fingerW, P.fingerLen };
            layers.push_back((const XrCompositionLayerBaseHeader*)&q);
        }
    }
    if (NowSec() < gEng.toastUntil && gView) {
        const int TW = 1024, TH = 128;
        if (!gToastPanel.sc) gToastPanel.Create(gSess, gGfx, gSwap, TW, TH);
        if (gToastPanel.sc && gDrawnToast != gEng.toastVersion) {
            DrawToast(gPix, TW, TH, gEng.toast, p.style);
            if (gToastPanel.Upload(gGfx, gSwap, gPix)) gDrawnToast = gEng.toastVersion;
        }
        if (gToastPanel.ready) {
            tq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            tq.space = gView;
            tq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            tq.subImage.swapchain = gToastPanel.sc;
            tq.subImage.imageRect = { {0, 0}, {TW, TH} };
            tq.pose.orientation.w = 1;
            tq.pose.position = { 0, -0.14f, -0.6f };
            tq.size = { 0.32f, 0.04f };
            layers.push_back((const XrCompositionLayerBaseHeader*)&tq);
        }
    }
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
