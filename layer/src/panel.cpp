#include "panel.h"
#include <algorithm>
#include <cwctype>
#include <cmath>
#include <map>
#include <memory>
#include <objidl.h>
namespace Gdiplus {
using std::max;
using std::min;
} // namespace Gdiplus
#include <gdiplus.h>

using namespace Gdiplus;

// ---------- graphics context ----------
void Gfx::Init(const void* chain) {
    Release();
    auto* h = (const XrBaseInStructure*)chain;
    for (; h; h = h->next) {
        if (h->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
            auto* b = (const XrGraphicsBindingD3D11KHR*)h;
            d11 = b->device; // ComPtr takes its own reference
            d11->GetImmediateContext(&ctx);
            api = 11;
            return;
        }
        if (h->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
            auto* b = (const XrGraphicsBindingD3D12KHR*)h;
            d12 = b->device;
            queue = b->queue;
            for (int i = 0; i < kRing; i++) {
                d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc[i]));
                if (alloc[i]) d12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[i].Get(), nullptr, IID_PPV_ARGS(&list[i]));
                if (list[i]) list[i]->Close();
            }
            d12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
            ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            api = 12;
            return;
        }
    }
    api = 0;
}
void Gfx::Release() {
    if (fence) WaitFence(fenceVal); // nothing of ours still running on the GPU before the lists go
    if (ev) CloseHandle(ev);
    *this = Gfx{}; // drops every ComPtr
}

void Gfx::WaitFence(UINT64 v) {
    if (!fence || fence->GetCompletedValue() >= v) return;
    if (ev && SUCCEEDED(fence->SetEventOnCompletion(v, ev))) WaitForSingleObject(ev, 1000);
}

// ---------- swapchain panel ----------
bool Panel::Create(XrSession s, Gfx& g, const XrFns& f, int W, int H) {
    if (g.api == 0) return false;
    uint32_t n = 0;
    f.EnumerateSwapchainFormats(s, 0, &n, nullptr);
    std::vector<int64_t> fmts(n);
    f.EnumerateSwapchainFormats(s, n, &n, fmts.data());
    const int64_t pref[] = { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM,
                             DXGI_FORMAT_R8G8B8A8_UNORM };
    int64_t fmt = 0;
    for (auto p : pref)
        if (std::find(fmts.begin(), fmts.end(), p) != fmts.end()) {
            fmt = p;
            break;
        }
    if (!fmt) {
        Log("panel: no usable swapchain format");
        return false;
    }
    swapRB = fmt == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || fmt == DXGI_FORMAT_R8G8B8A8_UNORM;

    XrSwapchainCreateInfo ci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    ci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    ci.format = fmt;
    ci.sampleCount = 1;
    ci.width = W;
    ci.height = H;
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1;
    XrResult r = f.CreateSwapchain(s, &ci, &sc);
    if (XR_FAILED(r)) {
        Log("panel: xrCreateSwapchain failed %d", r);
        sc = XR_NULL_HANDLE;
        return false;
    }
    w = W;
    h = H;
    uint32_t cnt = 0;
    f.EnumerateSwapchainImages(sc, 0, &cnt, nullptr);
    imgs.clear();
    if (g.api == 11) {
        std::vector<XrSwapchainImageD3D11KHR> im(cnt, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
        f.EnumerateSwapchainImages(sc, cnt, &cnt, (XrSwapchainImageBaseHeader*)im.data());
        for (auto& i : im)
            imgs.push_back(i.texture);
    } else {
        std::vector<XrSwapchainImageD3D12KHR> im(cnt, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
        f.EnumerateSwapchainImages(sc, cnt, &cnt, (XrSwapchainImageBaseHeader*)im.data());
        for (auto& i : im)
            imgs.push_back(i.texture);
    }
    Log("panel: swapchain %dx%d fmt %lld, %u images, api %d", W, H, (long long)fmt, cnt, g.api);
    return true;
}

bool Panel::Upload(Gfx& g, const XrFns& f, const std::vector<uint32_t>& px) {
    if (sc == XR_NULL_HANDLE) return false;
    std::vector<uint32_t> tmp;
    const uint32_t* src = px.data();
    if (swapRB) {
        tmp.resize(px.size());
        for (size_t i = 0; i < px.size(); i++) {
            uint32_t c = px[i];
            tmp[i] = (c & 0xFF00FF00) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16);
        }
        src = tmp.data();
    }
    uint32_t idx = 0;
    XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    if (XR_FAILED(f.AcquireSwapchainImage(sc, &ai, &idx))) return false;
    XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    wi.timeout = XR_INFINITE_DURATION;
    f.WaitSwapchainImage(sc, &wi);

    if (g.api == 11) {
        g.ctx->UpdateSubresource((ID3D11Texture2D*)imgs[idx], 0, nullptr, src, w * 4, 0);
    } else if (g.api == 12 && g.fence) {
        auto* tex = (ID3D12Resource*)imgs[idx];
        D3D12_RESOURCE_DESC desc = tex->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
        UINT64 total = 0;
        g.d12->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, nullptr, &total);
        fence = g.fence.Get();
        int u = uploadNext;
        uploadNext = (uploadNext + 1) % Gfx::kRing;
        g.WaitFence(uploadFence[u]); // this staging buffer's last copy (three uploads ago) is done
        if (upload[u] && uploadSize[u] < total) upload[u].Reset(); // too small for this picture
        if (!upload[u]) {
            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_UPLOAD };
            D3D12_RESOURCE_DESC bd{};
            bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bd.Width = total;
            bd.Height = 1;
            bd.DepthOrArraySize = 1;
            bd.MipLevels = 1;
            bd.SampleDesc.Count = 1;
            bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (SUCCEEDED(g.d12->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                         IID_PPV_ARGS(&upload[u]))))
                uploadSize[u] = total;
        }
        int k = g.slot;
        g.slot = (g.slot + 1) % Gfx::kRing;
        g.WaitFence(g.slotFence[k]); // this command allocator's last use is done
        uint8_t* m = nullptr;
        D3D12_RANGE rr{ 0, 0 };
        if (upload[u] && g.alloc[k] && g.list[k] && SUCCEEDED(upload[u]->Map(0, &rr, (void**)&m)) && m) {
            for (int y = 0; y < h; y++)
                memcpy(m + fp.Offset + (size_t)y * fp.Footprint.RowPitch, src + (size_t)y * w, (size_t)w * 4);
            upload[u]->Unmap(0, nullptr);
            g.alloc[k]->Reset();
            g.list[k]->Reset(g.alloc[k].Get(), nullptr);
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource = tex;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            g.list[k]->ResourceBarrier(1, &b);
            D3D12_TEXTURE_COPY_LOCATION dst{ tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
            dst.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION sl{ upload[u].Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
            sl.PlacedFootprint = fp;
            g.list[k]->CopyTextureRegion(&dst, 0, 0, 0, &sl, nullptr);
            std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
            g.list[k]->ResourceBarrier(1, &b);
            g.list[k]->Close();
            ID3D12CommandList* lists[] = { g.list[k].Get() };
            g.queue->ExecuteCommandLists(1, lists);
            // No CPU wait: the copy is queued before the image is released, so the compositor sees it in order.
            g.queue->Signal(g.fence.Get(), ++g.fenceVal);
            g.slotFence[k] = g.fenceVal;
            uploadFence[u] = g.fenceVal;
        }
    }
    XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    f.ReleaseSwapchainImage(sc, &ri);
    ready = true;
    return true;
}

void Panel::Destroy(const XrFns& f) {
    if (sc != XR_NULL_HANDLE && f.DestroySwapchain) f.DestroySwapchain(sc);
    sc = XR_NULL_HANDLE;
    if (fence) { // the GPU may still be copying from our staging buffers
        UINT64 v = 0;
        for (auto f2 : uploadFence)
            v = std::max(v, f2);
        if (fence->GetCompletedValue() < v) fence->SetEventOnCompletion(v, nullptr); // nullptr event: blocks until done
    }
    for (int i = 0; i < Gfx::kRing; i++) {
        upload[i].Reset();
        uploadFence[i] = uploadSize[i] = 0;
    }
    uploadNext = 0;
    imgs.clear();
    ready = false;
}

// ---------- drawing ----------
static ULONG_PTR gGdiToken = 0;
static std::map<std::wstring, std::unique_ptr<Image>> gIcons;

static void EnsureGdi() {
    if (gGdiToken) return;
    GdiplusStartupInput in;
    GdiplusStartup(&gGdiToken, &in, nullptr);
}
void DrawShutdown() {
    gIcons.clear();
    if (gGdiToken) GdiplusShutdown(gGdiToken);
    gGdiToken = 0;
}

static Image* Icon(const std::string& path) {
    if (path.empty()) return nullptr;
    std::wstring p = Utf8ToW(path);
    if (p.find(L':') == std::wstring::npos) p = ProfilesDir() + L"\\icons\\" + p;
    auto it = gIcons.find(p);
    if (it != gIcons.end()) return it->second.get();
    std::unique_ptr<Image> img(Image::FromFile(p.c_str()));
    if (img && img->GetLastStatus() != Ok) img.reset();
    if (!img) Log("icon not found: %s", WToUtf8(p).c_str());
    Image* raw = img.get();
    gIcons[p] = std::move(img);
    return raw;
}

static void Grab(Bitmap& bmp, std::vector<uint32_t>& out, int w, int h) {
    out.resize((size_t)w * h);
    BitmapData bd;
    Rect r(0, 0, w, h);
    if (bmp.LockBits(&r, ImageLockModeRead, PixelFormat32bppPARGB, &bd) == Ok) {
        for (int y = 0; y < h; y++)
            memcpy(&out[(size_t)y * w], (uint8_t*)bd.Scan0 + (size_t)y * bd.Stride, (size_t)w * 4);
        bmp.UnlockBits(&bd);
    }
}

static std::unique_ptr<Font> MakeFont(const Style& st, float px, int style) {
    FontFamily fam(Utf8ToW(st.font).c_str());
    if (fam.GetLastStatus() != Ok) return std::make_unique<Font>(FontFamily::GenericSansSerif(), px, style, UnitPixel);
    return std::make_unique<Font>(&fam, px, style, UnitPixel);
}

void DrawWheel(std::vector<uint32_t>& out, int S, const Profile& p, const WheelView& v) {
    EnsureGdi();
    const Style& st = p.style;
    Bitmap bmp(S, S, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.Clear(Color(0, 0, 0, 0));

    float c = S / 2.f, R = S / 2.f - 4, r = R * st.innerRatio;
    SolidBrush bg{ Color(st.bg) };
    g.FillEllipse(&bg, c - R, c - R, 2 * R, 2 * R);

    const Wheel* w = v.wheel;
    int n = w ? (int)w->items.size() : 0;
    float seg = n ? 360.f / n : 360.f;
    auto font = MakeFont(st, S * 0.045f * st.fontScale, FontStyleBold);
    auto smallFont = MakeFont(st, S * 0.034f * st.fontScale, FontStyleRegular);
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textB{ Color(st.text) }, accentB{ Color(st.accent) };
    Pen border(Color(st.border), 2.f);

    for (int i = 0; i < n; i++) {
        const WheelItem& it = w->items[i];
        float mid = -90.f + i * seg;
        float a0 = mid - seg / 2 + st.gapDeg / 2, sw = seg - st.gapDeg;
        GraphicsPath path;
        float ro = R - 6, ri = r + 4;
        path.AddArc(c - ro, c - ro, 2 * ro, 2 * ro, a0, sw);
        path.AddArc(c - ri, c - ri, 2 * ri, 2 * ri, a0 + sw, -sw);
        path.CloseFigure();
        bool hov = i == v.hovered;
        bool isOn = i < (int)v.on.size() && v.on[i];
        uint32_t col = hov ? st.hover : isOn ? st.toggled : (it.hasColor ? it.color : st.segment);
        SolidBrush sb{ Color(col) };
        g.FillPath(&sb, &path);
        if (hov) g.DrawPath(&border, &path);

        float rad = (mid) * 3.14159265f / 180.f;
        float tr = (ro + ri) / 2;
        float tx = c + std::cos(rad) * tr, ty = c + std::sin(rad) * tr;
        float boxW = std::min((ro - ri) * 1.25f, 2 * tr * std::sin(std::min(seg, 179.f) * 3.14159265f / 360.f) * 0.9f);
        float boxH = (ro - ri) * 0.9f;
        Image* icon = Icon(it.icon);
        if (icon) {
            float is = boxH * 0.45f;
            g.DrawImage(icon, tx - is / 2, ty - boxH * 0.42f, is, is);
            RectF tb(tx - boxW / 2, ty + boxH * 0.05f, boxW, boxH * 0.4f);
            g.DrawString(Utf8ToW(it.label).c_str(), -1, smallFont.get(), tb, &fmt, &textB);
        } else {
            RectF tb(tx - boxW / 2, ty - boxH / 2, boxW, boxH);
            g.DrawString(Utf8ToW(it.label).c_str(), -1, font.get(), tb, &fmt, &textB);
        }
        // Small marker for submenus.
        if (it.action.type == ActType::Wheel) {
            float mr = ro - 14;
            g.FillEllipse(&accentB, c + std::cos(rad) * mr - 5, c + std::sin(rad) * mr - 5, 10.f, 10.f);
        }
    }

    // Centre: title or hovered description.
    SolidBrush centerB{ Color(0xE0101014) };
    g.FillEllipse(&centerB, c - r, c - r, 2 * r, 2 * r);
    g.DrawEllipse(&border, c - r, c - r, 2 * r, 2 * r);
    std::string top = v.crumb, bottom;
    if (w && v.hovered >= 0 && v.hovered < n) {
        top = w->items[v.hovered].label;
        bottom = w->items[v.hovered].description;
    }
    if (n == 0) top = "(empty wheel)";
    RectF t1(c - r * 0.9f, c - r * 0.55f, r * 1.8f, r * (bottom.empty() ? 1.1f : 0.6f));
    g.DrawString(Utf8ToW(top).c_str(), -1, font.get(), t1, &fmt, bottom.empty() ? &accentB : &textB);
    if (!bottom.empty()) {
        RectF t2(c - r * 0.85f, c + r * 0.05f, r * 1.7f, r * 0.6f);
        g.DrawString(Utf8ToW(bottom).c_str(), -1, smallFont.get(), t2, &fmt, &accentB);
    }
    Grab(bmp, out, S, S);
}

void DrawToast(std::vector<uint32_t>& out, int W, int H, const std::string& text, const Style& st) {
    EnsureGdi();
    Bitmap bmp(W, H, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    GraphicsPath path;
    float rr = H * 0.4f;
    path.AddArc(2.f, 2.f, rr, rr, 180, 90);
    path.AddArc(W - rr - 2.f, 2.f, rr, rr, 270, 90);
    path.AddArc(W - rr - 2.f, H - rr - 2.f, rr, rr, 0, 90);
    path.AddArc(2.f, H - rr - 2.f, rr, rr, 90, 90);
    path.CloseFigure();
    SolidBrush bg{ Color(st.bg | 0x40000000u) };
    g.FillPath(&bg, &path);
    Pen pen(Color(st.accent), 2.f);
    g.DrawPath(&pen, &path);
    auto font = MakeFont(st, H * 0.38f, FontStyleBold);
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    SolidBrush tb{ Color(st.text) };
    RectF rc(0, 0, (float)W, (float)H);
    g.DrawString(Utf8ToW(text).c_str(), -1, font.get(), rc, &fmt, &tb);
    Grab(bmp, out, W, H);
}

void DrawGuide(std::vector<uint32_t>& out, int W, int H, const std::string& text, const Style& st) {
    EnsureGdi();
    Bitmap bmp(W, H, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    GraphicsPath path;
    float rr = 40.f;
    path.AddArc(2.f, 2.f, rr, rr, 180, 90);
    path.AddArc(W - rr - 2.f, 2.f, rr, rr, 270, 90);
    path.AddArc(W - rr - 2.f, H - rr - 2.f, rr, rr, 0, 90);
    path.AddArc(2.f, H - rr - 2.f, rr, rr, 90, 90);
    path.CloseFigure();
    SolidBrush bg{ Color(235, 22, 26, 34) };
    g.FillPath(&bg, &path);
    Pen pen(Color(255, 240, 140, 30), 4.f);
    g.DrawPath(&pen, &path);
    size_t a = text.find('\n'), b = text.rfind('\n');
    std::string title = text.substr(0, a), body = a == std::string::npos ? "" : text.substr(a + 1, b > a ? b - a - 1 : std::string::npos),
                foot = b != std::string::npos && b > a ? text.substr(b + 1) : "";
    StringFormat left;
    left.SetAlignment(StringAlignmentNear);
    left.SetLineAlignment(StringAlignmentNear);
    auto fTitle = MakeFont(st, H * 0.10f, FontStyleBold), fBody = MakeFont(st, H * 0.082f, FontStyleRegular), fFoot = MakeFont(st, H * 0.085f, FontStyleBold);
    SolidBrush orange{ Color(255, 240, 140, 30) }, white{ Color(255, 236, 240, 246) }, blue{ Color(255, 127, 196, 255) };
    float pad = W * 0.04f;
    g.DrawString(Utf8ToW(title).c_str(), -1, fTitle.get(), RectF(pad, H * 0.06f, W - 2 * pad, H * 0.16f), &left, &orange);
    g.DrawString(Utf8ToW(body).c_str(), -1, fBody.get(), RectF(pad, H * 0.25f, W - 2 * pad, H * 0.55f), &left, &white);
    g.DrawString(Utf8ToW(foot).c_str(), -1, fFoot.get(), RectF(pad, H * 0.82f, W - 2 * pad, H * 0.16f), &left, &blue);
    Grab(bmp, out, W, H);
}

// Touch cursor: a ring that thickens and brightens with closeness (level 0..7), a filled dot when touching (8).
void DrawCursor(std::vector<uint32_t>& out, int S, int level, const Style& st) {
    EnsureGdi();
    Bitmap bmp(S, S, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));
    float k = level / 8.f;
    float pad = S * 0.08f, d = S - 2 * pad;
    if (level >= 8) {
        SolidBrush fill{ Color(st.accent) };
        g.FillEllipse(&fill, pad, pad, d, d);
        Pen edge(Color(0xFFFFFFFF), S * 0.06f);
        g.DrawEllipse(&edge, pad, pad, d, d);
    } else {
        BYTE a = (BYTE)(90 + 150 * k);
        uint32_t c = st.accent;
        Pen ring(Color(a, (BYTE)(c >> 16), (BYTE)(c >> 8), (BYTE)c), S * (0.05f + 0.07f * k));
        g.DrawEllipse(&ring, pad, pad, d, d);
        SolidBrush dot{ Color((BYTE)(60 + 120 * k), 255, 255, 255) };
        float r = S * 0.06f;
        g.FillEllipse(&dot, S / 2.f - r, S / 2.f - r, 2 * r, 2 * r);
    }
    Grab(bmp, out, S, S);
}

// Laser pointer: a white dot with an accent ring, and a beam that fades toward the controller.
void DrawPointerDot(std::vector<uint32_t>& out, int S, const Style& st) {
    EnsureGdi();
    Bitmap bmp(S, S, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));
    float pad = S * 0.1f, d = S - 2 * pad;
    Pen ring(Color(st.accent), S * 0.12f);
    g.DrawEllipse(&ring, pad, pad, d, d);
    SolidBrush dot{ Color(255, 255, 255, 255) };
    float r = S * 0.22f;
    g.FillEllipse(&dot, S / 2.f - r, S / 2.f - r, 2 * r, 2 * r);
    Grab(bmp, out, S, S);
}
// Fingertip silhouette: a see-through index finger, tip at the texture top, fading toward the hand.
void DrawFinger(std::vector<uint32_t>& out, int W, int H, const Style& st) {
    EnsureGdi();
    Bitmap bmp(W, H, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));
    uint32_t c = st.accent;
    float m = W * 0.1f, w = W - 2 * m, top = m * 0.5f, bot = (float)H;
    GraphicsPath path; // rounded tip, sides widening a little toward the knuckle
    path.AddArc(m, top, w, w, 180.f, 180.f);
    path.AddLine(m + w, top + w / 2, W - m * 0.4f, bot);
    path.AddLine(m * 0.4f, bot, m, top + w / 2);
    path.CloseFigure();
    LinearGradientBrush fill(PointF(0, top), PointF(0, bot), Color(150, 255, 255, 255), Color(0, 255, 255, 255));
    g.FillPath(&fill, &path);
    LinearGradientBrush edgeB(PointF(0, top), PointF(0, bot), Color(230, (BYTE)(c >> 16), (BYTE)(c >> 8), (BYTE)c),
                              Color(80, (BYTE)(c >> 16), (BYTE)(c >> 8), (BYTE)c));
    Pen edge(&edgeB, W * 0.09f);
    g.DrawPath(&edge, &path);
    Pen nail(Color(110, 255, 255, 255), W * 0.04f); // fingernail outline
    g.DrawArc(&nail, m + w * 0.22f, top + w * 0.18f, w * 0.56f, w * 0.7f, 180.f, 180.f);
    g.DrawLine(&nail, m + w * 0.22f, top + w * 0.53f, m + w * 0.22f, top + w * 1.1f);
    g.DrawLine(&nail, m + w * 0.78f, top + w * 0.53f, m + w * 0.78f, top + w * 1.1f);
    Grab(bmp, out, W, H);
}

void DrawBeam(std::vector<uint32_t>& out, int W, int H, const Style& st) {
    out.assign((size_t)W * H, 0);
    uint32_t c = st.accent;
    for (int y = 0; y < H; y++) {
        // texture top = far end (hit), bottom = controller: fade out toward the controller
        float along = 1.f - (float)y / (H - 1);
        float a = 0.15f + 0.55f * (1.f - along);
        for (int x = 0; x < W; x++) {
            float across = 1.f - std::fabs((x + 0.5f) / W * 2.f - 1.f); // soft edges
            float A = a * std::min(1.f, across * 1.6f);
            uint32_t ai = (uint32_t)(A * 255.f);
            uint32_t r = ((c >> 16) & 255) * ai / 255, g = ((c >> 8) & 255) * ai / 255, b = (c & 255) * ai / 255;
            out[(size_t)y * W + x] = (ai << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void DrawKeyboard(std::vector<uint32_t>& out, int W, int Hp, const KeyboardView& kb, const Style& st) {
    EnsureGdi();
    Bitmap bmp(W, Hp, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    float uw, uh;
    const auto& keys = KbLayout(uw, uh, kb.binding);
    float pad = W * 0.006f;
    float sx = (W - 2 * pad) / uw, sy = (Hp - 2 * pad) / uh;

    auto roundRect = [](GraphicsPath& p, float x, float y, float w, float h, float r) {
        p.AddArc(x, y, r, r, 180, 90);
        p.AddArc(x + w - r, y, r, r, 270, 90);
        p.AddArc(x + w - r, y + h - r, r, r, 0, 90);
        p.AddArc(x, y + h - r, r, r, 90, 90);
        p.CloseFigure();
    };
    GraphicsPath bgp;
    roundRect(bgp, 0, 0, (float)W - 1, (float)Hp - 1, sy * 0.5f);
    SolidBrush bg{ Color(st.bg) };
    g.FillPath(&bg, &bgp);

    auto font = MakeFont(st, sy * 0.36f * st.fontScale, FontStyleBold);
    auto smallFont = MakeFont(st, sy * 0.26f * st.fontScale, FontStyleBold);
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textB{ Color(st.text) }, accentB{ Color(st.accent) };
    Pen border(Color(st.border), 2.f);
    bool showUpper = kb.shift != kb.caps;

    for (int i = 0; i < (int)keys.size(); i++) {
        const KbKey& k = keys[i];
        float x = pad + k.x * sx + sx * 0.05f, y = pad + k.y * sy + sy * 0.06f;
        float w = k.w * sx - sx * 0.10f, h = k.h * sy - sy * 0.12f;
        bool hov = kb.hover[0] == i || kb.hover[1] == i;
        bool latched = (k.kind == KB_SHIFT && kb.shift) || (k.kind == KB_CAPS && kb.caps) || (k.kind == KB_CTRL && kb.ctrl) ||
                       (k.kind == KB_ALT && kb.alt) || (k.kind == KB_MOVE && kb.moving);
        uint32_t col = st.segment;
        if (latched) col = st.accent & 0xA0FFFFFF;
        if (hov) col = st.hover;
        if (kb.pressed == i) col = 0xFFFFFFFF;
        GraphicsPath kp;
        roundRect(kp, x, y, w, h, sy * 0.22f);
        SolidBrush kbB{ Color(col) };
        g.FillPath(&kbB, &kp);
        if (hov) g.DrawPath(&border, &kp);
        std::string label = k.label;
        if (k.letter)
            label = showUpper ? k.shiftLabel : k.label;
        else if (k.kind == KB_KEY && kb.shift)
            label = k.shiftLabel;
        else if (k.kind == KB_MOVE && kb.moving)
            label = "Done";
        SolidBrush darkB{ Color(0xFF101418) };
        RectF rc(x, y, w, h);
        bool big = label.size() <= 2 || (unsigned char)label[0] >= 0x80;
        g.DrawString(Utf8ToW(label).c_str(), -1, big ? font.get() : smallFont.get(), rc, &fmt,
                     kb.pressed == i ? &darkB : (latched ? &darkB : &textB));
    }
    Grab(bmp, out, W, Hp);
}

void DrawList(std::vector<uint32_t>& out, int W, int H, const std::string& title, const std::vector<std::string>& labels, int cols,
              int hover, int pressed, const Style& st) {
    EnsureGdi();
    Bitmap bmp(W, H, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    std::vector<Cell> cells;
    Cell hd;
    ListLayout((int)labels.size(), cols, (float)H / W, !title.empty(), cells, hd);
    auto roundRect = [](GraphicsPath& gp, float x, float y, float w, float h, float r) {
        r = std::max(1.f, std::min(r, std::min(w, h)));
        gp.AddArc(x, y, r, r, 180, 90);
        gp.AddArc(x + w - r, y, r, r, 270, 90);
        gp.AddArc(x + w - r, y + h - r, r, r, 0, 90);
        gp.AddArc(x, y + h - r, r, r, 90, 90);
        gp.CloseFigure();
    };
    GraphicsPath bgp;
    roundRect(bgp, 1, 1, W - 2.f, H - 2.f, std::min(W, H) * 0.12f);
    SolidBrush bg{ Color(st.bg) };
    g.FillPath(&bg, &bgp);
    Pen edge(Color(st.border), 2.f);
    g.DrawPath(&edge, &bgp);
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush textB{ Color(st.text) }, accentB{ Color(st.accent) }, darkB{ Color(0xFF101418) };
    if (!title.empty()) {
        auto hf = MakeFont(st, hd.h * H * 0.62f * st.fontScale, FontStyleBold);
        RectF hr(hd.x * W, hd.y * H, hd.w * W, hd.h * H);
        g.DrawString(Utf8ToW(title).c_str(), -1, hf.get(), hr, &fmt, &accentB);
    }
    float cellH = cells.empty() ? 1.f : cells[0].h * H;
    auto font = MakeFont(st, std::max(9.f, cellH * 0.26f) * st.fontScale, FontStyleBold);
    auto subFont = MakeFont(st, std::max(8.f, cellH * 0.2f) * st.fontScale, FontStyleRegular);
    Pen hovPen(Color(st.border | 0xFF000000u), 3.f);
    for (int i = 0; i < (int)cells.size(); i++) {
        const Cell& c = cells[i];
        float x = c.x * W, y = c.y * H, w = c.w * W, h = c.h * H;
        uint32_t col = hover == i ? st.hover : st.segment;
        if (pressed == i) col = 0xFFFFFFFF;
        GraphicsPath cp;
        roundRect(cp, x, y, w, h, h * 0.25f);
        SolidBrush cb{ Color(col) };
        g.FillPath(&cb, &cp);
        if (hover == i) g.DrawPath(&hovPen, &cp);
        const std::string& lab = labels[i];
        size_t nl = lab.find('\n');
        Brush* tb = pressed == i ? (Brush*)&darkB : (Brush*)&textB;
        if (nl == std::string::npos) {
            RectF rc(x + 4, y + 2, w - 8, h - 4);
            g.DrawString(Utf8ToW(lab).c_str(), -1, font.get(), rc, &fmt, tb);
        } else {
            RectF r1(x + 4, y + 2, w - 8, h * 0.5f - 2), r2(x + 4, y + h * 0.5f, w - 8, h * 0.5f - 2);
            g.DrawString(Utf8ToW(lab.substr(0, nl)).c_str(), -1, font.get(), r1, &fmt, tb);
            g.DrawString(Utf8ToW(lab.substr(nl + 1)).c_str(), -1, subFont.get(), r2, &fmt, tb);
        }
    }
    Grab(bmp, out, W, H);
}

void DrawWrist(std::vector<uint32_t>& out, int S, const Profile& p, const WristStyle& ws, const WristView& v) {
    EnsureGdi();
    Bitmap bmp(S, S, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    if (!v.wheel) {
        Grab(bmp, out, S, S);
        return;
    }
    const Style& st = p.style;
    int n = (int)v.wheel->items.size() + (v.nested ? 1 : 0);
    std::vector<Cell> cells;
    Cell hd;
    WristLayout(n, ws.columns, cells, hd);
    auto roundRect = [](GraphicsPath& gp, float x, float y, float w, float h, float r) {
        gp.AddArc(x, y, r, r, 180, 90);
        gp.AddArc(x + w - r, y, r, r, 270, 90);
        gp.AddArc(x + w - r, y + h - r, r, r, 0, 90);
        gp.AddArc(x, y + h - r, r, r, 90, 90);
        gp.CloseFigure();
    };
    float bottom = hd.y + hd.h;
    for (auto& c : cells)
        bottom = std::max(bottom, c.y + c.h);
    GraphicsPath bgp;
    roundRect(bgp, 1, 1, S - 2.f, (bottom + 0.03f) * S - 2.f, S * 0.06f);
    SolidBrush bg{ Color(st.bg) };
    g.FillPath(&bg, &bgp);
    Pen edge(Color(st.border), 2.f);
    g.DrawPath(&edge, &bgp);

    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textB{ Color(st.text) }, accentB{ Color(st.accent) }, darkB{ Color(0xFF101418) };
    auto headFont = MakeFont(st, S * 0.055f * st.fontScale, FontStyleBold);
    RectF hr(hd.x * S, hd.y * S, hd.w * S, hd.h * S);
    g.DrawString(Utf8ToW(v.adjust ? std::string("Adjust: grip to move") : v.wheel->title).c_str(), -1, headFont.get(), hr, &fmt, &accentB);

    float cellPx = cells.empty() ? 0 : cells[0].h * S;
    auto font = MakeFont(st, std::max(10.f, cellPx * 0.17f) * st.fontScale, FontStyleBold);
    Pen hovPen(Color(st.border | 0xFF000000u), 3.f);
    for (int i = 0; i < (int)cells.size(); i++) {
        const Cell& c = cells[i];
        float x = c.x * S, y = c.y * S, w = c.w * S, h = c.h * S;
        bool back = v.nested && i == 0;
        const WheelItem* it = back ? nullptr : &v.wheel->items[i - (v.nested ? 1 : 0)];
        uint32_t col = (it && it->hasColor) ? it->color : st.segment;
        bool hov = v.hover == i;
        bool isOn = i < (int)v.on.size() && v.on[i];
        if (isOn) col = st.toggled;
        if (hov) col = st.hover;
        if (v.pressed == i) col = 0xFFFFFFFF;
        GraphicsPath cp;
        roundRect(cp, x, y, w, h, h * 0.22f);
        SolidBrush cb{ Color(col) };
        g.FillPath(&cb, &cp);
        if (hov) g.DrawPath(&hovPen, &cp);
        std::string label = back ? "\xE2\x80\xB9 Back" : it->label;
        Image* icon = it ? Icon(it->icon) : nullptr;
        RectF rc(x + 2, y + 2, w - 4, h - 4);
        if (icon) {
            float is = h * 0.48f;
            g.DrawImage(icon, x + w / 2 - is / 2, y + h * 0.08f, is, is);
            rc = RectF(x + 2, y + h * 0.58f, w - 4, h * 0.40f);
        }
        g.DrawString(Utf8ToW(label).c_str(), -1, font.get(), rc, &fmt, v.pressed == i ? &darkB : &textB);
    }
    Grab(bmp, out, S, S);
}

// ---------- in-game settings menu ----------
// A flat dashboard look (dark panels, one blue accent, list rows with thin separators): a header bar with the title,
// back and close; rows from MenuLayout (label and detail on the left; on the right a switch, a value with a slider
// and round - / +, or a chevron); a footer with the status and scroll arrows.
namespace MenuColors {
constexpr uint32_t PanelTop = 0xF5222830, PanelBottom = 0xF5161A20, Header = 0xFF12151A, Footer = 0xFF12151A;
constexpr uint32_t Separator = 0xFF2C323A, Hover = 0xFF2B323C, Pressed = 0xFF1A9FFF, Accent = 0xFF1A9FFF;
constexpr uint32_t Text = 0xFFEDEFF2, SubText = 0xFF9BA3AD, Heading = 0xFF7D8590, Disabled = 0xFF4A515A;
constexpr uint32_t Track = 0xFF3A414B, Knob = 0xFFFFFFFF, IconButton = 0xFF2A3038;
} // namespace MenuColors

// A path for one shape of a controller drawing, placed in `box` (design units -> pixels), mirrored for the left hand.
static void ArtPath(GraphicsPath& p, ArtShape shape, float x, float y, float w, float h, float turnDeg, float ringWidth, int hand,
                    const RectF& box) {
    float s = std::min(box.Width / kArtWidth, box.Height / kArtHeight);
    float ox = box.X + (box.Width - kArtWidth * s) / 2, oy = box.Y + (box.Height - kArtHeight * s) / 2;
    if (hand == 0) { // mirrored
        x = kArtWidth - x - w;
        turnDeg = -turnDeg;
    }
    RectF r(ox + x * s, oy + y * s, w * s, h * s);
    auto rounded = [&](float radius) {
        float d = std::min(radius * 2, std::min(r.Width, r.Height));
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
        p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
        p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
        p.CloseFigure();
    };
    switch (shape) {
    case ArtShape::Ellipse:
    case ArtShape::Circle:
        p.AddEllipse(r);
        break;
    case ArtShape::RoundRect:
        rounded(std::min(r.Width, r.Height) / 2.2f);
        break;
    case ArtShape::Pill:
        rounded(std::min(r.Width, r.Height) / 2);
        break;
    case ArtShape::Ring: {
        p.AddEllipse(r);
        float rw = ringWidth * s;
        p.AddEllipse(RectF(r.X + rw, r.Y + rw, r.Width - 2 * rw, r.Height - 2 * rw)); // alternate fill: the hole
        break;
    }
    }
    if (turnDeg != 0) {
        Matrix m;
        m.RotateAt(turnDeg, PointF(r.X + r.Width / 2, r.Y + r.Height / 2));
        p.Transform(&m);
    }
}

// A horizontal bar with its label, filled to `value` (0..1).
static void LiveBar(Graphics& g, Font* font, const wchar_t* label, float value, float x, float y, float w, float h) {
    using namespace MenuColors;
    SolidBrush text{ Color(SubText) }, track{ Color(Track) }, fill{ Color(Accent) };
    StringFormat left;
    left.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(label, -1, font, RectF(x, y, w, h * 0.55f), &left, &text);
    float by = y + h * 0.6f, bh = std::max(4.f, h * 0.16f);
    g.FillRectangle(&track, x, by, w, bh);
    g.FillRectangle(&fill, x, by, w * std::clamp(value, 0.f, 1.f), bh);
}

static void DrawLiveBars(Graphics& g, const RectF& box, int hand, const LiveHand& live, Font* font);

// One controller from above, what is pressed now lit, and its trigger / grip bars beside it.
// A traced outline's loops as one path (alternate fill: inner loops are holes), placed like ArtPath.
static void SilhouettePath(GraphicsPath& p, const SilLoop* loops, int count, const RectF& box, float designW = kArtWidth) {
    float s = std::min(box.Width / designW, box.Height / kArtHeight);
    float ox = box.X + (box.Width - designW * s) / 2, oy = box.Y + (box.Height - kArtHeight * s) / 2;
    for (int i = 0; i < count; i++) {
        std::vector<PointF> pts(loops[i].points);
        for (int k = 0; k < loops[i].points; k++)
            pts[k] = PointF(ox + loops[i].xy[2 * k] * s, oy + loops[i].xy[2 * k + 1] * s);
        if (pts.size() >= 3) p.AddPolygon(pts.data(), (INT)pts.size());
    }
}

// The letter on a face button (A/B, X/Y on the left; the Index has A/B on both, the Steam Frame's right hand has
// all four), the menu's three lines, or the D-pad's arrows.
static const wchar_t* PartLetter(int comp, int hand, ControllerKind kind) {
    bool xy = hand == 0 && kind != ControllerKind::Index;
    switch (comp) {
    case C_BTN1:
        return xy ? L"X" : L"A";
    case C_BTN2:
        return xy ? L"Y" : L"B";
    case C_BTN3:
        return L"X";
    case C_BTN4:
        return L"Y";
    case C_MENU:
        return L"\u2261";
    case C_DPAD_UP:
        return L"\u25B4";
    case C_DPAD_DOWN:
        return L"\u25BE";
    case C_DPAD_LEFT:
        return L"\u25C2";
    case C_DPAD_RIGHT:
        return L"\u25B8";
    default:
        return nullptr;
    }
}

// One controller picture: from above, or from the side (false when there is no side view for it). Pressed parts
// light up, touched ones get a ring, the stick shows where it is pushed; `highlight` (a part) gets a ring too.
static bool DrawControllerPicture(Graphics& g, const RectF& pic, int hand, const LiveHand& live, bool side, int highlight = -1) {
    using namespace MenuColors;
    SolidBrush body{ Color(0xFF3A414B) }, part{ Color(0xFF1C2027) }, lit{ Color(Accent) }, touched{ Color(0x661A9FFF) };
    SolidBrush plain{ Color(0xFF2E343C) }, white{ Color(0xFFFFFFFF) };
    Pen edge(Color(0xFF59616C), 1.5f), ring(Color(Accent), 3.f);
    FontFamily letterFamily(L"Segoe UI");
    StringFormat centered;
    centered.SetAlignment(StringAlignmentCenter);
    centered.SetLineAlignment(StringAlignmentCenter);
    auto drawPart = [&](GraphicsPath& p, int comp) {
        bool readable = comp >= 0;
        bool pressed = readable && live.press[comp], touchedOnly = readable && !pressed && live.touch[comp];
        g.FillPath(!readable ? &plain : pressed ? &lit : touchedOnly ? &touched : &part, &p);
        g.DrawPath(touchedOnly || (readable && comp == highlight) ? &ring : &edge, &p);
        if (comp == C_STICK && !side) {
            RectF b;
            p.GetBounds(&b);
            float cx = b.X + b.Width / 2 + live.stickX * b.Width * 0.32f, cy = b.Y + b.Height / 2 - live.stickY * b.Height * 0.32f;
            float d = b.Width * 0.3f;
            g.FillEllipse(&white, cx - d / 2, cy - d / 2, d, d);
        }
        if (const wchar_t* letter = readable && !side ? PartLetter(comp, hand, live.kind) : nullptr) {
            RectF b;
            p.GetBounds(&b);
            float size = (b.Width < b.Height ? b.Width : b.Height) * 0.6f;
            if (size >= 6.f) {
                Font f(&letterFamily, size, FontStyleBold, UnitPixel);
                g.DrawString(letter, -1, &f, b, &centered, &white);
            }
        }
    };
    if (const Silhouette* sil = SilhouetteFor(live.kind, hand)) { // the real outline, when there is a model
        if (side && !sil->sideBody) return false;
        float designW = side ? kSilSideWidth : kArtWidth;
        GraphicsPath outline(FillModeAlternate);
        SilhouettePath(outline, side ? sil->sideBody : sil->body, side ? sil->sideCount : sil->bodyCount, pic, designW);
        g.FillPath(&body, &outline);
        g.DrawPath(&edge, &outline);
        for (int i = 0; i < sil->partCount; i++) {
            if (sil->parts[i].side != side) continue;
            GraphicsPath p(FillModeAlternate);
            SilhouettePath(p, sil->parts[i].loops, sil->parts[i].loopCount, pic, designW);
            drawPart(p, SilhouetteComp(sil->parts[i].comp));
        }
        return true;
    }
    if (side) return false; // the simple drawings are from above only
    const ControllerArt& art = ArtFor(live.kind);
    for (const ArtPiece& b : art.body) {
        GraphicsPath p(FillModeAlternate);
        ArtPath(p, b.shape, b.x, b.y, b.w, b.h, b.turnDeg, b.ringWidth, hand, pic);
        g.FillPath(&body, &p);
        g.DrawPath(&edge, &p);
    }
    for (const ArtPart& a : art.parts) {
        if (!(a.hands & (hand == 0 ? 1 : 2))) continue;
        GraphicsPath p;
        ArtPath(p, a.shape, a.x, a.y, a.w, a.h, 0, 0, hand, pic);
        drawPart(p, a.comp);
    }
    return true;
}

static void DrawLiveController(Graphics& g, const RectF& box, int hand, const LiveHand& live, Font* font) {
    DrawControllerPicture(g, RectF(box.X, box.Y, box.Width * 0.46f, box.Height), hand, live, false);
    DrawLiveBars(g, box, hand, live, font);
}

// The overview: both controllers from above and the side, and a tag on each part with what it does.
static void DrawOverview(Graphics& g, int W, int H, const MenuView& v, const MenuViewRow& r, int rowIndex, float y, float h,
                         Font* titleFont, Font* lineFont, Font* smallFont, const std::function<bool(const MenuHit&)>& isHover) {
    using namespace MenuColors;
    OverviewPlaces pl;
    OverviewLayout(v, r, y, h, pl);
    auto rect = [&](const MenuBox& b) { return RectF(b.x * W, b.y * H, b.w * W, b.h * H); };
    SolidBrush captionB{ Color(Heading) }, textB{ Color(Text) }, subB{ Color(SubText) }, accentB{ Color(Accent) };
    SolidBrush cardB{ Color(0xFF2A3038) }, hoverB{ Color(0xFF343C47) };
    Pen lineP(Color(0xFF5D6673), 1.5f), cardEdge(Color(0xFF3A424D), 1.5f), hoverEdge(Color(Accent), 2.5f);
    StringFormat left;
    left.SetLineAlignment(StringAlignmentCenter);
    left.SetTrimming(StringTrimmingEllipsisCharacter);
    left.SetFormatFlags(StringFormatFlagsNoWrap);
    for (int hand = 0; hand < 2; hand++) {
        std::wstring cap = std::wstring(hand ? L"RIGHT  \u00B7  " : L"LEFT  \u00B7  ") + Utf8ToW(ControllerKindName(v.live[hand].kind));
        for (wchar_t& c : cap)
            c = (wchar_t)towupper(c);
        g.DrawString(cap.c_str(), -1, lineFont, rect(pl.caption[hand]), &left, &captionB);
        DrawControllerPicture(g, rect(pl.top[hand]), hand, v.live[hand], false);
        DrawControllerPicture(g, rect(pl.side[hand]), hand, v.live[hand], true);
    }
    for (const OverviewTagPlace& t : pl.tags) {
        const MenuTag& tag = r.tags[t.tag];
        RectF b = rect(t.box);
        bool hover = isHover({ MenuPart::Row, rowIndex, t.tag });
        if (t.ax >= 0) { // a line from the tag's edge to its part, with a dot on the part
            float px = t.ax * W, py = t.ay * H;
            float ex = px < b.X ? b.X : b.X + b.Width, ey = std::clamp(py, b.Y + 8, b.Y + b.Height - 8);
            g.DrawLine(hover ? &hoverEdge : &lineP, ex, ey, px, py);
            g.FillEllipse(hover ? &accentB : &subB, px - 3.5f, py - 3.5f, 7.f, 7.f);
        }
        GraphicsPath card;
        float rr = std::min(b.Height, 16.f);
        card.AddArc(b.X, b.Y, rr, rr, 180, 90);
        card.AddArc(b.X + b.Width - rr, b.Y, rr, rr, 270, 90);
        card.AddArc(b.X + b.Width - rr, b.Y + b.Height - rr, rr, rr, 0, 90);
        card.AddArc(b.X, b.Y + b.Height - rr, rr, rr, 90, 90);
        card.CloseFigure();
        g.FillPath(hover ? &hoverB : &cardB, &card);
        g.DrawPath(hover ? &hoverEdge : &cardEdge, &card);
        float lh = 0.03f * H, x = b.X + 10, w = b.Width - 16, ly = b.Y + 5;
        g.DrawString(Utf8ToW(tag.title).c_str(), -1, titleFont, RectF(x, ly, w, lh * 1.25f), &left, &textB);
        ly += lh * 1.25f;
        for (size_t i = 0; i < tag.lines.size() && i < 5; i++) {
            int kind = i < tag.lineKinds.size() ? tag.lineKinds[i] : 0;
            g.DrawString(Utf8ToW(tag.lines[i]).c_str(), -1, kind == 3 ? smallFont : lineFont, RectF(x, ly, w, lh), &left,
                         kind == 1 ? (Brush*)&accentB : kind >= 2 ? (Brush*)&subB : (Brush*)&textB);
            ly += lh;
        }
    }
}

// Beside the picture: the controller's name, the trigger and grip as bars, the stick's numbers.
static void DrawLiveBars(Graphics& g, const RectF& box, int hand, const LiveHand& live, Font* font) {
    using namespace MenuColors;
    float bx = box.X + box.Width * 0.52f, bw = box.Width * 0.44f, rowH = box.Height / 4;
    SolidBrush name{ Color(Text) };
    StringFormat left;
    left.SetLineAlignment(StringAlignmentCenter);
    std::wstring title = std::wstring(hand == 0 ? L"Left " : L"Right ") + Utf8ToW(ControllerKindName(live.kind));
    g.DrawString(title.c_str(), -1, font, RectF(bx, box.Y + rowH * 0.1f, bw, rowH * 0.7f), &left, &name);
    LiveBar(g, font, L"Trigger", live.trigger, bx, box.Y + rowH * 1.0f, bw, rowH * 0.9f);
    LiveBar(g, font, L"Grip", live.grip, bx, box.Y + rowH * 2.0f, bw, rowH * 0.9f);
    wchar_t stick[64];
    swprintf(stick, 64, L"Stick  %+.2f, %+.2f", live.stickX, live.stickY);
    SolidBrush sub{ Color(SubText) };
    g.DrawString(stick, -1, font, RectF(bx, box.Y + rowH * 3.0f, bw, rowH * 0.8f), &left, &sub);
}

// One stick to scale: the direction zones, the deadzones, where the stick is (white) and what the game sees (blue).
static void DrawLiveStick(Graphics& g, const RectF& box, int hand, const LiveHand& live, float stickOn, Font* font) {
    using namespace MenuColors;
    float d = std::min(box.Height * 0.94f, box.Width * 0.46f), r = d / 2;
    PointF c(box.X + box.Width * 0.05f + r, box.Y + box.Height / 2);
    RectF circle(c.X - r, c.Y - r, d, d);
    SolidBrush base{ Color(0xFF2A3038) }, zone{ Color(0x551A9FFF) }, dead{ Color(0xFF59616C) }, full{ Color(0x88F08C1E) };
    g.FillEllipse(&base, circle);
    const StickShape& s = live.shape;
    float half = s.directionAngleDeg > 0 ? s.directionAngleDeg : 45.f, from = r * stickOn;
    if (s.ignoreOutside && s.directionAngleDeg > 0 && half < 45) { // between the zones the game gets nothing: grey
        SolidBrush ignored{ Color(0xFF4A525C) }; // solid: no darker overlaps
        for (float axis : { -45.f, 45.f, 135.f, 225.f })
            g.FillPie(&ignored, circle, axis - (45 - half), 2 * (45 - half));
    }
    if (s.ignoreOutside && s.directionAngleDeg > 0 && s.ignoreCenter > 0) { // the centre: small pushes give the game nothing
        SolidBrush ignored{ Color(0xFF4A525C) }; // solid: no darker overlaps
        float ir = r * s.ignoreCenter;
        g.FillEllipse(&ignored, c.X - ir, c.Y - ir, 2 * ir, 2 * ir);
    }
    for (float axis : { -90.f, 0.f, 90.f, 180.f }) { // where up / right / down / left count for bindings
        GraphicsPath wedge;
        wedge.AddArc(circle, axis - half, 2 * half);
        wedge.AddArc(RectF(c.X - from, c.Y - from, 2 * from, 2 * from), axis + half, -2 * half);
        wedge.CloseFigure();
        g.FillPath(&zone, &wedge);
    }
    if (s.outerDeadzone > 0) {
        GraphicsPath band(FillModeAlternate);
        band.AddEllipse(circle);
        float ir = r * (1 - s.outerDeadzone);
        band.AddEllipse(RectF(c.X - ir, c.Y - ir, 2 * ir, 2 * ir));
        g.FillPath(&full, &band);
    }
    if (s.innerDeadzone > 0) {
        float ir = r * s.innerDeadzone;
        g.FillEllipse(&dead, c.X - ir, c.Y - ir, 2 * ir, 2 * ir);
    }
    Pen edge(Color(0xFF59616C), 2.f);
    g.DrawEllipse(&edge, circle);
    float gx = live.stickX, gy = live.stickY;
    ShapeStick(s, gx, gy);
    SolidBrush white{ Color(0xFFFFFFFF) }, game{ Color(Accent) };
    float dot = std::max(8.f, d * 0.07f);
    g.FillEllipse(&white, c.X + live.stickX * r - dot / 2, c.Y - live.stickY * r - dot / 2, dot, dot);
    if (s.ChangesGame()) {
        Pen gameRing(Color(Accent), 3.f);
        g.DrawEllipse(&gameRing, c.X + gx * r - dot * 0.8f, c.Y - gy * r - dot * 0.8f, dot * 1.6f, dot * 1.6f);
    }
    // the numbers, and which direction counts right now
    const char* dirs[4] = { "Up", "Right", "Down", "Left" };
    const float axes[4][2] = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } };
    std::string counting = "none";
    for (int i = 0; i < 4; i++)
        if (live.stickX * axes[i][0] + live.stickY * axes[i][1] > stickOn && InDirectionZone(s, live.stickX, live.stickY, axes[i][0], axes[i][1]))
            counting = dirs[i];
    wchar_t lines[3][80];
    swprintf(lines[0], 80, L"Stick:  %+.2f, %+.2f", live.stickX, live.stickY);
    swprintf(lines[1], 80, L"Game sees:  %+.2f, %+.2f", gx, gy);
    swprintf(lines[2], 80, L"Direction:  %hs", counting.c_str());
    SolidBrush text{ Color(Text) }, sub{ Color(SubText) };
    StringFormat left;
    left.SetLineAlignment(StringAlignmentCenter);
    float tx = box.X + box.Width * 0.56f, lineH = box.Height / 4;
    for (int i = 0; i < 3; i++)
        g.DrawString(lines[i], -1, font, RectF(tx, box.Y + lineH * (i + 0.4f), box.Width * 0.44f, lineH), &left, i == 0 ? &text : &sub);
    (void)hand;
}

// A wheel or a wrist page drawn by the in-game drawers into a box of the menu (hovered: that button lit).
static void DrawPreviewPicture(Graphics& g, const RectF& box, const Wheel& w, bool wrist, int cols, int hovered, const Style& st) {
    int S = std::max(64, (int)std::min(box.Width, box.Height));
    std::vector<uint32_t> px;
    Profile tmp;
    tmp.style = st;
    if (wrist) {
        WristStyle ws;
        ws.columns = cols;
        WristView wv;
        wv.wheel = &w;
        wv.visible = true;
        wv.hover = hovered;
        DrawWrist(px, S, tmp, ws, wv);
    } else {
        WheelView wv;
        wv.wheel = &w;
        wv.visible = true;
        wv.hovered = hovered;
        wv.crumb = w.title;
        DrawWheel(px, S, tmp, wv);
    }
    if (px.size() != (size_t)S * S) return;
    Bitmap pic(S, S, S * 4, PixelFormat32bppPARGB, (BYTE*)px.data());
    float side = std::min(box.Width, box.Height);
    g.DrawImage(&pic, RectF(box.X + (box.Width - side) / 2, box.Y + (box.Height - side) / 2, side, side));
}

void DrawMenu(std::vector<uint32_t>& out, int W, int H, const MenuView& v, const Style& st) {
    using namespace MenuColors;
    EnsureGdi();
    Bitmap bmp(W, H, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    auto roundRect = [](GraphicsPath& gp, float x, float y, float w, float h, float r) {
        r = std::max(1.f, std::min(r, std::min(w, h)));
        gp.AddArc(x, y, r, r, 180, 90);
        gp.AddArc(x + w - r, y, r, r, 270, 90);
        gp.AddArc(x + w - r, y + h - r, r, r, 0, 90);
        gp.AddArc(x, y + h - r, r, r, 90, 90);
        gp.CloseFigure();
    };
    const float corner = std::min(W, H) * 0.035f;
    const float hf = MenuHeightFactor(v.visibleRows);
    const float headerH = H * kMenuHeaderH / hf, footerY = H * (1 - kMenuFooterH / hf);

    // panel: a soft vertical gradient, clipped to rounded corners
    GraphicsPath panel;
    roundRect(panel, 0, 0, (float)W, (float)H, corner);
    LinearGradientBrush grad(PointF(0, 0), PointF(0, (float)H), Color(PanelTop), Color(PanelBottom));
    g.FillPath(&grad, &panel);
    g.SetClip(&panel);
    SolidBrush headerB{ Color(Header) }, footerB{ Color(Footer) };
    g.FillRectangle(&headerB, 0.f, 0.f, (float)W, headerH);
    g.FillRectangle(&footerB, 0.f, footerY, (float)W, H - footerY);
    SolidBrush accentB{ Color(Accent) };
    g.FillRectangle(&accentB, 0.f, headerH - 3, (float)W, 3.f); // the accent line under the header
    g.ResetClip();
    Pen outline(Color(0xFF2E343C), 2.f);
    g.DrawPath(&outline, &panel);

    SolidBrush textB{ Color(Text) }, subB{ Color(SubText) }, headingB{ Color(Heading) }, disabledB{ Color(Disabled) };
    SolidBrush hoverB{ Color(Hover) }, pressedB{ Color(Pressed) }, trackB{ Color(Track) }, knobB{ Color(Knob) };
    SolidBrush iconB{ Color(IconButton) };
    Pen separator(Color(Separator), 1.5f);
    const float rowH = (footerY - headerH) / v.visibleRows;
    auto titleFont = MakeFont(st, headerH * 0.34f * st.fontScale, FontStyleBold);
    auto font = MakeFont(st, rowH * 0.27f * st.fontScale, FontStyleRegular);
    auto valueFont = MakeFont(st, rowH * 0.25f * st.fontScale, FontStyleBold);
    auto detailFont = MakeFont(st, rowH * 0.2f * st.fontScale, FontStyleRegular);
    auto headingFont = MakeFont(st, rowH * 0.19f * st.fontScale, FontStyleBold);
    auto iconFont = MakeFont(st, rowH * 0.34f * st.fontScale, FontStyleRegular);
    StringFormat center, left, oneLine;
    center.SetAlignment(StringAlignmentCenter);
    center.SetLineAlignment(StringAlignmentCenter);
    left.SetLineAlignment(StringAlignmentCenter);
    left.SetTrimming(StringTrimmingEllipsisCharacter);
    left.SetFormatFlags(StringFormatFlagsNoWrap);
    oneLine.SetAlignment(StringAlignmentCenter);
    oneLine.SetLineAlignment(StringAlignmentCenter);
    oneLine.SetTrimming(StringTrimmingEllipsisCharacter);
    oneLine.SetFormatFlags(StringFormatFlagsNoWrap);

    std::vector<MenuBox> boxes;
    MenuLayout(v, boxes);
    auto isHover = [&](const MenuHit& h) { return h.part != MenuPart::None && v.hover == h; };
    auto isPressed = [&](const MenuHit& h) { return h.part != MenuPart::None && v.pressed == h; };

    // title: left, after the back button's place
    float titleX = W * (v.canBack ? 0.14f : 0.04f);
    RectF titleRect(titleX, 0, W * 0.8f - titleX, headerH);
    g.DrawString(Utf8ToW(v.title).c_str(), -1, titleFont.get(), titleRect, &left, &textB);

    // a small round icon button (header, - / +, scroll arrows)
    auto iconButton = [&](const MenuBox& b, const wchar_t* glyph, bool enabled) {
        float x = b.x * W, y = b.y * H, w = b.w * W, h = b.h * H;
        float d = std::min(w, h);
        RectF circle(x + (w - d) / 2, y + (h - d) / 2, d, d);
        bool hover = enabled && isHover(b.hit), pressed = enabled && isPressed(b.hit);
        SolidBrush fill{ Color(pressed ? Pressed : hover ? 0xFF3A4350 : IconButton) };
        g.FillEllipse(&fill, circle);
        if (hover && !pressed) {
            Pen ring(Color(Accent), 2.f);
            g.DrawEllipse(&ring, circle);
        }
        g.DrawString(glyph, -1, iconFont.get(), circle, &center, enabled ? (Brush*)&textB : (Brush*)&disabledB);
    };

    for (const MenuBox& b : boxes) {
        if (b.hit.sub >= 0 && b.hit.row >= 0 && v.rows[b.hit.row].kind == MenuRowKind::Overview)
            continue; // the overview's tags: drawn with its pictures (these boxes are for pointing)
        if (b.hit.row >= 0 && v.rows[b.hit.row].kind == MenuRowKind::Preview) {
            const MenuViewRow& r = v.rows[b.hit.row];
            int n = r.PreviewCount();
            if (b.hit.sub < 0 && !r.previews.empty()) { // the picture
                int hov = v.hover.row == b.hit.row && v.hover.sub >= 0 && v.hover.sub < n ? v.hover.sub : r.previewSelected;
                SolidBrush well{ Color(0xFF181C22) };
                GraphicsPath wp;
                roundRect(wp, b.x * W, b.y * H, b.w * W, b.h * H, b.h * H * 0.06f);
                g.FillPath(&well, &wp);
                RectF in(b.x * W + 6, b.y * H + 6, b.w * W - 12, b.h * H - 12);
                DrawPreviewPicture(g, in, r.previews[0], r.previewWrist, r.previewCols, hov, st);
                if (n == 0)
                    g.DrawString(L"No buttons yet", -1, detailFont.get(), RectF(b.x * W, b.y * H + b.h * H * 0.8f, b.w * W, b.h * H * 0.15f),
                                 &oneLine, &subB);
            } else if (b.hit.sub >= n) { // a side button
                int k = b.hit.sub - n;
                if (k >= (int)r.items.size()) continue;
                float x = b.x * W, y = b.y * H, w = b.w * W, h = b.h * H;
                GraphicsPath btn;
                roundRect(btn, x, y, w, h, h * 0.35f);
                bool hover = isHover(b.hit), pressed = isPressed(b.hit);
                bool danger = r.items[k].rfind("Delete", 0) == 0;
                SolidBrush fill{ Color(pressed ? Pressed : hover ? 0xFF3A4350 : 0xFF2C333C) };
                g.FillPath(&fill, &btn);
                Pen ring(Color(hover && !pressed ? Accent : 0xFF3C4550), hover ? 2.5f : 1.f);
                g.DrawPath(&ring, &btn);
                SolidBrush red{ Color(0xFFFF5A5F) };
                g.DrawString(Utf8ToW(r.items[k]).c_str(), -1, valueFont.get(), RectF(x + 12, y, w - 24, h), &oneLine,
                             danger ? (Brush*)&red : (Brush*)&textB);
            }
            continue; // the buttons in the picture are drawn by it (these boxes are for pointing)
        }
        switch (b.hit.part) {
        case MenuPart::Back:
            iconButton(b, L"\x2039", true);
            continue;
        case MenuPart::Close:
            iconButton(b, L"\x2715", true);
            continue;
        case MenuPart::ScrollUp:
            iconButton(b, L"\x25B4", v.first > 0);
            continue;
        case MenuPart::ScrollDown:
            iconButton(b, L"\x25BE", v.first < MenuMaxFirst(v));
            continue;
        case MenuPart::Less:
            iconButton(b, v.rows[b.hit.row].kind == MenuRowKind::Choice ? L"\x2039" : L"\x2212", v.rows[b.hit.row].canLess);
            continue;
        case MenuPart::More:
            iconButton(b, v.rows[b.hit.row].kind == MenuRowKind::Choice ? L"\x203A" : L"+", v.rows[b.hit.row].canMore);
            continue;
        default:
            break;
        }
        const MenuViewRow& r = v.rows[b.hit.row];
        float x = b.x * W, y = b.y * H, w = b.w * W, h = b.h * H;
        const float inset = W * 0.04f;
        if (r.kind == MenuRowKind::Overview) {
            DrawOverview(g, W, H, v, r, b.hit.row, b.y, b.h, valueFont.get(), detailFont.get(), headingFont.get(), isHover);
            continue;
        }
        if (r.kind == MenuRowKind::Bar) { // a row of big buttons; the first one stands out
            int k = b.hit.sub;
            if (k < 0 || k >= (int)r.items.size()) continue;
            GraphicsPath btn;
            roundRect(btn, x, y, w, h, h * 0.35f);
            bool hover = isHover(b.hit), pressed = isPressed(b.hit);
            SolidBrush fill{ Color(pressed ? Pressed : k == 0 ? Accent : hover ? 0xFF3A4350 : 0xFF2C333C) };
            g.FillPath(&fill, &btn);
            if (hover && !pressed) {
                Pen ring(Color(k == 0 ? 0xFFFFFFFF : Accent), 2.5f);
                g.DrawPath(&ring, &btn);
            }
            g.DrawString(Utf8ToW(r.items[k]).c_str(), -1, valueFont.get(), RectF(x, y, w, h), &oneLine, &textB);
            continue;
        }
        if (r.kind == MenuRowKind::Tiles) { // a big button with a title and a line under it
            int k = b.hit.sub;
            if (k < 0 || k >= (int)r.items.size()) continue;
            GraphicsPath tile;
            roundRect(tile, x, y, w, h, std::min(w, h) * 0.18f);
            bool hover = isHover(b.hit), pressed = isPressed(b.hit);
            SolidBrush fill{ Color(pressed ? Pressed : hover ? 0xFF3A4350 : 0xFF2C333C) };
            g.FillPath(&fill, &tile);
            Pen ring(Color(hover && !pressed ? Accent : 0xFF3C4550), hover ? 3.f : 1.5f);
            g.DrawPath(&ring, &tile);
            SolidBrush bar{ Color(Accent) };
            if (!(k < (int)r.previews.size() && !r.previews[k].name.empty()))
                g.FillRectangle(&bar, x + w * 0.08f, y + h * 0.16f, w * 0.12f, std::max(3.f, h * 0.025f)); // a short accent line
            float tin = w * 0.08f;
            bool hasPic = k < (int)r.previews.size() && !r.previews[k].name.empty();
            if (hasPic) { // a small picture of the wheel or page, the text to its right
                float side = h * 0.84f;
                DrawPreviewPicture(g, RectF(x + h * 0.08f, y + h * 0.08f, side, side), r.previews[k], r.previewWrist, r.previewCols, -1, st);
                x += side + h * 0.08f;
                w -= side + h * 0.08f;
                tin = w * 0.06f;
            }
            bool hasDetail = k < (int)r.itemDetails.size() && !r.itemDetails[k].empty();
            StringFormat wrap;
            wrap.SetTrimming(StringTrimmingEllipsisWord);
            auto tileFont = MakeFont(st, std::min(headerH * 0.34f, h * 0.16f) * st.fontScale, FontStyleBold);
            g.DrawString(Utf8ToW(r.items[k]).c_str(), -1, tileFont.get(), RectF(x + tin, y + h * 0.22f, w - 2 * tin, h * 0.3f), &left,
                         pressed ? (Brush*)&knobB : (Brush*)&textB);
            if (hasDetail)
                g.DrawString(Utf8ToW(r.itemDetails[k]).c_str(), -1, detailFont.get(), RectF(x + tin, y + h * 0.54f, w - 2 * tin, h * 0.42f),
                             &wrap, pressed ? (Brush*)&knobB : (Brush*)&subB);
            continue;
        }
        if (r.kind == MenuRowKind::Live) { // a live picture across the row
            RectF area(inset, y + h * 0.04f, W - 2 * inset, h * 0.9f);
            if (r.live == LiveKind::Stick)
                DrawLiveStick(g, area, r.liveHand, v.live[r.liveHand], v.stickOn, detailFont.get());
            else if (r.live == LiveKind::Controller)
                DrawLiveController(g, area, r.liveHand, v.live[r.liveHand], detailFont.get());
            else
                for (int hand = 0; hand < 2; hand++)
                    DrawLiveController(g, RectF(area.X + area.Width / 2 * hand, area.Y, area.Width / 2 - 8, area.Height), hand, v.live[hand],
                                       detailFont.get());
            g.DrawLine(&separator, inset, y + h - 1, W - inset, y + h - 1);
            continue;
        }
        if (r.kind == MenuRowKind::Heading) { // small grey capitals, sitting low
            std::string caps = r.label;
            for (char& c : caps)
                c = (char)toupper((unsigned char)c);
            RectF hr(inset, y + h * 0.35f, W - 2 * inset, h * 0.6f);
            g.DrawString(Utf8ToW(caps).c_str(), -1, headingFont.get(), hr, &left, &headingB);
            continue;
        }
        // the whole row lights up when pointed at (for numbers / choices: the full width, - / + included)
        bool rowHover = isHover(b.hit) || (r.kind == MenuRowKind::Number || r.kind == MenuRowKind::Choice) &&
                                              v.hover.row == b.hit.row && v.hover.part != MenuPart::None;
        float fullW = (r.kind == MenuRowKind::Number || r.kind == MenuRowKind::Choice) ? (float)W : w;
        if (isPressed(b.hit))
            g.FillRectangle(&pressedB, x, y, fullW, h);
        else if (rowHover) {
            g.FillRectangle(&hoverB, x, y, fullW, h);
            g.FillRectangle(&accentB, x, y, 5.f, h); // the accent bar on the left
        }
        else if (r.isWarn) { // may need your attention: a soft amber row with an amber bar
            SolidBrush warnFill{ Color(0x30F2C94C) }, warnBar{ Color(0xFFF2C94C) };
            g.FillRectangle(&warnFill, x, y, fullW, h);
            g.FillRectangle(&warnBar, x, y, 5.f, h);
        }
        g.DrawLine(&separator, inset, y + h - 1, W - inset, y + h - 1);

        float valueW = r.kind == MenuRowKind::Toggle ? h * 1.4f : r.kind == MenuRowKind::Link ? h * 0.6f : 0;
        float textW = (r.kind == MenuRowKind::Number || r.kind == MenuRowKind::Choice ? 0.58f * W : w) - inset - valueW - 10;
        SolidBrush dangerB{ Color(0xFFFF5A5F) };
        SolidBrush warnB{ Color(0xFFF2C94C) };
        Brush* tb = isPressed(b.hit) ? (Brush*)&knobB : r.isDanger ? (Brush*)&dangerB : r.isWarn ? (Brush*)&warnB : (Brush*)&textB;
        Brush* sb = isPressed(b.hit) ? (Brush*)&knobB : (Brush*)&subB;
        if (r.detail.empty()) {
            RectF lr(inset, y, textW, h);
            g.DrawString(Utf8ToW(r.label).c_str(), -1, font.get(), lr, &left, tb);
        } else {
            RectF l1(inset, y + h * 0.1f, textW, h * 0.46f), l2(inset, y + h * 0.52f, textW, h * 0.38f);
            g.DrawString(Utf8ToW(r.label).c_str(), -1, font.get(), l1, &left, tb);
            g.DrawString(Utf8ToW(r.detail).c_str(), -1, detailFont.get(), l2, &left, sb);
        }

        switch (r.kind) {
        case MenuRowKind::Toggle: { // a switch: a track and a knob, blue when on
            float tw = h * 0.9f, th = h * 0.44f, tx = W - inset - tw, ty = y + (h - th) / 2;
            GraphicsPath track;
            roundRect(track, tx, ty, tw, th, th);
            g.FillPath(r.isOn ? (Brush*)&accentB : (Brush*)&trackB, &track);
            float kd = th - 6, kx = r.isOn ? tx + tw - kd - 3 : tx + 3;
            g.FillEllipse(&knobB, kx, ty + 3, kd, kd);
            break;
        }
        case MenuRowKind::Number:
        case MenuRowKind::Choice: { // the value between - and +, and for numbers a thin slider under it
            float vx = 0.69f * W, vw = 0.2f * W;
            RectF vr(vx, y + h * (r.fraction >= 0 ? 0.12f : 0.f), vw, h * (r.fraction >= 0 ? 0.6f : 1.f));
            g.DrawString(Utf8ToW(r.value).c_str(), -1, valueFont.get(), vr, &oneLine, &textB);
            if (r.fraction >= 0) {
                float sx = vx + vw * 0.1f, sw = vw * 0.8f, sy = y + h * 0.74f;
                g.FillRectangle(&trackB, sx, sy, sw, 4.f);
                g.FillRectangle(&accentB, sx, sy, sw * r.fraction, 4.f);
                g.FillEllipse(&knobB, sx + sw * r.fraction - 5, sy - 3, 10.f, 10.f);
            }
            break;
        }
        case MenuRowKind::Link: { // a chevron
            RectF cr(W - inset - valueW, y, valueW, h);
            g.DrawString(L"\x203A", -1, titleFont.get(), cr, &center, isPressed(b.hit) ? (Brush*)&knobB : (Brush*)&subB);
            break;
        }
        default:
            break;
        }
    }
    // footer: what to do now, and a hint
    std::string status = !v.status.empty() ? v.status
                         : v.stickFree ? "Point and pull the trigger  \xC2\xB7  move the sticks freely: they only test here"
                                       : "Point and pull the trigger  \xC2\xB7  stick: scroll / change a value";
    RectF sr(W * 0.04f, footerY, W * 0.7f, H - footerY);
    g.DrawString(Utf8ToW(status).c_str(), -1, detailFont.get(), sr, &left, v.status.empty() ? (Brush*)&headingB : (Brush*)&accentB);
    if (v.covered) { // the keyboard lies on the pane: dim what is behind it
        SolidBrush dim(Color(150, 8, 10, 14));
        g.FillRectangle(&dim, 0.f, 0.f, (REAL)W, (REAL)H);
    }
    Grab(bmp, out, W, H);
}
