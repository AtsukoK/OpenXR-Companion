// Headset mirror window: one eye of the game's picture with every quad layer (ours and the game's)
// drawn on top at its real place, so the desktop shows what the headset shows.
#include "mirror.h"
#include <d3dcompiler.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;

static HeadsetMirror* gSelf = nullptr;
static const wchar_t* kClass = L"OpenXRCompanionMirror";
static const UINT WM_MIRROR_QUIT = WM_APP + 7;

// ------------------------------------------------------------ swapchain bookkeeping
void HeadsetMirror::OnCreate(XrSwapchain sc, const XrSwapchainCreateInfo* ci) {
    std::lock_guard l(mx);
    Sw& s = sws[(uint64_t)sc];
    s = Sw{};
    s.format = ci->format;
    s.w = ci->width;
    s.h = ci->height;
    s.arraySize = ci->arraySize;
    s.mips = ci->mipCount;
    s.samples = ci->sampleCount;
}
void HeadsetMirror::OnImages(XrSwapchain sc, const XrSwapchainImageBaseHeader* imgs, uint32_t n) {
    if (!imgs || !n || imgs->type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) return;
    std::lock_guard l(mx);
    auto it = sws.find((uint64_t)sc);
    if (it == sws.end()) return;
    auto* im = (const XrSwapchainImageD3D11KHR*)imgs;
    it->second.imgs.clear();
    for (uint32_t i = 0; i < n; i++)
        it->second.imgs.push_back(im[i].texture);
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
        srvs.erase(t);
    }
    sws.erase(it);
}

// ------------------------------------------------------------ window
void HeadsetMirror::SetFullScreen(HWND window, bool on) {
    if (on == isFullScreen) return;
    if (on) {
        MONITORINFO mi{ sizeof mi };
        if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &mi)) return;
        styleBeforeFullScreen = GetWindowLongW(window, GWL_STYLE);
        GetWindowPlacement(window, &placeBeforeFullScreen);
        SetWindowLongW(window, GWL_STYLE, (styleBeforeFullScreen & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        const RECT& r = mi.rcMonitor; // the whole monitor, not rcWork (which stops at the taskbar)
        SetWindowPos(window, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        SetWindowLongW(window, GWL_STYLE, styleBeforeFullScreen);
        SetWindowPlacement(window, &placeBeforeFullScreen);
        SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    isFullScreen = on;
}

LRESULT CALLBACK HeadsetMirror::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    HeadsetMirror* self = gSelf;
    switch (m) {
    case WM_SIZE:
        if (self && LOWORD(l) > 0 && HIWORD(l) > 0) {
            self->clientW = LOWORD(l);
            self->clientH = HIWORD(l);
            InterlockedExchange(&self->sized, 1);
        }
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
    case WM_SYSCOMMAND: // maximize (button or title double-click) = borderless full screen over the taskbar too
        if (self && (w & 0xFFF0) == SC_MAXIMIZE) {
            self->SetFullScreen(h, true);
            return 0;
        }
        break;
    case WM_ACTIVATE: // it got focus anyway (title bar, maximize button): give it straight back to the game,
                      // or the game's keys (quick load from a wrist button...) would land in this window
        if (LOWORD(w) != WA_INACTIVE && l) {
            DWORD pid = 0;
            GetWindowThreadProcessId((HWND)l, &pid);
            if (pid == GetCurrentProcessId()) SetForegroundWindow((HWND)l);
        }
        break;
    case WM_LBUTTONDBLCLK: // double-click the picture: in and out of full screen
        if (self) self->SetFullScreen(h, !self->isFullScreen);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void HeadsetMirror::StartWindow() {
    if (thread) return;
    gSelf = this;
    userClosed = 0;
    thread = CreateThread(
        nullptr, 0,
        [](LPVOID p) -> DWORD {
            auto* self = (HeadsetMirror*)p;
            SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            WNDCLASSEXW wc{ sizeof(wc) };
            wc.style = CS_DBLCLKS;
            wc.lpfnWndProc = WndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = kClass;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
            RegisterClassExW(&wc);
            RECT r{ 0, 0, 1280, 720 };
            AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
            HWND h =
                CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_APPWINDOW, kClass, L"OpenXR Companion - Headset mirror", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
            if (!h) {
                Log("mirror: window failed %lu", GetLastError());
                return 1;
            }
            RECT cr;
            GetClientRect(h, &cr);
            self->clientW = std::max(1L, cr.right);
            self->clientH = std::max(1L, cr.bottom);
            ShowWindow(h, SW_SHOWNOACTIVATE); // don't take the keyboard from the game
            self->topApplied = false;         // Frame applies on-top to the new window
            self->hwnd = h;
            MSG m;
            while (GetMessageW(&m, nullptr, 0, 0) > 0) {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
            self->hwnd = nullptr;
            self->isFullScreen = false;
            return 0;
        },
        this, 0, nullptr);
}

void HeadsetMirror::StopWindow() {
    if (!thread) return;
    for (int i = 0; i < 100 && !hwnd; i++)
        Sleep(5); // still starting up
    if (hwnd) PostMessageW(hwnd, WM_MIRROR_QUIT, 0, 0);
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);
    thread = nullptr;
    hwnd = nullptr;
}

void HeadsetMirror::SetOn(bool v) {
    if (v == on) return;
    on = v;
    if (on) {
        failed = false;
        StartWindow();
        Log("mirror: on");
    } else {
        rtv.Reset();
        chain.Reset(); // the swapchain belongs to the window
        StopWindow();
        Log("mirror: off");
    }
}

bool HeadsetMirror::ClosedByUser() {
    return InterlockedExchange(&userClosed, 0) != 0;
}

void HeadsetMirror::Shutdown() {
    SetOn(false);
    ReleaseGpu();
}

// ------------------------------------------------------------ GPU
static const char* kShader = R"(
cbuffer C : register(b0) { float4 P[4]; float4 UV; float4 Mode; float4 Fade; };
Texture2D T : register(t0); SamplerState S : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID) {
    V o; o.pos = P[id];
    o.uv = float2((id & 1) ? UV.z : UV.x, (id & 2) ? UV.w : UV.y);
    return o;
}
// Catmull-Rom from 9 bilinear taps: a sharper stretch than plain linear when the window is bigger than the picture
float4 Bicubic(float2 uv) {
    float w, h; T.GetDimensions(w, h);
    float2 sz = float2(w, h), p = uv * sz, t1 = floor(p - 0.5) + 0.5, f = p - t1;
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12 = w1 + w2, o12 = w2 / w12;
    float2 t0 = (t1 - 1) / sz, t3 = (t1 + 2) / sz, t12 = (t1 + o12) / sz;
    float4 c = 0;
    c += T.SampleLevel(S, float2(t0.x, t0.y), 0) * w0.x * w0.y;   c += T.SampleLevel(S, float2(t12.x, t0.y), 0) * w12.x * w0.y;  c += T.SampleLevel(S, float2(t3.x, t0.y), 0) * w3.x * w0.y;
    c += T.SampleLevel(S, float2(t0.x, t12.y), 0) * w0.x * w12.y; c += T.SampleLevel(S, float2(t12.x, t12.y), 0) * w12.x * w12.y; c += T.SampleLevel(S, float2(t3.x, t12.y), 0) * w3.x * w12.y;
    c += T.SampleLevel(S, float2(t0.x, t3.y), 0) * w0.x * w3.y;   c += T.SampleLevel(S, float2(t12.x, t3.y), 0) * w12.x * w3.y;  c += T.SampleLevel(S, float2(t3.x, t3.y), 0) * w3.x * w3.y;
    return saturate(c);
}
// Shrinking with no mipmaps: four taps across one window pixel's footprint (fine up to a 3x shrink)
float4 Footprint(float2 uv) {
    float2 dx = ddx(uv) * 0.25, dy = ddy(uv) * 0.25;
    return 0.25 * (T.SampleLevel(S, uv + dx + dy, 0) + T.SampleLevel(S, uv + dx - dy, 0) + T.SampleLevel(S, uv - dx + dy, 0) +
                   T.SampleLevel(S, uv - dx - dy, 0));
}
float4 ps(V i) : SV_Target {
    float4 c = Mode.y > 0.5 ? Bicubic(i.uv) : Mode.z > 0.5 ? Footprint(i.uv) : T.Sample(S, i.uv); // else the mipmaps (trilinear)
    float4 o = Mode.x < 0.5 ? float4(c.rgb, 1)            // opaque
             : Mode.x > 1.5 ? float4(c.rgb * c.a, c.a)  // straight alpha -> premultiplied
             : c;                                        // premultiplied
    if (Fade.z > 0.5) o *= saturate((i.uv.x - Fade.x) / (Fade.y - Fade.x)); // soft edge (both eyes: the nose side)
    return o;
}
)";

bool HeadsetMirror::InitGpu(ID3D11Device* d) {
    if (dev.Get() == d && vs) return true;
    ReleaseGpu();
    dev = d; // ComPtr takes its own reference
    ComPtr<ID3DBlob> bv, bp, err;
    if (FAILED(D3DCompile(kShader, strlen(kShader), "mirror", nullptr, nullptr, "vs", "vs_4_0", 0, 0, &bv, &err)) ||
        FAILED(D3DCompile(kShader, strlen(kShader), "mirror", nullptr, nullptr, "ps", "ps_4_0", 0, 0, &bp, &err))) {
        Log("mirror: shader compile failed %s", err ? (const char*)err->GetBufferPointer() : "");
        return false;
    }
    dev->CreateVertexShader(bv->GetBufferPointer(), bv->GetBufferSize(), nullptr, &vs);
    dev->CreatePixelShader(bp->GetBufferPointer(), bp->GetBufferSize(), nullptr, &ps);
    D3D11_BUFFER_DESC bd{ 112, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER };
    dev->CreateBuffer(&bd, nullptr, &cb);
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    sd.MipLODBias = -0.5f; // sharper: plain trilinear already blends in the half-size picture at a 1.2x shrink, which looked soft
    dev->CreateSamplerState(&sd, &samp);
    D3D11_BLEND_DESC bl{};
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev->CreateBlendState(&bl, &blendOpaque);
    auto& rt = bl.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_ONE;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    dev->CreateBlendState(&bl, &blendPremul);
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &rast);
    // our own pipeline state, so the game's is left exactly as it was
    ComPtr<ID3D11Device1> d1;
    if (SUCCEEDED(dev.As(&d1))) {
        D3D_FEATURE_LEVEL fl = dev->GetFeatureLevel();
        d1->CreateDeviceContextState(0, &fl, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &state);
    }
    if (!vs || !ps || !cb || !state) {
        Log("mirror: GPU setup failed");
        return false;
    }
    return true;
}

void HeadsetMirror::ReleaseGpu() {
    srvs.clear();
    eyeSrv.Reset();
    eyeCopy.Reset();
    eyeDesc = {};
    rtv.Reset();
    chain.Reset();
    vs.Reset();
    ps.Reset();
    cb.Reset();
    samp.Reset();
    blendOpaque.Reset();
    blendPremul.Reset();
    rast.Reset();
    state.Reset();
    dev.Reset();
}

static DXGI_FORMAT ViewFormat(DXGI_FORMAT tex, int64_t asked) {
    if (asked) return (DXGI_FORMAT)asked; // the format the swapchain was asked for is the one its pixels are in
    switch (tex) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default:
        return tex;
    }
}

ID3D11ShaderResourceView* HeadsetMirror::Srv(ID3D11Texture2D* t, int64_t fmt) {
    auto it = srvs.find(t);
    if (it != srvs.end()) return it->second.Get();
    D3D11_TEXTURE2D_DESC d;
    t->GetDesc(&d);
    ComPtr<ID3D11ShaderResourceView> v;
    if ((d.BindFlags & D3D11_BIND_SHADER_RESOURCE) && d.SampleDesc.Count == 1) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = ViewFormat(d.Format, fmt);
        if (d.ArraySize > 1) {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            sd.Texture2DArray.MipLevels = 1;
            sd.Texture2DArray.ArraySize = 1;
        } else {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MipLevels = 1;
        }
        if (FAILED(dev->CreateShaderResourceView(t, &sd, &v))) v.Reset();
    }
    srvs[t] = v; // null too: don't try again every frame
    return v.Get();
}

// The window's swapchain and render target, made on first use and resized with the window.
bool HeadsetMirror::PrepareWindow(HWND h, float& W, float& H) {
    if (!chain) {
        ComPtr<IDXGIDevice> dx;
        ComPtr<IDXGIAdapter> ad;
        ComPtr<IDXGIFactory2> fac;
        if (SUCCEEDED(dev.As(&dx)) && SUCCEEDED(dx->GetAdapter(&ad)) &&
            SUCCEEDED(ad->GetParent(IID_PPV_ARGS(&fac)))) {
            DXGI_SWAP_CHAIN_DESC1 sd{};
            sd.Width = clientW;
            sd.Height = clientH;
            sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            sd.SampleDesc.Count = 1;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.BufferCount = 2;
            sd.Scaling = DXGI_SCALING_STRETCH;
            sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            HRESULT hr = fac->CreateSwapChainForHwnd(dev.Get(), h, &sd, nullptr, nullptr, &chain);
            if (FAILED(hr))
                Log("mirror: swapchain failed 0x%08lx", hr);
            else
                fac->MakeWindowAssociation(h, DXGI_MWA_NO_ALT_ENTER);
        }
        if (!chain) {
            failed = true;
            return false;
        }
        InterlockedExchange(&sized, 0);
    }
    if (InterlockedExchange(&sized, 0)) {
        rtv.Reset();
        chain->ResizeBuffers(0, clientW, clientH, DXGI_FORMAT_UNKNOWN, 0);
    }
    if (!rtv) {
        ComPtr<ID3D11Texture2D> bb;
        if (SUCCEEDED(chain->GetBuffer(0, IID_PPV_ARGS(&bb)))) {
            D3D11_RENDER_TARGET_VIEW_DESC rd{};
            rd.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            dev->CreateRenderTargetView(bb.Get(), &rd, &rtv);
        }
        if (!rtv) return false;
    }
    DXGI_SWAP_CHAIN_DESC1 cd;
    chain->GetDesc1(&cd);
    W = (float)cd.Width;
    H = (float)cd.Height;
    return true;
}

// No VR view this frame (TTWVR's theater menus, loading screens): show the game's flat screen, its biggest
// quad layer, fitted to the window. Black when there is nothing to show, never the window's old garbage.
void HeadsetMirror::FlatFrame(ID3D11DeviceContext* ctx, const std::vector<const XrCompositionLayerBaseHeader*>& layers, HWND h) {
    const XrCompositionLayerQuad* best = nullptr;
    Sw* bestSw = nullptr;
    for (auto* L : layers) {
        if (L->type != XR_TYPE_COMPOSITION_LAYER_QUAD) continue;
        auto* q = (const XrCompositionLayerQuad*)L;
        auto it = sws.find((uint64_t)q->subImage.swapchain);
        if (it == sws.end() || it->second.last < 0 || it->second.last >= (int)it->second.imgs.size() || it->second.arraySize > 1) continue;
        if (best && q->size.width * q->size.height <= best->size.width * best->size.height) continue;
        best = q;
        bestSw = &it->second;
    }
    if (best != flatShown) {
        flatShown = best;
        Log(best ? "mirror: no VR view, showing the game's flat screen" : "mirror: no VR view and no flat screen: black");
    }
    float W, H;
    if (!PrepareWindow(h, W, H)) return;
    const float black[4] = { 0, 0, 0, 1 };
    ctx->ClearRenderTargetView(rtv.Get(), black);
    ID3D11ShaderResourceView* srv = best ? Srv(bestSw->imgs[bestSw->last], bestSw->format) : nullptr;
    const XrRect2Di& r = best ? best->subImage.imageRect : XrRect2Di{};
    if (srv && r.extent.width > 0 && r.extent.height > 0) {
        ComPtr<ID3D11DeviceContext1> c1;
        if (FAILED(ctx->QueryInterface(IID_PPV_ARGS(&c1)))) return;
        ComPtr<ID3DDeviceContextState> prev; // the game's own state, put back after drawing
        c1->SwapDeviceContextState(state.Get(), &prev);
        // fit, keeping the screen's shape (its size in metres, as the headset shows it)
        float aspect = best->size.height > 0 ? best->size.width / best->size.height : (float)r.extent.width / r.extent.height;
        float sx = 1.f, sy = 1.f;
        if (aspect > W / H)
            sy = (W / aspect) / H;
        else
            sx = (H * aspect) / W;
        struct CB {
            float P[4][4];
            float UV[4];
            float Mode[4];
            float Fade[4];
        } c{};
        const float corner[4][2] = { { -1, 1 }, { 1, 1 }, { -1, -1 }, { 1, -1 } };
        for (int i = 0; i < 4; i++) {
            c.P[i][0] = corner[i][0] * sx;
            c.P[i][1] = corner[i][1] * sy;
            c.P[i][2] = 0.5f;
            c.P[i][3] = 1;
        }
        float iw = (float)bestSw->w, ih = (float)bestSw->h;
        c.UV[0] = r.offset.x / iw;
        c.UV[1] = r.offset.y / ih;
        c.UV[2] = (r.offset.x + r.extent.width) / iw;
        c.UV[3] = (r.offset.y + r.extent.height) / ih;
        c.Mode[1] = W * sx > r.extent.width * 1.02f ? 1.f : 0.f; // stretched -> sharper bicubic
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &c, 0, 0);
        D3D11_VIEWPORT vp{ 0, 0, W, H, 0, 1 };
        ctx->RSSetViewports(1, &vp);
        ctx->RSSetState(rast.Get());
        ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
        ctx->OMSetBlendState(blendOpaque.Get(), nullptr, 0xffffffff);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ctx->IASetInputLayout(nullptr);
        ctx->VSSetShader(vs.Get(), nullptr, 0);
        ctx->PSSetShader(ps.Get(), nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        ctx->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        ctx->PSSetSamplers(0, 1, samp.GetAddressOf());
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->Draw(4, 0);
        ID3D11ShaderResourceView* none = nullptr;
        ctx->PSSetShaderResources(0, 1, &none);
        c1->SwapDeviceContextState(prev.Get(), nullptr);
    }
    GrabIfAsked(ctx);
    chain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
}

// Tests: read the window's picture back.
void HeadsetMirror::GrabIfAsked(ID3D11DeviceContext* ctx) {
    if (!grab) return;
    grab = false;
    ComPtr<ID3D11Texture2D> bb;
    if (SUCCEEDED(chain->GetBuffer(0, IID_PPV_ARGS(&bb)))) {
        D3D11_TEXTURE2D_DESC bd;
        bb->GetDesc(&bd);
        bd.Usage = D3D11_USAGE_STAGING;
        bd.BindFlags = 0;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        bd.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> st;
        if (SUCCEEDED(dev->CreateTexture2D(&bd, nullptr, &st))) {
            ctx->CopyResource(st.Get(), bb.Get());
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(ctx->Map(st.Get(), 0, D3D11_MAP_READ, 0, &m))) {
                grabW = bd.Width;
                grabH = bd.Height;
                grabbed.resize((size_t)grabW * grabH);
                for (int y = 0; y < grabH; y++)
                    memcpy(&grabbed[(size_t)y * grabW], (uint8_t*)m.pData + (size_t)y * m.RowPitch, grabW * 4);
                ctx->Unmap(st.Get(), 0);
            }
        }
    }
}

// ------------------------------------------------------------ one frame
void HeadsetMirror::Frame(ID3D11Device* d, ID3D11DeviceContext* ctx, const std::vector<const XrCompositionLayerBaseHeader*>& layers,
                          XrTime t, const LocateFn& locate) {
    if (!on || failed || !d || !ctx) return;
    if (fps > 0) {
        double now = NowSec();
        if (now - lastFrame < 1.0 / fps - 0.002) return;
        lastFrame = now;
    }
    HWND h = hwnd;
    if (!h) return;
    if (onTop != topApplied) { // async: never wait on the window thread from the game's frame
        topApplied = onTop;
        SetWindowPos(h, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    }
    if (!InitGpu(d)) {
        failed = true;
        return;
    }
    std::lock_guard l(mx);

    // the game's picture for this eye
    const XrCompositionLayerProjection* proj = nullptr;
    for (auto* L : layers)
        if (L->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            proj = (const XrCompositionLayerProjection*)L;
            break;
        }
    if (!proj || proj->viewCount < 1) {
        FlatFrame(ctx, layers, h);
        return;
    }
    flatShown = nullptr;
    const XrCompositionLayerProjectionView& view = proj->views[std::min<uint32_t>(eye, proj->viewCount - 1)];
    auto sit = sws.find((uint64_t)view.subImage.swapchain);
    if (sit == sws.end() || sit->second.last < 0 || sit->second.last >= (int)sit->second.imgs.size()) return;
    Sw& esw = sit->second;
    ID3D11Texture2D* etex = esw.imgs[esw.last];
    D3D11_TEXTURE2D_DESC td;
    etex->GetDesc(&td);
    if (td.SampleDesc.Count != 1) {
        if (!failed) Log("mirror: multisampled eye images aren't supported");
        failed = true;
        return;
    }
    XrRect2Di rc = view.subImage.imageRect;
    if (rc.extent.width <= 0 || rc.extent.height <= 0) return;

    float W, H;
    if (!PrepareWindow(h, W, H)) return;

    // Crop: centred on where the eye looks straight ahead, zoomed, then cut to the window's shape.
    const XrFovf& fov = view.fov;
    float tL = std::tan(fov.angleLeft), tR = std::tan(fov.angleRight), tU = std::tan(fov.angleUp), tD = std::tan(fov.angleDown);
    float ex = (float)rc.extent.width, ey = (float)rc.extent.height;
    float cu = (0 - tL) / (tR - tL), cv = (tU - 0) / (tU - tD);
    float z = std::clamp(zoom, 1.f, 3.f);
    float vw = ex / z, vh = vw * H / W;
    if (vh > ey / z) {
        vh = ey / z;
        vw = vh * W / H;
    }
    if (vw > ex) {
        vw = ex;
    }
    if (vh > ey) {
        vh = ey;
    }
    float cw = vw / ex, ch = vh / ey;
    // Sideways: centred on where the eye looks straight ahead (on Quest that sits toward the nose, so the
    // middle of the picture put the view too far to one side), but never closer than kSideMargin to the
    // picture's side edges: the nose side has a black unrendered strip. Small margin top and bottom.
    const float kSideMargin = 0.12f; // 0.08 still showed the strip on Quest 3 when steadying lagged
    const float m = 0.04f;
    bool steady = smoothing > 0.001f;
    float sideM = std::max(m, kSideMargin);
    // Both eyes: the other eye fills the nose side, so the view can centre on where this eye looks even at zoom 1.
    // Its picture is drawn as if far away (only its direction counts): distant things line up, close ones double a little.
    const bool noseLeft = eye == 1; // the right eye's nose side is on its left
    const XrCompositionLayerProjectionView* ov = nullptr;
    ID3D11ShaderResourceView* otherSrv = nullptr;
    float ou0 = 0, ov0 = 0, ou1 = 1, ov1 = 1;
    if (bothEyes && proj->viewCount > 1) {
        ov = &proj->views[eye ? 0 : 1];
        auto oit = sws.find((uint64_t)ov->subImage.swapchain);
        if (oit != sws.end() && oit->second.last >= 0 && oit->second.last < (int)oit->second.imgs.size() &&
            (oit->second.arraySize == 1 || ov->subImage.imageArrayIndex == 0) && oit->second.samples == 1) {
            otherSrv = Srv(oit->second.imgs[oit->second.last], oit->second.format);
            float tw = (float)oit->second.w, th = (float)oit->second.h;
            const XrRect2Di& orc = ov->subImage.imageRect;
            ou0 = orc.offset.x / tw;
            ov0 = orc.offset.y / th;
            ou1 = (orc.offset.x + orc.extent.width) / tw;
            ov1 = (orc.offset.y + orc.extent.height) / th;
        }
        if (!otherSrv) ov = nullptr;
    }
    const bool combine = ov != nullptr;
    float mL = sideM, mR = sideM; // how close the view may come to this eye picture's left / right edge
    if (combine) (noseLeft ? mL : mR) = -0.3f;
    // Steadying never changes the crop: zoom means the same with it on or off (1 = the whole eye picture).
    // It lags only into the room the zoom leaves (limited below), so at 1 it barely moves; from ~1.3 it has room.
    float lo = mL, hi = 1.f - mR - cw;
    float cl = lo <= hi ? std::clamp(cu - cw / 2, lo, hi) : (lo + hi) / 2;
    float ct = std::clamp(cv - ch / 2, std::min(m, (1.f - ch) / 2), std::max(1.f - m - ch, (1.f - ch) / 2));

    // window pixels per eye pixel: above 1 the picture is stretched -> sharper bicubic stretch
    bool magnify = W / (cw * ex) > 1.02f;

    // The eye picture: straight from the game's image (light), or a copied part with mipmaps for smooth shrinking.
    // Drawing from the game's image is safe here: it is drawn now, before the game gets the image back.
    float shrink = (cw * ex) / W;
    ID3D11ShaderResourceView* direct = light && esw.arraySize == 1 && shrink <= 3.f ? Srv(etex, esw.format) : nullptr;
    lastDirect = direct != nullptr;
    ID3D11ShaderResourceView* eyeView = direct;
    // eye-picture UV (0..1 over the eye's part) -> the texture drawn from
    float uo = 0, vo = 0, uScale = 1, vScale = 1;
    if (direct) {
        uo = rc.offset.x / (float)td.Width;
        vo = rc.offset.y / (float)td.Height;
        uScale = ex / td.Width;
        vScale = ey / td.Height;
    } else {
        if (!eyeCopy || eyeDesc.Width != (UINT)rc.extent.width || eyeDesc.Height != (UINT)rc.extent.height || eyeDesc.Format != td.Format ||
            eyeFmt != esw.format) {
            eyeSrv.Reset();
            eyeCopy.Reset();
            D3D11_TEXTURE2D_DESC cdsc{};
            cdsc.Width = rc.extent.width;
            cdsc.Height = rc.extent.height;
            cdsc.MipLevels = 0;
            cdsc.ArraySize = 1;
            cdsc.Format = td.Format; // full mip chain, for smooth shrinking
            cdsc.SampleDesc.Count = 1;
            cdsc.Usage = D3D11_USAGE_DEFAULT;
            cdsc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            cdsc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
            D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = ViewFormat(td.Format, esw.format);
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MipLevels = (UINT)-1;
            eyeMips =
                SUCCEEDED(dev->CreateTexture2D(&cdsc, nullptr, &eyeCopy)) && SUCCEEDED(dev->CreateShaderResourceView(eyeCopy.Get(), &sd, &eyeSrv));
            if (!eyeMips) { // format can't make mipmaps: one level, as before
                eyeSrv.Reset();
                eyeCopy.Reset();
                cdsc.MipLevels = 1;
                cdsc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                cdsc.MiscFlags = 0;
                sd.Texture2D.MipLevels = 1;
                if (FAILED(dev->CreateTexture2D(&cdsc, nullptr, &eyeCopy))) {
                    Log("mirror: copy texture failed (format %d)", td.Format);
                    failed = true;
                    return;
                }
            }
            if (!eyeSrv && FAILED(dev->CreateShaderResourceView(eyeCopy.Get(), &sd, &eyeSrv))) {
                Log("mirror: eye view failed (format %d / %lld)", td.Format, (long long)esw.format);
                failed = true;
                return;
            }
            eyeDesc = cdsc;
            eyeFmt = esw.format;
            Log("mirror: eye %dx%d format %d (asked %lld) mips %d", rc.extent.width, rc.extent.height, td.Format, (long long)esw.format,
                (int)eyeMips);
        }
        D3D11_BOX box{
            (UINT)rc.offset.x, (UINT)rc.offset.y, 0, (UINT)(rc.offset.x + rc.extent.width), (UINT)(rc.offset.y + rc.extent.height), 1
        };
        ctx->CopySubresourceRegion(eyeCopy.Get(), 0, 0, 0, 0, etex, D3D11CalcSubresource(0, view.subImage.imageArrayIndex, td.MipLevels), &box);
        if (eyeMips) ctx->GenerateMips(eyeSrv.Get());
        eyeView = eyeSrv.Get();
    }
    struct CB {
        float P[4][4];
        float UV[4];
        float Mode[4];
        float Fade[4];
    } c{};
    ComPtr<ID3D11DeviceContext1> c1;
    if (FAILED(ctx->QueryInterface(IID_PPV_ARGS(&c1)))) {
        failed = true;
        return;
    }
    ComPtr<ID3DDeviceContextState> prev; // the game's own state, put back after drawing
    c1->SwapDeviceContextState(state.Get(), &prev);
    D3D11_VIEWPORT vp{ 0, 0, W, H, 0, 1 };
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(rast.Get());
    ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(vs.Get(), nullptr, 0);
    ctx->PSSetShader(ps.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
    ctx->PSSetSamplers(0, 1, samp.GetAddressOf());
    auto draw = [&](ID3D11ShaderResourceView* srv, float mode, ID3D11BlendState* bs) {
        c.Mode[0] = mode;
        bool isOther = otherSrv && srv == otherSrv, isEye = srv == eyeView || isOther;
        c.Mode[1] = isEye && magnify ? 1.f : 0.f;
        c.Mode[2] = isEye && (direct || isOther) && !magnify ? 1.f : 0.f;
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &c, 0, 0);
        ctx->OMSetBlendState(bs, nullptr, 0xffffffff);
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->Draw(4, 0);
    };
    // Steadied camera: the head's real position, its direction smoothed (shakes ironed out, real turns
    // followed). Everything, the game's picture included, is drawn as seen from it.
    XrPosef cam = view.pose;
    if (steady) {
        float dt = lastT && t > lastT ? (float)((t - lastT) * 1e-9) : 0.f;
        cam.orientation = camSmooth.Apply(view.pose, dt, smoothing).orientation;
        // Never lag so far that the window runs past the eye picture's edge (or its masked border):
        // pull the camera back toward the real eye just enough to stay inside the headroom.
        const XrQuaternionf& real = view.pose.orientation;
        Vec3 f = RotateInv(real, Rotate(cam.orientation, { 0, 0, -1 }));
        if (f.z < -0.01f) {
            float dx = f.x / -f.z, dy = f.y / -f.z;
            float lx = (dx > 0 ? 1 - mR - cl - cw : cl - mL) * (tR - tL);
            float ly = (dy > 0 ? ct - m : 1 - m - ct - ch) * (tU - tD);
            lx = std::max(lx * 0.85f, 0.f);
            ly = std::max(ly * 0.85f, 0.f); // spare a little for roll
            float k = 1.f;
            if (std::fabs(dx) > lx) k = std::min(k, lx / std::fabs(dx));
            if (std::fabs(dy) > ly) k = std::min(k, ly / std::fabs(dy));
            if (k < 1.f) { // nlerp real -> smoothed by k
                XrQuaternionf a = real, b = cam.orientation;
                if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0) {
                    b.x = -b.x;
                    b.y = -b.y;
                    b.z = -b.z;
                    b.w = -b.w;
                }
                XrQuaternionf q{ a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k, a.w + (b.w - a.w) * k };
                float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
                if (n > 1e-6f) {
                    q.x /= n;
                    q.y /= n;
                    q.z /= n;
                    q.w /= n;
                    cam.orientation = q;
                }
            }
        } else
            cam.orientation = real;
    } else
        camSmooth.Reset();
    lastT = t;
    XrPosef eyeInv = PInv(cam);
    // a quad (pose in the game's space, half sizes) -> clip corners for the cropped window; false = all behind
    auto project = [&](const XrPosef& pose, float hw, float hh) {
        XrPosef inEye = PMul(eyeInv, pose);
        const float cs[4][2] = { { -hw, hh }, { hw, hh }, { -hw, -hh }, { hw, -hh } };
        bool anyFront = false;
        for (int i = 0; i < 4; i++) {
            Vec3 p = V(inEye.position) + Rotate(inEye.orientation, { cs[i][0], cs[i][1], 0 });
            float w = -p.z;
            float x = (2 * p.x + (tR + tL) * p.z) / (tR - tL);
            float y = (2 * p.y + (tU + tD) * p.z) / (tU - tD);
            x = x / cw + w * (1 - 2 * cl - cw) / cw; // full eye frame -> the cropped window
            y = y / ch + w * (1 - 1 / ch + 2 * ct / ch);
            c.P[i][0] = x;
            c.P[i][1] = y;
            c.P[i][2] = w - 0.01f;
            c.P[i][3] = w; // behind the eye -> clipped
            if (w > 0.01f) anyFront = true;
        }
        return anyFront;
    };

    // the eye picture
    if (!steady && !combine) { // straight copy, filling the window
        const float full[4][2] = { { -1, 1 }, { 1, 1 }, { -1, -1 }, { 1, -1 } };
        for (int i = 0; i < 4; i++) {
            c.P[i][0] = full[i][0];
            c.P[i][1] = full[i][1];
            c.P[i][2] = 0.5f;
            c.P[i][3] = 1;
        }
        c.UV[0] = uo + cl * uScale;
        c.UV[1] = vo + ct * vScale;
        c.UV[2] = uo + (cl + cw) * uScale;
        c.UV[3] = vo + (ct + ch) * vScale;
        draw(eyeView, 0, blendOpaque.Get());
    } else { // as a flat picture 1 m in front of the real eye, filling its field of view, seen from the steadied camera
        const float clear[4] = { 0, 0, 0, 1 };
        ctx->ClearRenderTargetView(rtv.Get(), clear);
        if (combine) { // the other eye first, underneath, far away and seen from this eye's spot
            const XrFovf& of = ov->fov;
            float oL = std::tan(of.angleLeft), oR = std::tan(of.angleRight), oU = std::tan(of.angleUp), oD = std::tan(of.angleDown);
            const float D = 50.f;
            XrPosef at{ ov->pose.orientation, view.pose.position };
            XrPosef local{ { 0, 0, 0, 1 }, { (oL + oR) / 2 * D, (oU + oD) / 2 * D, -D } };
            if (project(PMul(at, local), (oR - oL) / 2 * D, (oU - oD) / 2 * D)) {
                c.UV[0] = ou0;
                c.UV[1] = ov0;
                c.UV[2] = ou1;
                c.UV[3] = ov1;
                draw(otherSrv, 0, blendOpaque.Get());
            }
            // this eye on top, fading out toward its nose side (over its black strip)
            float f0 = noseLeft ? 0.10f : 0.90f, f1 = noseLeft ? 0.18f : 0.82f;
            c.Fade[0] = uo + f0 * uScale;
            c.Fade[1] = uo + f1 * uScale;
            c.Fade[2] = 1.f;
        }
        XrPosef local{ { 0, 0, 0, 1 }, { (tL + tR) / 2, (tU + tD) / 2, -1.f } };
        if (project(PMul(view.pose, local), (tR - tL) / 2, (tU - tD) / 2)) {
            c.UV[0] = uo;
            c.UV[1] = vo;
            c.UV[2] = uo + uScale;
            c.UV[3] = vo + vScale;
            draw(eyeView, 0, combine ? blendPremul.Get() : blendOpaque.Get());
        }
        c.Fade[2] = 0.f;
    }

    // every quad layer after the game's picture, where it really is
    std::map<uint64_t, std::pair<bool, XrPosef>> spaces;
    bool past = false;
    for (auto* L : layers) {
        if ((const void*)L == (const void*)proj) {
            past = true;
            continue;
        }
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
            XrPosef rel{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
            bool ok = q->space == proj->space || locate(q->space, proj->space, t, rel);
            sp = spaces.emplace((uint64_t)q->space, std::make_pair(ok, rel)).first;
        }
        if (!sp->second.first) continue;
        if (!project(PMul(sp->second.second, q->pose), q->size.width / 2, q->size.height / 2)) continue;
        float iw = (float)qs->second.w, ih = (float)qs->second.h;
        const XrRect2Di& r = q->subImage.imageRect;
        c.UV[0] = r.offset.x / iw;
        c.UV[1] = r.offset.y / ih;
        c.UV[2] = (r.offset.x + r.extent.width) / iw;
        c.UV[3] = (r.offset.y + r.extent.height) / ih;
        float mode = !(q->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) ? 0.f
                     : (q->layerFlags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT)     ? 2.f
                                                                                            : 1.f;
        draw(srv, mode, mode == 0.f ? blendOpaque.Get() : blendPremul.Get());
    }
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
    c1->SwapDeviceContextState(prev.Get(), nullptr);
    GrabIfAsked(ctx);
    chain->Present(0, DXGI_PRESENT_DO_NOT_WAIT); // never hold the game up
}
