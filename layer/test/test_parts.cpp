// Tests of single parts of the layer (no engine flow): game hand offsets, the overlay, game input.
#include "test_support.h"
#include <map>

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
