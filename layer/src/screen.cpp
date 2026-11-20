#include "screen.h"
#include "common.h"
#include "wincap.h"
#include <dwmapi.h>
#include <psapi.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>


template <class T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

struct Cfg { int monitor = 0; uintptr_t hwnd = 0; int fps = 30, maxW = 1920; bool cursor = true; float opacity = 1.f; };

struct ScreenCapture::Impl {
    std::thread th;
    std::atomic<bool> quit{ false };
    std::mutex mx;
    Cfg cfg; uint64_t cfgVersion = 0;
    std::vector<uint32_t> px; int w = 0, h = 0; bool fresh = false;
    int count = 0, primary = 1, shown = 0; std::string msg; uint64_t msgVersion = 0;
    int rx = 0, ry = 0, rw = 0, rh = 0;
    bool windowGone = false;
    void Run();
    void RunWindow(const Cfg& c, uint64_t ver);
    void Publish(const std::vector<uint32_t>& base, int bw, int bh, int px, int py, const Cfg& c);
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

void ScreenCapture::Configure(int monitor, uintptr_t hwnd, int fps, int maxWidth, bool cursor, float opacity) {
    if (!d) d = new Impl;
    {
        std::lock_guard l(d->mx);
        Cfg n{ monitor, hwnd, fps, maxWidth, cursor, opacity };
        if (n.monitor != d->cfg.monitor || n.hwnd != d->cfg.hwnd || n.fps != d->cfg.fps || n.maxW != d->cfg.maxW || n.cursor != d->cfg.cursor || n.opacity != d->cfg.opacity) {
            d->cfg = n; d->cfgVersion++; d->windowGone = false;
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

void ScreenCapture::Rect(int& x, int& y, int& w, int& h) {
    x = y = w = h = 0;
    if (!d) return;
    std::lock_guard l(d->mx);
    x = d->rx; y = d->ry; w = d->rw; h = d->rh;
}

bool ScreenCapture::WindowGone() {
    if (!d) return false;
    std::lock_guard l(d->mx);
    return d->windowGone;
}

static std::string Narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

// Windows that can be shown: visible, not minimised, not hidden by Windows, with a title, not this game.
std::vector<WindowInfo> ScreenCapture::Windows() {
    std::vector<WindowInfo> v;
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        auto& v = *(std::vector<WindowInfo>*)lp;
        if (!IsWindowVisible(h) || IsIconic(h) || GetWindow(h, GW_OWNER)) return TRUE;
        if (GetWindowLongW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
        DWORD cloaked = 0; DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
        if (cloaked) return TRUE;
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        if (pid == GetCurrentProcessId()) return TRUE;
        wchar_t t[512]; int n = GetWindowTextW(h, t, 512);
        if (n <= 0) return TRUE;
        std::wstring title(t, n);
        if (title == L"Program Manager") return TRUE;
        std::wstring exe;
        if (HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
            wchar_t path[MAX_PATH]; DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(ph, 0, path, &len)) { exe = path; exe = exe.substr(exe.find_last_of(L"\\/") + 1); }
            CloseHandle(ph);
        }
        v.push_back({ (uintptr_t)h, Narrow(title), Narrow(exe) });
        return TRUE;
    }, (LPARAM)&v);
    auto low = [](std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; };
    std::stable_sort(v.begin(), v.end(), [&](const WindowInfo& a, const WindowInfo& b) {
        std::string ea = low(a.exe), eb = low(b.exe);
        return ea != eb ? ea < eb : low(a.title) < low(b.title);
    });
    return v;
}

bool ScreenCapture::Running() const { return d && d->th.joinable(); }

bool ScreenCapture::TakeFrame(std::vector<uint32_t>& out, int& w, int& h) {
    if (!d) return false;
    std::lock_guard l(d->mx);
    if (!d->fresh) return false;
    out.swap(d->px); w = d->w; h = d->h; d->fresh = false; // a swap, not an 8 MB copy on the game's render thread
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

// GPU: shrink a captured texture (mipmaps, halving until it fits maxW), read it back, turn it.
struct Reader {
    ID3D11Texture2D* mip = nullptr; ID3D11ShaderResourceView* srv = nullptr; ID3D11Texture2D* staging = nullptr;
    int level = 0, lw = 0, lh = 0, cw0 = 0, ch0 = 0;
    void Release() { SafeRelease(srv); SafeRelease(mip); SafeRelease(staging); level = lw = lh = cw0 = ch0 = 0; }
    bool Read(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* tex, int cw, int ch, int maxW, DXGI_MODE_ROTATION rot,
              std::vector<uint32_t>& base, int& bw, int& bh) {
        if (cw <= 0 || ch <= 0) return false;
        D3D11_TEXTURE2D_DESC td{}; tex->GetDesc(&td);
        cw = std::min<int>(cw, td.Width); ch = std::min<int>(ch, td.Height);
        int want = 0; while ((cw >> want) > maxW && want < 6) want++;
        if (!staging || want != level || cw != cw0 || ch != ch0) {
            Release();
            level = want; cw0 = cw; ch0 = ch; lw = std::max(1, cw >> want); lh = std::max(1, ch >> want);
            if (level > 0) {
                D3D11_TEXTURE2D_DESC md{}; md.Width = cw; md.Height = ch; md.Format = td.Format;
                md.MipLevels = level + 1; md.ArraySize = 1; md.SampleDesc = { 1, 0 };
                md.Usage = D3D11_USAGE_DEFAULT; md.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
                md.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
                if (SUCCEEDED(dev->CreateTexture2D(&md, nullptr, &mip))) dev->CreateShaderResourceView(mip, nullptr, &srv);
            }
            D3D11_TEXTURE2D_DESC sd{}; sd.Width = lw; sd.Height = lh; sd.Format = td.Format;
            sd.MipLevels = 1; sd.ArraySize = 1; sd.SampleDesc = { 1, 0 };
            sd.Usage = D3D11_USAGE_STAGING; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            dev->CreateTexture2D(&sd, nullptr, &staging);
        }
        if (!staging || (level > 0 && !srv)) return false;
        D3D11_BOX box{ 0, 0, 0, (UINT)cw, (UINT)ch, 1 };
        if (level == 0) ctx->CopySubresourceRegion(staging, 0, 0, 0, 0, tex, 0, &box);
        else {
            ctx->CopySubresourceRegion(mip, 0, 0, 0, 0, tex, 0, &box);
            ctx->GenerateMips(srv);
            ctx->CopySubresourceRegion(staging, 0, 0, 0, 0, mip, level, nullptr);
        }
        D3D11_MAPPED_SUBRESOURCE m{};
        if (FAILED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m))) return false;
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
        return true;
    }
};

// Pointer dot + opacity, then hand the picture to the layer. px/py < 0 = no pointer.
void ScreenCapture::Impl::Publish(const std::vector<uint32_t>& base, int bw, int bh, int ptx, int pty, const Cfg& c) {
    std::vector<uint32_t> out = base;
    if (c.cursor && ptx >= 0 && pty >= 0 && ptx < bw && pty < bh) DrawPointer(out, bw, bh, ptx, pty, std::max(3, bw / 220));
    if (c.opacity < 0.999f) { // premultiplied
        uint32_t a = (uint32_t)(c.opacity * 255.f + 0.5f);
        for (auto& p : out) {
            uint32_t r = ((p >> 16) & 0xFF) * a / 255, g = ((p >> 8) & 0xFF) * a / 255, b = (p & 0xFF) * a / 255;
            p = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    std::lock_guard l(mx); px.swap(out); w = bw; h = bh; fresh = true;
}

// One window, with Windows.Graphics.Capture.
void ScreenCapture::Impl::RunWindow(const Cfg& c, uint64_t ver) {
    HWND hwnd = (HWND)c.hwnd;
    wchar_t t[256]; int tn = GetWindowTextW(hwnd, t, 256);
    std::string title = Narrow(std::wstring(t, std::max(0, tn)));
    if (!IsWindow(hwnd)) { std::lock_guard l(mx); windowGone = true; return; }
    ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, fls, 3, D3D11_SDK_VERSION, &dev, nullptr, &ctx))) {
        SetMsg("window '" + title + "' can't be mirrored (no graphics device)");
        SleepUntil(std::chrono::steady_clock::now() + std::chrono::seconds(1), quit);
        return;
    }
    std::string err;
    WindowCapture* wc = WinCapOpen(hwnd, dev, err);
    if (!wc) {
        SetMsg("window '" + title + "' can't be mirrored: " + err);
        SafeRelease(ctx); SafeRelease(dev);
        SleepUntil(std::chrono::steady_clock::now() + std::chrono::seconds(1), quit);
        return;
    }
    SetMsg("window '" + title + "', " + (c.fps ? std::to_string(c.fps) + " fps" : std::string("every frame")));
    Reader rd; std::vector<uint32_t> base; int bw = 0, bh = 0;
    auto nextAt = std::chrono::steady_clock::now();
    POINT lastPt{ -99999, -99999 };
    while (!quit) {
        { std::lock_guard l(mx); if (cfgVersion != ver) break; }
        if (!IsWindow(hwnd)) { std::lock_guard l(mx); windowGone = true; break; }
        RECT r{};
        if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r))) GetWindowRect(hwnd, &r);
        { std::lock_guard l(mx); rx = r.left; ry = r.top; rw = r.right - r.left; rh = r.bottom - r.top; }
        int cw = 0, ch = 0; bool closed = false;
        ID3D11Texture2D* tex = WinCapNext(wc, cw, ch, closed);
        if (closed) { std::lock_guard l(mx); windowGone = true; break; }
        POINT pt{}; GetCursorPos(&pt);
        bool moved = pt.x != lastPt.x || pt.y != lastPt.y;
        if (tex) { rd.Read(dev, ctx, tex, cw, ch, c.maxW, DXGI_MODE_ROTATION_IDENTITY, base, bw, bh); tex->Release(); }
        if (base.empty() || (!tex && !(c.cursor && moved))) { std::this_thread::sleep_for(std::chrono::milliseconds(4)); continue; }
        lastPt = pt;
        int pw = r.right - r.left, ph = r.bottom - r.top;
        int ptx = -1, pty = -1;
        if (pw > 0 && ph > 0 && pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom) {
            ptx = (pt.x - r.left) * bw / pw; pty = (pt.y - r.top) * bh / ph;
        }
        Publish(base, bw, bh, ptx, pty, c);
        if (c.fps > 0) { nextAt += std::chrono::microseconds(1000000 / c.fps); auto n = std::chrono::steady_clock::now(); if (nextAt < n) nextAt = n; SleepUntil(nextAt, quit); }
    }
    WinCapClose(wc);
    rd.Release(); SafeRelease(ctx); SafeRelease(dev);
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
        if (c.hwnd) {
            RunWindow(c, ver);
            bool gone; { std::lock_guard l(mx); gone = windowGone; }
            if (gone) { SetMsg("the window was closed"); while (!quit) { { std::lock_guard l(mx); if (cfgVersion != ver) break; } std::this_thread::sleep_for(std::chrono::milliseconds(20)); } }
            continue;
        }
        auto outs = Enumerate();
        if (outs.empty()) { SetMsg("no monitor found"); Free(outs); SleepUntil(std::chrono::steady_clock::now() + std::chrono::seconds(2), quit); continue; }
        int prim = 1;
        for (size_t i = 0; i < outs.size(); i++) { const RECT& r = outs[i].desc.DesktopCoordinates; if (r.left == 0 && r.top == 0) prim = (int)i + 1; }
        int idx = c.monitor <= 0 ? prim : std::min<int>(c.monitor, (int)outs.size());
        OutInfo& o = outs[idx - 1];
        {
            std::lock_guard l(mx); count = (int)outs.size(); primary = prim; shown = idx;
            const RECT& r = o.desc.DesktopCoordinates; rx = r.left; ry = r.top; rw = r.right - r.left; rh = r.bottom - r.top;
        }
        const RECT dr = o.desc.DesktopCoordinates;
        const int deskW = dr.right - dr.left, deskH = dr.bottom - dr.top;
        const DXGI_MODE_ROTATION rot = o.desc.Rotation;

        ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
        IDXGIOutput1* o1 = nullptr; IDXGIOutputDuplication* dup = nullptr;
        Reader rd;
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
                    rd.Read(dev, ctx, tex, (int)td.Width, (int)td.Height, c.maxW, rot, base, bw, bh);
                    tex->Release();
                }
            }
            SafeRelease(res);
            dup->ReleaseFrame();
            if (base.empty() || (!newImage && !(c.cursor && ptrChanged))) continue;

            Publish(base, bw, bh, ptrVisible && deskW > 0 && deskH > 0 ? ptrX * bw / deskW : -1, ptrVisible && deskW > 0 && deskH > 0 ? ptrY * bh / deskH : -1, c);
            if (c.fps > 0) { nextAt += std::chrono::microseconds(1000000 / c.fps); auto n = std::chrono::steady_clock::now(); if (nextAt < n) nextAt = n; SleepUntil(nextAt, quit); }
        }
        rd.Release();
        SafeRelease(dup); SafeRelease(o1); SafeRelease(ctx); SafeRelease(dev);
        base.clear();
        if (!ok && !quit) SleepUntil(std::chrono::steady_clock::now() + std::chrono::seconds(1), quit);
    }
}
