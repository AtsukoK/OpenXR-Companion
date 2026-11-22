#pragma once
#include "common.h"
#include <map>
#include <memory>
#include <optional>

// Physical controller parts. BTN1 = A (right) / X (left), BTN2 = B / Y.
enum Comp { C_BTN1, C_BTN2, C_TRIGGER, C_GRIP, C_STICK, C_THUMBREST, C_MENU, C_COUNT };
// How the part is read. PRESS = click or analog over threshold.
enum Feat { F_PRESS, F_TOUCH, F_UP, F_DOWN, F_LEFT, F_RIGHT, F_COUNT };

struct InputId {
    int hand = -1, comp = -1, feat = F_PRESS;
    bool valid() const { return hand >= 0 && comp >= 0; }
    bool operator==(const InputId& o) const { return hand == o.hand && comp == o.comp && feat == o.feat; }
};
bool ParseInput(const std::string& s, InputId& out);
std::string InputName(const InputId& i);
int ParseCompName(const std::string& s); // "trigger", "primary", ... -> Comp or -1

enum class ActType { None, Key, Sequence, XrInput, Wheel, Back, Close, Layer, Reload, Keyboard, Page, Adjust, Screen, Offsets, Mirror };

// Text <-> enum tables for profile settings: { value, "text in the file" }.
template <class E>
struct EnumName {
    E value;
    const char* key;
};
template <class E, size_t N>
bool ParseEnum(const EnumName<E> (&table)[N], const std::string& text, E& out) {
    for (const auto& n : table) {
        if (text == n.key) {
            out = n.value;
            return true;
        }
    }
    return false;
}
template <class E, size_t N>
const char* EnumKey(const EnumName<E> (&table)[N], E value) {
    for (const auto& n : table)
        if (n.value == value) return n.key;
    return "";
}

// How a binding's input has to be used to fire it.
enum class Gesture { Press, Tap, Hold, DoubleTap, WhileHeld };
inline constexpr EnumName<Gesture> kGestureNames[] = { { Gesture::Press, "press" },
                                                       { Gesture::Tap, "tap" },
                                                       { Gesture::Hold, "hold" },
                                                       { Gesture::DoubleTap, "double_tap" },
                                                       { Gesture::WhileHeld, "while_held" } };

// How a wheel item is chosen: on letting go of the opener, on letting the stick back, or with a button.
enum class ConfirmMode { Auto, Release, StickReturn, Button };
inline constexpr EnumName<ConfirmMode> kConfirmModeNames[] = { { ConfirmMode::Auto, "auto" },
                                                               { ConfirmMode::Release, "release" },
                                                               { ConfirmMode::StickReturn, "stick_return" },
                                                               { ConfirmMode::Button, "button" } };

// Which hand picks wheel items.
enum class SelectHand { Opener, Left, Right };
inline constexpr EnumName<SelectHand> kSelectHandNames[] = { { SelectHand::Opener, "opener" },
                                                             { SelectHand::Left, "left" },
                                                             { SelectHand::Right, "right" } };

// What of a binding's input the game stops seeing.
enum class BindingBlock { None, Trigger, All };
inline constexpr EnumName<BindingBlock> kBindingBlockNames[] = { { BindingBlock::None, "none" },
                                                                 { BindingBlock::Trigger, "trigger" },
                                                                 { BindingBlock::All, "all" } };

// Which way the text on a wrist panel reads.
enum class TextUp { Fingers, Between, AlongArm, Custom };
inline constexpr EnumName<TextUp> kTextUpNames[] = {
    { TextUp::Fingers, "fingers" }, { TextUp::Between, "between" }, { TextUp::AlongArm, "along_arm" }, { TextUp::Custom, "custom" }
};

// How wrist panel buttons are pressed.
enum class PressMethod { Poke, Point, Both };
inline constexpr EnumName<PressMethod> kPressMethodNames[] = { { PressMethod::Poke, "poke" },
                                                               { PressMethod::Point, "point" },
                                                               { PressMethod::Both, "both" } };

// When the controller's aim ray is corrected (Quest Touch controllers start it too high).
enum class LaserCorrection { Auto, Always, Off };
inline constexpr EnumName<LaserCorrection> kLaserCorrectionNames[] = { { LaserCorrection::Auto, "auto" },
                                                                       { LaserCorrection::Always, "always" },
                                                                       { LaserCorrection::Off, "off" } };

// What a wheel is placed relative to. "Locked" = it stays where it opened instead of following.
enum class WheelAnchor { Hand, HandLocked, Head, HeadLocked };
inline constexpr EnumName<WheelAnchor> kWheelAnchorNames[] = { { WheelAnchor::Hand, "hand" },
                                                               { WheelAnchor::HandLocked, "hand_locked" },
                                                               { WheelAnchor::Head, "head" },
                                                               { WheelAnchor::HeadLocked, "head_locked" } };
inline bool IsHandAnchor(WheelAnchor a) {
    return a == WheelAnchor::Hand || a == WheelAnchor::HandLocked;
}
inline bool IsLockedAnchor(WheelAnchor a) {
    return a == WheelAnchor::HandLocked || a == WheelAnchor::HeadLocked;
}

// How wheel items are picked.
enum class SelectMethod { Thumbstick, Point, Both };
inline constexpr EnumName<SelectMethod> kSelectMethodNames[] = { { SelectMethod::Thumbstick, "thumbstick" },
                                                                 { SelectMethod::Point, "point" },
                                                                 { SelectMethod::Both, "both" } };

// Which of the game's controls are held back while a wheel is open.
enum class BlockWhileOpen { None, Minimal, Hand, All };
inline constexpr EnumName<BlockWhileOpen> kBlockWhileOpenNames[] = { { BlockWhileOpen::None, "none" },
                                                                     { BlockWhileOpen::Minimal, "minimal" },
                                                                     { BlockWhileOpen::Hand, "hand" },
                                                                     { BlockWhileOpen::All, "all" } };

// Which side of the forearm a panel or the screen sits on.
enum class ArmSide { Inner /* palm side */, Back /* watch side */ };
inline constexpr EnumName<ArmSide> kArmSideNames[] = { { ArmSide::Inner, "inner" }, { ArmSide::Back, "back" } };

// Where the mirrored screen is shown.
enum class ScreenPlace { WristLeft, WristRight, Front /* stays where it appears */, Head /* follows the view */ };
constexpr ScreenPlace kScreenPlaces[] = { ScreenPlace::WristLeft, ScreenPlace::WristRight, ScreenPlace::Front, ScreenPlace::Head };
// The text used in profile files: wrist_left, wrist_right, front, head.
const char* ScreenPlaceKey(ScreenPlace p);
bool ParseScreenPlace(const std::string& key, ScreenPlace& out);
inline bool IsWristPlace(ScreenPlace p) {
    return p == ScreenPlace::WristLeft || p == ScreenPlace::WristRight;
}
inline int WristHandOf(ScreenPlace p) {
    return p == ScreenPlace::WristLeft ? 0 : 1;
}

// What the mirrored screen shows: a whole monitor or one window.
enum class ScreenSource { Monitor, Window };

// How an action runs; which modes apply depends on the action type (see the profile documentation).
enum class ActMode {
    Unknown,
    Tap,
    Hold,
    Toggle,
    On,
    Off,
    NextMonitor,
    PrevMonitor,
    NextFps,
    NextPlace,
    NextWindow,
    PrevWindow,
    Choose,
    Panel,
    Enable,
    Grab,
    Reset
};
// The text used in profile files ("tap", "next_monitor", ...); Unknown for anything else.
ActMode ParseActMode(const std::string& text);
const char* ActModeKey(ActMode m);

struct Action {
    ActType type = ActType::None;
    std::string keys;            // key: "Ctrl+S"
    ActMode mode = ActMode::Tap; // key/xr_input: tap|hold|toggle ; layer: hold|toggle|on|off
    int tapMs = 60;
    std::string steps;                       // sequence: "Ctrl+C; wait 100; down W; up W"
    std::string wheel;                       // wheel: name
    ConfirmMode confirm = ConfirmMode::Auto; // wheel
    std::string layer;                       // layer: name
    std::string page;                        // page: wrist panel page name
    InputId target;                          // xr_input: game input to fake
    // screen: mode toggle|on|off|next_monitor|prev_monitor|next_fps|next_place
    std::optional<ScreenPlace> place; // screen: where to show it when turned on (none = the profile's place)
    std::string window;               // screen: show the first window whose title or program contains this
    int monitor = -1;                 // screen: which monitor when turned on (-1 = keep, 0 = main, 1..N left to right)
};

struct Binding {
    std::string name;
    bool enabled = true;
    std::vector<InputId> chord; // last entry is the trigger, the rest must be held
    std::string chordText;
    Gesture gesture = Gesture::Press;
    int holdMs = 400, tapMs = 300, doubleTapMs = 350;
    std::vector<std::string> layers, notLayers;
    BindingBlock block = BindingBlock::Trigger;
    bool tapPassthrough = false;
    Action action;
};

struct WheelItem {
    std::string label, description, icon;
    bool hasColor = false;
    uint32_t color = 0;
    Action action;
};
struct Wheel {
    std::string name, title;
    std::vector<WheelItem> items;
};

// A position and angle set by hand in adjust mode, relative to what the thing is attached to
// (a controller, the head, or the head's direction when it appeared). Unset = use the numbers.
struct FreePose {
    bool set = false;
    XrPosef p{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
};

struct Style {
    WheelAnchor anchor = WheelAnchor::HeadLocked;
    float radius = 0.127f, distance = 0.70f, headDown = 0.10f;
    float handUp = 0.10f, handToward = 0.06f;
    float innerRatio = 0.34f, gapDeg = 1.5f;
    std::string font = "Segoe UI";
    float fontScale = 1.f;
    int texSize = 768;
    uint32_t bg = 0xB0101418, segment = 0xD0283040, hover = 0xF03C8CFF, text = 0xFFFFFFFF, accent = 0xFF7FC4FF, border = 0x60FFFFFF;
    uint32_t toggled = 0xE02E9E5B;         // buttons whose toggle (key, game input or layer) is currently on
    std::map<std::string, FreePose> poses; // per anchor (its file text), placed by hand in adjust mode
};

struct Selection {
    SelectMethod method = SelectMethod::Both;
    SelectHand hand = SelectHand::Opener;
    float deadzone = 0.5f;
    int confirmComp = C_TRIGGER; // used when confirm = button
    int cancelComp = C_BTN2;
    int submenuDwellMs = 450; // only for release-confirm wheels
    bool closeAfterSelect = true;
    int autoCloseMs = 0;
};

// Docking the mirrored screen and the VR keyboard together (set from the bar under the keyboard).
// Docking the mirrored screen and the VR keyboard together.
enum class DockMode {
    Apart,
    ScreenOnKeys /* the screen sits above the keyboard */,
    KeysUnderScreen /* the keyboard sits under the screen */
};
struct DockStyle {
    DockMode mode = DockMode::Apart;
    FreePose pose;     // the screen in the keyboard's frame, when set by hand (Adjust dock)
    float width = 0.f; // screen width while docked to the keyboard (0 = as wide as the keyboard)
};
struct KbStyle {
    float width = 0.56f;    // metres
    float distance = 0.42f; // in front of the head when opened
    float below = 0.30f;    // below eye level
    bool closeOnEnter = false;
    FreePose pose; // placed by hand in adjust mode: relative to the head (turned like the head) when it opens
};

struct WristStyle {
    bool enabled = false;
    bool mirror = false;                // show the other forearm's pages instead of its own
    std::map<std::string, Wheel> pages; // the panel's own pages (title + items)
    std::string start = "main";         // first page shown
    ArmSide face = ArmSide::Inner;
    float size = 0.11f;              // panel width and height in metres
    float towardElbow = 0.13f;       // from the controller grip back along the forearm
    TextUp textUp = TextUp::Fingers; // fingers = top of the text toward the fingers (hand held up, palm to face)
                                     // custom = rotate_deg from along_arm (set by turning it in adjust mode)
                                     // between = halfway (45 degrees) between fingers and along_arm (arm half raised)
                                     // along_arm = text runs along the forearm (arm held across the body)
    float lift = 0.025f;             // off the skin
    float side = 0.f;                // sideways across the forearm (set by adjust mode)
    float rotateDeg = 0.f;           // text_up = custom: turn of the text from along_arm, degrees
    int columns = 3;
    float facingDeg = 35.f; // how squarely the panel must face the eyes
    float gazeDeg = 20.f;   // how close to the centre of view it must be
    int showMs = 250, hideMs = 350;
    PressMethod press = PressMethod::Both;
    FreePose pose; // placed by hand in adjust mode (relative to the controller)
};

// Pointer correction and helpers. Quest (Touch) controllers report an aim ray that starts above
// the index finger and points about 15 degrees up from where a pointing finger goes.
struct PointerStyle {
    LaserCorrection correction = LaserCorrection::Auto; // Auto = Touch controllers only
    float pitchDeg = 15.f;                              // turn the ray down by this much
    float downM = 0.02f;                                // move the ray start down (toward the finger) by this much
    float forwardM = 0.f;                               // and forward along the ray
    bool finger = true;                                 // show a fingertip silhouette near a wrist panel
    // Where the poking fingertip sits, measured from the laser start: forward along the laser, up, and toward
    // the middle (mirrored for each hand). Used for poking wrist panels and for drawing the fingertip.
    float tipForwardM = 0.015f, tipUpM = 0.f, tipInwardM = 0.f;
    bool guide = true;     // keep a short laser showing while a wrist panel is up
    float smoothing = 0.f; // steady the laser against shaky hands (0 = off .. 1 = strongest)
};

// A desktop monitor shown in VR (mirrored with Desktop Duplication).
struct ScreenStyle {
    int monitor = 0; // 0 = the main monitor, 1..N = monitors from left to right
    ScreenSource source = ScreenSource::Monitor;
    std::string window;                             // source window: the first window whose title or program name contains this
    std::string windowTitle;                        // remembered from the last pick: a window with exactly this title wins
    int fps = 30;                                   // capture rate cap; 0 = as fast as the headset draws
    std::vector<int> fpsSteps{ 10, 15, 30, 60, 0 }; // what "next frame rate" cycles through
    ScreenPlace place = ScreenPlace::WristLeft;
    // on a forearm
    float wristWidth = 0.16f, towardElbow = 0.14f, lift = 0.03f, side = 0.f, rotateDeg = 0.f;
    ArmSide face = ArmSide::Back;
    // front (placed in front of you when shown) / head (follows your view)
    float width = 0.8f, distance = 0.9f, below = 0.05f;
    int maxPx = 0; // widest captured image in pixels (0 = 1920); halved only while over 1.5x this
    float opacity = 1.f;
    bool cursor = true;                    // draw the mouse pointer
    bool startOn = false;                  // shown when the game starts
    std::map<std::string, FreePose> poses; // per place (ScreenPlaceKey), placed by hand in adjust mode
    bool interact = true;                  // point at it to move the mouse, trigger = left click, A/X = right click, stick = scroll
    bool refocus = true;                   // give the game its window back after clicking on the desktop
    float barScale = 0.7f;                 // size of the button bar under the screen
    float smoothing = 0.f;                 // steady the screen against shaky hands (0 = off .. 1 = strongest)
};

// Controller offsets for the GAME: turn and move the controller poses the game sees (hands, guns,
// its own laser). Angles in degrees, positions in metres, in the controller's own frame:
// pitch + = tip up, yaw + = turn left, roll + = roll left; x + = right, y + = up, z + = back (OpenXR).
struct HandOffset {
    float pitch = 0, yaw = 0, roll = 0, x = 0, y = 0, z = 0;
    bool Zero() const { return !pitch && !yaw && !roll && !x && !y && !z; }
    HandOffset Mirrored() const { return { pitch, -yaw, -roll, -x, y, z }; } // right hand <-> left hand
};
struct OffsetStyle {
    bool enabled = true;   // off = the game sees the controllers as they are
    bool linkHands = true; // the left hand is the right hand mirrored
    bool aimSame = true;   // the aim pose (game lasers) gets the same offset as the grip (hands, guns)
    float smoothing = 0.f; // steady the game's hands against shaky hands (0 = off .. 1 = strongest)
    HandOffset o[2][2];    // [hand 0 left, 1 right][0 grip, 1 aim] as stored
    // What actually applies, with linking and "aim same as grip" worked out.
    HandOffset Effective(int hand, int kind) const {
        int k = aimSame ? 0 : kind;
        return linkHands && hand == 0 ? o[1][k].Mirrored() : o[hand][k];
    }
};

struct Profile {
    mutable std::string sourceName; // a save into default.json moves the profile to user_defaults.json
    PointerStyle pointer;
    OffsetStyle offsets;
    bool enabled = true;
    BlockWhileOpen blockWhileOpen = BlockWhileOpen::Hand;
    float pressOn = 0.6f, pressOff = 0.4f, stickOn = 0.7f, stickOff = 0.5f;
    bool haptics = true;
    float hapticAmp = 0.35f;
    bool notifications = true;
    bool focusWindow = false;
    Style style;
    Selection sel;
    KbStyle kb;
    DockStyle dock;
    ScreenStyle screen;
    WristStyle wrist[2]; // [0] left forearm, [1] right forearm; each is pressed by the other hand
    // The panel whose pages a forearm shows (either one can mirror the other, not both).
    const WristStyle& WristPages(int h) const { return wrist[h].mirror ? wrist[1 - h] : wrist[h]; }
    std::vector<Binding> bindings;
    std::map<std::string, Wheel> wheels;
};

void PrepareDefaults();
std::wstring ActiveProfilePath();                // <data>\\active_profile.txt: the profile selected in the editor
std::string ActiveProfileName();                 // "" = none / invalid
void SetActiveProfileName(const std::string& n); // default.json = the built-in standard; a changed old one becomes user_defaults.json
// Loads <exe>.json from the profiles folder, else user_defaults.json, else default.json (written with a sample if missing).
std::shared_ptr<Profile> LoadProfile(std::wstring& pathOut, std::string& err);
const char* DefaultProfileJson();
// Adjust mode: write the wheel and wrist panel placement of p back into its profile file.
bool SavePlacement(const Profile& p, std::string& err);
// The monitor or window last picked in the headset becomes the profile's default.
bool SaveScreenChoice(const Profile& p, std::string& err);
// Controller offsets changed in the headset: write the controller_offsets section.
bool SaveOffsets(const Profile& p, std::string& err);
// Headset mirror window: app-wide (not per profile), in %LOCALAPPDATA%\OpenXRCompanion\headset_mirror.json,
// shared with the editor's toolbar button.
struct MirrorSettings {
    bool on = false;
    int eye = 1;
    float zoom = 1.3f;
    int fps = 60;
    float smoothing = 0.5f;
    bool onTop = false;
    bool bothEyes = false; // "both_eyes": the other eye fills the nose side
    bool light = true; // "quality": "light" draws straight from the game's image (cheap), "full" copies it with mipmaps
};
// Head smoothing (experimental, motion sickness risk): steadies the head the GAME renders from.
// mode 0 = for recording (the headset still shows a steady world), 1 = in the headset too.
struct HeadSmoothSettings {
    bool on = false;
    int mode = 0;
    float strength = 0.5f;
    bool keepOn = false;
};
bool LoadHeadSmoothSettings(HeadSmoothSettings& h);
void SwitchOffHeadSmoothing(); // writes "on": false, keeps the rest
std::wstring HeadSmoothSettingsPath();
std::wstring MirrorSettingsPath();
bool LoadMirrorSettings(MirrorSettings& m);
bool SaveMirrorSettings(const MirrorSettings& m);
std::shared_ptr<Profile> ParseProfileText(const std::string& text, std::string& err);
