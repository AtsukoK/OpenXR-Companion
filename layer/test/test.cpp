#include <sstream>
#include <filesystem>
#include <fstream>
#include "../src/panel.h"
#include <dxgi1_4.h>
#include "../src/keys.h"
#include "../src/screen.h"
#include "../src/mirror.h"
#include <thread>
#include <cstdio>
#include <objidl.h>
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
using namespace Gdiplus;

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static void SavePng(const std::vector<uint32_t>& px, int w, int h, const wchar_t* path) {
    Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)px.data());
    CLSID png; UINT n = 0, sz = 0; GetImageEncodersSize(&n, &sz);
    std::vector<BYTE> buf(sz); auto* enc = (ImageCodecInfo*)buf.data(); GetImageEncoders(n, sz, enc);
    for (UINT i = 0; i < n; i++) if (wcscmp(enc[i].MimeType, L"image/png") == 0) png = enc[i].Clsid;
    bmp.Save(path, &png);
}


// ---------------------------------------------------------------- a pretend OpenXR runtime under the real layer DLL
#include <openxr/openxr_loader_negotiation.h>
namespace fakert {
    static XrQuaternionf head{ 0, 0, 0, 1 };                 // the real head (yaw only here)
    static float (*yawAt)(XrTime) = nullptr;                 // or: the head turning over time
    static std::map<uint64_t, XrReferenceSpaceType> spaces;
    static uint64_t nextHandle = 100;
    static XrPosef submitted[2]; static bool gotFrame = false;
    static XrResult XRAPI_CALL Ok() { return XR_SUCCESS; }
    static XrResult XRAPI_CALL CreateInstance(const XrInstanceCreateInfo*, const XrApiLayerCreateInfo*, XrInstance* i) { *i = (XrInstance)(uintptr_t)1; return XR_SUCCESS; }
    static XrResult XRAPI_CALL CreateSession(XrInstance, const XrSessionCreateInfo*, XrSession* s) { *s = (XrSession)(uintptr_t)2; return XR_SUCCESS; }
    static XrResult XRAPI_CALL CreateReferenceSpace(XrSession, const XrReferenceSpaceCreateInfo* ci, XrSpace* sp) { uint64_t h = nextHandle++; spaces[h] = ci->referenceSpaceType; *sp = (XrSpace)(uintptr_t)h; return XR_SUCCESS; }
    static XrResult XRAPI_CALL CreateHandle(void*, const void*, uint64_t* out) { *out = nextHandle++; return XR_SUCCESS; }
    static XrResult XRAPI_CALL StringToPath(XrInstance, const char*, XrPath* p) { *p = nextHandle++; return XR_SUCCESS; }
    static XrResult XRAPI_CALL LocateSpace(XrSpace sp, XrSpace, XrTime t, XrSpaceLocation* loc) {
        loc->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
        bool view = spaces.count((uint64_t)sp) && spaces[(uint64_t)sp] == XR_REFERENCE_SPACE_TYPE_VIEW;
        XrQuaternionf h = head;
        if (yawAt) { float y = yawAt(t) * 0.0087266f; h = { 0, std::sin(y), 0, std::cos(y) }; }
        loc->pose = view ? XrPosef{ h, { 0, 1.6f, 0 } } : XrPosef{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
        return XR_SUCCESS;
    }
    static XrResult XRAPI_CALL LocateViews(XrSession, const XrViewLocateInfo* li, XrViewState* vs, uint32_t cap, uint32_t* cnt, XrView* v) {
        *cnt = 2; if (cap < 2) return XR_SUCCESS;
        vs->viewStateFlags = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
        bool rel = spaces.count((uint64_t)li->space) && spaces[(uint64_t)li->space] == XR_REFERENCE_SPACE_TYPE_VIEW;
        for (int i = 0; i < 2; i++) {
            Vec3 eye{ i ? 0.032f : -0.032f, 0, 0 };
            v[i].pose = rel ? XrPosef{ { 0, 0, 0, 1 }, X(eye) } : XrPosef{ head, X(Vec3{ 0, 1.6f, 0 } + Rotate(head, eye)) };
            v[i].fov = { -0.8f, 0.8f, 0.8f, -0.8f };
        }
        return XR_SUCCESS;
    }
    static XrResult XRAPI_CALL EndFrame(XrSession, const XrFrameEndInfo* fi) {
        for (uint32_t i = 0; i < fi->layerCount; i++)
            if (fi->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                auto* p = (const XrCompositionLayerProjection*)fi->layers[i];
                for (uint32_t k = 0; k < p->viewCount && k < 2; k++) submitted[k] = p->views[k].pose;
                gotFrame = true;
            }
        return XR_SUCCESS;
    }
    static XrResult XRAPI_CALL Gipa(XrInstance, const char* n, PFN_xrVoidFunction* fn) {
        std::string s = n;
        if (s == "xrCreateSession") *fn = (PFN_xrVoidFunction)CreateSession;
        else if (s == "xrCreateReferenceSpace") *fn = (PFN_xrVoidFunction)CreateReferenceSpace;
        else if (s == "xrLocateSpace") *fn = (PFN_xrVoidFunction)LocateSpace;
        else if (s == "xrLocateViews") *fn = (PFN_xrVoidFunction)LocateViews;
        else if (s == "xrEndFrame") *fn = (PFN_xrVoidFunction)EndFrame;
        else if (s == "xrStringToPath") *fn = (PFN_xrVoidFunction)StringToPath;
        else if (s == "xrCreateActionSet" || s == "xrCreateAction" || s == "xrCreateActionSpace") *fn = (PFN_xrVoidFunction)CreateHandle;
        else *fn = (PFN_xrVoidFunction)Ok;
        return XR_SUCCESS;
    }
}
static float YawDeg(const XrQuaternionf& q) { return 2.f * std::atan2(q.y, q.w) * 57.2958f; }

// Runs the real layer DLL over the pretend runtime, the way an Unreal game (head on its own) or a Unity game (eye views in
// the world) asks for the head; turns 90 degrees at once; returns the yaw the game got and the yaw the headset was given.
static bool RunHeadSmoothingThroughLayer(bool unrealStyle, XrTime startT, std::vector<float>& gameYaw, std::vector<float>& shownYaw, std::string& why, bool twoThreads = false) {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe; dir = dir.substr(0, dir.find_last_of(L"\\/"));
    HMODULE dll = LoadLibraryW((dir + L"\\OpenXRCompanion.dll").c_str());
    if (!dll) { why = "OpenXRCompanion.dll not found next to the tests"; return false; }
    auto negotiate = (PFN_xrNegotiateLoaderApiLayerInterface)GetProcAddress(dll, "xrNegotiateLoaderApiLayerInterface");
    XrNegotiateLoaderInfo li{ XR_LOADER_INTERFACE_STRUCT_LOADER_INFO, XR_LOADER_INFO_STRUCT_VERSION, sizeof(XrNegotiateLoaderInfo),
                              XR_CURRENT_LOADER_API_LAYER_VERSION, XR_CURRENT_LOADER_API_LAYER_VERSION, XR_CURRENT_API_VERSION, XR_CURRENT_API_VERSION };
    XrNegotiateApiLayerRequest rq{ XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST, XR_API_LAYER_INFO_STRUCT_VERSION, sizeof(XrNegotiateApiLayerRequest) };
    if (!negotiate || XR_FAILED(negotiate(&li, nullptr, &rq))) { why = "negotiation failed"; return false; }
    XrApiLayerNextInfo next{ XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO, XR_API_LAYER_NEXT_INFO_STRUCT_VERSION, sizeof(XrApiLayerNextInfo) };
    strcpy_s(next.layerName, "XR_APILAYER_NOVENDOR_openxr_companion");
    next.nextGetInstanceProcAddr = fakert::Gipa; next.nextCreateApiLayerInstance = fakert::CreateInstance; next.next = nullptr;
    XrApiLayerCreateInfo ci{ XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO, XR_API_LAYER_CREATE_INFO_STRUCT_VERSION, sizeof(XrApiLayerCreateInfo) };
    ci.nextInfo = &next;
    XrInstanceCreateInfo ii{ XR_TYPE_INSTANCE_CREATE_INFO }; strcpy_s(ii.applicationInfo.applicationName, "fake game");
    XrInstance inst;
    if (XR_FAILED(rq.createApiLayerInstance(&ii, &ci, &inst))) { why = "instance failed"; return false; }
    auto get = [&](const char* n) { PFN_xrVoidFunction f = nullptr; rq.getInstanceProcAddr(inst, n, &f); return f; };
    auto createSession = (PFN_xrCreateSession)get("xrCreateSession");
    auto createRef = (PFN_xrCreateReferenceSpace)get("xrCreateReferenceSpace");
    auto locateSpace = (PFN_xrLocateSpace)get("xrLocateSpace");
    auto locateViews = (PFN_xrLocateViews)get("xrLocateViews");
    auto endFrame = (PFN_xrEndFrame)get("xrEndFrame");
    XrSessionCreateInfo sc{ XR_TYPE_SESSION_CREATE_INFO }; XrSession sess;
    createSession(inst, &sc, &sess);
    XrReferenceSpaceCreateInfo rs{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO }; rs.poseInReferenceSpace.orientation.w = 1;
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE; XrSpace world; createRef(sess, &rs, &world);
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW; XrSpace headSp; createRef(sess, &rs, &headSp);
    XrTime t = startT; const XrTime step = 11111111; // 90 fps
    for (int frame = 0; frame < 330; frame++, t += step) {
        float yaw = frame < 150 ? 0.f : 90.f; // settle first, then turn
        fakert::head = { 0, std::sin(yaw * 0.0087266f), 0, std::cos(yaw * 0.0087266f) };
        XrPosef eyes[2];
        if (unrealStyle) { // the head on its own, the eyes relative to it
            if (twoThreads && frame % 3 != 0) { XrSpaceLocation ahead{ XR_TYPE_SPACE_LOCATION }; locateSpace(headSp, world, t + step, &ahead); } // the game thread already asks for the next frame
            XrSpaceLocation loc{ XR_TYPE_SPACE_LOCATION }; locateSpace(headSp, world, t, &loc);
            XrViewLocateInfo vl{ XR_TYPE_VIEW_LOCATE_INFO }; vl.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; vl.displayTime = t; vl.space = headSp;
            XrViewState vs{ XR_TYPE_VIEW_STATE }; XrView v[2]{ { XR_TYPE_VIEW }, { XR_TYPE_VIEW } }; uint32_t n;
            locateViews(sess, &vl, &vs, 2, &n, v);
            for (int k = 0; k < 2; k++) eyes[k] = PMul(loc.pose, v[k].pose);
            gameYaw.push_back(YawDeg(loc.pose.orientation));
        } else { // the eye views in the world
            XrViewLocateInfo vl{ XR_TYPE_VIEW_LOCATE_INFO }; vl.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; vl.displayTime = t; vl.space = world;
            XrViewState vs{ XR_TYPE_VIEW_STATE }; XrView v[2]{ { XR_TYPE_VIEW }, { XR_TYPE_VIEW } }; uint32_t n;
            locateViews(sess, &vl, &vs, 2, &n, v);
            for (int k = 0; k < 2; k++) eyes[k] = v[k].pose;
            gameYaw.push_back(YawDeg(v[0].pose.orientation));
        }
        XrCompositionLayerProjectionView pv[2]{ { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } };
        for (int k = 0; k < 2; k++) { pv[k].pose = eyes[k]; pv[k].fov = { -0.8f, 0.8f, 0.8f, -0.8f }; }
        XrCompositionLayerProjection pl{ XR_TYPE_COMPOSITION_LAYER_PROJECTION }; pl.space = world; pl.viewCount = 2; pl.views = pv;
        const XrCompositionLayerBaseHeader* lay[1] = { (const XrCompositionLayerBaseHeader*)&pl };
        XrFrameEndInfo fe{ XR_TYPE_FRAME_END_INFO }; fe.displayTime = t; fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE; fe.layerCount = 1; fe.layers = lay;
        fakert::gotFrame = false; endFrame(sess, &fe);
        shownYaw.push_back(fakert::gotFrame ? YawDeg(fakert::submitted[0].orientation) : -999.f);
    }
    return true;
}

// ---------------- D3D12 panel uploads on a software GPU (WARP), with a pretend swapchain of 3 images
namespace fake12 {
    static std::vector<ID3D12Resource*> imgs; static uint32_t next = 0, acquired = 0;
    static XrResult XRAPI_CALL Formats(XrSession, uint32_t cap, uint32_t* n, int64_t* f) { *n = 1; if (cap && f) f[0] = DXGI_FORMAT_B8G8R8A8_UNORM; return XR_SUCCESS; }
    static XrResult XRAPI_CALL Create(XrSession, const XrSwapchainCreateInfo*, XrSwapchain* sc) { *sc = (XrSwapchain)(uintptr_t)0x1234; return XR_SUCCESS; }
    static XrResult XRAPI_CALL Destroy(XrSwapchain) { return XR_SUCCESS; }
    static XrResult XRAPI_CALL Images(XrSwapchain, uint32_t cap, uint32_t* n, XrSwapchainImageBaseHeader* h) {
        *n = (uint32_t)imgs.size();
        if (cap && h) for (uint32_t i = 0; i < cap && i < imgs.size(); i++) ((XrSwapchainImageD3D12KHR*)h)[i].texture = imgs[i];
        return XR_SUCCESS;
    }
    static XrResult XRAPI_CALL Acquire(XrSwapchain, const XrSwapchainImageAcquireInfo*, uint32_t* i) { *i = acquired = next; next = (next + 1) % (uint32_t)imgs.size(); return XR_SUCCESS; }
    static XrResult XRAPI_CALL Wait(XrSwapchain, const XrSwapchainImageWaitInfo*) { return XR_SUCCESS; }
    static XrResult XRAPI_CALL Release(XrSwapchain, const XrSwapchainImageReleaseInfo*) { return XR_SUCCESS; }
}
// Uploads several frames through Panel::Upload and reads one image back. false + why when D3D12/WARP is missing.
static bool D3D12PanelRun(std::vector<uint32_t>& got, uint32_t& want, int& uploads, double& msPerUpload, std::string& why) {
    IDXGIFactory4* fac = nullptr; IDXGIAdapter* warp = nullptr; ID3D12Device* dev = nullptr; ID3D12CommandQueue* q = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&fac))) || FAILED(fac->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
        FAILED(D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) { why = "(no D3D12 WARP device)"; return false; }
    D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q));
    const int W = 64, H = 32;
    for (int i = 0; i < 3; i++) {
        D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC td{}; td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = W; td.Height = H; td.DepthOrArraySize = 1; td.MipLevels = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1; td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        ID3D12Resource* t = nullptr; dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&t));
        fake12::imgs.push_back(t);
    }
    XrFns f; f.EnumerateSwapchainFormats = fake12::Formats; f.CreateSwapchain = fake12::Create; f.DestroySwapchain = fake12::Destroy;
    f.EnumerateSwapchainImages = fake12::Images; f.AcquireSwapchainImage = fake12::Acquire; f.WaitSwapchainImage = fake12::Wait; f.ReleaseSwapchainImage = fake12::Release;
    XrGraphicsBindingD3D12KHR gb{ XR_TYPE_GRAPHICS_BINDING_D3D12_KHR }; gb.device = dev; gb.queue = q;
    Gfx g; g.Init(&gb);
    Panel pn;
    bool ok = pn.Create((XrSession)(uintptr_t)1, g, f, W, H);
    std::vector<uint32_t> px(W * H);
    uploads = 10;
    double t0 = NowSec();
    for (int i = 0; i < uploads && ok; i++) { want = 0xFF000000u | (uint32_t)(i * 20) << 16 | 0x3366u; std::fill(px.begin(), px.end(), want); ok = pn.Upload(g, f, px); }
    msPerUpload = (NowSec() - t0) * 1000.0 / uploads;
    if (ok) { // read back the last image written
        ID3D12Resource* tex = fake12::imgs[fake12::acquired];
        D3D12_RESOURCE_DESC desc = tex->GetDesc(); D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; UINT64 total = 0;
        dev->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, nullptr, &total);
        D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_READBACK };
        D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource* rb = nullptr; dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb));
        ID3D12CommandAllocator* al = nullptr; ID3D12GraphicsCommandList* l = nullptr;
        dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al));
        dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al, nullptr, IID_PPV_ARGS(&l));
        D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = tex; b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE; l->ResourceBarrier(1, &b);
        D3D12_TEXTURE_COPY_LOCATION src{ tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX }; src.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION dst{ rb, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT }; dst.PlacedFootprint = fp;
        l->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        std::swap(b.Transition.StateBefore, b.Transition.StateAfter); l->ResourceBarrier(1, &b);
        l->Close(); ID3D12CommandList* ls[] = { l }; q->ExecuteCommandLists(1, ls);
        q->Signal(g.fence, ++g.fenceVal); g.WaitFence(g.fenceVal);
        uint8_t* m = nullptr; D3D12_RANGE rr{ 0, (SIZE_T)total }; rb->Map(0, &rr, (void**)&m);
        got.resize(W * H);
        for (int y = 0; y < H; y++) memcpy(got.data() + y * W, m + fp.Offset + (size_t)y * fp.Footprint.RowPitch, W * 4);
        rb->Unmap(0, nullptr); rb->Release(); l->Release(); al->Release();
    }
    pn.Destroy(f); g.Release();
    for (auto t : fake12::imgs) t->Release();
    fake12::imgs.clear();
    q->Release(); dev->Release(); warp->Release(); fac->Release();
    if (!ok) why = "(panel create/upload failed)";
    return ok;
}

int main(int argc, char** argv) {
    // Always the app's built-in default profile, never the user's own bindings.
    std::string err;
    auto p = ParseProfileText(DefaultProfileJson(), err);
    CHECK(p != nullptr, ("built-in default profile parses " + err).c_str());
    if (!p) return 1;
    p->notifications = true;
    p->sel.method = "thumbstick"; // the tests drive the stick, whatever the user picked
    p->blockWhileOpen = "hand";
    {
        for (auto& b : p->bindings) if (b.action.type == ActType::Wheel) b.enabled = false;
        Binding ob; ob.name = "test wheel"; ob.chordText = "right.thumbrest";
        InputId r; ParseInput("right.thumbrest", r); ob.chord.push_back(r);
        ob.gesture = "double_tap"; ob.doubleTapMs = 400; ob.block = "none";
        ob.action.type = ActType::Wheel; ob.action.wheel = "right"; ob.action.confirm = "stick_return";
        p->bindings.insert(p->bindings.begin(), ob);
    }
    Engine e; e.SetProfile(p);
    Frame f; double t = 0;
    f.headValid = true; f.head.position = { 0, 1.6f, 0 };
    f.h[1].gripValid = true; f.h[1].gripPose.position = { 0.2f, 1.1f, -0.3f };
    auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };

    step(5);
    // double tap the right thumbrest
    f.h[1].rest = true; step(5); f.h[1].rest = false; step(5);
    CHECK(!e.view.visible, "single thumbrest tap does not open");
    f.h[1].rest = true; step(5);
    CHECK(e.view.visible, "double tap thumbrest opens wheel");
    CHECK(e.Blocked(1, C_STICK) && e.Blocked(1, C_TRIGGER), "right hand blocked while open");
    CHECK(!e.Blocked(0, C_TRIGGER), "left hand free while open");
    f.h[1].rest = false; step(3);
    CHECK(e.view.visible, "lifting the thumb keeps stick_return wheel open");
    // hover item 5 (More..., 6 items, up-left = 300 deg)
    int moreIdx = -1, nItems = (int)p->wheels["right"].items.size();
    for (int i = 0; i < nItems; i++) if (p->wheels["right"].items[i].action.type == ActType::Wheel) moreIdx = i;
    float ang = moreIdx * 6.2831853f / nItems;
    f.h[1].sx = std::sin(ang) * 0.95f; f.h[1].sy = std::cos(ang) * 0.95f; step(3);
    CHECK(moreIdx >= 0 && e.view.hovered == moreIdx, "stick toward the submenu item hovers it");
    std::vector<uint32_t> px;
    DrawWheel(px, p->style.texSize, *p, e.view);
    SavePng(px, p->style.texSize, p->style.texSize, L"wheel_main.png");
    f.h[1].sx = 0; f.h[1].sy = 0; step(3);
    CHECK(e.view.visible && e.view.wheel && e.view.wheel->name == "Right hand more (sub-wheel)", "stick return on submenu pushes 'more'");
    {
        auto& mw = p->wheels["Right hand more (sub-wheel)"].items; int bi = 0;
        for (int i = 0; i < (int)mw.size(); i++) if (mw[i].action.type == ActType::Back) bi = i;
        float a2 = bi * 6.2831853f / mw.size();
        f.h[1].sx = std::sin(a2) * 0.95f; f.h[1].sy = std::cos(a2) * 0.95f; step(3);
    }
    DrawWheel(px, p->style.texSize, *p, e.view);
    SavePng(px, p->style.texSize, p->style.texSize, L"wheel_more.png");
    f.h[1].sx = 0; f.h[1].sy = 0; step(3);
    CHECK(e.view.visible && e.view.wheel->name == "right", "selecting Back pops to the first wheel");
    f.h[1].b2 = true; step(2); f.h[1].b2 = false; step(2);
    CHECK(!e.view.visible, "B cancels wheel");
    CHECK(e.Blocked(1, C_BTN2) == false, "block released after B let go");

    // shift layer chord: left grip + left stick click, then A blocked & fires (A in layer)
    {   // the test brings its own layer binding, so the user's edits to default.json cannot break it
        Binding lb; lb.name = "test shift"; lb.chordText = "left.grip+left.thumbstick.click";
        InputId a, b; ParseInput("left.grip", a); ParseInput("left.thumbstick.click", b);
        lb.chord = { a, b }; lb.gesture = "while_held"; lb.block = "trigger";
        lb.action.type = ActType::Layer; lb.action.mode = "hold"; lb.action.layer = "testshift";
        p->bindings.push_back(lb);
        e.SetProfile(p);
    }
    f.h[0].grip = 1.f; step(3);
    CHECK(!e.Blocked(0, C_GRIP), "grip alone passes to game");
    f.h[0].sc = true; step(3);
    CHECK(e.Blocked(0, C_STICK), "layer trigger (stick click) blocked from game");
    CHECK(!e.Blocked(1, C_BTN1), "A not blocked before press");
    f.h[0].sc = false; step(3);
    f.h[0].grip = 0; step(10);
    CHECK(!e.Blocked(0, C_STICK), "stick unblocked after release");

    // ---------------- keyboard
    {
        Binding kbB; kbB.name = "kb"; kbB.chordText = "left.menu";
        InputId m; ParseInput("left.menu", m); kbB.chord.push_back(m);
        kbB.gesture = "press"; kbB.action.type = ActType::Keyboard;
        p->bindings.push_back(kbB);
        e.SetProfile(p);
        f.h[0].menu = true; step(2); f.h[0].menu = false; step(2);
        CHECK(e.kb.visible, "left menu opens keyboard");
        CHECK(e.Blocked(0, C_TRIGGER) && e.Blocked(1, C_TRIGGER), "both triggers hidden from game while typing");
        float uw, uh; const auto& keys = KbLayout(uw, uh);
        auto aimAt = [&](int hand, int key) {
            const KbKey& k = keys[key];
            float u = k.x + k.w / 2, v = k.y + k.h / 2;
            Vec3 local{ (u / uw - 0.5f) * e.kb.widthM, (0.5f - v / uh) * e.kb.heightM, 0 };
            Vec3 target = V(e.kb.pose.position) + Rotate(e.kb.pose.orientation, local);
            Vec3 from{ hand ? 0.15f : -0.15f, 1.25f, -0.15f };
            f.h[hand].aimValid = true;
            f.h[hand].aimPose.position = X(from);
            f.h[hand].aimPose.orientation = BillboardTo(target, from);
        };
        {
            int fk = 0; for (auto& k : keys) if (k.combo.size() >= 2 && k.combo[0] == 'F' && isdigit((unsigned char)k.combo[1])) fk++;
            CHECK(fk == 12 && keys[0].combo == "F1" && keys[11].combo == "F12", "keyboard has an F1-F12 row on top");
        }
        int shiftKey = -1, qKey = -1, closeKey = -1;
        for (int i = 0; i < (int)keys.size(); i++) {
            if (keys[i].kind == KB_SHIFT && shiftKey < 0) shiftKey = i;
            if (keys[i].combo == "q") qKey = i;
            if (keys[i].kind == KB_CLOSE) closeKey = i;
        }
        aimAt(1, shiftKey); aimAt(0, qKey); step(30);
        CHECK(e.kb.hover[1] == shiftKey, "right ray hovers Shift");
        CHECK(e.kb.hover[0] == qKey, "left ray hovers Q");
        {
            const KbKey& k = keys[qKey];
            Vec3 want = V(e.kb.pose.position) + Rotate(e.kb.pose.orientation, { ((k.x + k.w / 2) / uw - 0.5f) * e.kb.widthM, (0.5f - (k.y + k.h / 2) / uh) * e.kb.heightM, 0 });
            CHECK(e.pointer[0].on && e.pointer[1].on && Len(V(e.pointer[0].dotPose.position) - want) < 0.01f, "keyboard: both controllers show a pointer, the dot sits on the aimed key");
            CHECK(e.pointer[0].beamLen > 0.1f, "keyboard: the pointer beam reaches from the controller to the key");
        }
        f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        CHECK(e.kb.shift, "trigger on Shift latches shift");
        std::vector<uint32_t> kpx; int KW = 1500, KH = (int)(KW * uh / uw);
        DrawKeyboard(kpx, KW, KH, e.kb, p->style);
        SavePng(kpx, KW, KH, L"keyboard.png");
        aimAt(1, closeKey); step(3);
        f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        CHECK(!e.kb.visible, "Close key closes keyboard");
        f.h[0].menu = true; step(2); f.h[0].menu = false; step(2);
        f.h[1].b2 = true; step(2); f.h[1].b2 = false; step(2);
        CHECK(!e.kb.visible, "B closes keyboard");
    }

    // ---------------- wrist panel (left inner forearm, poked by the right hand)
    {
        Wheel tw; tw.name = "wristtest"; tw.title = "Wrist";
        const char* labels[] = { "Map", "Inventory", "Keyboard", "Save", "More" };
        for (int i = 0; i < 5; i++) {
            WheelItem it; it.label = labels[i];
            it.action.type = i == 2 ? ActType::Keyboard : ActType::Key; it.action.keys = "F13";
            if (i == 4) { it.action.type = ActType::Page; it.action.page = "second"; }
            tw.items.push_back(it);
        }
        Wheel second; second.name = "second"; second.title = "Second";
        { WheelItem it; it.label = "Esc"; it.action.type = ActType::Key; it.action.keys = "F13"; second.items.push_back(it); }
        p->wrist[0].pages["wristtest"] = tw; p->wrist[0].pages["second"] = second;
        p->wrist[0].enabled = true; p->wrist[0].start = "wristtest";
        p->wrist[0].face = "inner"; p->wrist[0].press = "both";
        e.SetProfile(p);
        f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} }; // looking straight ahead (-Z)
        // Left grip turned so the palm side faces the eyes, 0.4 m in front.
        f.h[0].gripValid = true;
        f.h[0].gripPose = XrPosef{ {0, -0.70710678f, 0, 0.70710678f}, {0, 1.6f, -0.4f} };
        f.h[1].aimValid = true; f.h[1].aimPose = XrPosef{ {0,0,0,1}, {0.3f, 1.2f, -0.3f} }; // right hand away
        step(5);
        CHECK(!e.wrist[0].visible, "wrist panel waits for the look to last");
        step(30);
        CHECK(e.wrist[0].visible, "turned wrist + look shows the wrist panel");
        std::vector<uint32_t> wpx; DrawWrist(wpx, 512, *p, p->wrist[0], e.wrist[0]);
        SavePng(wpx, 512, 512, L"wrist_panel.png");

        // Poke the third cell (Keyboard) from the front.
        std::vector<Cell> cells; Cell hd; WristLayout(5, p->wrist[0].columns, cells, hd);
        const Cell& c = cells[2];
        float u = c.x + c.w / 2, v = c.y + c.h / 2;
        auto tipAt = [&](float depth) {
            Vec3 l{ (u - 0.5f) * p->wrist[0].size, (0.5f - v) * p->wrist[0].size, depth + 0.015f };
            f.h[1].aimPose.orientation = e.wrist[0].pose.orientation; // pointing into the panel
            f.h[1].aimPose.position = X(V(e.wrist[0].pose.position) + Rotate(e.wrist[0].pose.orientation, l));
        };
        tipAt(0.03f); step(30);
        CHECK(e.wrist[0].hover == 2, "finger in front of a button hovers it");
        CHECK(!e.kb.visible, "hovering alone does not press");
        CHECK(e.wrist[0].cursor && e.wrist[0].cursorLevel > 0 && e.wrist[0].cursorLevel < 8, "touch cursor shows while the finger is close");
        {
            Vec3 lc = RotateInv(e.wrist[0].pose.orientation, V(e.wrist[0].cursorPose.position) - V(e.wrist[0].pose.position));
            CHECK(std::fabs(lc.x - (u - 0.5f) * p->wrist[0].size) < 0.002f && std::fabs(lc.y - (0.5f - v) * p->wrist[0].size) < 0.002f, "touch cursor sits under the fingertip");
            float sizeFar = e.wrist[0].cursorSize; tipAt(0.005f); step(1);
            CHECK(e.wrist[0].cursorSize < sizeFar && e.wrist[0].cursorLevel >= 7, "touch cursor shrinks as the finger closes in");
            std::vector<uint32_t> cpx; DrawCursor(cpx, 128, 4, p->style); SavePng(cpx, 128, 128, L"cursor_near.png");
            DrawCursor(cpx, 128, 8, p->style); SavePng(cpx, 128, 128, L"cursor_touch.png");
        }
        tipAt(-0.01f); step(2);
        CHECK(e.kb.visible, "poking through the button runs its action (keyboard)");
        CHECK(!e.wrist[0].visible, "wrist panel steps aside for the keyboard");
        f.h[1].b2 = true; step(2); f.h[1].b2 = false; step(2);

        // Laser at the More cell (index 4) + trigger opens the second page with a Back cell.
        step(40);
        {
            const Cell& m = cells[4];
            Vec3 l{ (m.x + m.w / 2 - 0.5f) * p->wrist[0].size, (0.5f - (m.y + m.h / 2)) * p->wrist[0].size, 0.25f };
            f.h[1].aimPose.orientation = e.wrist[0].pose.orientation;
            f.h[1].aimPose.position = X(V(e.wrist[0].pose.position) + Rotate(e.wrist[0].pose.orientation, l));
            step(3);
            f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(3);
            CHECK(e.wrist[0].visible && e.wrist[0].wheel && e.wrist[0].wheel->name == "second" && e.wrist[0].nested, "page button opens its page, with Back");
        }

        // Looking away hides it; the back of the wrist does not show the inner panel.
        f.h[1].aimPose.position = { 0.3f, 1.2f, -0.3f };
        step(40);
        CHECK(e.wrist[0].visible, "panel back after the keyboard closes");
        f.head.orientation = { 0, 0.5f, 0, 0.8660254f }; // head turned 60 degrees left
        step(50);
        CHECK(!e.wrist[0].visible, "looking away hides the wrist panel");
        f.head.orientation = { 0, 0, 0, 1 };
        f.h[0].gripPose.orientation = { 0, 0.70710678f, 0, 0.70710678f }; // palm facing away
        step(50);
        CHECK(!e.wrist[0].visible, "arm turned the other way keeps it hidden");

        // Right forearm, mirroring the left panel's pages.
        p->wrist[1] = p->wrist[0]; p->wrist[1].pages.clear(); p->wrist[1].mirror = true;
        e.SetProfile(p);
        f.h[0].gripValid = false;
        f.h[0].aimValid = true; f.h[0].aimPose = XrPosef{ {0,0,0,1}, {-0.3f, 1.2f, -0.3f} };
        f.h[1].gripValid = true;
        f.h[1].gripPose = XrPosef{ {0, 0.70710678f, 0, 0.70710678f}, {0, 1.6f, -0.4f} }; // palm side toward the eyes
        step(40);
        CHECK(e.wrist[1].visible && e.wrist[1].wheel && e.wrist[1].wheel->name == "wristtest", "right forearm shows the mirrored left pages");
        CHECK(!e.wrist[0].visible, "left panel stays hidden meanwhile");

        // Right forearm with its own pages.
        Wheel own; own.name = "rmain"; own.title = "Right";
        { WheelItem it; it.label = "Jump"; it.action.type = ActType::Key; it.action.keys = "F13"; own.items.push_back(it); }
        p->wrist[1].mirror = false; p->wrist[1].pages["rmain"] = own; p->wrist[1].start = "rmain";
        e.SetProfile(p);
        step(40);
        CHECK(e.wrist[1].visible && e.wrist[1].wheel && e.wrist[1].wheel->name == "rmain", "right forearm shows its own pages");
        std::vector<uint32_t> rpx; DrawWrist(rpx, 512, *p, p->wrist[1], e.wrist[1]);
        SavePng(rpx, 512, 512, L"wrist_right.png");

        // Left forearm mirroring the right panel's own pages.
        p->wrist[0].mirror = true;
        e.SetProfile(p);
        f.h[1].gripValid = false;
        f.h[1].aimValid = true; f.h[1].aimPose = XrPosef{ {0,0,0,1}, {0.3f, 1.2f, -0.3f} };
        f.h[0].gripValid = true;
        f.h[0].gripPose = XrPosef{ {0, -0.70710678f, 0, 0.70710678f}, {0, 1.6f, -0.4f} };
        step(40);
        CHECK(e.wrist[0].visible && e.wrist[0].wheel && e.wrist[0].wheel->name == "rmain", "left forearm can mirror the right panel");
    }

    // Index-style opener: double touch of B. Pressing B twice fast (looting) must not open it.
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        for (auto& b : q->bindings) if (b.action.type == ActType::Wheel) b.enabled = false;
        Binding ob; ob.name = "touch wheel"; ob.chordText = "right.secondary.touch";
        InputId r; ParseInput("right.secondary.touch", r); ob.chord.push_back(r);
        ob.gesture = "double_tap"; ob.doubleTapMs = 400; ob.block = "none";
        ob.action.type = ActType::Wheel; ob.action.wheel = "right"; ob.action.confirm = "stick_return";
        q->bindings.insert(q->bindings.begin(), ob);
        Engine e; e.SetProfile(q);
        Frame f; double t = 0;
        f.headValid = true; f.head.position = { 0, 1.6f, 0 };
        f.h[1].gripValid = true; f.h[1].gripPose.position = { 0.2f, 1.1f, -0.3f };
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(5);
        for (int k = 0; k < 2; k++) {   // two quick real presses: touch first, then click
            f.h[1].b2t = true; step(1); f.h[1].b2 = true; step(3); f.h[1].b2 = false; f.h[1].b2t = false; step(4);
        }
        CHECK(!e.view.visible, "pressing B twice fast does not open the double-touch wheel");
        step(60);
        f.h[1].b2t = true; step(4); f.h[1].b2t = false; step(4); f.h[1].b2t = true; step(4);
        CHECK(e.view.visible, "touching B twice without pressing opens the wheel");
    }

    // Adjust mode: grab a wrist panel / the open wheel with the grip, move and resize it, then save.
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        Binding ab; ab.name = "adjust"; ab.chordText = "left.x";
        InputId ai; ParseInput("left.x", ai); ab.chord.push_back(ai);
        ab.gesture = "press"; ab.block = "trigger"; ab.action.type = ActType::Adjust;
        q->bindings.push_back(ab);
        q->style.anchor = "hand_locked"; // a wheel at the hand is brought out in adjust mode (one in front of you is not)
        float elbow0 = q->wrist[0].towardElbow, size0 = q->wrist[0].size, down0 = q->style.headDown, dist0 = q->style.distance, rad0 = q->style.radius;
        Engine e; e.SetProfile(q);
        Frame f; double t = 0;
        f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        // Left arm down and away from the face: normally no panel.
        f.h[0].gripValid = true; f.h[0].gripPose = XrPosef{ {0,0,0,1}, {-0.3f, 1.0f, -0.2f} };
        f.h[1].gripValid = f.h[1].aimValid = true; f.h[1].gripPose = XrPosef{ {0,0,0,1}, {0.3f, 1.0f, -0.2f} }; f.h[1].aimPose = f.h[1].gripPose;
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(30);
        CHECK(!e.wrist[0].visible, "adjust: panel hidden before adjust mode");
        f.h[0].b1 = true; step(2); f.h[0].b1 = false; step(3);
        CHECK(e.adjust && e.wrist[0].visible && e.wrist[0].adjust, "adjust: mode on shows the wrist panel without looking at it");
        // Right grip grabs the left panel and moves 4 cm along +Z of the left grip (toward the elbow).
        Vec3 pc = V(e.wrist[0].pose.position);
        f.h[1].gripPose.position = X(pc + Vec3{ 0.01f, 0, 0 }); f.h[1].aimPose = f.h[1].gripPose; step(2);
        f.h[1].grip = 1; step(2);
        CHECK(e.Blocked(1, C_GRIP), "adjust: the grabbing grip is hidden from the game");
        f.h[1].gripPose.position = X(V(f.h[1].gripPose.position) + Vec3{ 0, 0, 0.04f }); f.h[1].aimPose = f.h[1].gripPose;
        f.h[1].sy = 1; step(10); f.h[1].sy = 0;
        XrQuaternionf q0 = e.wrist[0].pose.orientation;
        f.h[1].gripPose.orientation = { 0, 0, std::sin(0.2618f), std::cos(0.2618f) }; step(3); // turn the grabbing hand 30 degrees
        XrQuaternionf dq = QMul(e.wrist[0].pose.orientation, QInv(q0));
        float turned = 2.f * std::acos(std::clamp(std::fabs(dq.w), 0.f, 1.f)) * 57.2958f;
        CHECK(turned > 27.f && turned < 33.f, ("adjust: the panel turns with the grabbing hand, turned " + std::to_string(turned)).c_str());
        f.h[1].gripPose.orientation = { 0, 0, 0, 1 }; step(3);
        {
            XrPosef rel = PMul(PInv(f.h[0].gripPose), e.wrist[0].pose);
            CHECK(q->wrist[0].pose.set && std::fabs(rel.position.z - (q->wrist[0].pose.p.position.z)) < 0.002f, "adjust: the panel's place is kept relative to its arm");
        }
        (void)elbow0;
        CHECK(q->wrist[0].size > size0, "adjust: stick up while holding makes the panel bigger");
        CHECK(std::fabs(V(e.wrist[0].pose.position).z - (pc.z + 0.04f)) < 0.003f, "adjust: the panel follows the hand");
        f.h[1].grip = 0; step(3);
        // Open the right wheel (double tap thumbrest), grab it with the left grip, lift it 5 cm.
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.3f };
        step(70);
        CHECK(e.view.visible && e.wrist[0].visible, "adjust: the main wheel shows by itself, next to the wrist panels");
        Vec3 wc = V(e.view.pose.position);
        { XrVector3f k0 = f.h[0].gripPose.position, k1 = f.h[1].gripPose.position; // hands well away from the wheel
          f.h[0].gripPose.position = X(wc + Vec3{ -0.6f, -0.3f, 0 }); f.h[1].gripPose.position = X(wc + Vec3{ 0.6f, -0.3f, 0 }); step(20);
          f.h[0].gripPose.position = k0; f.h[1].gripPose.position = k1; }
        CHECK(e.view.alpha < 0.4f, ("adjust: the wheel is see-through while no hand is near it, alpha " + std::to_string(e.view.alpha)).c_str());
        f.h[0].gripPose.position = X(wc); step(2);
        f.h[0].grip = 1; step(2);
        f.h[0].gripPose.position = X(wc + Vec3{ 0, 0.05f, 0 }); f.h[0].sy = -1; step(10); f.h[0].sy = 0;
        CHECK(q->style.poses.count(q->style.anchor) && q->style.poses[q->style.anchor].set && std::fabs(V(e.view.pose.position).y - (wc.y + 0.05f)) < 0.004f,
              "adjust: dragging the wheel up raises it and keeps that spot");
        step(30); CHECK(e.view.alpha > 0.95f, ("adjust: the wheel is solid while it is held, alpha " + std::to_string(e.view.alpha)).c_str());
        (void)down0; (void)dist0;
        CHECK(q->style.radius < rad0, "adjust: stick down while holding makes the wheel smaller");
        f.h[0].grip = 0; step(3);
        f.h[0].b1 = true; step(2); f.h[0].b1 = false; step(3);
        CHECK(!e.adjust && e.saveRequested && !e.view.visible, "adjust: using it again leaves adjust mode, closes the wheel and asks to save");
    }

    // Adjust mode with the wheel in front of you: it is not brought out by itself (it got in the way)
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        Binding ab; ab.name = "adjust"; ab.chordText = "left.x";
        InputId ai; ParseInput("left.x", ai); ab.chord.push_back(ai);
        ab.gesture = "press"; ab.block = "trigger"; ab.action.type = ActType::Adjust;
        q->bindings.push_back(ab);
        q->style.anchor = "head_locked";
        Engine e; e.SetProfile(q);
        Frame f; double t = 0;
        f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose = f.h[h].aimPose = XrPosef{ {0,0,0,1}, {h ? 0.3f : -0.3f, 1.0f, -0.2f} }; }
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(5); f.h[0].b1 = true; step(2); f.h[0].b1 = false; step(100);
        CHECK(e.adjust && !e.view.visible, "adjust: a wheel set to appear in front of you is not brought out by itself");
    }

    // Adjust mode: the keyboard can be grabbed and placed; next time it opens there (relative to where you face)
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        auto bind = [&](const char* in, ActType t) {
            Binding ab; ab.name = in; ab.chordText = in; InputId ai; ParseInput(in, ai); ab.chord.push_back(ai);
            ab.gesture = "press"; ab.block = "trigger"; ab.action.type = t; q->bindings.push_back(ab);
        };
        bind("left.x", ActType::Adjust); bind("right.a", ActType::Keyboard);
        Engine e; e.SetProfile(q);
        Frame f; double t = 0;
        f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose = f.h[h].aimPose = XrPosef{ {0,0,0,1}, {h ? 0.3f : -0.3f, 1.0f, -0.2f} }; }
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(5);
        f.h[1].b1 = true; step(2); f.h[1].b1 = false; step(3);
        Vec3 k0 = V(e.kb.pose.position);
        CHECK(e.kb.visible && std::fabs(k0.z + 0.45f) < 0.02f && std::fabs(k0.y - 1.3f) < 0.02f, "keyboard: by default it opens 0.45 m ahead and 0.3 m below the eyes");
        f.h[0].b1 = true; step(2); f.h[0].b1 = false; step(3);
        CHECK(e.adjust && e.kb.visible, "keyboard: adjust mode can be switched on while it is open, and it stays open");
        f.h[1].gripPose.position = X(k0); step(2);
        f.h[1].grip = 1; step(2);
        f.h[1].gripPose.position = X(k0 + Vec3{ 0.1f, 0.15f, 0 }); step(5);
        Vec3 k1 = V(e.kb.pose.position);
        CHECK(std::fabs(k1.x - (k0.x + 0.1f)) < 0.005f && std::fabs(k1.y - (k0.y + 0.15f)) < 0.005f && q->kb.pose.set, "keyboard: grabbed in adjust mode it follows the hand and keeps that spot");
        f.h[1].grip = 0; step(3);
        f.h[1].b1 = true; step(2); f.h[1].b1 = false; step(3); // close
        f.h[0].b1 = true; step(2); f.h[0].b1 = false; step(3); // leave adjust
        f.head = XrPosef{ {0, std::sin(0.785398f), 0, std::cos(0.785398f)}, {0, 1.6f, 0} }; // face 90 degrees left
        f.h[1].b1 = true; step(2); f.h[1].b1 = false; step(3);
        Vec3 k2 = V(e.kb.pose.position);
        CHECK(e.kb.visible && std::fabs(k2.x - (k0.z + 0.0f)) < 0.02f && std::fabs(k2.y - 1.45f) < 0.02f && std::fabs(k2.z + 0.1f) < 0.02f,
              ("keyboard: the placed spot turns with you (" + std::to_string(k2.x) + ", " + std::to_string(k2.y) + ", " + std::to_string(k2.z) + ")").c_str());
    }

    // Docking: the bar under the keyboard puts the screen above the keys (or the keys under the screen); Adjust dock moves it
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        Binding ab; ab.name = "kb"; ab.chordText = "right.a"; InputId ai; ParseInput("right.a", ai); ab.chord.push_back(ai);
        ab.gesture = "press"; ab.block = "trigger"; ab.action.type = ActType::Keyboard; q->bindings.push_back(ab);
        Engine e; e.SetProfile(q);
        Frame f; double t = 0;
        f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose = f.h[h].aimPose = XrPosef{ {0,0,0,1}, {h ? 0.3f : -0.3f, 1.0f, -0.2f} }; }
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(5); f.h[1].b1 = true; step(2); f.h[1].b1 = false; step(3);
        CHECK(e.kb.visible && e.kb.barLabels.size() == 5, "dock: the keyboard has a bar with 5 buttons under it");
        auto pressBar = [&](int cell) {
            int n = (int)e.kb.barLabels.size();
            Vec3 local{ ((cell + 0.5f) / n - 0.5f) * e.kb.barW, 0, 0.3f };
            f.h[0].aimPose = XrPosef{ e.kb.barPose.orientation, X(V(e.kb.barPose.position) + Rotate(e.kb.barPose.orientation, local)) };
            step(2); f.h[0].trig = 1; step(2); f.h[0].trig = 0; step(2);
            f.h[0].aimPose = XrPosef{ {0,0,0,1}, {-0.3f, 1.0f, -0.2f} }; step(2);
        };
        pressBar(0);
        Vec3 up = Rotate(e.kb.pose.orientation, { 0, 1, 0 });
        float above = Dot(V(e.screen.pose.position) - V(e.kb.pose.position), up);
        CHECK(q->dock.mode == DockMode::ScreenOnKeys && e.screen.on && e.screen.poseValid && above > e.kb.heightM / 2, "dock: 'Dock screen above keys' turns the screen on and puts it above the keyboard");
        CHECK(std::fabs(e.screen.widthM - e.kb.widthM) < 0.001f, "dock: the docked screen is as wide as the keyboard by default");
        pressBar(2);
        CHECK(e.dockAdjust, "dock: 'Adjust dock' starts placing the docked screen");
        Vec3 s0 = V(e.screen.pose.position);
        f.h[1].gripPose.position = X(s0); step(2); f.h[1].grip = 1; step(2);
        f.h[1].gripPose.position = X(s0 + Vec3{ 0.2f, 0.05f, 0 }); step(5);
        f.h[1].grip = 0; step(3);
        Vec3 s1 = V(e.screen.pose.position);
        CHECK(q->dock.pose.set && std::fabs(s1.x - (s0.x + 0.2f)) < 0.01f, "dock: the docked screen can be grabbed and stays where it was put");
        pressBar(2);
        CHECK(!e.dockAdjust, "dock: 'Done adjusting' ends it");
        pressBar(1);
        XrPosef rel = PMul(PInv(e.kb.pose), e.screen.pose);
        CHECK(q->dock.mode == DockMode::KeysUnderScreen && Len(V(rel.position) - V(q->dock.pose.p.position)) < 0.01f, "dock: 'Dock keys under screen' keeps the same spacing, the keyboard following the screen");
        pressBar(1);
        CHECK(q->dock.mode == DockMode::Apart, "dock: pressing it again undocks");
    }

    // Adjust mode saving: write the placement into a (temporary) profile file and read it back.
    {
        std::wstring path = ProfilesDir() + L"\\vro_save_test.json";
        { std::ofstream o(path, std::ios::binary); o << DefaultProfileJson(); }
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->sourceName = "vro_save_test.json";
        q->style.handUp = 0.17f; q->style.radius = 0.15f; q->wrist[0].towardElbow = 0.2f; q->wrist[0].side = -0.01f; q->wrist[1].size = 0.13f;
        q->wrist[0].textUp = "custom"; q->wrist[0].rotateDeg = 33.f;
        q->dock.mode = DockMode::KeysUnderScreen; q->dock.pose = { true, XrPosef{ {0,0,0,1}, {0.01f, 0.2f, -0.03f} } }; q->dock.width = 0.7f;
        q->kb.pose = { true, XrPosef{ {0,0,0,1}, {0.1f, -0.25f, -0.4f} } };
        std::string serr;
        bool ok = SavePlacement(*q, serr);
        std::ifstream in(path, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); in.close();
        auto back = ParseProfileText(ss.str(), err);
        CHECK(ok && back && std::fabs(back->style.handUp - 0.17f) < 1e-4f && std::fabs(back->style.radius - 0.15f) < 1e-4f
              && std::fabs(back->wrist[0].towardElbow - 0.2f) < 1e-4f && std::fabs(back->wrist[0].side + 0.01f) < 1e-4f
              && std::fabs(back->wrist[1].size - 0.13f) < 1e-4f && back->wrist[0].textUp == "custom" && std::fabs(back->wrist[0].rotateDeg - 33.f) < 0.1f && back->wheels.size() == q->wheels.size(),
              ("adjust: positions are saved to the profile file and the rest is kept " + serr).c_str());
        CHECK(back && back->dock.mode == DockMode::KeysUnderScreen && back->dock.pose.set && std::fabs(back->dock.pose.p.position.y - 0.2f) < 0.002f && std::fabs(back->dock.width - 0.7f) < 0.002f
              && back->kb.pose.set && std::fabs(back->kb.pose.p.position.z + 0.4f) < 0.002f, "adjust: the keyboard spot and the dock are saved too");
        DeleteFileW(path.c_str());
    }


    // ---------------- D3D12 panel uploads (queued, no CPU wait for the game's GPU work)
    {
        std::vector<uint32_t> got; uint32_t want = 0; int n = 0; double ms = 0; std::string why;
        bool ran = D3D12PanelRun(got, want, n, ms, why);
        CHECK(ran, ("D3D12 panel: uploads run on a software GPU " + why).c_str());
        if (ran) {
            bool all = !got.empty(); for (auto c : got) if (c != want) all = false;
            char b[160]; snprintf(b, sizeof b, "D3D12 panel: the last of %d uploads reaches the swapchain image (%08X, wanted %08X)", n, got.empty() ? 0u : got[0], want);
            CHECK(all, b);
            snprintf(b, sizeof b, "D3D12 panel: %.2f ms per upload", ms); printf("info: %s\n", b);
        }
    }
    // ---------------- head smoothing through the real layer DLL and a pretend runtime (Unreal-style and Unity-style games)
    {
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        std::wstring td = std::wstring(tmp) + L"vro_head_layer_test";
        std::error_code fec; std::filesystem::remove_all(td, fec); std::filesystem::create_directories(td, fec);
        _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", td.c_str());
        { std::ofstream o(td + L"\\head_smoothing.json"); o << "{ \"on\": true, \"keep_on\": true, \"mode\": \"headset\", \"strength\": 5 }"; }
        std::vector<float> game, shown; std::string why;
        bool ran = RunHeadSmoothingThroughLayer(true, 1000000000, game, shown, why);
        CHECK(ran, ("head smoothing (layer): the real layer runs over a pretend runtime " + why).c_str());
        if (ran) {
            float g = game[150 + 27], sh = shown[150 + 27]; // 0.3 s after a sudden 90 degree turn
            CHECK(g > 3.f && g < 60.f, ("head smoothing (layer, Unreal-style head): the game's camera trails the turn, " + std::to_string(g) + " deg of 90 after 0.3 s").c_str());
            CHECK(std::fabs(sh - 90.f) < 0.5f, ("head smoothing (layer, Unreal-style head): 'in the headset' hands the headset the real head (" + std::to_string(sh) + " deg)").c_str());
            CHECK(game.back() > 85.f, ("head smoothing (layer, Unreal-style head): the camera catches up (" + std::to_string(game.back()) + " deg after 2 s)").c_str());
        }
        game.clear(); shown.clear();
        ran = RunHeadSmoothingThroughLayer(false, 1000000000 + 400 * 11111111LL, game, shown, why);
        if (ran) {
            float g = game[150 + 27], sh = shown[150 + 27];
            CHECK(g > 3.f && g < 60.f, ("head smoothing (layer, Unity-style eye views): the game's camera trails the turn, " + std::to_string(g) + " deg").c_str());
            CHECK(std::fabs(sh - 90.f) < 0.5f, ("head smoothing (layer, Unity-style eye views): 'in the headset' hands the headset the real head (" + std::to_string(sh) + " deg)").c_str());
        }
        // Unreal asks for the head at two times per frame (render thread: this frame, game thread: the next), in an
        // uneven order; with the head turning steadily the camera must stay smooth (no jitter)
        fakert::yawAt = [](XrTime tt) { return (float)((tt % 100000000000LL) * 1e-9) * 60.f; }; // 60 deg/s
        game.clear(); shown.clear();
        ran = RunHeadSmoothingThroughLayer(true, 1000000000 + 800 * 11111111LL, game, shown, why, true);
        fakert::yawAt = nullptr;
        if (ran) {
            float worst = 0;
            auto wrap = [](float d) { while (d > 180.f) d -= 360.f; while (d < -180.f) d += 360.f; return d; };
            for (size_t i = 200; i + 2 < game.size(); i++) worst = std::max(worst, std::fabs(wrap(game[i + 2] - game[i + 1]) - wrap(game[i + 1] - game[i])));
            CHECK(worst < 0.05f, ("head smoothing (layer, Unreal two threads): no jitter while turning, worst wobble " + std::to_string(worst) + " deg").c_str());
        }
        _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", L"");
    }

    // ---------------- head smoothing: a heavy camera (same smoothing for every movement, eases in and out)
    {
        auto yawOf = [](const XrQuaternionf& q) { return 2.f * std::atan2(q.y, q.w) * 57.2958f; };
        XrQuaternionf turned{ 0, std::sin(0.785398f), 0, std::cos(0.785398f) }; // 90 degrees
        auto run = [&](float strength, float secs, std::vector<float>* trace = nullptr) {
            HeavyCam c; float dt = 1.f / 90.f;
            c.Apply({ 0, 0, 0, 1 }, dt, strength);
            float y = 0;
            for (int i = 0; i < (int)(secs / dt); i++) { y = yawOf(c.Apply(turned, dt, strength)); if (trace) trace->push_back(y); }
            return y;
        };
        std::vector<float> tr; run(5.f, 4.f, &tr);
        bool monotonic = true; for (size_t i = 1; i < tr.size(); i++) if (tr[i] + 1e-3f < tr[i - 1] || tr[i] > 90.01f) monotonic = false;
        float first = tr[0], third = tr[9], half = tr[(size_t)(0.6f * 90)];
        CHECK(first < 0.5f && tr[2] - tr[1] > tr[1] - tr[0], ("head smoothing: eases into a sudden 90 deg turn (first frame " + std::to_string(first) + " deg)").c_str());
        CHECK(half > 20.f && half < 60.f, ("head smoothing: strength 5 is a heavy camera, about half way after 0.6 s (" + std::to_string(half) + " deg)").c_str());
        CHECK(tr.back() > 89.f && monotonic, "head smoothing: settles on the new direction with no overshoot");
        CHECK(run(10.f, 0.6f) < run(5.f, 0.6f) && run(5.f, 0.6f) < run(1.f, 0.6f), "head smoothing: higher strength = heavier");
        (void)third;
    }

    // ---------------- standard default.json vs user_defaults.json (in a throwaway settings folder)
    {
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        std::wstring td = std::wstring(tmp) + L"vro_defaults_test";
        std::error_code fec; std::filesystem::remove_all(td, fec);
        _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", td.c_str());
        std::wstring pd = ProfilesDir(), def = pd + L"\\default.json", user = pd + L"\\user_defaults.json";
        auto slurp = [](const std::wstring& p) { std::ifstream i(p, std::ios::binary); std::stringstream s; s << i.rdbuf(); return s.str(); };
        auto exists = [](const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; };
        // an old, edited default.json
        std::string edited = std::string(DefaultProfileJson()).replace(std::string(DefaultProfileJson()).find("\"haptics\": true"), 15, "\"haptics\": false");
        { std::ofstream o(def, std::ios::binary); o << edited; }
        std::wstring used; std::string lerr;
        auto lp = LoadProfile(used, lerr);
        CHECK(exists(user) && slurp(user) == edited, "defaults: an edited default.json is kept as user_defaults.json");
        CHECK(ActiveProfileName() == "user_defaults.json", "defaults: the moved user_defaults.json becomes the selected profile");
        CHECK(slurp(def) == DefaultProfileJson(), "defaults: default.json becomes the built-in standard");
        CHECK(lp && lp->sourceName == "user_defaults.json" && !lp->haptics, "defaults: games use user_defaults.json when it exists");
        // the split happens once: a later edit of default.json is replaced by the standard, not moved again
        DeleteFileW(user.c_str());
        { std::ofstream o(def, std::ios::binary); o << edited; }
        lp = LoadProfile(used, lerr);
        CHECK(!exists(user) && slurp(def) == DefaultProfileJson() && lp && lp->sourceName == "default.json", "defaults: without user_defaults.json games use the standard, refreshed each start");
        // a save by the layer (offsets) on the standard goes to user_defaults.json
        lp->offsets.o[1][0].pitch = 12.f;
        std::string serr;
        bool sok = SaveOffsets(*lp, serr);
        auto back = ParseProfileText(slurp(user), lerr);
        CHECK(sok && lp->sourceName == "user_defaults.json" && back && std::fabs(back->offsets.o[1][0].pitch - 12.f) < 0.05f && slurp(def) == DefaultProfileJson(),
              ("defaults: a save from the headset goes to user_defaults.json, default.json stays standard " + serr).c_str());
        // the profile selected in the editor always wins, even over a profile named after the game
        { std::ofstream o(pd + L"\\other.json", std::ios::binary); o << DefaultProfileJson(); }
        { std::ofstream o(pd + L"\\" + Utf8ToW(ExeName()) + L".json", std::ios::binary); o << DefaultProfileJson(); }
        SetActiveProfileName("other.json");
        lp = LoadProfile(used, lerr);
        CHECK(lp && lp->sourceName == "other.json", "profiles: the selected profile is used, even when the game has its own");
        SetActiveProfileName("gone.json");
        lp = LoadProfile(used, lerr);
        CHECK(lp && _stricmp(lp->sourceName.c_str(), (ExeName() + ".json").c_str()) == 0, "profiles: a selected profile that no longer exists falls back to the game's own");
        DeleteFileW((pd + L"\\" + Utf8ToW(ExeName()) + L".json").c_str());
        // a headset save on the standard while user_defaults.json exists: a new profile, and it becomes the selected one
        SetActiveProfileName("default.json");
        std::string userBefore = slurp(user);
        lp = LoadProfile(used, lerr);
        lp->offsets.o[1][0].pitch = 7.f;
        sok = SaveOffsets(*lp, serr);
        CHECK(sok && lp->sourceName == "user_defaults 2.json" && slurp(user) == userBefore && ActiveProfileName() == "user_defaults 2.json"
              && slurp(def) == DefaultProfileJson(), "defaults: a headset save on the standard never replaces user_defaults.json; it makes user_defaults 2.json and selects it");
        // an unedited old default.json is not moved
        std::filesystem::remove_all(td, fec);
        ProfilesDir();
        { std::ofstream o(def, std::ios::binary); o << DefaultProfileJson(); }
        lp = LoadProfile(used, lerr);
        CHECK(!exists(user) && lp && lp->sourceName == "default.json", "defaults: an unchanged default.json stays the standard");
        _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", L"");
        std::filesystem::remove_all(td, fec);
    }

    // ---------------- mirrored screen: actions
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.fpsSteps = { 15, 30, 60, 0 }; q->screen.fps = 30;
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head.orientation.w = 1; f.h[0].gripValid = true; f.h[0].gripPose.orientation.w = 1;
        double t = 0; auto step = [&] { t += 0.011; f.t = t; e.Update(f); };
        Action tg; tg.type = ActType::Screen; tg.mode = "toggle"; tg.place = "wrist_left";
        CHECK(!e.screen.on, "screen: off at start (start_on false)");
        e.ScreenAction(tg); step();
        CHECK(e.screen.on && e.screen.place == "wrist_left" && e.screen.poseValid && e.IsOn(tg), "screen: wrist button turns it on, on the left wrist");
        Action fr2 = tg; fr2.place = "front";
        e.ScreenAction(fr2); step();
        CHECK(e.screen.on && e.screen.place == "front" && e.screen.poseValid && !e.IsOn(tg) && e.IsOn(fr2), "screen: a button for another place moves it there");
        CHECK(e.screen.pose.position.z < -0.5f, "screen: front is placed in front of the head");
        e.ScreenAction(fr2); step();
        CHECK(!e.screen.on && !e.screen.poseValid, "screen: the same button turns it off");
        Action nf; nf.type = ActType::Screen; nf.mode = "next_fps";
        e.ScreenAction(nf); CHECK(e.screen.fps == 60, "screen: next frame rate 30 -> 60");
        e.ScreenAction(nf); CHECK(e.screen.fps == 0, "screen: next frame rate 60 -> every frame");
        e.ScreenAction(nf); CHECK(e.screen.fps == 15, "screen: next frame rate wraps to 15");
        e.screen.monitorCount = 3; e.screen.primaryIndex = 2; e.screen.monitor = 0;
        Action nm; nm.type = ActType::Screen; nm.mode = "next_monitor";
        e.ScreenAction(nm); CHECK(e.screen.monitor == 3 && e.screen.on, "screen: next monitor from the main one (2 of 3) is 3, and shows it");
        e.ScreenAction(nm); CHECK(e.screen.monitor == 1, "screen: next monitor wraps 3 -> 1");
        nm.mode = "prev_monitor"; e.ScreenAction(nm); CHECK(e.screen.monitor == 3, "screen: previous monitor wraps 1 -> 3");
        Action np; np.type = ActType::Screen; np.mode = "next_place";
        e.screen.place = "head"; e.ScreenAction(np); CHECK(e.screen.place == "wrist_left", "screen: next place cycles head -> left wrist");
        std::string perr;
        auto bad = ParseProfileText(R"({"bindings":[{"name":"x","input":"left.x","action":{"type":"screen","place":"ceiling"}}]})", perr);
        CHECK(!bad, "screen: unknown place is a profile error");
    }
    // ---------------- mirrored screen: adjust mode
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.place = "front"; q->screen.distance = 0.9f; q->screen.below = 0.05f; q->screen.width = 0.8f;
        q->wheels.clear(); // keep adjust mode from opening a wheel in the way
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head.orientation.w = 1; f.head.position = { 0, 1.6f, 0 };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose.orientation.w = f.h[h].aimPose.orientation.w = 1; }
        f.h[0].gripPose.position = { -0.3f, 1.0f, -0.2f };
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.2f };
        double t = 0; auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step();
        CHECK(!e.screen.shown, "screen adjust: hidden while off");
        Action adj; adj.type = ActType::Adjust;
        e.ScreenAction(Action{}); // no-op guard
        // turn adjust mode on through the engine's own action path
        Action on; on.type = ActType::Screen; on.mode = "off"; e.ScreenAction(on);
        q->bindings.clear();
        Binding b; b.name = "adj"; InputId i; ParseInput("left.menu", i); b.chord.push_back(i); b.chordText = "left.menu"; b.action = adj; b.block = "none";
        q->bindings.push_back(b); e.SetProfile(q);
        f.h[0].menu = true; step(); f.h[0].menu = false; step();
        CHECK(e.adjust && e.screen.shown && e.screen.poseValid, "screen adjust: shown in adjust mode even while off");
        Vec3 c = V(e.screen.pose.position);
        f.h[1].gripPose.position = X(c + Vec3{ 0.05f, 0, 0 }); step();
        f.h[1].grip = 1.f; step(2);
        f.h[1].gripPose.position = X(c + Vec3{ 0.05f, -0.2f, 0.3f }); f.h[1].sy = 1.f; step(10);
        f.h[1].grip = 0.f; f.h[1].sy = 0.f; step();
        Vec3 moved = V(e.screen.pose.position) - c;
        CHECK(q->screen.poses.count("front") && Len(moved - Vec3{ 0, -0.2f, 0.3f }) < 0.01f && q->screen.width > 0.8f,
              "screen adjust: grip moves it anywhere (x, y, z), stick makes it bigger");
        f.h[0].menu = true; step(); f.h[0].menu = false; step();
        CHECK(!e.adjust && !e.screen.shown && e.saveRequested, "screen adjust: finishing hides it again and saves");
    }
    // ---------------- mirrored screen: pointing at it works the desktop mouse
    {
        Keys::SetDryRun(true);
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.place = "front"; q->screen.distance = 1.0f; q->screen.below = 0.f; q->screen.width = 1.0f;
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose.orientation.w = f.h[h].aimPose.orientation.w = 1; }
        f.h[0].gripPose.position = f.h[0].aimPose.position = { -0.3f, 1.0f, -0.2f };
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.2f };
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f }; // straight at the middle of the screen
        double t = 0; auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
        e.screen.monX = -1920; e.screen.monY = 0; e.screen.monW = 1920; e.screen.monH = 1080; e.screen.aspect = 9.f / 16.f;
        Action on; on.type = ActType::Screen; on.mode = "on"; e.ScreenAction(on);
        step(2);
        int x, y, wh, fo; bool l, r; Keys::DryState(x, y, l, r, wh, fo);
        CHECK(e.screen.hoverHand == 1 && std::abs(x - (-960)) <= 2 && std::abs(y - 540) <= 2, ("screen use: pointing at the middle puts the mouse in the middle of that monitor (" + std::to_string(x) + ", " + std::to_string(y) + ")").c_str());
        CHECK(e.Blocked(1, C_TRIGGER) && e.Blocked(1, C_BTN1) && e.pointer[1].on, "screen use: the pointing hand's trigger and A go to the desktop, and the laser shows");
        f.h[1].trig = 1.f; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(l, "screen use: trigger holds the left mouse button");
        f.h[1].aimPose.position = { 0.25f, 1.6f, -0.2f }; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(l && x > -960 + 400, "screen use: dragging moves the mouse with the button held");
        f.h[1].trig = 0.f; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(!l, "screen use: letting go of the trigger lets go of the button");
        f.h[1].b1 = true; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(r, "screen use: A holds the right mouse button");
        f.h[1].b1 = false; step(2);
        f.h[1].sy = 1.f; step(20); f.h[1].sy = 0.f; step(); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(wh >= 120 * 1 && wh <= 120 * 4, ("screen use: stick up scrolls (" + std::to_string(wh / 120) + " notches)").c_str());
        f.h[1].aimPose.position = { 3.f, 1.6f, -0.2f }; step(60); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(e.screen.hoverHand < 0 && fo == 1 && !e.Blocked(1, C_TRIGGER), "screen use: pointing away gives the game its window and buttons back");
        Keys::SetDryRun(false);
    }
    // ---------------- mirrored screen: windows instead of monitors
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        Engine e; e.SetProfile(q);
        Action nw; nw.type = ActType::Screen; nw.mode = "next_window";
        uint64_t ask0 = e.screen.windowAsk;
        e.ScreenAction(nw);
        CHECK(e.screen.on && e.screen.source == "window" && e.screen.windowStep == 1 && e.screen.windowAsk == ask0 + 1, "screen window: next window switches to windows and asks for the next one");
        nw.mode = "prev_window"; e.ScreenAction(nw);
        CHECK(e.screen.windowStep == -1, "screen window: previous window asks for the one before");
        Action nm; nm.type = ActType::Screen; nm.mode = "next_monitor"; e.screen.monitorCount = 2;
        e.ScreenAction(nm);
        CHECK(e.screen.source == "monitor", "screen window: next monitor goes back to monitors");
        Action sw; sw.type = ActType::Screen; sw.mode = "on"; sw.window = "chrome";
        e.ScreenAction(sw);
        CHECK(e.screen.source == "window" && e.screen.windowMatch == "chrome", "screen window: a button can show a window by name");
        std::string perr;
        auto pj = ParseProfileText(R"({"screen":{"source":"window","window":"discord"}})", perr);
        CHECK(pj && pj->screen.source == "window" && pj->screen.window == "discord", "screen window: profile can start on a window by name");
    }
    // ---------------- mirrored screen: real window capture on this PC
    {
        auto wins = ScreenCapture::Windows();
        printf("windows found: %d\n", (int)wins.size());
        for (size_t i = 0; i < wins.size() && i < 12; i++) printf("  %s | %s\n", wins[i].exe.c_str(), wins[i].title.substr(0, 50).c_str());
        int pick = -1;
        for (int i = 0; i < (int)wins.size() && pick < 0; i++) { std::string x = wins[i].exe; for (auto& ch : x) ch = (char)tolower((unsigned char)ch); if (x.find("chrome") != std::string::npos || x.find("firefox") != std::string::npos || x.find("msedge") != std::string::npos) pick = i; }
        if (pick < 0 && !wins.empty()) pick = 0;
        if (pick >= 0) {
            ScreenCapture c; c.Configure(0, wins[pick].hwnd, 30, 1280, true, 1.f);
            std::vector<uint32_t> px; int w = 0, h = 0; bool got = false;
            for (int i = 0; i < 300 && !got; i++) { got = c.TakeFrame(px, w, h); if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            int n, prim, shown; std::string msg; uint64_t mv; c.Status(n, prim, shown, msg, mv);
            int rx, ry, rw, rh; c.Rect(rx, ry, rw, rh);
            char b[300]; snprintf(b, sizeof b, "screen window: capture '%s' -> %dx%d, window at %d,%d %dx%d (%s)", wins[pick].exe.c_str(), w, h, rx, ry, rw, rh, msg.c_str());
            CHECK(got && w > 0 && (int)px.size() == w * h && rw > 0, b);
            if (got && getenv("VRO_SCREEN_BMP")) {
                std::string fn = std::string(getenv("VRO_SCREEN_BMP")) + "win.bmp";
                FILE* fo = fopen(fn.c_str(), "wb");
                if (fo) {
                    BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
                    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32;
                    fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + w * h * 4;
                    fwrite(&fh, sizeof fh, 1, fo); fwrite(&ih, sizeof ih, 1, fo); fwrite(px.data(), 4, px.size(), fo); fclose(fo);
                }
            }
            c.Stop();
        }
    }
    // ---------------- mirrored screen: button bar and picker
    {
        Keys::SetDryRun(true);
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.place = "front"; q->screen.distance = 1.0f; q->screen.below = 0.f; q->screen.width = 1.0f;
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose.orientation.w = f.h[h].aimPose.orientation.w = 1; }
        f.h[0].gripPose.position = f.h[0].aimPose.position = { -0.3f, 1.0f, -0.2f };
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.2f };
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f };
        double t = 0; auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
        e.screen.monX = 0; e.screen.monY = 0; e.screen.monW = 1920; e.screen.monH = 1080; e.screen.aspect = 9.f / 16.f;
        Action on; on.type = ActType::Screen; on.mode = "on"; e.ScreenAction(on);
        step(2);
        CHECK(e.screen.barVisible && e.screen.barLabels.size() == 6, "screen bar: pointing at the screen shows its button bar");
        auto aimAt = [&](const XrPosef& pose, float w, float h, const Cell& c) { // point the right laser at a cell's middle
            Vec3 local{ (c.x + c.w / 2 - 0.5f) * w, (0.5f - (c.y + c.h / 2)) * h, 0 };
            Vec3 p = V(pose.position) + Rotate(pose.orientation, local);
            f.h[1].aimPose.position = X(p + Vec3{ 0, 0, 0.5f });
        };
        std::vector<Cell> cells; Cell hd;
        ListLayout(6, 6, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[0]); step(2);
        CHECK(e.screen.barHover == 0 && e.screen.hoverHand < 0, "screen bar: the laser on 'Choose...' highlights it and doesn't move the mouse");
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(e.screen.pickOpen && e.screen.wantChoices, "screen bar: 'Choose...' opens the picker and asks for the list");
        e.screen.wantChoices = false;
        e.screen.choices.clear();
        { ScreenChoice c; c.label = "Monitor 1\n1920 x 1080"; c.monitor = 1; e.screen.choices.push_back(c); }
        for (int i = 0; i < 15; i++) { ScreenChoice c; c.label = "app" + std::to_string(i) + "\nWindow " + std::to_string(i); c.hwnd = 1000 + i; e.screen.choices.push_back(c); }
        e.screen.choicesVersion++; step(2);
        CHECK(e.screen.pickLabels.size() == 14 && e.screen.pickLabels[12] == "More \xE2\x80\xBA", "screen picker: 12 per page, then More and Close");
        ListLayout((int)e.screen.pickLabels.size(), e.screen.pickCols, e.screen.pickH / e.screen.pickW, true, cells, hd);
        aimAt(e.screen.pickPose, e.screen.pickW, e.screen.pickH, cells[3]); step(2);
        CHECK(e.screen.pickHover == 3, "screen picker: the laser highlights an entry");
        {
            std::vector<uint32_t> px; int W = 1000, H = (int)(1000 * e.screen.pickH / e.screen.pickW);
            DrawList(px, W, H, e.screen.pickTitle, e.screen.pickLabels, e.screen.pickCols, e.screen.pickHover, -1, q->style);
            SavePng(px, W, H, L"screen_picker.png");
            int BW = 1200, BH = (int)(1200 * e.screen.barH / e.screen.barW);
            DrawList(px, BW, BH, "", e.screen.barLabels, (int)e.screen.barLabels.size(), 1, -1, q->style);
            SavePng(px, BW, BH, L"screen_bar.png");
        }
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(!e.screen.pickOpen && e.screen.source == "window" && e.screen.hwnd == 1002, "screen picker: picking a window shows that window");
        // keyboard button
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f }; step(2);
        ListLayout(6, 6, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[3]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(e.kb.visible, "screen bar: 'Keyboard' opens the VR keyboard");
        f.h[1].aimPose.position = { 3.f, 1.6f, -0.2f }; step(80);
        int x, y, wh, fo; bool l, r; Keys::DryState(x, y, l, r, wh, fo);
        CHECK(fo == 0, "screen bar: the game doesn't take the focus back while the keyboard is up");
        e.kb.visible = false; // closed
        // Move...: a list of places with Cancel
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f }; step(2);
        ListLayout(6, 6, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[2]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(e.screen.pickOpen && e.screen.pickLabels.size() == 5 && e.screen.pickLabels[4] == "Cancel" && e.screen.barLabels[2] == "Cancel move",
              "screen bar: 'Move...' lists the places with a Cancel");
        ListLayout(5, e.screen.pickCols, e.screen.pickH / e.screen.pickW, true, cells, hd);
        aimAt(e.screen.pickPose, e.screen.pickW, e.screen.pickH, cells[4]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(!e.screen.pickOpen && e.screen.place == "front", "screen bar: Cancel leaves it where it was");
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f }; step(2);
        ListLayout(6, 6, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[2]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        ListLayout(5, e.screen.pickCols, e.screen.pickH / e.screen.pickW, true, cells, hd);
        aimAt(e.screen.pickPose, e.screen.pickW, e.screen.pickH, cells[3]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(e.screen.place == "head", "screen bar: picking 'Following my view' moves it there");
        // Adjust: turns adjust mode on; the bar becomes one Done button
        float bw0 = e.screen.barW;
        f.h[1].aimPose.position = X(V(e.screen.pose.position) + Vec3{ 0, 0, 0.5f }); step(2);
        ListLayout(6, 6, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[4]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(e.adjust && e.screen.barVisible && e.screen.barLabels.size() == 1, "screen bar: 'Adjust' starts adjust mode and the bar turns into Done");
        ListLayout(1, 1, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[0]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(!e.adjust && e.saveRequested, "screen bar: 'Done adjusting' ends adjust mode and saves");
        CHECK(std::fabs(bw0 - 1.0f * 0.7f) < 0.01f, ("screen bar: 30% smaller by default (" + std::to_string(bw0) + " m wide on a 1 m screen)").c_str());
        Keys::SetDryRun(false);
    }
    // ---------------- the screen remembers what was picked in the headset
    {
        std::wstring path = ProfilesDir() + L"\\vro_choice_test.json";
        { std::ofstream o(path, std::ios::binary); o << DefaultProfileJson(); }
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->sourceName = "vro_choice_test.json";
        CHECK(q->screen.source == "monitor", "screen choice: the default is a monitor");
        q->screen.source = "window"; q->screen.window = "chrome"; q->screen.windowTitle = "Chat - Twitch";
        std::string serr; bool ok = SaveScreenChoice(*q, serr); if (!ok) printf("save error: %s\n", serr.c_str());
        std::ifstream in(path, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); in.close();
        auto back = ParseProfileText(ss.str(), err);
        CHECK(ok && back && back->screen.source == "window" && back->screen.window == "chrome" && back->screen.windowTitle == "Chat - Twitch" && back->wheels.size() == q->wheels.size(),
              "screen choice: a picked window is saved as the default and the rest of the profile is kept");
        Engine e; e.SetProfile(back);
        CHECK(e.screen.source == "window" && e.screen.windowMatch == "chrome" && e.screen.windowTitle == "Chat - Twitch", "screen choice: the next start asks for that same window");
        q->screen.source = "monitor"; q->screen.monitor = 2; SaveScreenChoice(*q, serr);
        std::ifstream in2(path, std::ios::binary); std::stringstream s2; s2 << in2.rdbuf(); in2.close();
        auto back2 = ParseProfileText(s2.str(), err);
        CHECK(back2 && back2->screen.source == "monitor" && back2->screen.monitor == 2, "screen choice: a picked monitor is saved as the default");
        Action nm; nm.type = ActType::Screen; nm.mode = "next_monitor"; e.screen.monitorCount = 2; e.ScreenAction(nm);
        CHECK(e.screen.choiceChanged, "screen choice: switching in the headset asks to remember it");
        DeleteFileW(path.c_str());
    }
    // ---------------- steadying for shaky hands
    {
        auto run = [&](float strength, float& jitter, float& lagAfter) {
            auto q = ParseProfileText(DefaultProfileJson(), err);
            q->screen.place = "wrist_left"; q->screen.smoothing = strength;
            Engine e; e.SetProfile(q);
            Frame f; f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
            f.h[0].gripValid = true; f.h[0].gripPose = XrPosef{ {0,0,0,1}, {-0.2f, 1.2f, -0.3f} };
            Action on; on.type = ActType::Screen; on.mode = "on"; e.ScreenAction(on);
            double t = 0;
            float mn = 1e9f, mx = -1e9f;
            for (int i = 0; i < 400; i++) { // 4.4 s of a 6 Hz, 3 mm shake
                t += 0.011; f.t = t;
                f.h[0].gripPose.position.x = -0.2f + 0.003f * std::sin((float)t * 6.f * 6.2832f);
                e.Update(f);
                if (i > 200) { mn = std::min(mn, e.screen.pose.position.x); mx = std::max(mx, e.screen.pose.position.x); }
            }
            jitter = mx - mn;
            float x0 = e.screen.pose.position.x;
            for (int i = 0; i < 20; i++) { t += 0.011; f.t = t; f.h[0].gripPose.position.x = -0.2f + 0.10f * (i + 1) / 20.f; e.Update(f); } // a real 10 cm move
            for (int i = 0; i < 27; i++) { t += 0.011; f.t = t; e.Update(f); } // then 0.3 s still
            lagAfter = std::fabs((x0 + 0.10f) - e.screen.pose.position.x);
        };
        float j0, l0, j1, l1;
        run(0.f, j0, l0); run(0.6f, j1, l1);
        char b[200]; snprintf(b, sizeof b, "steady screen: a 6 Hz shake moves it %.1f mm instead of %.1f mm", j1 * 1000, j0 * 1000);
        CHECK(j1 < j0 / 3.f, b);
        snprintf(b, sizeof b, "steady screen: a real 10 cm move is followed (%.1f mm behind 0.3 s later)", l1 * 1000);
        CHECK(l1 < 0.01f, b);
        PoseSmoother ps; XrPosef a{ {0,0,0,1}, {0,0,0} };
        XrPosef r = ps.Apply(a, 0.011f, 0.f); r = ps.Apply(XrPosef{ {0,0,0,1}, {0.05f,0,0} }, 0.011f, 0.f);
        CHECK(std::fabs(r.position.x - 0.05f) < 1e-6f, "steady: 0 = off, no smoothing at all");
    }
    // ---------------- game-wide hand steadying: untracked poses, several times per frame
    {
        const uint64_t ok = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
                            XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
        const uint64_t lost = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT; // position not valid
        XrTime t = 1000000000; const XrTime fr = 11000000;
        auto hand = [](float x) { return XrPosef{ {0,0,0,1}, {x, 1.f, -0.3f} }; };
        // 1) tracking lost with a junk (zero) position: passed through, never blended into the hand
        SpaceSteady s;
        for (int i = 0; i < 50; i++) s.Step(t += fr, hand(0.3f), ok, 0.5f);
        XrPosef junk{ {0,0,0,1}, {0,0,0} };
        XrPosef o = s.Step(t += fr, junk, lost, 0.5f);
        CHECK(o.position.x == 0.f && o.position.y == 0.f, "hand steadying: an untracked pose is passed through untouched");
        o = s.Step(t += fr, hand(0.3f), ok, 0.5f);
        CHECK(std::fabs(o.position.x - 0.3f) < 1e-5f && std::fabs(o.position.y - 1.f) < 1e-5f,
              "hand steadying: after tracking returns the hand is right where it is (no stretch from the lost pose)");
        // 2) two reads per frame (now + predicted): the older time gets a steadied answer too, no raw/steadied flip
        SpaceSteady a;
        float worst = 0.f; XrTime tt = 1000000000;
        for (int i = 0; i < 200; i++) {
            tt += fr;
            float xPred = 0.3f + 0.5f * (float)(tt * 1e-9); // hand moving 0.5 m/s
            float xNow = xPred - 0.5f * 0.02f;               // the same hand 20 ms earlier
            XrPosef pr = a.Step(tt, hand(xPred), ok, 0.5f);
            XrPosef nw = a.Step(tt - 20000000, hand(xNow), ok, 0.5f);
            if (i > 50) worst = std::max(worst, std::fabs((pr.position.x - xPred) - (nw.position.x - xNow)));
        }
        char b[160]; snprintf(b, sizeof b, "hand steadying: the earlier read gets the same steadying (difference %.2f mm)", worst * 1000);
        CHECK(worst < 0.002f, b);
    }
    // ---------------- hand-placed positions are saved and read back
    {
        std::wstring path = ProfilesDir() + L"\\vro_pose_test.json";
        { std::ofstream o(path, std::ios::binary); o << DefaultProfileJson(); }
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->sourceName = "vro_pose_test.json";
        XrPosef p{ { 0, std::sin(0.3f), 0, std::cos(0.3f) }, { 0.1f, -0.2f, 0.3f } };
        q->wrist[1].pose = { true, p }; q->style.poses["head_locked"] = { true, p }; q->screen.poses["wrist_right"] = { true, p };
        std::string serr; bool ok = SavePlacement(*q, serr);
        std::ifstream in(path, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); in.close();
        auto back = ParseProfileText(ss.str(), err);
        auto same = [&](const FreePose& a) { return a.set && std::fabs(a.p.position.z - 0.3f) < 1e-3f && std::fabs(a.p.orientation.y - std::sin(0.3f)) < 1e-3f; };
        CHECK(ok && back && same(back->wrist[1].pose) && same(back->style.poses["head_locked"]) && same(back->screen.poses["wrist_right"]) && !back->wrist[0].pose.set,
              "hand-placed positions and angles of wheel, wrist panels and screen are saved and read back");
        DeleteFileW(path.c_str());
    }
    // ---------------- mirrored screen: real capture on this PC
    // Controller offsets for the game: profile section, panel +/- buttons, stick slider, grab to set.
    {
        auto q = ParseProfileText(R"({ "controller_offsets": { "enabled": true, "link_hands": true, "aim_same_as_grip": true,
            "right": { "grip": { "pitch_deg": 12, "yaw_deg": 4, "roll_deg": -3, "x_cm": 1, "y_cm": 2, "z_cm": -1.5 } } } })", err);
        CHECK(q != nullptr, ("offsets: profile with controller_offsets parses " + err).c_str());
        if (q) {
            HandOffset l = q->offsets.Effective(0, 1), r = q->offsets.Effective(1, 1);
            CHECK(r.pitch == 12 && std::fabs(r.y - 0.02f) < 1e-5f && std::fabs(r.z + 0.015f) < 1e-5f, "offsets: aim uses the grip values (aim same as grip)");
            CHECK(l.pitch == 12 && l.yaw == -4 && l.roll == 3 && std::fabs(l.x + 0.01f) < 1e-5f, "offsets: the left hand is the right hand mirrored");
            Engine e; e.SetProfile(q);
            XrPosef o; bool has = e.OffsetPose(1, 0, o);
            Vec3 fwd = Rotate(o.orientation, { 0, 0, -1 });
            CHECK(has && fwd.y > 0.19f && fwd.y < 0.23f, "offsets: +12 pitch tips the game's forward up by 12 degrees");
        }
        auto d = ParseProfileText(DefaultProfileJson(), err);
        bool found = false;
        for (auto& [n, w] : d->wrist[0].pages) for (auto& it : w.items) if (it.action.type == ActType::Offsets) found = true;
        CHECK(found, "offsets: the default left wrist panel has a Controller offsets button");
        Engine e; e.SetProfile(d);
        Frame f; double t = 0;
        f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose = f.h[h].aimPose = XrPosef{ {0,0,0,1}, {h ? 0.3f : -0.3f, 1.0f, -0.2f} }; }
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(3);
        XrPosef none; CHECK(!e.OffsetPose(1, 0, none), "offsets: nothing is moved while every value is 0");
        Action pa; pa.type = ActType::Offsets; pa.mode = "panel";
        e.OffsetsAction(pa); step(2);
        CHECK(e.offView.open && e.offView.labels.size() == 27, "offsets: the panel opens with its 27 buttons");
        {
            Vec3 n = Rotate(e.offView.pose.orientation, { 0, 0, 1 }), toEye = Norm(V(f.head.position) - V(e.offView.pose.position));
            Vec3 upv = Rotate(e.offView.pose.orientation, { 0, 1, 0 });
            CHECK(n.x * toEye.x + n.y * toEye.y + n.z * toEye.z > 0.999f && upv.z < 0.f, "offsets: the panel faces the eyes, its top leaning away (not toward you)");
        }
        auto aimAt = [&](int cell) {
            std::vector<Cell> cells; Cell hd;
            ListLayout((int)e.offView.labels.size(), e.offView.cols, e.offView.h / e.offView.w, true, cells, hd);
            const Cell& c = cells[cell];
            Vec3 local{ (c.x + c.w / 2 - 0.5f) * e.offView.w, (0.5f - (c.y + c.h / 2)) * e.offView.h, 0.3f };
            f.h[1].aimPose = XrPosef{ e.offView.pose.orientation, X(V(e.offView.pose.position) + Rotate(e.offView.pose.orientation, local)) };
        };
        aimAt(5); step(2); // pitch +
        CHECK(e.offView.hover == 5 && e.Blocked(1, C_TRIGGER), "offsets: pointing at Pitch + hovers it, the trigger is kept from the game");
        f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        CHECK(e.offs.o[1][0].pitch == 1.f, "offsets: one press on Pitch + adds 1 degree");
        f.h[1].trig = 1; step(70); f.h[1].trig = 0; step(2); // held 0.77 s: repeats after 0.45 s
        CHECK(e.offs.o[1][0].pitch > 4.f, ("offsets: holding Pitch + keeps adding, now " + std::to_string(e.offs.o[1][0].pitch)).c_str());
        aimAt(4); step(2); f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        CHECK(e.offs.o[1][0].pitch == 0.f, "offsets: pressing the value in the middle resets it");
        aimAt(7); step(2); f.h[1].sx = 1; step(50); f.h[1].sx = 0; step(2); // yaw row, stick right for 0.55 s
        CHECK(e.offs.o[1][0].yaw > 3.f && e.offs.o[1][0].yaw < 6.f, ("offsets: the stick slides the pointed value, yaw " + std::to_string(e.offs.o[1][0].yaw)).c_str());
        step(100);
        CHECK(e.offsSaveRequested, "offsets: changes are saved a moment later");
        e.offsSaveRequested = false;
        {
            std::vector<uint32_t> px; int W = 900, H = (int)(900 * e.offView.h / e.offView.w);
            DrawList(px, W, H, e.offView.title, e.offView.labels, e.offView.cols, 5, -1, d->style);
            SavePng(px, W, H, L"offsets_panel.png");
        }
        // Grab to set: hold the left grip, tip the controller 10 degrees down, let go -> +10 pitch kept.
        aimAt(25); step(2); f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2); // reset all
        CHECK(e.offs.o[1][0].Zero(), "offsets: Reset all clears them");
        aimAt(22); step(2); f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        f.h[1].aimPose.position = { 3, 3, 3 }; // point away
        f.h[0].grip = 1; step(2);
        CHECK(e.offGrabHand == 0 && e.Blocked(0, C_GRIP), "offsets: grab to set starts on the grip, the grip is kept from the game");
        float a = -10.f * 3.14159265f / 180.f * 0.5f;
        f.h[0].gripPose.orientation = { std::sin(a), 0, 0, std::cos(a) }; step(3);
        XrPosef held; e.OffsetPose(0, 0, held);
        Vec3 world = Rotate(QMul(f.h[0].gripPose.orientation, held.orientation), { 0, 0, -1 });
        CHECK(std::fabs(world.y) < 0.01f, "offsets: while grabbing, the game's hand keeps its angle");
        f.h[0].grip = 0; step(2);
        CHECK(e.offGrabHand < 0 && std::fabs(e.offs.o[1][0].pitch - 10.f) < 0.2f, ("offsets: letting go keeps +10 pitch, got " + std::to_string(e.offs.o[1][0].pitch)).c_str());
        // Armed by mistake: after 5 s without a grip it gives up, the grips go back to the game and a later grip changes nothing.
        aimAt(22); step(2); f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        f.h[1].aimPose.position = { 3, 3, 3 };
        CHECK(e.offView.labels[22].rfind("Now hold grip", 0) == 0 && e.Blocked(0, C_GRIP), "offsets: Grab to set armed, the grips wait for it");
        step(500); // 5.5 s
        CHECK(e.offView.labels[22].rfind("Grab to set", 0) == 0 && !e.Blocked(0, C_GRIP), "offsets: an armed Grab to set gives up after 5 s and frees the grips");
        float before = e.offs.o[1][0].pitch;
        f.h[0].grip = 1; step(2); f.h[0].gripPose.orientation = { std::sin(a * 2), 0, 0, std::cos(a * 2) }; step(3); f.h[0].grip = 0; step(2);
        CHECK(e.offGrabHand < 0 && e.offs.o[1][0].pitch == before, "offsets: a grip after that leaves the offsets alone");
        Action off; off.type = ActType::Offsets; off.mode = "enable"; e.OffsetsAction(off); step(1);
        CHECK(!e.OffsetPose(1, 0, none), "offsets: turning them off gives the game the plain controllers");
    }

    // Headset mirror: the game's eye picture with a quad layer drawn where it really is.
    {
        ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx);
        CHECK(dev != nullptr, "mirror: test D3D11 device");
        if (dev) {
            auto makeTex = [&](int w, int h, DXGI_FORMAT f, uint32_t px) {
                std::vector<uint32_t> d((size_t)w * h, px);
                D3D11_TEXTURE2D_DESC td{}; td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = f; td.SampleDesc.Count = 1;
                td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
                D3D11_SUBRESOURCE_DATA sd{ d.data(), (UINT)w * 4, 0 };
                ID3D11Texture2D* t = nullptr; dev->CreateTexture2D(&td, &sd, &t); return t;
            };
            ID3D11Texture2D* eyeTex = makeTex(800, 800, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0xFFFF0000); // RGBA bytes: blue
            ID3D11Texture2D* quadTex = makeTex(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 0xFFFF0000);  // BGRA bytes: red
            HeadsetMirror m; m.zoom = 1.f; m.fps = 0;
            XrSwapchain se = (XrSwapchain)(uintptr_t)101, sq = (XrSwapchain)(uintptr_t)102; XrSpace sp = (XrSpace)(uintptr_t)7;
            auto reg = [&](XrSwapchain sc, ID3D11Texture2D* t, int w, int h, DXGI_FORMAT f) {
                XrSwapchainCreateInfo ci{ XR_TYPE_SWAPCHAIN_CREATE_INFO }; ci.format = f; ci.width = w; ci.height = h; ci.arraySize = 1; ci.mipCount = 1; ci.sampleCount = 1; ci.faceCount = 1;
                m.OnCreate(sc, &ci);
                XrSwapchainImageD3D11KHR im{ XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR }; im.texture = t;
                m.OnImages(sc, (XrSwapchainImageBaseHeader*)&im, 1);
                m.OnAcquire(sc, 0); m.OnRelease(sc);
            };
            reg(se, eyeTex, 800, 800, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
            reg(sq, quadTex, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
            XrCompositionLayerProjectionView views[2]{ { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } };
            for (auto& v : views) { v.pose.orientation.w = 1; v.fov = { -0.7853982f, 0.7853982f, 0.7853982f, -0.7853982f }; v.subImage.swapchain = se; v.subImage.imageRect = { {0, 0}, {800, 800} }; }
            XrCompositionLayerProjection proj{ XR_TYPE_COMPOSITION_LAYER_PROJECTION }; proj.space = sp; proj.viewCount = 2; proj.views = views;
            XrCompositionLayerQuad q{ XR_TYPE_COMPOSITION_LAYER_QUAD }; q.space = sp; q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; q.subImage.swapchain = sq; q.subImage.imageRect = { {0, 0}, {64, 64} };
            q.pose.orientation.w = 1; q.pose.position = { 0, 0, -0.5f }; q.size = { 0.2f, 0.2f };
            std::vector<const XrCompositionLayerBaseHeader*> L{ (const XrCompositionLayerBaseHeader*)&proj, (const XrCompositionLayerBaseHeader*)&q };
            m.SetOn(true);
            for (int i = 0; i < 200 && !m.Window(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            CHECK(m.Window() != nullptr, "mirror: the desktop window opens");
            auto locate = [](XrSpace, XrSpace, XrTime, XrPosef&) { return false; };
            m.Frame(dev, ctx, L, 1, locate);
            m.grab = true; m.Frame(dev, ctx, L, 2, locate);
            bool got = !m.grabbed.empty();
            CHECK(got, ("mirror: a picture comes out " + std::to_string(m.grabW) + "x" + std::to_string(m.grabH)).c_str());
            if (got) {
                int W = m.grabW, Hh = m.grabH;
                auto at = [&](int x, int y) { return m.grabbed[(size_t)y * W + x] & 0xFFFFFF; };
                // quad: 0.2 m wide at 0.5 m with 45 degree half-angles -> 20% of the eye's width either side of the centre
                // steadying (on by default) crops to 1 - 2 * (4% margin + 7% headroom) of the eye's width
                float cw = m.smoothing > 0.001f ? 1.f - 2 * (0.04f + 0.07f) : 1.f;
                int qx = (int)(W * 0.1f / cw);
                CHECK(at(W / 2, Hh / 2) == 0xFF0000, "mirror: the panel shows in the middle, in front of the game");
                CHECK(at(W / 2 + qx - 6, Hh / 2) == 0xFF0000 && at(W / 2 + qx + 6, Hh / 2) == 0x0000FF, "mirror: the panel's edge is where the headset shows it");
                CHECK(at(10, 10) == 0x0000FF, "mirror: the game's picture fills the rest");
                SavePng(m.grabbed, W, Hh, L"mirror_test.png");
            }
            // Steadying: a 1 degree head shake moves the panel in the window much less than without it.
            auto edgeShift = [&](float smooth) {
                m.smoothing = smooth;
                auto firstRed = [&]() { int W = m.grabW, y = m.grabH / 2; for (int x = 0; x < W; x++) if ((m.grabbed[(size_t)y * W + x] & 0xFFFFFF) == 0xFF0000) return x; return -1; };
                XrTime tt = 1000000000LL;
                for (auto& v : views) v.pose.orientation = { 0, 0, 0, 1 };
                for (int i = 0; i < 30; i++) { tt += 11000000; m.Frame(dev, ctx, L, tt, locate); } // settle
                m.grab = true; tt += 11000000; m.Frame(dev, ctx, L, tt, locate); int x0 = firstRed();
                float a = 0.5f * 3.14159265f / 180.f; // turn 1 degree left
                for (auto& v : views) v.pose.orientation = { 0, std::sin(a), 0, std::cos(a) };
                m.grab = true; tt += 11000000; m.Frame(dev, ctx, L, tt, locate); int x1 = firstRed();
                for (auto& v : views) v.pose.orientation = { 0, 0, 0, 1 };
                return std::abs(x1 - x0);
            };
            int raw = edgeShift(0.f), steadied = edgeShift(1.f);
            CHECK(raw > 8 && steadied * 3 < raw, ("mirror: steadying keeps a small head shake out of the picture (" + std::to_string(raw) + " px -> " + std::to_string(steadied) + " px)").c_str());
            m.Shutdown();
            CHECK(m.Window() == nullptr && !m.On(), "mirror: turning it off closes the window");
            eyeTex->Release(); quadTex->Release(); ctx->Release(); dev->Release();
        }
    }

    {
        auto mons = ScreenCapture::Monitors();
        printf("monitors found: %d\n", (int)mons.size());
        for (size_t i = 0; i < mons.size(); i++) printf("  %d: %dx%d at %d,%d%s\n", (int)i + 1, mons[i].w, mons[i].h, mons[i].x, mons[i].y, mons[i].primary ? " (main)" : "");
        for (int m = 0; m <= (int)mons.size() && !mons.empty(); m++) {
            ScreenCapture c; c.Configure(m, 0, 30, getenv("VRO_SCREEN_MAXW") ? atoi(getenv("VRO_SCREEN_MAXW")) : 1280, true, 1.f);
            std::vector<uint32_t> px; int w = 0, h = 0; bool got = false;
            for (int i = 0; i < 300 && !got; i++) { got = c.TakeFrame(px, w, h); if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            int n, prim, shown; std::string msg; uint64_t mv; c.Status(n, prim, shown, msg, mv);
            char b[256]; snprintf(b, sizeof b, "screen: capture monitor setting %d -> %dx%d (%s)", m, w, h, msg.c_str());
            CHECK(got && w > 0 && (int)px.size() == w * h, b);
            if (got && getenv("VRO_SCREEN_BMP")) { // look at what was captured
                std::string fn = std::string(getenv("VRO_SCREEN_BMP")) + std::to_string(m) + ".bmp";
                FILE* fo = fopen(fn.c_str(), "wb");
                if (fo) {
                    BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
                    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32;
                    fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + w * h * 4;
                    fwrite(&fh, sizeof fh, 1, fo); fwrite(&ih, sizeof ih, 1, fo); fwrite(px.data(), 4, px.size(), fo); fclose(fo);
                }
            }
            c.Stop();
        }
    }

    printf("\n%d failures\n", fails);
    Keys::Shutdown();
    DrawShutdown();
    return fails;
}
