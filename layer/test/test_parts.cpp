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
