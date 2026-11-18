// Window capture with Windows.Graphics.Capture (the API Discord, OBS and the Snipping Tool use).
// Works for browsers and other GPU-drawn windows, and for windows covered by other windows.
// Kept apart from screen.cpp so the WinRT headers stay in one place.
#include "wincap.h"
#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <dxgi.h>
#include <atomic>

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wd3d = winrt::Windows::Graphics::DirectX::Direct3D11;

struct WindowCapture {
    wgc::GraphicsCaptureItem item{ nullptr };
    wgc::Direct3D11CaptureFramePool pool{ nullptr };
    wgc::GraphicsCaptureSession session{ nullptr };
    wd3d::IDirect3DDevice dev{ nullptr };
    winrt::Windows::Graphics::SizeInt32 size{};
    std::atomic<bool> closed{ false };
};

bool WinCapSupported() {
    try { return wgc::GraphicsCaptureSession::IsSupported(); } catch (...) { return false; }
}

WindowCapture* WinCapOpen(HWND hwnd, ID3D11Device* d3d, std::string& err) {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {} // already initialised on this thread
    try {
        if (!wgc::GraphicsCaptureSession::IsSupported()) { err = "window capture needs Windows 10 version 1903 or newer"; return nullptr; }
        auto wc = new WindowCapture;
        auto interop = winrt::get_activation_factory<wgc::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        winrt::check_hresult(interop->CreateForWindow(hwnd, winrt::guid_of<wgc::GraphicsCaptureItem>(), winrt::put_abi(wc->item)));
        winrt::com_ptr<IDXGIDevice> dxgi;
        winrt::check_hresult(d3d->QueryInterface(IID_PPV_ARGS(dxgi.put())));
        winrt::com_ptr<::IInspectable> insp;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), insp.put()));
        wc->dev = insp.as<wd3d::IDirect3DDevice>();
        wc->size = wc->item.Size();
        wc->pool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(wc->dev,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, wc->size);
        wc->session = wc->pool.CreateCaptureSession(wc->item);
        try { wc->session.IsCursorCaptureEnabled(false); } catch (...) {} // we draw our own pointer dot
        try { wc->session.IsBorderRequired(false); } catch (...) {}       // Windows 11: no yellow border
        wc->item.Closed([wc](auto&&, auto&&) { wc->closed = true; });
        wc->session.StartCapture();
        return wc;
    } catch (const winrt::hresult_error& e) {
        char b[64]; snprintf(b, sizeof b, "0x%08lX", (unsigned long)e.code().value);
        err = std::string("window capture failed (") + b + ")";
    } catch (...) { err = "window capture failed"; }
    return nullptr;
}

ID3D11Texture2D* WinCapNext(WindowCapture* wc, int& cw, int& ch, bool& closed) {
    closed = wc->closed;
    if (closed) return nullptr;
    try {
        auto frame = wc->pool.TryGetNextFrame();
        if (!frame) return nullptr;
        auto cs = frame.ContentSize();
        if (cs.Width != wc->size.Width || cs.Height != wc->size.Height) { // the window was resized
            wc->size = cs;
            wc->pool.Recreate(wc->dev, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, cs);
        }
        auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ID3D11Texture2D* tex = nullptr;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&tex)));
        cw = cs.Width; ch = cs.Height;
        return tex; // the caller releases it; the frame itself goes back to the pool here
    } catch (...) { closed = true; return nullptr; }
}

void WinCapClose(WindowCapture* wc) {
    if (!wc) return;
    try { if (wc->session) wc->session.Close(); if (wc->pool) wc->pool.Close(); } catch (...) {}
    delete wc;
}
