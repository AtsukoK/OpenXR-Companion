// Tests of single parts of the layer (no engine flow): game hand offsets, the overlay, game input.
#include "test_support.h"
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>

// ---------------- the overlay (OverlayRenderer) on a software D3D11 device, with pretend swapchains
namespace fake11 {
static ID3D11Device* dev = nullptr;
static std::map<uint64_t, std::vector<ID3D11Texture2D*>> chains;
static uint64_t nextChain = 0x5000;
static XrResult XRAPI_CALL Formats(XrSession, uint32_t cap, uint32_t* n, int64_t* f) {
    *n = 1;
    if (cap && f) f[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL Create(XrSession, const XrSwapchainCreateInfo* ci, XrSwapchain* sc) {
    uint64_t h = nextChain++;
    for (int i = 0; i < 3; i++) {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = ci->width;
        td.Height = ci->height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D* t = nullptr;
        dev->CreateTexture2D(&td, nullptr, &t);
        chains[h].push_back(t);
    }
    *sc = (XrSwapchain)(uintptr_t)h;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL Destroy(XrSwapchain sc) {
    for (auto t : chains[(uint64_t)sc])
        t->Release();
    chains.erase((uint64_t)sc);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL Images(XrSwapchain sc, uint32_t cap, uint32_t* n, XrSwapchainImageBaseHeader* h) {
    auto& c = chains[(uint64_t)sc];
    *n = (uint32_t)c.size();
    if (cap && h)
        for (uint32_t i = 0; i < cap && i < c.size(); i++)
            ((XrSwapchainImageD3D11KHR*)h)[i].texture = c[i];
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL Acquire(XrSwapchain, const XrSwapchainImageAcquireInfo*, uint32_t* i) {
    *i = 0;
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL Wait(XrSwapchain, const XrSwapchainImageWaitInfo*) {
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL Release(XrSwapchain, const XrSwapchainImageReleaseInfo*) {
    return XR_SUCCESS;
}
} // namespace fake11

void TestGameHandOffsets() { // game hand offsets (GameHandOffsets): only registered hand spaces move, the base gets the inverse, lost poses pass through
    const XrSpaceLocationFlags tracked = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                         XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    XrSpace hand = (XrSpace)(uintptr_t)11, world = (XrSpace)(uintptr_t)12, other = (XrSpace)(uintptr_t)13;
    GameHandOffsets go;
    go.AddSpace(hand, 1, PoseKind::Grip);
    GameHandOffsets::Settings st;
    st.on = true;
    st.has[1][0] = true;
    st.pose[1][0] = { { 0, 0, 0, 1 }, { 0, 0, -0.1f } }; // 10 cm forward
    go.SetSettings(st);
    XrPosef p{ { 0, 0, 0, 1 }, { 0, 1, 0 } };
    go.Apply(hand, world, 1, tracked, p, nullptr, nullptr, nullptr);
    CHECK(std::fabs(p.position.z + 0.1f) < 1e-4f, "game offsets: the right grip is moved 10 cm forward");
    XrPosef q{ { 0, 0, 0, 1 }, { 0, 1, 0 } };
    go.Apply(other, world, 1, tracked, q, nullptr, nullptr, nullptr);
    CHECK(q.position.z == 0.f, "game offsets: a space that isn't a hand is left alone");
    XrPosef r{ { 0, 0, 0, 1 }, { 0, 0, -0.1f } };
    go.Apply(world, hand, 1, tracked, r, nullptr, nullptr, nullptr);
    CHECK(std::fabs(r.position.z) < 1e-4f, "game offsets: located relative to the hand, the inverse offset applies");
    XrPosef lost{ { 0, 0, 0, 1 }, { 0, 1, 0 } };
    go.Apply(hand, world, 2, XR_SPACE_LOCATION_POSITION_VALID_BIT, lost, nullptr, nullptr, nullptr);
    CHECK(lost.position.z == 0.f, "game offsets: a pose without a valid angle passes through");
    go.RemoveSpace(hand);
    XrPosef gone{ { 0, 0, 0, 1 }, { 0, 1, 0 } };
    go.Apply(hand, world, 3, tracked, gone, nullptr, nullptr, nullptr);
    CHECK(gone.position.z == 0.f, "game offsets: a destroyed space is no longer moved");
}

void TestOverlay() { // overlay: what is visible ends up as quad layers in the frame, nothing when hidden, swapchains freed at the end
    ID3D11DeviceContext* ctx = nullptr;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &fake11::dev,
                      nullptr, &ctx);
    if (!fake11::dev) {
        printf("skip: overlay (no D3D11 WARP device)\n");
    } else {
        XrFns f;
        f.EnumerateSwapchainFormats = fake11::Formats;
        f.CreateSwapchain = fake11::Create;
        f.DestroySwapchain = fake11::Destroy;
        f.EnumerateSwapchainImages = fake11::Images;
        f.AcquireSwapchainImage = fake11::Acquire;
        f.WaitSwapchainImage = fake11::Wait;
        f.ReleaseSwapchainImage = fake11::Release;
        XrGraphicsBindingD3D11KHR gb{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
        gb.device = fake11::dev;
        Gfx g;
        g.Init(&gb);
        std::string err;
        Engine e;
        e.SetProfile(ParseProfileText(DefaultProfileJson(), err));
        OverlayRenderer ov(e);
        OverlayContext c;
        c.session = (XrSession)(uintptr_t)2;
        c.gfx = &g;
        c.swap = &f;
        c.local = (XrSpace)(uintptr_t)3;
        c.view = (XrSpace)(uintptr_t)4;
        LayerList layers;
        ov.Render(c, layers);
        CHECK(layers.empty(), "overlay: nothing visible, nothing added to the frame");
        e.kb.visible = true;
        e.kb.barLabels = { "A", "B" };
        e.kb.version++;
        e.kb.barVersion++;
        e.toast = "hello";
        e.toastUntil = NowSec() + 5;
        e.toastVersion++;
        ov.Render(c, layers);
        int quads = 0, inView = 0;
        for (auto* l : layers)
            if (l->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
                quads++;
                if (((const XrCompositionLayerQuad*)l)->space == c.view) inView++;
            }
        CHECK(quads == 3, ("overlay: the keyboard, its bar and a toast are 3 quads, got " + std::to_string(quads)).c_str());
        CHECK(inView == 1, "overlay: the toast is head-locked, the rest world-locked");
        CHECK(fake11::chains.size() == 3, "overlay: one swapchain each");
        ov.Destroy(c);
        CHECK(fake11::chains.empty(), "overlay: every swapchain is freed when the session ends");
        g.Release();
        ctx->Release();
        fake11::dev->Release();
        fake11::dev = nullptr;
    }
}

void TestGameInputFilter() { // game input filter: path parsing, values pass through when nothing is held, changedSinceLastSync follows our values
    int hand = -1, comp = -1;
    InputFeature f = InputFeature::Value;
    CHECK(ParseGameInputPath("/user/hand/right/input/trigger/value", hand, comp, f) && hand == 1 && comp == C_TRIGGER &&
              f == InputFeature::Value,
          "game input: right trigger value parses");
    CHECK(ParseGameInputPath("/user/hand/left/input/thumbstick", hand, comp, f) && hand == 0 && f == InputFeature::Vector,
          "game input: a bare thumbstick path is the 2D value");
    CHECK(!ParseGameInputPath("/user/hand/left/input/system/click", hand, comp, f), "game input: the system button is not ours to change");
    std::string err;
    Engine e;
    e.SetProfile(ParseProfileText(DefaultProfileJson(), err));
    GameInputFilter gi;
    gi.SetHandPaths(101, 102);
    XrAction act = (XrAction)(uintptr_t)77;
    gi.AddBinding(act, 1, C_TRIGGER, InputFeature::Value);
    auto noRequery = [](XrPath, XrActionStateFloat&) {
        return false;
    };
    XrActionStateFloat st{ XR_TYPE_ACTION_STATE_FLOAT };
    st.currentState = 0.4f;
    st.isActive = XR_TRUE;
    gi.NewSync();
    gi.Filter(e, act, XR_NULL_PATH, 5, st, noRequery);
    CHECK(st.currentState == 0.4f && !st.changedSinceLastSync, "game input: nothing held, the game's value passes through");
    st.currentState = 0.9f;
    gi.NewSync();
    gi.Filter(e, act, XR_NULL_PATH, 6, st, noRequery);
    CHECK(st.changedSinceLastSync && st.lastChangeTime == 6, "game input: a new value is reported as changed");
    XrActionStateFloat other{ XR_TYPE_ACTION_STATE_FLOAT };
    other.currentState = 0.7f;
    gi.Filter(e, (XrAction)(uintptr_t)78, XR_NULL_PATH, 7, other, noRequery);
    CHECK(other.currentState == 0.7f, "game input: an action we don't know is left alone");
}

void TestFingertip() { // the poking fingertip: 1.5 cm ahead of the laser start by default, moved by the pointer settings
    std::string err;
    auto p = ParseProfileText(DefaultProfileJson(), err);
    Engine e;
    e.SetProfile(p);
    Frame f;
    f.headValid = true;
    for (int h = 0; h < 2; h++) {
        f.h[h].aimValid = true;
        f.h[h].aimPose = { { 0, 0, 0, 1 }, { h ? 0.2f : -0.2f, 1.f, -0.3f } }; // pointing straight ahead (-z)
    }
    f.t = 1.0;
    e.Update(f);
    Vec3 tip = e.FingertipPos(1);
    CHECK(std::fabs(tip.z - (-0.315f)) < 1e-4f && std::fabs(tip.x - 0.2f) < 1e-4f, "fingertip: 1.5 cm ahead of the laser start by default");
    p->pointer.tipForwardM = 0.03f;
    p->pointer.tipUpM = 0.01f;
    p->pointer.tipInwardM = 0.02f;
    Vec3 r = e.FingertipPos(1), l = e.FingertipPos(0);
    CHECK(std::fabs(r.z - (-0.33f)) < 1e-4f && std::fabs(r.y - 1.01f) < 1e-4f, "fingertip: forward and up move it");
    CHECK(std::fabs(r.x - 0.18f) < 1e-4f && std::fabs(l.x - (-0.18f)) < 1e-4f, "fingertip: toward the middle is mirrored for each hand");
}

void TestFingertipAdjust() { // Adjust mode: grab the right fingertip with the left grip, move it, let go: it stays there
    std::string err;
    auto p = ParseProfileText(DefaultProfileJson(), err);
    Engine e;
    e.SetProfile(p);
    Frame f;
    f.headValid = true;
    f.head.position = { 0, 1.6f, 0 };
    f.h[1].aimValid = f.h[1].gripValid = true;
    f.h[1].aimPose = f.h[1].gripPose = { { 0, 0, 0, 1 }, { 0.2f, 1.f, -0.3f } };
    f.h[0].gripValid = f.h[0].aimValid = true;
    double t = 1.0;
    auto step = [&] {
        t += 0.011;
        f.t = t;
        e.Update(f);
    };
    step();
    Vec3 start = e.FingertipPos(1);
    e.adjust = true;
    f.h[0].gripPose = { { 0, 0, 0, 1 }, { start.x, start.y, start.z } };
    f.h[0].aimPose = f.h[0].gripPose;
    step();
    CHECK(e.pointer[1].finger, "fingertip adjust: the fingertips show in Adjust mode");
    f.h[0].grip = 1.f;
    step();
    f.h[0].gripPose.position = { start.x, start.y + 0.01f, start.z - 0.01f }; // 1 cm up, 1 cm further forward
    step();
    f.h[0].grip = 0.f;
    step();
    CHECK(std::fabs(p->pointer.tipForwardM - 0.025f) < 0.002f && std::fabs(p->pointer.tipUpM - 0.01f) < 0.002f,
          ("fingertip adjust: dragged 1 cm up and forward, now " + std::to_string(p->pointer.tipForwardM) + " / " +
           std::to_string(p->pointer.tipUpM))
              .c_str());
    e.adjust = false;
}

void TestKeySequences() { // key sequences: good steps pass, bad waits and unknown keys are refused with the step named
    std::string err;
    CHECK(Keys::ValidateSequence("Ctrl+C; wait 100; down W; wait 500; up W; F5", err), "keys: full sequence is valid");
    err.clear();
    CHECK(!Keys::ValidateSequence("F5; wait 0", err) && err.find("wait 0") != std::string::npos, "keys: wait 0 refused, step named");
    err.clear();
    CHECK(!Keys::ValidateSequence("down Banana", err) && err.find("Banana") != std::string::npos, "keys: unknown key in down refused");
    err.clear();
    CHECK(Keys::ValidateSequence("", err), "keys: empty sequence is valid (does nothing)");
}

// Controller buttons page: its quick choices are ordinary bindings, written as the editor writes them.
// Key bindings aren't driven here (they would type real keys); a layer stands in for the key on the touch one.
void TestQuickBindings() {
    const char* text = R"JSON({
      "bindings": [
        { "name": "Right A: touch", "input": "right.a.touch", "gesture": "while_held", "block": "none",
          "action": { "type": "layer", "layer": "touching", "mode": "hold" } },
        { "name": "Right B acts as Right A", "input": "right.b", "gesture": "while_held", "block": "trigger",
          "action": { "type": "xr_input", "target": "right.a", "mode": "hold" } },
        { "name": "Right trigger: off in the game", "input": "right.trigger", "gesture": "while_held", "block": "trigger",
          "action": { "type": "none" } }
      ]
    })JSON";
    std::string err;
    auto p = ParseProfileText(text, err);
    Action touchingLayer;
    touchingLayer.type = ActType::Layer;
    touchingLayer.layer = "touching";
    CHECK(p != nullptr && p->bindings.size() == 3, ("quick bindings: the profile reads " + err).c_str());
    if (!p) return;
    Engine e;
    e.SetProfile(p);
    Frame f;
    double t = 0;
    auto step = [&](int frames = 3) {
        for (int i = 0; i < frames; i++) {
            t += 0.011;
            f.t = t;
            e.Update(f);
        }
    };
    step();

    f.h[1].b1t = true; // resting a thumb on A
    step();
    CHECK(e.IsOn(touchingLayer), "quick bindings: touching A starts its binding");
    f.h[1].b1 = true; // and pressing it
    step();
    CHECK(!e.Blocked(1, C_BTN1), "quick bindings: a touch binding never hides the press from the game");
    f.h[1].b1 = f.h[1].b1t = false;
    step();
    CHECK(!e.IsOn(touchingLayer), "quick bindings: letting go of A ends it");

    f.h[1].b2 = true; // B acts as A
    step();
    CHECK(e.Blocked(1, C_BTN2), "quick bindings: the remapped button is hidden from the game while held");
    CHECK(e.Ov(1, C_BTN1).on && e.Ov(1, C_BTN1).press, "quick bindings: the game sees the other button pressed instead");
    f.h[1].b2 = false;
    step();
    CHECK(!e.Blocked(1, C_BTN2) && !e.Ov(1, C_BTN1).on, "quick bindings: letting go gives both buttons back");

    f.h[1].trig = 1.f; // trigger turned off in the game
    step();
    CHECK(e.Blocked(1, C_TRIGGER), "quick bindings: a button turned off is hidden from the game while used");
    f.h[1].trig = 0.f;
    step();
    CHECK(!e.Blocked(1, C_TRIGGER), "quick bindings: and given back when let go");
}

// "Only when held for a moment" (like the Bannerlord VR radial menu): holding A runs the binding and the game never
// sees A; a quick tap of A still reaches the game as a normal press.
void TestHoldOnlyQuickBinding() {
    const char* text = R"JSON({
      "bindings": [
        { "name": "Right A: held (hold)", "input": "right.a", "gesture": "hold", "hold_ms": 400, "tap_passthrough": true,
          "block": "trigger", "action": { "type": "layer", "layer": "held", "mode": "hold" } }
      ]
    })JSON";
    std::string err;
    auto p = ParseProfileText(text, err);
    CHECK(p != nullptr, ("hold only: the profile reads " + err).c_str());
    if (!p) return;
    Action heldLayer;
    heldLayer.type = ActType::Layer;
    heldLayer.layer = "held";
    Engine e;
    e.SetProfile(p);
    Frame f;
    double t = 0;
    bool sawPress = false;
    auto step = [&](double secs) {
        for (double end = t + secs; t < end;) {
            t += 0.011;
            f.t = t;
            e.Update(f);
            sawPress = sawPress || (e.Ov(1, C_BTN1).on && e.Ov(1, C_BTN1).press);
        }
    };
    step(0.05);

    f.h[1].b1 = true; // a quick tap
    step(0.15);
    CHECK(e.Blocked(1, C_BTN1) && !e.IsOn(heldLayer), "hold only: while A is down the game doesn't see it, nothing runs yet");
    f.h[1].b1 = false;
    sawPress = false;
    step(0.05);
    CHECK(sawPress, "hold only: a quick tap still reaches the game as a press of A");
    step(0.2);

    f.h[1].b1 = true; // a hold
    sawPress = false;
    step(0.6);
    CHECK(e.IsOn(heldLayer) && e.Blocked(1, C_BTN1), "hold only: held long enough, the binding runs and A stays hidden");
    f.h[1].b1 = false;
    step(0.15);
    CHECK(!sawPress && !e.IsOn(heldLayer) && !e.Blocked(1, C_BTN1), "hold only: after a hold the game gets no press of A, all let go");
}

// The guided setup of positions at the start of a game (setup_positions.json, in a throwaway settings folder).
// "When the wrist panels show" on its own (from the in-headset wrist settings): the panels follow their real rule,
// the card shows live numbers, A ends it and saves, the setup file is left alone.
void TestWristViewSetup() {
    std::string err;
    Engine e;
    e.SetProfile(ParseProfileText(DefaultProfileJson(), err));
    Frame f;
    double t = 0;
    f.headValid = true;
    f.head.position = { 0, 1.6f, 0 };
    for (auto& h : f.h) { h.gripValid = h.aimValid = true; h.gripPose = h.aimPose = { { 0, 0, 0, 1 }, { 0, 1.2f, -0.3f } }; }
    auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
    step(500);
    e.StartWristViewSetup();
    step(30);
    CHECK(e.adjust && e.wristViewTest && e.guideText.find("WHEN THE WRIST PANELS SHOW") != std::string::npos &&
              e.guideText.find("Left:") != std::string::npos,
          "wrist view: the step starts, panels follow their real rule, the card has a line per panel");
    f.h[1].b1 = true; step(3); f.h[1].b1 = false; step(3);
    CHECK(!e.adjust && !e.wristViewTest && e.setup.step < 0, "wrist view: A ends it (adjust mode off)");
}

// Modes: turning one on (toggle / on) turns the others off, unless set to be together; a held mode stacks on top;
// the badge on the wrist panels and the wheel names the modes on; a binding only in a mode wins over the plain one.
void TestModes() {
    std::string err;
    auto p = ParseProfileText(R"({ "modes": { "driving": { "color": "#3080F0", "together": true } }, "bindings": [
      { "name": "Y = yes", "input": "right.b", "gesture": "press", "action": { "type": "key", "keys": "Y" } },
      { "name": "Y = K in combat", "input": "right.b", "gesture": "press", "layers": "combat", "action": { "type": "key", "keys": "K" } } ] })", err);
    CHECK(p && err.empty() && p->modes.count("driving") && p->modes["driving"].together, ("modes: the modes list reads " + err).c_str());
    if (!p) return;
    Engine e;
    e.SetProfile(p);
    auto run = [&](const char* l, ActMode m) { Action a; a.type = ActType::Layer; a.layer = l; a.mode = m; e.MenuRunAction(a); };
    run("combat", ActMode::Toggle);
    CHECK(e.ModeOn("combat") && e.wrist[0].mode == "COMBAT" && e.wrist[1].mode == "COMBAT", "modes: on, and the badge says so");
    run("looting", ActMode::On);
    CHECK(e.ModeOn("looting") && !e.ModeOn("combat") && e.wrist[0].mode == "LOOTING", "modes: turning another on turns the first off");
    run("driving", ActMode::Toggle);
    CHECK(e.ModeOn("driving") && e.ModeOn("looting"), "modes: a mode set to 'together' doesn't turn the others off");
    run("looting", ActMode::Toggle);
    CHECK(!e.ModeOn("looting") && e.ModeOn("driving") && e.wrist[0].mode == "DRIVING", "modes: toggle again turns it off");
    run("driving", ActMode::Off);
    CHECK(e.wrist[0].mode.empty(), "modes: none on, no badge");
    auto names = e.ModeNames();
    CHECK(std::find(names.begin(), names.end(), "combat") != names.end() && std::find(names.begin(), names.end(), "driving") != names.end(),
          "modes: the menu lists every mode the profile uses");
}

void TestSetupPositions() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring td = std::wstring(tmp) + L"vro_setup_test";
    std::error_code fec;
    std::filesystem::remove_all(td, fec);
    _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", td.c_str());
    auto slurp = [](const std::wstring& p) { std::ifstream i(p, std::ios::binary); std::stringstream s; s << i.rdbuf(); return s.str(); };
    std::string err;
    auto run = [&](bool pending, int presses, int& stepsSeen, bool& adjustDuring, bool& guideShown, bool& aBlocked) {
        if (pending) { std::ofstream o(Engine::SetupFile(), std::ios::binary); o << "{ \"pending\": true }"; }
        else std::filesystem::remove(Engine::SetupFile(), fec);
        auto ep = std::make_unique<Engine>();
        Engine& e = *ep;
        e.SetProfile(ParseProfileText(DefaultProfileJson(), err));
        Frame f;
        double t = 0;
        f.headValid = true;
        f.head.position = { 0, 1.6f, 0 };
        auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(500); // > 4 s tracked: the file is read
        stepsSeen = (int)e.setup.steps.size();
        adjustDuring = e.adjust;
        guideShown = !e.guideText.empty();
        aBlocked = e.Blocked(1, C_BTN1);
        for (int k = 0; k < presses; k++) { f.h[1].b1 = true; step(3); f.h[1].b1 = false; step(3); }
        return ep;
    };
    int n; bool adj, guide, blocked;
    {
        auto ep = run(true, 0, n, adj, guide, blocked);
        Engine& e = *ep;
        CHECK(n >= 4 && adj && guide, "setup: a pending file starts adjust mode with the guide card a few seconds in");
        CHECK(blocked, "setup: A is kept from the game while it runs");
        CHECK(e.guideText.find("1 of") != std::string::npos, "setup: the card says step 1 of N");
        std::vector<uint32_t> px;
        DrawGuide(px, 1400, 400, e.guideText, e.prof->style);
        SavePng(px, 1400, 400, L"setup_guide.png"); // for review
    }
    {
        int all = n;
        auto ep = run(true, all, n, adj, guide, blocked); // A through every step
        Engine& e = *ep;
        CHECK(!e.adjust && e.guideText.empty() && e.setup.step < 0 && e.saveRequested, "setup: A through every step ends it and saves the positions");
        std::string f = slurp(Engine::SetupFile());
        CHECK(f.find("\"pending\": false") != std::string::npos, "setup: afterwards the file says pending false (runs once)");
    }
    {
        auto ep = run(false, 0, n, adj, guide, blocked);
        Engine& e = *ep;
        CHECK(!adj && !guide && e.setup.step < 0, "setup: without the file nothing happens");
    }
    {
        auto ep = run(true, 0, n, adj, guide, blocked);
        Engine& e = *ep;
        Action a;
        a.type = ActType::Adjust;
        e.MenuRunAction(a); // the Calibrate button: adjust off
        Frame f;
        f.headValid = true;
        f.t = 99;
        e.Update(f);
        CHECK(e.setup.step < 0 && e.guideText.empty(), "setup: the Calibrate button ends it early (positions kept)");
    }
    _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", L"");
    std::filesystem::remove_all(td, fec);
}

// Stick options (all off by default): deadzones for what the game sees, direction zones for bindings, and the
// double flick (double tap and hold) gesture.
void TestStickOptions() {
    // Shaping alone.
    {   // the direction zone straightens the game's stick: up and 15 degrees right is plain up; a diagonal is left alone
        StickShape z;
        z.directionAngleDeg = 30;
        float x = std::sin(15 * 3.14159265f / 180), y = std::cos(15 * 3.14159265f / 180);
        ShapeStick(z, x, y);
        CHECK(std::fabs(x) < 1e-5f && std::fabs(y - 1) < 1e-4f, "stick: inside the up zone the game gets plain up (no turning)");
        float dx = 0.7f, dy = 0.7f;
        ShapeStick(z, dx, dy);
        CHECK(std::fabs(dx - 0.7f) < 1e-5f && std::fabs(dy - 0.7f) < 1e-5f, "stick: a diagonal outside every zone is left as it is");
        float rx = 0.9f, ry = -0.2f;
        ShapeStick(z, rx, ry);
        CHECK(std::fabs(ry) < 1e-5f && rx > 0.9f, "stick: right and a little down is plain right, same length");
    }
    {   // ignore outside: a diagonal gives the game no push at all; inside the zones it still straightens
        StickShape z;
        z.directionAngleDeg = 30;
        z.ignoreOutside = true;
        float dx = 0.7f, dy = 0.7f;
        ShapeStick(z, dx, dy);
        CHECK(dx == 0.f && dy == 0.f, "stick: ignore outside: a diagonal gives the game no push (no jump, no turn)");
        float x = std::sin(20 * 3.14159265f / 180), y = std::cos(20 * 3.14159265f / 180);
        ShapeStick(z, x, y);
        CHECK(x == 0.f && std::fabs(y - 1) < 1e-4f, "stick: ignore outside: up and 20 degrees right is still plain up (jump, no turn)");
        z.ignoreCenter = 0.7f; // "Ignore small pushes below 70%"
        float sx = 0.f, sy = 0.5f; // straight up but only half way
        ShapeStick(z, sx, sy);
        CHECK(sx == 0.f && sy == 0.f, "stick: ignore outside: a push shorter than 'direction counts at' gives the game nothing");
        float fx = 0.f, fy = 0.8f;
        ShapeStick(z, fx, fy);
        CHECK(fx == 0.f && fy > 0.79f, "stick: ignore outside: a firm straight push still reaches the game");
        StickShape off;
        off.ignoreOutside = true; // without a direction zone it does nothing
        float ox = 0.7f, oy = 0.7f;
        ShapeStick(off, ox, oy);
        CHECK(ox == 0.7f && oy == 0.7f, "stick: ignore outside does nothing without a direction zone");
        std::string err;
        auto p = ParseProfileText(R"({"sticks":{"right":{"direction_angle":25,"ignore_outside":true}}})", err);
        CHECK(p && p->sticks[1].ignoreOutside && p->sticks[1].directionAngleDeg == 25 && !p->sticks[0].ignoreOutside,
              "stick: ignore_outside is read from the profile");
        CHECK(p && p->sticks[1].ignoreCenter == 0.f, "stick: ignore_center is off (0) unless set");
        auto d = ParseProfileText(DefaultProfileJson(), err);
        bool allOff = d != nullptr;
        for (int h = 0; d && h < 2; h++)
            allOff = allOff && !d->sticks[h].ChangesGame() && !d->sticks[h].ignoreOutside && d->sticks[h].ignoreCenter == 0.f;
        CHECK(allOff, "stick: the built-in default has every stick option off (no deadzones, zones, ignoring)");
    }
    StickShape off;
    float x = 0.1f, y = 0.f;
    ShapeStick(off, x, y);
    CHECK(x == 0.1f, "sticks: with nothing set the stick passes through unchanged");
    StickShape dz;
    dz.innerDeadzone = 0.2f;
    dz.outerDeadzone = 0.2f;
    x = 0.15f, y = 0.f;
    ShapeStick(dz, x, y);
    CHECK(x == 0.f, "sticks: a push inside the inner deadzone counts as none");
    x = 0.f, y = 0.5f;
    ShapeStick(dz, x, y);
    CHECK(std::fabs(y - 0.5f) < 1e-4f && x == 0.f, "sticks: the rest is stretched (halfway stays halfway with 0.2 / 0.2)");
    x = 0.6f, y = 0.6f; // 0.85 long: past the outer deadzone
    ShapeStick(dz, x, y);
    CHECK(std::fabs(std::sqrt(x * x + y * y) - 1.f) < 1e-4f && std::fabs(x - y) < 1e-5f, "sticks: past the outer deadzone is full, same direction");

    // The game's stick through the filter: shaped per hand.
    std::string err;
    auto p = ParseProfileText(R"JSON({ "sticks": { "right": { "inner_deadzone": 0.25 } },
      "bindings": [
        { "name": "Up = layer up", "input": "right.thumbstick.up", "gesture": "while_held", "block": "none",
          "action": { "type": "layer", "layer": "up", "mode": "hold" } },
        { "name": "Right = layer right", "input": "right.thumbstick.right", "gesture": "while_held", "block": "none",
          "action": { "type": "layer", "layer": "right", "mode": "hold" } },
        { "name": "Sprint", "input": "left.thumbstick.up", "gesture": "double_tap_hold", "double_tap_ms": 350, "block": "none",
          "action": { "type": "layer", "layer": "sprint", "mode": "hold" } }
      ] })JSON",
                              err);
    CHECK(p != nullptr && p->sticks[1].innerDeadzone == 0.25f && p->sticks[0].innerDeadzone == 0.f,
          ("sticks: the profile's stick options read, per hand " + err).c_str());
    if (!p) return;
    Engine e;
    e.SetProfile(p);
    GameInputFilter gi;
    gi.SetHandPaths(101, 102);
    XrAction move = (XrAction)(uintptr_t)90;
    gi.AddBinding(move, 0, C_STICK, InputFeature::Vector);
    gi.AddBinding(move, 1, C_STICK, InputFeature::Vector);
    auto noRequery = [](XrPath, XrActionStateVector2f&) {
        return false;
    };
    XrActionStateVector2f st{ XR_TYPE_ACTION_STATE_VECTOR2F };
    st.isActive = XR_TRUE;
    st.currentState = { 0.2f, 0.f };
    gi.NewSync();
    gi.Filter(e, move, 102, 1, st, noRequery);
    CHECK(st.currentState.x == 0.f, "sticks: the game sees no push inside the right stick's inner deadzone");
    st.currentState = { 0.2f, 0.f };
    gi.Filter(e, move, 101, 1, st, noRequery);
    CHECK(st.currentState.x == 0.2f, "sticks: the left stick (nothing set) passes through");
    st.currentState = { 0.2f, 0.f };
    gi.Filter(e, move, XR_NULL_PATH, 1, st, noRequery);
    CHECK(st.currentState.x == 0.2f, "sticks: both hands asked at once, the left stick pushed further (nothing set): left alone");
    {   // A zone on the right stick only; the game reads one action from both sticks, and on/off actions on the axes.
        std::string err2;
        auto pz = ParseProfileText(R"JSON({ "sticks": { "right": { "direction_angle": 20 } } })JSON", err2);
        Engine ez;
        ez.SetProfile(pz);
        GameInputFilter gz;
        gz.SetHandPaths(101, 102);
        XrAction both = (XrAction)(uintptr_t)91, turnX = (XrAction)(uintptr_t)92, jumpY = (XrAction)(uintptr_t)93;
        gz.AddBinding(both, 0, C_STICK, InputFeature::Vector);
        gz.AddBinding(both, 1, C_STICK, InputFeature::Vector);
        gz.AddBinding(turnX, 1, C_STICK, InputFeature::X);
        gz.AddBinding(jumpY, 1, C_STICK, InputFeature::Y);
        ez.stickX[1] = 0.25f; // up and about 15 degrees right, on the right stick
        ez.stickY[1] = 0.9f;
        XrActionStateVector2f v{ XR_TYPE_ACTION_STATE_VECTOR2F };
        v.isActive = XR_TRUE;
        v.currentState = { 0.25f, 0.9f };
        gz.NewSync();
        gz.Filter(ez, both, XR_NULL_PATH, 1, v, noRequery);
        CHECK(v.currentState.x == 0.f && v.currentState.y > 0.9f,
              "sticks: one action on both sticks, asked for any hand: the right stick's zone straightens it (was skipped)");
        auto noBool = [](XrPath, XrActionStateBoolean&) { return false; };
        XrActionStateBoolean b{ XR_TYPE_ACTION_STATE_BOOLEAN };
        b.isActive = XR_TRUE;
        b.currentState = XR_TRUE; // what the runtime made of x = 0.25 (a turn on any push)
        gz.Filter(ez, turnX, 102, 1, b, noBool);
        CHECK(b.currentState == XR_FALSE, "sticks: an on/off turn on the stick's x is off while the push is straightened to up");
        b.currentState = XR_TRUE;
        gz.Filter(ez, jumpY, 102, 1, b, noBool);
        CHECK(b.currentState == XR_TRUE, "sticks: the on/off jump on y still fires");
        ez.stickX[1] = 0.7f; // a real diagonal: outside the zone, left as it is
        ez.stickY[1] = 0.7f;
        b.currentState = XR_TRUE;
        gz.Filter(ez, turnX, 102, 1, b, noBool);
        CHECK(b.currentState == XR_TRUE, "sticks: on a diagonal (outside the zone) the turn is left to the game");
    }

    // Direction zones: none set, a diagonal is both up and right; with 30 degrees it is neither.
    Action upLayer, rightLayer, sprintLayer;
    upLayer.type = rightLayer.type = sprintLayer.type = ActType::Layer;
    upLayer.layer = "up";
    rightLayer.layer = "right";
    sprintLayer.layer = "sprint";
    Frame f;
    double t = 0;
    auto step = [&](double secs) {
        for (double end = t + secs; t < end;) {
            t += 0.011;
            f.t = t;
            e.Update(f);
        }
    };
    f.h[1].sx = 0.75f;
    f.h[1].sy = 0.8f;
    step(0.05);
    CHECK(e.IsOn(upLayer) && e.IsOn(rightLayer), "sticks: no direction zone: a diagonal counts as up and right (as before)");
    f.h[1].sx = f.h[1].sy = 0;
    step(0.05);
    p->sticks[1].directionAngleDeg = 30;
    f.h[1].sx = 0.75f;
    f.h[1].sy = 0.8f;
    step(0.05);
    CHECK(!e.IsOn(upLayer) && !e.IsOn(rightLayer), "sticks: 30 degree zones: a diagonal counts as neither");
    f.h[1].sx = 0.3f;
    f.h[1].sy = 0.95f;
    step(0.05);
    CHECK(e.IsOn(upLayer) && !e.IsOn(rightLayer), "sticks: forward leaning a little right counts only as up");
    f.h[1].sx = f.h[1].sy = 0;
    step(0.05);

    // Double flick forward and hold = sprint while held; a single flick does nothing.
    f.h[0].sy = 1.f;
    step(0.08);
    f.h[0].sy = 0.f;
    step(0.4);
    CHECK(!e.IsOn(sprintLayer), "sticks: one flick forward doesn't start the double flick binding");
    f.h[0].sy = 1.f;
    step(0.08);
    f.h[0].sy = 0.f;
    step(0.1);
    f.h[0].sy = 1.f;
    step(0.3);
    CHECK(e.IsOn(sprintLayer), "sticks: a second flick soon after starts it, held while pushed");
    f.h[0].sy = 0.f;
    step(0.05);
    CHECK(!e.IsOn(sprintLayer), "sticks: letting the stick back ends it");
}

// The Steam Frame: its own parts in profiles and game paths, and the game's Index / Touch inputs moved onto it.
void TestSteamFrame() {
    InputId id;
    CHECK(ParseInput("right.button3", id) && id.comp == C_BTN3 && InputName(id) == "right.button3", "frame: right X is button3");
    CHECK(ParseInput("left.dpad_up.touch", id) && id.comp == C_DPAD_UP && id.feat == F_TOUCH, "frame: left D-pad up parses");
    CHECK(ParseInput("left.bumper", id) && InputName(id) == "left.bumper", "frame: the bumper round-trips");
    int hand, comp;
    InputFeature f;
    CHECK(ParseGameInputPath("/user/hand/right/input/x/click", hand, comp, f) && comp == C_BTN3, "frame: the game's right X is BTN3");
    CHECK(ParseGameInputPath("/user/hand/left/input/x/click", hand, comp, f) && comp == C_BTN1, "frame: the left X stays BTN1");
    CHECK(ParseGameInputPath("/user/hand/left/input/view/click", hand, comp, f) && comp == C_MENU, "frame: View is the menu");
    CHECK(ParseGameInputPath("/user/hand/left/input/dpad_left/click", hand, comp, f) && comp == C_DPAD_LEFT, "frame: D-pad left");
    const std::string idx = "/interaction_profiles/valve/index_controller", touch = "/interaction_profiles/oculus/touch_controller";
    CHECK(SteamFramePathFor(idx, "/user/hand/right/input/a/click") == "/user/hand/right/input/a/click", "frame: Index right A stays A");
    CHECK(SteamFramePathFor(idx, "/user/hand/left/input/a/click") == "/user/hand/left/input/dpad_down/click", "frame: Index left A is D-pad down");
    CHECK(SteamFramePathFor(idx, "/user/hand/left/input/b/touch") == "/user/hand/left/input/dpad_up/touch", "frame: Index left B is D-pad up");
    CHECK(SteamFramePathFor(touch, "/user/hand/left/input/y/click") == "/user/hand/left/input/dpad_up/click", "frame: Touch left Y is D-pad up");
    CHECK(SteamFramePathFor(touch, "/user/hand/left/input/menu/click") == "/user/hand/left/input/view/click", "frame: Touch menu is View");
    CHECK(SteamFramePathFor(idx, "/user/hand/right/input/system/click") == "/user/hand/right/input/menu/click", "frame: Index system is Menu");
    CHECK(SteamFramePathFor(idx, "/user/hand/left/input/squeeze/force") == "/user/hand/left/input/squeeze/value", "frame: grip force is value");
    CHECK(SteamFramePathFor(idx, "/user/hand/right/input/thumbstick/y") == "/user/hand/right/input/thumbstick/y", "frame: stick stays");
    CHECK(SteamFramePathFor(idx, "/user/hand/right/input/trackpad/x").empty(), "frame: no trackpad on the Frame");
    CHECK(SteamFramePathFor(touch, "/user/hand/right/input/thumbrest/touch").empty(), "frame: no thumbrest on the Frame");
    CHECK(SteamFramePathFor(idx, "/user/hand/right/input/aim/pose") == "/user/hand/right/input/aim/pose", "frame: poses stay");
    CHECK(ControllerKindOf("/interaction_profiles/valve/frame_controller_valve") == ControllerKind::SteamFrame, "frame: the kind");
    CHECK(ParseInput("right.trackpad.up", id) && id.comp == C_TRACKPAD && id.feat == F_UP && InputName(id) == "right.trackpad.up",
          "trackpad: its own part, with swipe directions");
    CHECK(ParseGameInputPath("/user/hand/right/input/trackpad/force", hand, comp, f) && comp == C_TRACKPAD, "trackpad: the game's trackpad");
    CHECK(ParseInput("right.button3.touch", id) && id.comp == C_BTN3 && id.feat == F_TOUCH, "frame: X has touch");
    CHECK(ParseInput("left.grip.full", id) && id.comp == C_GRIP && id.feat == F_FULL && InputName(id) == "left.grip.full",
          "frame: the grip's click at the end (full squeeze)");
    CHECK(!ParseInput("left.a.full", id), "frame: only triggers and grips have a full pull");
}

void TestPressPoints() { // a binding's own press point (hair trigger, long pull) and the Index grip's full squeeze (force)
    std::string err;
    auto p = ParseProfileText(R"({ "bindings": [
      { "name": "hair", "input": "right.trigger", "gesture": "while_held", "press_at": 0.1,
        "action": { "type": "layer", "layer": "hair", "mode": "hold" } },
      { "name": "long", "input": "left.trigger", "gesture": "while_held", "press_at": 0.9,
        "action": { "type": "layer", "layer": "long", "mode": "hold" } },
      { "name": "full", "input": "left.grip.full", "gesture": "while_held",
        "action": { "type": "layer", "layer": "full", "mode": "hold" } } ] })", err);
    CHECK(p && err.empty(), ("press points: profile parses: " + err).c_str());
    if (!p) return;
    CHECK(std::fabs(p->bindings[0].pressAt - 0.1f) < 1e-4f, "press points: press_at is read");
    Engine e;
    e.SetProfile(p);
    Frame f;
    f.headValid = true;
    for (auto& h : f.h) {
        h.kind = ControllerKind::Index;
        h.gripValid = h.aimValid = true;
        h.aimPose = h.gripPose = { { 0, 0, 0, 1 }, { 0, 1.f, -0.3f } };
    }
    double t = 1.0;
    auto step = [&] { t += 0.011; f.t = t; e.Update(f); };
    auto on = [&](int i) { return e.IsOn(p->bindings[i].action); };
    step();
    f.h[1].trig = 0.15f; step();
    CHECK(on(0), "press points: a hair trigger fires at 15%");
    f.h[1].trig = 0.05f; step();
    CHECK(!on(0), "press points: and lets go below it");
    f.h[0].trig = 0.8f; step();
    CHECK(!on(1), "press points: a long pull doesn't fire at 80%");
    f.h[0].trig = 0.95f; step();
    CHECK(on(1), "press points: but does at 95%");
    f.h[0].trig = 0.85f; step();
    CHECK(on(1), "press points: a small wobble below the point keeps it held");
    f.h[0].trig = 0; step();
    f.h[0].grip = 1.f; f.h[0].gripForce = 0.2f; step();
    CHECK(!on(2), "index full squeeze: a closed grip alone isn't a full squeeze");
    f.h[0].gripForce = 0.6f; step();
    CHECK(on(2), "index full squeeze: a firm squeeze (force) is");
    f.h[0].gripForce = 0.1f; step();
    CHECK(!on(2), "index full squeeze: easing off ends it");
}

void TestDoubleFlickRelease() { // a double flick with release_at: steer freely while it runs, stops when the stick comes back to the middle
    std::string err;
    auto p = ParseProfileText(R"({ "sticks": { "left": { "direction_angle": 30 } }, "bindings": [
      { "name": "sprint", "input": "left.thumbstick.up", "gesture": "double_tap_hold", "double_tap_ms": 350, "release_at": 0.3, "block": "none",
        "action": { "type": "layer", "layer": "sprint", "mode": "hold" } },
      { "name": "old", "input": "left.thumbstick.down", "gesture": "double_tap_hold", "double_tap_ms": 350, "block": "none",
        "action": { "type": "layer", "layer": "old", "mode": "hold" } } ] })", err);
    CHECK(p && err.empty(), ("double flick: profile parses: " + err).c_str());
    if (!p) return;
    CHECK(std::fabs(p->bindings[0].releaseAt - 0.3f) < 1e-4f && p->bindings[1].releaseAt == 0, "double flick: release_at is read");
    Engine e;
    e.SetProfile(p);
    Frame f;
    f.headValid = true;
    for (auto& h : f.h) { h.gripValid = h.aimValid = true; h.aimPose = h.gripPose = { { 0, 0, 0, 1 }, { 0, 1.f, -0.3f } }; }
    double t = 1.0;
    auto step = [&] { t += 0.02; f.t = t; e.Update(f); };
    auto on = [&](int i) { return e.IsOn(p->bindings[i].action); };
    auto flick = [&](float x, float y) { f.h[0].sx = x; f.h[0].sy = y; step(); f.h[0].sx = f.h[0].sy = 0; step(); };
    step();
    flick(0, 1);
    f.h[0].sy = 1; step();
    CHECK(on(0), "double flick: a second flick forward starts it");
    f.h[0].sx = 0.9f; f.h[0].sy = 0.3f; step(); // steered well out of the 30 degree zone
    CHECK(on(0), "double flick: steering sideways keeps it going");
    f.h[0].sx = 0.2f; f.h[0].sy = 0.1f; step();
    CHECK(!on(0), "double flick: letting the stick back toward the middle stops it");
    f.h[0].sx = f.h[0].sy = 0; step(); step();
    t += 1.0;
    flick(0, -1);
    f.h[0].sy = -1; step();
    CHECK(on(1), "double flick (old way): starts");
    f.h[0].sx = 0.9f; f.h[0].sy = -0.3f; step();
    CHECK(!on(1), "double flick (old way): leaving the direction zone stops it");
}

void TestDoubleTapSingle() { // double-tap with a single-tap action: a lone tap runs it after the window; a double tap or a long press never does
    std::string err;
    auto p = ParseProfileText(R"({ "modes": { "dbl": { "together": true }, "single": { "together": true }, "dbl2": { "together": true } },
      "bindings": [
      { "name": "wait", "input": "left.thumbstick", "gesture": "double_tap", "double_tap_ms": 350, "block": "trigger",
        "action": { "type": "layer", "layer": "dbl", "mode": "toggle" },
        "tap_action": { "type": "layer", "layer": "single", "mode": "toggle" } },
      { "name": "plain", "input": "right.thumbstick", "gesture": "double_tap", "double_tap_ms": 350,
        "action": { "type": "layer", "layer": "dbl2", "mode": "toggle" } } ] })", err);
    CHECK(p && err.empty(), ("single tap: profile parses: " + err).c_str());
    if (!p) return;
    Engine e;
    e.SetProfile(p);
    Frame f;
    f.headValid = true;
    for (auto& h : f.h) { h.gripValid = h.aimValid = true; h.aimPose = h.gripPose = { { 0, 0, 0, 1 }, { 0, 1.f, -0.3f } }; }
    double t = 1.0;
    auto step = [&] { t += 0.02; f.t = t; e.Update(f); };
    auto wait = [&](double s) { for (double end = t + s; t < end;) step(); };
    auto layer = [&](const char* n) { Action a; a.type = ActType::Layer; a.layer = n; return e.IsOn(a); };
    auto click = [&](int h) { f.h[h].sc = true; step(); step(); f.h[h].sc = false; step(); };
    step();
    click(0);
    CHECK(!layer("single"), "single tap: not yet, inside the double-tap window");
    wait(0.4);
    CHECK(layer("single") && !layer("dbl"), "single tap: a lone tap runs the single-tap action after the window");
    wait(0.5);
    click(0);
    wait(0.05);
    click(0);
    wait(0.5);
    CHECK(layer("dbl") && layer("single"), "single tap: a double tap runs the double action only (single stays as it was)");
    f.h[0].sc = true;
    wait(0.6); // a long press
    f.h[0].sc = false;
    wait(0.5);
    CHECK(layer("single"), "single tap: a long press doesn't run it");
    click(1);
    wait(0.5);
    CHECK(!layer("dbl2"), "single tap: none set = a lone tap does nothing (off by default)");
}

void TestReloadRedraws() { // after a profile reload the wrist panels and keyboard get new version numbers (so they are drawn again)
    std::string err;
    auto p = ParseProfileText(DefaultProfileJson(), err);
    Engine e;
    e.SetProfile(p);
    e.wrist[0].version = 7;
    e.wrist[1].version = 3;
    e.kb.version = 5;
    auto q = ParseProfileText(DefaultProfileJson(), err);
    e.SetProfile(q);
    CHECK(e.wrist[0].version > 7 && e.wrist[1].version > 3 && e.kb.version > 5, "reload: panel versions keep counting up (old pictures are redrawn)");
}
