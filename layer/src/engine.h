#pragma once
#include "config.h"
#include "wrist_panels.h"
#include "screen_mirror.h"
#include "engine_types.h"
#include "vr_keyboard.h"
#include "offsets_panel.h"
#include "in_game_menu.h"
#include <functional>
#include <set>

// One of the game's own controls on a controller part: its action's name ("Jump"), from what the game binds.
struct GameUse {
    int hand = 0, comp = 0;
    bool touch = false; // bound to touching the part, not pressing it
    std::string name;
    bool operator==(const GameUse& o) const { return hand == o.hand && comp == o.comp && touch == o.touch && name == o.name; }
};

struct HandRaw {
    bool b1 = false, b1t = false, b2 = false, b2t = false;
    float trig = 0;
    bool trigT = false;
    float grip = 0;
    float sx = 0, sy = 0;
    bool sc = false, st = false;
    bool rest = false, menu = false;
    bool more[C_COUNT]{}, moreT[C_COUNT]{}; // pressed / touched: the Steam Frame's parts C_FRAME_FIRST .. C_FRAME_LAST
    float padX = 0, padY = 0, padPress = 0; // the Index trackpad / WMR touchpad: where it is touched, how hard pressed
    bool padTouch = false;
    bool gripValid = false, aimValid = false;
    bool touchCtl = false; // a Meta Touch controller (its aim ray needs correcting)
    ControllerKind kind = ControllerKind::Unknown; // from the runtime's interaction profile (the menu's pictures)
    XrPosef gripPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } }, aimPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
};
struct Frame {
    HandRaw h[2];
    bool headValid = false;
    XrPosef head{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    double t = 0;
};
// A fake value pushed into the game's own actions bound to a controller part.
struct Override {
    bool on = false;
    bool press = false, touch = false;
    float value = 0, x = 0, y = 0;
};
struct WheelView {
    bool visible = false;
    const Wheel* wheel = nullptr;
    int hovered = -1;
    std::string crumb;
    XrPosef pose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float size = 0.24f;
    std::vector<bool> on; // per item: its toggle is currently on
    uint64_t version = 0;
    float alpha = 1.f; // see-through while adjusting other things (1 = solid)
};
// Keyboard layout in key units (one letter key = 1x1).
enum KbKind { KB_KEY, KB_SHIFT, KB_CAPS, KB_CTRL, KB_ALT, KB_CLOSE, KB_MOVE, KB_CONTROLLER };
struct KbKey {
    std::string label, shiftLabel, combo;
    float x, y, w, h;
    int kind;
    bool letter;
    bool repeat;
};
// forBinding: the layout used while picking what a binding does: two more rows (mouse buttons and the wheel,
// Home / End / Page keys, and "Controller button" for a button the game should see).
const std::vector<KbKey>& KbLayout(float& unitsW, float& unitsH, bool forBinding = false);

struct KeyboardView {
    bool visible = false;
    XrPosef pose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float widthM = 0.56f, heightM = 0.19f;
    int hover[2] = { -1, -1 };
    int pressed = -1;
    bool shift = false, caps = false, ctrl = false, alt = false;
    bool moving = false; // Move key: grab it with a grip to place it, stick = size
    bool binding = false; // picking what a binding does: the binding layout (see KbLayout)
    uint64_t version = 0;
    // button bar under the keyboard: docking with the screen
    XrPosef barPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float barW = 0.4f, barH = 0.05f;
    std::vector<std::string> barLabels;
    int barHover = -1, barPressed = -1;
    uint64_t barVersion = 0;
};

// Wrist panel: buttons on the forearm, shown when you turn it toward your eyes and look at it.
struct WristView {
    bool visible = false;
    const Wheel* wheel = nullptr;
    bool nested = false; // a Back cell is added in front of the items
    int hover = -1;      // cell index
    int pressed = -1;
    XrPosef pose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float sizeM = 0.11f;
    std::vector<bool> on; // per cell: its toggle is currently on
    uint64_t version = 0;
    // Touch cursor: a ring on the panel under the pressing hand's fingertip.
    bool cursor = false;
    XrPosef cursorPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float cursorSize = 0.02f;
    int cursorLevel = 0;    // 0 far .. 7 almost touching, 8 touching/pushed in
    float touchDepth = 1.f; // how far the other hand's fingertip is in front of the panel (m; <= 0 pushed in)
    // Closer than this the fingertip takes over from the laser: the laser hides and poking picks the button.
    static constexpr float kPokeTakeoverM = 0.035f;
    bool adjust = false; // adjust mode: the header says so
};
struct Cell {
    float x, y, w, h;
}; // 0..1, y down
// Title strip (optional) plus a grid, for a panel aspect = height / width; cells in 0..1 of each side.
void ListLayout(int n, int cols, float aspect, bool title, std::vector<Cell>& out, Cell& header);
// Header strip plus a grid; the same numbers are used for drawing and hit tests.
void WristLayout(int count, int columns, std::vector<Cell>& out, Cell& header);

// One entry of the screen picker: a monitor or an open window.
struct ScreenChoice {
    std::string label;
    int monitor = -1;
    uintptr_t hwnd = 0;
    bool current = false;
};

// Mirrored desktop monitor. The engine decides if and where; the layer captures and draws it.
struct ScreenView {
    bool on = false;
    bool shown = false; // on, or adjust mode (shown so it can be moved)
    ScreenPlace place = ScreenPlace::WristLeft;
    int monitor = 0; // 0 = main, 1..N left to right
    int fps = 30;    // 0 = every headset frame
    XrPosef pose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float widthM = 0.16f;
    bool poseValid = false;
    // Filled in by the layer from the capture: how many monitors there are, and which is the main one.
    int monitorCount = 0, primaryIndex = 1;
    int monX = 0, monY = 0, monW = 0, monH = 0; // the shown monitor or window on the desktop (pixels)
    // Showing one window instead of a monitor. The engine asks, the layer picks the window.
    ScreenSource source = ScreenSource::Monitor;
    int windowStep = 0;         // asked: +1 next / -1 previous window (layer clears it)
    std::string windowMatch;    // asked: a window whose title or program contains this (layer clears it)
    uint64_t windowAsk = 0;     // bumped on every ask
    uintptr_t hwnd = 0;         // the window shown (set by the layer)
    std::string windowTitle;    // asked with windowMatch: prefer a window with exactly this title
    bool choiceChanged = false; // picked in the headset: the layer saves it as the new default
    float aspect = 9.f / 16.f;  // picture height / width
    // Pointing at it: which hand moves the mouse (-1 = none), and whether pointing works right now.
    int hoverHand = -1;
    bool interactive = false;
    // Button bar under the screen (shown while you point at it) and the picker (monitors + windows).
    bool barVisible = false, pickOpen = false;
    XrPosef barPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } }, pickPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float barW = 0.3f, barH = 0.04f, pickW = 0.4f, pickH = 0.44f;
    int barHover = -1, pickHover = -1, barPressed = -1, pickPressed = -1;
    std::vector<std::string> barLabels, pickLabels;
    std::string pickTitle;
    int pickCols = 2;
    uint64_t uiVersion = 0;
    // The engine asks for the list; the layer fills it (monitors first, then windows).
    bool wantChoices = false;
    std::vector<ScreenChoice> choices;
    uint64_t choicesVersion = 0;
};

// Controller offsets panel (in front of you) and the live offsets the layer applies to the game.
struct OffsetView {
    bool open = false;
    XrPosef pose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float w = 0.42f, h = 0.5f;
    int cols = 3;
    std::string title;
    std::vector<std::string> labels;
    int hover = -1, pressed = -1;
    uint64_t version = 0;
};

// Things that can be grabbed with a grip and placed by hand.

struct HapticReq {
    int hand;
    float amp;
    float sec;
};
// Laser pointer of one controller: a beam from the controller and a dot where it hits a wheel,
// the keyboard or a wrist panel. Shown only while it is aimed at one of them.
struct PointerView {
    bool on = false;
    XrPosef dotPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float dotSize = 0.012f;
    XrPosef beamPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float beamLen = 0.f;
    bool dot = true; // false: a guide beam that hits nothing (wrist panel up, aimed elsewhere)
    // Fingertip silhouette, drawn along the (corrected) pointing ray near a wrist panel.
    bool finger = false;
    XrPosef fingerPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float fingerLen = 0.08f, fingerW = 0.022f;
};

class Engine {
    friend class WristPanels;
    friend class ScreenMirror;
    friend class OffsetsPanel;
    friend class VrKeyboard;
    friend class InGameMenu;

public:
    // ---------------- driving it (the layer, once per frame)
    void SetProfile(std::shared_ptr<Profile> p);
    void Reset();
    void Update(const Frame& f);
    bool Active() const { return prof && prof->enabled; }
    std::shared_ptr<Profile> prof;

    // ---------------- what the game should see
    bool Blocked(int hand, int comp) const { return blockOut[hand][comp]; }    // held back for a menu
    const Override& Ov(int hand, int comp) const { return ovOut[hand][comp]; } // pushed into the game by a binding
    const StickShape& StickShapeOf(int hand) const { return prof->sticks[hand]; } // deadzones of the game's stick
    // The pose the game's grip (kind 0) or aim (kind 1) space of a hand is moved by; false = leave it.
    bool OffsetPose(int hand, int kind, XrPosef& out) const;
    OffsetStyle offs; // live controller offsets (from the profile, changed in the headset)

    // ---------------- what the overlay draws (each view has a version that changes with its look)
    WheelView view;
    KeyboardView kb;
    WristView wrist[2];     // [0] left forearm, [1] right forearm
    PointerView pointer[2]; // per controller
    ScreenView screen;
    OffsetView offView;
    MenuView menuView; // the in-game settings menu
    std::vector<GameUse> gameUses; // the game's own controls on the controllers in use (set by the layer)
    ControllerKind KindOf(int hand) const { return fr.h[hand].kind; } // which controller a hand holds now
    std::string toast;
    uint64_t toastVersion = 0;
    double toastUntil = 0;

    // ---------------- requests the layer carries out
    std::vector<HapticReq> haptics;
    bool reloadRequested = false;
    bool saveRequested = false;                    // leaving adjust mode: write the placement to the profile
    bool offsSaveRequested = false;                // write offs into the profile
    bool menuSaveRequested = false;                // the in-game menu changed the profile (bindings, sticks): write them
    MirrorRequest mirrorAsk = MirrorRequest::None; // the headset mirror window
    bool mirrorOn = false;                         // set by the layer

    // ---------------- state other parts read
    bool adjust = false;  // Adjust mode: grab wrist panels, wheels, the screen and the keyboard to place them
    int offGrabHand = -1; // grab-to-set running on this hand (its grip is held)

    // ---------------- actions and helpers used by the feature classes
    void ScreenAction(const Action& a);  // screen on/off, monitor, frame rate, place
    void OffsetsAction(const Action& a); // panel, enable, on, off, grab, reset
    void Toast(const std::string& s, double sec = 2.0);
    // Where a hand's poking fingertip is (the corrected laser start, moved by the pointer's fingertip settings).
    Vec3 FingertipPos(int hand) const;
    // Is this action a toggle that is currently on? (key toggle, game input toggle, layer)
    bool IsOn(const Action& a) const;
    // Change the profile's bindings (the in-game menu): whatever a binding is doing is ended first, and the
    // per-binding state is rebuilt after, so a held key or layer never sticks.
    void EditBindings(const std::function<void(std::vector<Binding>&)>& edit);

private:
    WristPanels wristPanels{ *this };
    ScreenMirror screenMirror{ *this };
    VrKeyboard keyboard{ *this };
    OffsetsPanel offsetsPanel{ *this };
    InGameMenu menu{ *this };
    struct BState {
        bool armed = false;
        double armTime = 0;
        bool fired = false;
        bool needEnd = false;
        double lastTap = -1;
    };
    struct Pulse {
        int hand, comp;
        Override o;
        double until;
    };
    struct WheelRun {
        bool open = false;
        std::vector<const Wheel*> stack;
        int hovered = -1, opener = -1, openerHand = 1, selHand = 1;
        ConfirmMode confirm = ConfirmMode::Auto;
        double hoverSince = 0, openTime = 0;
        bool stickOut = false, locked = false;
        XrPosef pose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    };

    Frame fr;
    bool cur[2][C_COUNT][F_COUNT]{}, prev[2][C_COUNT][F_COUNT]{};
    bool blockOut[2][C_COUNT]{}, sticky[2][C_COUNT]{};
    Override ovOut[2][C_COUNT];
    std::vector<BState> bs;
    std::set<std::string> layers;
    std::set<std::string> keyToggles;
    std::map<std::pair<int, int>, Override> xrToggles;
    std::map<int, std::pair<std::pair<int, int>, Override>> xrHolds; // binding -> override
    std::vector<Pulse> pulses;
    WheelRun wr;
    double now = 0, lastNow = 0;
    // What a grip is holding while things are being placed by hand.
    struct GrabRun {
        GrabTarget target = GrabTarget::None;
        int hand = -1;
        Vec3 off;
        XrPosef rel{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
        bool Active() const { return target != GrabTarget::None; }
    };
    // grabbing: the thing keeps the position and angle it had in the grabbing hand
    void StartGrab(GrabTarget target, int hand, const XrPosef& obj);
    XrPosef GrabbedPose() const;
    XrPosef WheelFrame(int hand) const;
    GrabRun grab;
    double adjustClosedAt = -1;
    void UpdateAdjustWheel();
    void UpdateAdjustFingertips();
    void UpdateAdjustFade();

public:
    bool dockAdjust = false; // "Adjust dock": grab the docked one to set where it sits

private:
    float wheelAlphaDrawn = 1.f;

    // ---------------- VR keyboard (keyboard.cpp)

    // ---------------- mirrored screen (screen_ui.cpp)
    PoseSmoother aimSmooth[2], screenSmooth;

    // ---------------- controller offsets panel (offsets.cpp)

    // ---------------- bindings, wheel, pointers (engine.cpp, wheel.cpp)
    bool Down(const InputId& i) const { return cur[i.hand][i.comp][i.feat]; }
    bool Pressed(const InputId& i) const { return cur[i.hand][i.comp][i.feat] && !prev[i.hand][i.comp][i.feat]; }
    void ComputeDown();
    bool LayersOk(const Binding& b) const;
    void ProcessBindings();
    void Fire(int bi);
    void EndB(int bi);
    bool RunAction(const Action& a, int hand, int bi, bool fromWheel);
    void EndAction(const Action& a, int bi);
    void SetLayer(const std::string& l, bool on);
    void OpenWheel(const Action& a, int bi, int hand);
    void UpdateWheel();
    void Select(int idx);
    void CloseWheel();
    void PushWheel(const Wheel* w);
    void ComputeBlocks();
    bool CompActive(int h, int c) const;
    XrPosef WheelPose(int hand) const;
    static Override MakeOv(const InputId& t);
    void Dirty() { view.version++; }
    void UpdatePointers();

    // wrist panel
};
