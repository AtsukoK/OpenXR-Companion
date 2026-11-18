#include "screen.h"
#include "common.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>


template <class T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

struct Cfg { int monitor = 0, fps = 30, maxW = 1920; bool cursor = true; float opacity = 1.f; };

struct ScreenCapture::Impl {
    std::thread th;
    std::atomic<bool> quit{ false };
    std::mutex mx;
    Cfg cfg; uint64_t cfgVersion = 0;
    std::vector<uint32_t> px; int w = 0, h = 0; bool fresh = false;
    int count = 0, primary = 1, shown = 0; std::string msg; uint64_t msgVersion = 0;
    void Run();
    void SetMsg(const std::string& m) { std::lock_guard l(mx); if (m != msg) { msg = m; msgVersion++; Log("[screen] %s", m.c_str()); } }
};

struct OutInfo { IDXGIAdapter1* ad = nullptr; IDXGIOutput* out = nullptr; DXGI_OUTPUT_DESC desc{}; };

// Every monitor attached to the desktop, on every graphics card, sorted left to right.
static std::vector<OutInfo> Enumerate() {
    std::vector<OutInfo> v;
    IDXGIFactory1* f = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&f))) return v;
    IDXGIAdapter1* ad = nullptr;
    for (UINT a = 0; f->EnumAdapters1(a, &ad) != DXGI_ERROR_NOT_FOUND; a++) {
        IDXGIOutput* o = nullptr;
        for (UINT i = 0; ad->EnumOutputs(i, &o) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_OUTPUT_DESC d{}; o->GetDesc(&d);
            if (!d.AttachedToDesktop) { o->Release(); continue; }
            ad->AddRef();
            v.push_back({ ad, o, d });
        }
        ad->Release();
    }
    f->Release();
    std::sort(v.begin(), v.end(), [](const OutInfo& a, const OutInfo& b) {
        const RECT& x = a.desc.DesktopCoordinates; const RECT& y = b.desc.DesktopCoordinates;
        return x.left != y.left ? x.left < y.left : x.top < y.top;
    });
    return v;
}
static void Free(std::vector<OutInfo>& v) { for (auto& o : v) { SafeRelease(o.out); SafeRelease(o.ad); } v.clear(); }

std::vector<MonitorInfo> ScreenCapture::Monitors() {
    auto v = Enumerate();
    std::vector<MonitorInfo> r;
    for (auto& o : v) {
        const RECT& c = o.desc.DesktopCoordinates;
        r.push_back({ o.desc.DeviceName, (int)c.left, (int)c.top, (int)(c.right - c.left), (int)(c.bottom - c.top), c.left == 0 && c.top == 0 });
    }
    Free(v);
    return r;
}

void ScreenCapture::Configure(int monitor, int fps, int maxWidth, bool cursor, float opacity) {
    if (!d) d = new Impl;
    {
        std::lock_guard l(d->mx);
        Cfg n{ monitor, fps, maxWidth, cursor, opacity };
        if (n.monitor != d->cfg.monitor || n.fps != d->cfg.fps || n.maxW != d->cfg.maxW || n.cursor != d->cfg.cursor || n.opacity != d->cfg.opacity) {
            d->cfg = n; d->cfgVersion++;
        }
    }
    if (!d->th.joinable()) { d->quit = false; d->th = std::thread([this] { d->Run(); }); }
}

void ScreenCapture::Stop() {
    if (!d) return;
    d->quit = true;
    if (d->th.joinable()) d->th.join();
    delete d; d = nullptr;
}

bool ScreenCapture::Running() const { return d && d->th.joinable(); }

bool ScreenCapture::TakeFrame(std::vector<uint32_t>& out, int& w, int& h) {
    if (!d) return false;
    std::lock_guard l(d->mx);
    if (!d->fresh) return false;
    out = d->px; w = d->w; h = d->h; d->fresh = false;
    return true;
}

void ScreenCapture::Status(int& count, int& primary, int& shown, std::string& msg, uint64_t& ver) {
    count = 0; primary = 1; shown = 0; ver = 0;
    if (!d) return;
    std::lock_guard l(d->mx);
    count = d->count; primary = d->primary; shown = d->shown; msg = d->msg; ver = d->msgVersion;
}

// ------------------------------------------------------------ capture thread
static void SleepUntil(std::chrono::steady_clock::time_point t, const std::atomic<bool>& quit) {
    while (!quit && std::chrono::steady_clock::now() < t) std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

static void DrawPointer(std::vector<uint32_t>& px, int w, int h, int cx, int cy, int r) {
    for (int y = std::max(0, cy - r - 2); y <= std::min(h - 1, cy + r + 2); y++)
        for (int x = std::max(0, cx - r - 2); x <= std::min(w - 1, cx + r + 2); x++) {
            int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
            if (d2 <= r * r) px[(size_t)y * w + x] = 0xFFFFFFFF;
            else if (d2 <= (r + 2) * (r + 2)) px[(size_t)y * w + x] = 0xFF000000;
        }
}

void ScreenCapture::Impl::Run() {
    // Per-monitor DPI awareness for this thread only (the game's own setting is left alone):
    // Desktop Duplication can refuse callers that aren't DPI aware.
    using SetCtx = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
    if (auto f = (SetCtx)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext"))
        f(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    std::vector<uint32_t> base; int bw = 0, bh = 0; // last captured image, without the pointer
    while (!quit) {
        Cfg c; uint64_t ver;
        { std::lock_guard l(mx); c = cfg; ver = cfgVersion; }
        auto outs = Enumerate();
        if (outs.empty()) { SetMsg("no monitor found"); Free(outs); SleepUntil(std::chrono::steady_clock::now() + std::chrono::seconds(2), quit); continue; }
        int prim = 1;
        for (size_t i = 0; i < outs.size(); i++) { const RECT& r = outs[i].desc.DesktopCoordinates; if (r.left == 0 && r.top == 0) prim = (int)i + 1; }
        int idx = c.monitor <= 0 ? prim : std::min<int>(c.monitor, (int)outs.size());
        OutInfo& o = outs[idx - 1];
        { std::lock_guard l(mx); count = (int)outs.size(); primary = prim; shown = idx; }
        const RECT dr = o.desc.DesktopCoordinates;
        const int deskW = dr.right - dr.left, deskH = dr.bottom - dr.top;
        const DXGI_MODE_ROTATION rot = o.desc.Rotation;

        ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
        IDXGIOutput1* o1 = nullptr; IDXGIOutputDuplication* dup = nullptr;
        ID3D11Texture2D* mip = nullptr; ID3D11ShaderResourceView* srv = nullptr; ID3D11Texture2D* staging = nullptr;
        int level = 0, lw = 0, lh = 0;
        bool ok = false;
        D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        HRESULT hr = D3D11CreateDevice(o.ad, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, fls, 3, D3D11_SDK_VERSION, &dev, nullptr, &ctx);
        if (SUCCEEDED(hr)) hr = o.out->QueryInterface(__uuidof(IDXGIOutput1), (void**)&o1);
        if (SUCCEEDED(hr)) hr = o1->DuplicateOutput(dev, &dup);
        if (FAILED(hr)) {
            char b[160]; snprintf(b, sizeof b, "monitor %d of %d can't be mirrored (0x%08lX)%s", idx, (int)outs.size(), (unsigned long)hr,
                hr == E_ACCESSDENIED ? ": Windows is showing a secure screen (UAC / lock)" : hr == DXGI_ERROR_UNSUPPORTED ? ": not supported on this graphics card" :
                hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE ? ": too many programs are mirroring it" : "");
            SetMsg(b);
        } else {
            char b[160]; snprintf(b, sizeof b, "monitor %d of %d (%dx%d), %s", idx, (int)outs.size(), deskW, deskH,
                c.fps ? (std::to_string(c.fps) + " fps").c_str() : "every frame");
            SetMsg(b); ok = true;
        }
        Free(outs);

        auto nextAt = std::chrono::steady_clock::now();
        bool ptrVisible = false; int ptrX = 0, ptrY = 0;
        while (ok && !quit) {
            { std::lock_guard l(mx); if (cfgVersion != ver) break; } // any change: start over with a fresh full frame
            DXGI_OUTDUPL_FRAME_INFO fi{}; IDXGIResource* res = nullptr;
            hr = dup->AcquireNextFrame(100, &fi, &res);
            if (hr == DXGI_ERROR_WAIT_TIMEOUT) continue;
            if (FAILED(hr)) { // ACCESS_LOST: mode change, full-screen app, UAC... make a new duplication
                if (hr != DXGI_ERROR_ACCESS_LOST) { char b[80]; snprintf(b, sizeof b, "capture stopped (0x%08lX), retrying", (unsigned long)hr); SetMsg(b); }
                break;
            }
            bool ptrChanged = false;
            if (fi.LastMouseUpdateTime.QuadPart) {
                ptrChanged = ptrVisible != (bool)fi.PointerPosition.Visible || ptrX != fi.PointerPosition.Position.x || ptrY != fi.PointerPosition.Position.y;
                ptrVisible = fi.PointerPosition.Visible; ptrX = fi.PointerPosition.Position.x; ptrY = fi.PointerPosition.Position.y;
            }
            bool newImage = fi.LastPresentTime.QuadPart != 0; // 0 = only the mouse moved (the image may be empty)
            if (newImage && res) {
                ID3D11Texture2D* tex = nullptr;
                res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
                if (tex) {
                    D3D11_TEXTURE2D_DESC td{}; tex->GetDesc(&td);
                    int want = 0; while ((int)(td.Width >> want) > c.maxW && want < 6) want++;
                    if (!staging || want != level || lw != (int)std::max(1u, td.Width >> want) || lh != (int)std::max(1u, td.Height >> want)) {
                        SafeRelease(srv); SafeRelease(mip); SafeRelease(staging);
                        level = want; lw = std::max(1u, td.Width >> want); lh = std::max(1u, td.Height >> want);
                        if (level > 0) {
                            D3D11_TEXTURE2D_DESC md = td;
                            md.MipLevels = level + 1; md.ArraySize = 1; md.SampleDesc = { 1, 0 };
                            md.Usage = D3D11_USAGE_DEFAULT; md.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
                            md.CPUAccessFlags = 0; md.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
                            if (SUCCEEDED(dev->CreateTexture2D(&md, nullptr, &mip))) dev->CreateShaderResourceView(mip, nullptr, &srv);
                        }
                        D3D11_TEXTURE2D_DESC sd = td;
                        sd.Width = lw; sd.Height = lh; sd.MipLevels = 1; sd.ArraySize = 1; sd.SampleDesc = { 1, 0 };
                        sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
                        dev->CreateTexture2D(&sd, nullptr, &staging);
                    }
                    if (staging && (level == 0 || srv)) {
                        if (level == 0) ctx->CopyResource(staging, tex);
                        else {
                            ctx->CopySubresourceRegion(mip, 0, 0, 0, 0, tex, 0, nullptr);
                            ctx->GenerateMips(srv);
                            ctx->CopySubresourceRegion(staging, 0, 0, 0, 0, mip, level, nullptr);
                        }
                        D3D11_MAPPED_SUBRESOURCE m{};
                        if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
                            // The duplicated image is in the panel's own orientation: turn it like the desktop.
                            bool swap = rot == DXGI_MODE_ROTATION_ROTATE90 || rot == DXGI_MODE_ROTATION_ROTATE270;
                            bw = swap ? lh : lw; bh = swap ? lw : lh;
                            base.resize((size_t)bw * bh);
                            auto src = [&](int x, int y) { return ((const uint32_t*)((const uint8_t*)m.pData + (size_t)y * m.RowPitch))[x]; };
                            for (int y = 0; y < bh; y++) {
                                uint32_t* row = &base[(size_t)y * bw];
                                for (int x = 0; x < bw; x++) {
                                    int sx = x, sy = y;
                                    if (rot == DXGI_MODE_ROTATION_ROTATE90) { sx = y; sy = lh - 1 - x; }
                                    else if (rot == DXGI_MODE_ROTATION_ROTATE180) { sx = lw - 1 - x; sy = lh - 1 - y; }
                                    else if (rot == DXGI_MODE_ROTATION_ROTATE270) { sx = lw - 1 - y; sy = x; }
                                    row[x] = src(sx, sy) | 0xFF000000;
                                }
                            }
                            ctx->Unmap(staging, 0);
                        }
                    }
                    tex->Release();
                }
            }
            SafeRelease(res);
            dup->ReleaseFrame();
            if (base.empty() || (!newImage && !(c.cursor && ptrChanged))) continue;

            std::vector<uint32_t> out = base;
            if (c.cursor && ptrVisible && deskW > 0 && deskH > 0)
                DrawPointer(out, bw, bh, ptrX * bw / deskW, ptrY * bh / deskH, std::max(3, bw / 220));
            if (c.opacity < 0.999f) { // premultiplied
                uint32_t a = (uint32_t)(c.opacity * 255.f + 0.5f);
                for (auto& p : out) {
                    uint32_t r = ((p >> 16) & 0xFF) * a / 255, g = ((p >> 8) & 0xFF) * a / 255, b = (p & 0xFF) * a / 255;
                    p = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
            { std::lock_guard l(mx); px.swap(out); w = bw; h = bh; fresh = true; }
            if (c.fps > 0) { nextAt += std::chrono::microseconds(1000000 / c.fps); auto n = std::chrono::steady_clock::now(); if (nextAt < n) nextAt = n; SleepUntil(nextAt, quit); }
        }
        SafeRelease(staging); SafeRelease(srv); SafeRelease(mip);
        SafeRelease(dup); SafeRelease(o1); SafeRelease(ctx); SafeRelease(dev);
        base.clear();
        if (!ok && !quit) SleepUntil(std::chrono::steady_clock::now() + std::chrono::seconds(1), quit);
    }
}
