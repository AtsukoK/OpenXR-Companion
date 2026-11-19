// Headset mirror window: one eye of the game's picture with every quad layer (ours and the game's)
// drawn on top at its real place, so the desktop shows what the headset shows.
#include "mirror.h"
#include <d3dcompiler.h>
#include <algorithm>

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while (0)

static HeadsetMirror* gSelf = nullptr;
static const wchar_t* kClass = L"OpenXRCompanionMirror";
static const UINT WM_MIRROR_QUIT = WM_APP + 7;

// ------------------------------------------------------------ swapchain bookkeeping
void HeadsetMirror::OnCreate(XrSwapchain sc, const XrSwapchainCreateInfo* ci) {
    std::lock_guard l(mx);
    Sw& s = sws[(uint64_t)sc];
    s = Sw{};
    s.format = ci->format; s.w = ci->width; s.h = ci->height; s.arraySize = ci->arraySize; s.mips = ci->mipCount; s.samples = ci->sampleCount;
}
void HeadsetMirror::OnImages(XrSwapchain sc, const XrSwapchainImageBaseHeader* imgs, uint32_t n) {
    if (!imgs || !n || imgs->type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) return;
    std::lock_guard l(mx);
    auto it = sws.find((uint64_t)sc);
    if (it == sws.end()) return;
    auto* im = (const XrSwapchainImageD3D11KHR*)imgs;
    it->second.imgs.clear();
    for (uint32_t i = 0; i < n; i++) it->second.imgs.push_back(im[i].texture);
}
void HeadsetMirror::OnAcquire(XrSwapchain sc, uint32_t idx) {
    std::lock_guard l(mx);
    auto it = sws.find((uint64_t)sc);
    if (it != sws.end()) it->second.pending.push_back(idx);
}
void HeadsetMirror::OnRelease(XrSwapchain sc) {
    std::lock_guard l(mx);
    auto it = sws.find((uint64_t)sc);
    if (it == sws.end() || it->second.pending.empty()) return;
    it->second.last = (int)it->second.pending.front(); // images are released in the order they were acquired
    it->second.pending.erase(it->second.pending.begin());
}
void HeadsetMirror::OnDestroy(XrSwapchain sc) {
    std::lock_guard l(mx);
    auto it = sws.find((uint64_t)sc);
    if (it == sws.end()) return;
    for (auto* t : it->second.imgs) {
        auto s = srvs.find(t);
        if (s != srvs.end()) { SAFE_RELEASE(s->second); srvs.erase(s); }
    }
    sws.erase(it);
}

// ------------------------------------------------------------ window
LRESULT CALLBACK HeadsetMirror::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    HeadsetMirror* self = gSelf;
    switch (m) {
    case WM_SIZE:
        if (self && LOWORD(l) > 0 && HIWORD(l) > 0) { self->clientW = LOWORD(l); self->clientH = HIWORD(l); InterlockedExchange(&self->sized, 1); }
        return 0;
    case WM_CLOSE: // the X: the layer turns the mirror off (and remembers it)
        if (self) InterlockedExchange(&self->userClosed, 1);
        return 0;
    case WM_MIRROR_QUIT:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(h, m, w, l);
}

void HeadsetMirror::StartWindow() {
    if (thread) return;
    gSelf = this;
    userClosed = 0;
    thread = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
        auto* self = (HeadsetMirror*)p;
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = kClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassExW(&wc);
        RECT r{ 0, 0, 1280, 720 };
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        HWND h = CreateWindowExW(0, kClass, L"OpenXR Companion - Headset mirror", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
        if (!h) { Log("mirror: window failed %lu", GetLastError()); return 1; }
        RECT cr; GetClientRect(h, &cr); self->clientW = std::max(1L, cr.right); self->clientH = std::max(1L, cr.bottom);
        ShowWindow(h, SW_SHOWNOACTIVATE); // don't take the keyboard from the game
        self->hwnd = h;
        MSG m;
        while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
        self->hwnd = nullptr;
        return 0;
    }, this, 0, nullptr);
}

void HeadsetMirror::StopWindow() {
    if (!thread) return;
    for (int i = 0; i < 100 && !hwnd; i++) Sleep(5); // still starting up
    if (hwnd) PostMessageW(hwnd, WM_MIRROR_QUIT, 0, 0);
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread); thread = nullptr; hwnd = nullptr;
}

void HeadsetMirror::SetOn(bool v) {
    if (v == on) return;
    on = v;
    if (on) { failed = false; StartWindow(); Log("mirror: on"); }
    else {
        SAFE_RELEASE(rtv); SAFE_RELEASE(chain); // the swapchain belongs to the window
        StopWindow(); Log("mirror: off");
    }
}

bool HeadsetMirror::ClosedByUser() { return InterlockedExchange(&userClosed, 0) != 0; }

void HeadsetMirror::Shutdown() {
    SetOn(false);
    ReleaseGpu();
}

// ------------------------------------------------------------ GPU
static const char* kShader = R"(
cbuffer C : register(b0) { float4 P[4]; float4 UV; float4 Mode; };
Texture2D T : register(t0); SamplerState S : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID) {
    V o; o.pos = P[id];
    o.uv = float2((id & 1) ? UV.z : UV.x, (id & 2) ? UV.w : UV.y);
    return o;
}
float4 ps(V i) : SV_Target {
    float4 c = T.Sample(S, i.uv);
    if (Mode.x < 0.5) return float4(c.rgb, 1);          // opaque
    if (Mode.x > 1.5) return float4(c.rgb * c.a, c.a);  // straight alpha -> premultiplied
    return c;                                           // premultiplied
}
)";

bool HeadsetMirror::InitGpu(ID3D11Device* d) {
    if (dev == d && vs) return true;
    ReleaseGpu();
    dev = d; dev->AddRef();
    ID3DBlob *bv = nullptr, *bp = nullptr, *err = nullptr;
    if (FAILED(D3DCompile(kShader, strlen(kShader), "mirror", nullptr, nullptr, "vs", "vs_4_0", 0, 0, &bv, &err)) ||
        FAILED(D3DCompile(kShader, strlen(kShader), "mirror", nullptr, nullptr, "ps", "ps_4_0", 0, 0, &bp, &err))) {
        Log("mirror: shader compile failed %s", err ? (const char*)err->GetBufferPointer() : "");
        SAFE_RELEASE(bv); SAFE_RELEASE(bp); SAFE_RELEASE(err); return false;
    }
    dev->CreateVertexShader(bv->GetBufferPointer(), bv->GetBufferSize(), nullptr, &vs);
    dev->CreatePixelShader(bp->GetBufferPointer(), bp->GetBufferSize(), nullptr, &ps);
    SAFE_RELEASE(bv); SAFE_RELEASE(bp); SAFE_RELEASE(err);
    D3D11_BUFFER_DESC bd{ 96, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER };
    dev->CreateBuffer(&bd, nullptr, &cb);
    D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&sd, &samp);
    D3D11_BLEND_DESC bl{}; bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev->CreateBlendState(&bl, &blendOpaque);
    auto& rt = bl.RenderTarget[0];
    rt.BlendEnable = TRUE; rt.SrcBlend = D3D11_BLEND_ONE; rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE; rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    dev->CreateBlendState(&bl, &blendPremul);
    D3D11_RASTERIZER_DESC rd{}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &rast);
    // our own pipeline state, so the game's is left exactly as it was
    ID3D11Device1* d1 = nullptr;
    if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&d1)))) {
        D3D_FEATURE_LEVEL fl = dev->GetFeatureLevel();
        d1->CreateDeviceContextState(0, &fl, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &state);
        d1->Release();
    }
    if (!vs || !ps || !cb || !state) { Log("mirror: GPU setup failed"); return false; }
    return true;
}

void HeadsetMirror::ReleaseGpu() {
    for (auto& [t, s] : srvs) SAFE_RELEASE(s);
    srvs.clear();
    SAFE_RELEASE(eyeSrv); SAFE_RELEASE(eyeCopy); eyeDesc = {};
    SAFE_RELEASE(rtv); SAFE_RELEASE(chain);
    SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(cb); SAFE_RELEASE(samp);
    SAFE_RELEASE(blendOpaque); SAFE_RELEASE(blendPremul); SAFE_RELEASE(rast); SAFE_RELEASE(state);
    SAFE_RELEASE(dev);
}

static DXGI_FORMAT ViewFormat(DXGI_FORMAT tex, int64_t asked) {
    if (asked) return (DXGI_FORMAT)asked; // the format the swapchain was asked for is the one its pixels are in
    switch (tex) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default: return tex;
    }
}

ID3D11ShaderResourceView* HeadsetMirror::Srv(ID3D11Texture2D* t, int64_t fmt) {
    auto it = srvs.find(t);
    if (it != srvs.end()) return it->second;
    D3D11_TEXTURE2D_DESC d; t->GetDesc(&d);
    ID3D11ShaderResourceView* v = nullptr;
    if ((d.BindFlags & D3D11_BIND_SHADER_RESOURCE) && d.SampleDesc.Count == 1) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = ViewFormat(d.Format, fmt);
        if (d.ArraySize > 1) { sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY; sd.Texture2DArray.MipLevels = 1; sd.Texture2DArray.ArraySize = 1; }
        else { sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1; }
        if (FAILED(dev->CreateShaderResourceView(t, &sd, &v))) v = nullptr;
    }
    srvs[t] = v; // null too: don't try again every frame
    return v;
}

// ------------------------------------------------------------ one frame
void HeadsetMirror::Frame(ID3D11Device* d, ID3D11DeviceContext* ctx, const std::vector<const XrCompositionLayerBaseHeader*>& layers,
                          XrTime t, const LocateFn& locate) {
    if (!on || failed || !d || !ctx) return;
    if (fps > 0) { double now = NowSec(); if (now - lastFrame < 1.0 / fps - 0.002) return; lastFrame = now; }
    HWND h = hwnd;
    if (!h) return;
    if (!InitGpu(d)) { failed = true; return; }
    std::lock_guard l(mx);

    // the game's picture for this eye
    const XrCompositionLayerProjection* proj = nullptr;
    for (auto* L : layers) if (L->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) { proj = (const XrCompositionLayerProjection*)L; break; }
    if (!proj || proj->viewCount < 1) return;
    const XrCompositionLayerProjectionView& view = proj->views[std::min<uint32_t>(eye, proj->viewCount - 1)];
    auto sit = sws.find((uint64_t)view.subImage.swapchain);
    if (sit == sws.end() || sit->second.last < 0 || sit->second.last >= (int)sit->second.imgs.size()) return;
    Sw& esw = sit->second;
    ID3D11Texture2D* etex = esw.imgs[esw.last];
    D3D11_TEXTURE2D_DESC td; etex->GetDesc(&td);
    if (td.SampleDesc.Count != 1) { if (!failed) Log("mirror: multisampled eye images aren't supported"); failed = true; return; }
    XrRect2Di rc = view.subImage.imageRect;
    if (rc.extent.width <= 0 || rc.extent.height <= 0) return;

    // window swapchain
    if (!chain) {
        IDXGIDevice* dx = nullptr; IDXGIAdapter* ad = nullptr; IDXGIFactory2* fac = nullptr;
        if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dx))) && SUCCEEDED(dx->GetAdapter(&ad)) && SUCCEEDED(ad->GetParent(IID_PPV_ARGS(&fac)))) {
            DXGI_SWAP_CHAIN_DESC1 sd{};
            sd.Width = clientW; sd.Height = clientH; sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM; sd.SampleDesc.Count = 1;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2; sd.Scaling = DXGI_SCALING_STRETCH;
            sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            HRESULT hr = fac->CreateSwapChainForHwnd(dev, h, &sd, nullptr, nullptr, &chain);
            if (FAILED(hr)) Log("mirror: swapchain failed 0x%08lx", hr);
            else fac->MakeWindowAssociation(h, DXGI_MWA_NO_ALT_ENTER);
        }
        SAFE_RELEASE(fac); SAFE_RELEASE(ad); SAFE_RELEASE(dx);
        if (!chain) { failed = true; return; }
        InterlockedExchange(&sized, 0);
    }
    if (InterlockedExchange(&sized, 0)) { SAFE_RELEASE(rtv); chain->ResizeBuffers(0, clientW, clientH, DXGI_FORMAT_UNKNOWN, 0); }
    if (!rtv) {
        ID3D11Texture2D* bb = nullptr;
        if (SUCCEEDED(chain->GetBuffer(0, IID_PPV_ARGS(&bb)))) {
            D3D11_RENDER_TARGET_VIEW_DESC rd{}; rd.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            dev->CreateRenderTargetView(bb, &rd, &rtv);
            bb->Release();
        }
        if (!rtv) return;
    }
    DXGI_SWAP_CHAIN_DESC1 cd; chain->GetDesc1(&cd);
    float W = (float)cd.Width, H = (float)cd.Height;

    // copy the eye's part of the image (the game may reuse its image right after this frame)
    if (!eyeCopy || eyeDesc.Width != (UINT)rc.extent.width || eyeDesc.Height != (UINT)rc.extent.height || eyeDesc.Format != td.Format || eyeFmt != esw.format) {
        SAFE_RELEASE(eyeSrv); SAFE_RELEASE(eyeCopy);
        D3D11_TEXTURE2D_DESC cdsc{};
        cdsc.Width = rc.extent.width; cdsc.Height = rc.extent.height; cdsc.MipLevels = 1; cdsc.ArraySize = 1; cdsc.Format = td.Format;
        cdsc.SampleDesc.Count = 1; cdsc.Usage = D3D11_USAGE_DEFAULT; cdsc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(dev->CreateTexture2D(&cdsc, nullptr, &eyeCopy))) { Log("mirror: copy texture failed (format %d)", td.Format); failed = true; return; }
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{}; sd.Format = ViewFormat(td.Format, esw.format); sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
        if (FAILED(dev->CreateShaderResourceView(eyeCopy, &sd, &eyeSrv))) { Log("mirror: eye view failed (format %d / %lld)", td.Format, (long long)esw.format); failed = true; return; }
        eyeDesc = cdsc; eyeFmt = esw.format;
        Log("mirror: eye %dx%d format %d (asked %lld)", rc.extent.width, rc.extent.height, td.Format, (long long)esw.format);
    }
    D3D11_BOX box{ (UINT)rc.offset.x, (UINT)rc.offset.y, 0, (UINT)(rc.offset.x + rc.extent.width), (UINT)(rc.offset.y + rc.extent.height), 1 };
    ctx->CopySubresourceRegion(eyeCopy, 0, 0, 0, 0, etex, D3D11CalcSubresource(0, view.subImage.imageArrayIndex, td.MipLevels), &box);

    // Crop: centred on where the eye looks straight ahead, zoomed, then cut to the window's shape.
    const XrFovf& fov = view.fov;
    float tL = std::tan(fov.angleLeft), tR = std::tan(fov.angleRight), tU = std::tan(fov.angleUp), tD = std::tan(fov.angleDown);
    float ex = (float)rc.extent.width, ey = (float)rc.extent.height;
    float cu = (0 - tL) / (tR - tL), cv = (tU - 0) / (tU - tD);
    float z = std::clamp(zoom, 1.f, 3.f);
    float vw = ex / z, vh = vw * H / W;
    if (vh > ey / z) { vh = ey / z; vw = vh * W / H; }
    if (vw > ex) { vw = ex; } if (vh > ey) { vh = ey; }
    float cw = vw / ex, ch = vh / ey;
    float cl = std::clamp(cu - cw / 2, 0.f, 1.f - cw), ct = std::clamp(cv - ch / 2, 0.f, 1.f - ch);

    struct CB { float P[4][4]; float UV[4]; float Mode[4]; } c{};
    ID3D11DeviceContext1* c1 = nullptr;
    if (FAILED(ctx->QueryInterface(IID_PPV_ARGS(&c1)))) { failed = true; return; }
    ID3DDeviceContextState* prev = nullptr;
    c1->SwapDeviceContextState(state, &prev);
    D3D11_VIEWPORT vp{ 0, 0, W, H, 0, 1 };
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(rast);
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(vs, nullptr, 0); ctx->PSSetShader(ps, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &cb); ctx->PSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetSamplers(0, 1, &samp);
    auto draw = [&](ID3D11ShaderResourceView* srv, float mode, ID3D11BlendState* bs) {
        c.Mode[0] = mode;
        ctx->UpdateSubresource(cb, 0, nullptr, &c, 0, 0);
        ctx->OMSetBlendState(bs, nullptr, 0xffffffff);
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->Draw(4, 0);
    };
    // the eye picture, filling the window
    const float full[4][2] = { { -1, 1 }, { 1, 1 }, { -1, -1 }, { 1, -1 } };
    for (int i = 0; i < 4; i++) { c.P[i][0] = full[i][0]; c.P[i][1] = full[i][1]; c.P[i][2] = 0.5f; c.P[i][3] = 1; }
    c.UV[0] = cl; c.UV[1] = ct; c.UV[2] = cl + cw; c.UV[3] = ct + ch;
    draw(eyeSrv, 0, blendOpaque);

    // every quad layer after the game's picture, where it really is
    XrPosef eyeInv = PInv(view.pose);
    std::map<uint64_t, std::pair<bool, XrPosef>> spaces;
    bool past = false;
    for (auto* L : layers) {
        if ((const void*)L == (const void*)proj) { past = true; continue; }
        if (!past || L->type != XR_TYPE_COMPOSITION_LAYER_QUAD) continue;
        auto* q = (const XrCompositionLayerQuad*)L;
        if (q->eyeVisibility == XR_EYE_VISIBILITY_LEFT && eye != 0) continue;
        if (q->eyeVisibility == XR_EYE_VISIBILITY_RIGHT && eye != 1) continue;
        auto qs = sws.find((uint64_t)q->subImage.swapchain);
        if (qs == sws.end() || qs->second.last < 0 || qs->second.last >= (int)qs->second.imgs.size() || qs->second.arraySize > 1) continue;
        ID3D11ShaderResourceView* srv = Srv(qs->second.imgs[qs->second.last], qs->second.format);
        if (!srv) continue;
        auto sp = spaces.find((uint64_t)q->space);
        if (sp == spaces.end()) {
            XrPosef rel{ {0,0,0,1}, {0,0,0} };
            bool ok = q->space == proj->space || locate(q->space, proj->space, t, rel);
            sp = spaces.emplace((uint64_t)q->space, std::make_pair(ok, rel)).first;
        }
        if (!sp->second.first) continue;
        XrPosef inEye = PMul(eyeInv, PMul(sp->second.second, q->pose)); // quad pose in the eye's frame
        float hw = q->size.width / 2, hh = q->size.height / 2;
        const float cs[4][2] = { { -hw, hh }, { hw, hh }, { -hw, -hh }, { hw, -hh } };
        bool anyFront = false;
        for (int i = 0; i < 4; i++) {
            Vec3 p = V(inEye.position) + Rotate(inEye.orientation, { cs[i][0], cs[i][1], 0 });
            float w = -p.z;
            float x = (2 * p.x + (tR + tL) * p.z) / (tR - tL);
            float y = (2 * p.y + (tU + tD) * p.z) / (tU - tD);
            // full eye frame -> the cropped window
            x = x / cw + w * (1 - 2 * cl - cw) / cw;
            y = y / ch + w * (1 - 1 / ch + 2 * ct / ch);
            c.P[i][0] = x; c.P[i][1] = y; c.P[i][2] = w - 0.01f; c.P[i][3] = w; // behind the eye -> clipped
            if (w > 0.01f) anyFront = true;
        }
        if (!anyFront) continue;
        float iw = (float)qs->second.w, ih = (float)qs->second.h;
        const XrRect2Di& r = q->subImage.imageRect;
        c.UV[0] = r.offset.x / iw; c.UV[1] = r.offset.y / ih; c.UV[2] = (r.offset.x + r.extent.width) / iw; c.UV[3] = (r.offset.y + r.extent.height) / ih;
        float mode = !(q->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) ? 0.f
                   : (q->layerFlags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT) ? 2.f : 1.f;
        draw(srv, mode, mode == 0.f ? blendOpaque : blendPremul);
    }
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
    c1->SwapDeviceContextState(prev, nullptr);
    SAFE_RELEASE(prev); c1->Release();
    if (grab) { // tests: read the picture back
        grab = false;
        ID3D11Texture2D* bb = nullptr;
        if (SUCCEEDED(chain->GetBuffer(0, IID_PPV_ARGS(&bb)))) {
            D3D11_TEXTURE2D_DESC bd; bb->GetDesc(&bd);
            bd.Usage = D3D11_USAGE_STAGING; bd.BindFlags = 0; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; bd.MiscFlags = 0;
            ID3D11Texture2D* st = nullptr;
            if (SUCCEEDED(dev->CreateTexture2D(&bd, nullptr, &st))) {
                ctx->CopyResource(st, bb);
                D3D11_MAPPED_SUBRESOURCE m;
                if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
                    grabW = bd.Width; grabH = bd.Height; grabbed.resize((size_t)grabW * grabH);
                    for (int y = 0; y < grabH; y++) memcpy(&grabbed[(size_t)y * grabW], (uint8_t*)m.pData + (size_t)y * m.RowPitch, grabW * 4);
                    ctx->Unmap(st, 0);
                }
                st->Release();
            }
            bb->Release();
        }
    }
    chain->Present(0, DXGI_PRESENT_DO_NOT_WAIT); // never hold the game up
}
