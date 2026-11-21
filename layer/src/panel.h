#pragma once
#include "engine.h"
#include <wrl/client.h>

struct XrFns {
    PFN_xrEnumerateSwapchainFormats EnumerateSwapchainFormats = nullptr;
    PFN_xrCreateSwapchain CreateSwapchain = nullptr;
    PFN_xrDestroySwapchain DestroySwapchain = nullptr;
    PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages = nullptr;
    PFN_xrAcquireSwapchainImage AcquireSwapchainImage = nullptr;
    PFN_xrWaitSwapchainImage WaitSwapchainImage = nullptr;
    PFN_xrReleaseSwapchainImage ReleaseSwapchainImage = nullptr;
};

// The game's graphics device and our upload helpers. GPU objects are held in ComPtr (they free themselves);
// Release() waits for our GPU work first, then drops them.
struct Gfx {
    template <class T> using Com = Microsoft::WRL::ComPtr<T>;
    int api = 0; // 0 none, 11 = D3D11, 12 = D3D12
    Com<ID3D11Device> d11;
    Com<ID3D11DeviceContext> ctx;
    Com<ID3D12Device> d12;
    Com<ID3D12CommandQueue> queue;
    // D3D12 uploads: a ring of command lists, so an upload never waits for the game's GPU work.
    // A slot is only waited for when it comes round again (three uploads later, normally long done).
    static const int kRing = 3;
    Com<ID3D12CommandAllocator> alloc[kRing];
    Com<ID3D12GraphicsCommandList> list[kRing];
    UINT64 slotFence[kRing]{};
    int slot = 0;
    Com<ID3D12Fence> fence;
    UINT64 fenceVal = 0;
    HANDLE ev = nullptr; // owned: closed in Release()
    void WaitFence(UINT64 v); // until the GPU has passed v (no-op when it has)
    void Init(const void* bindingChain);
    void Release();
};

class Panel {
public:
    int w = 0, h = 0;
    XrSwapchain sc = XR_NULL_HANDLE;
    bool ready = false; // at least one image released
    bool Create(XrSession s, Gfx& g, const XrFns& f, int w, int h);
    bool Upload(Gfx& g, const XrFns& f, const std::vector<uint32_t>& bgraPremul);
    void Destroy(const XrFns& f);

private:
    bool swapRB = false;
    std::vector<void*> imgs;
    Gfx::Com<ID3D12Resource> upload[Gfx::kRing]; // D3D12: one staging buffer per ring slot of this panel
    UINT64 uploadFence[Gfx::kRing]{};
    UINT64 uploadSize[Gfx::kRing]{};
    int uploadNext = 0;
    ID3D12Fence* fence = nullptr; // the Gfx fence (outlives the panel, not owned), for Destroy
};

// CPU drawing (GDI+), output is premultiplied BGRA.
void DrawWheel(std::vector<uint32_t>& out, int size, const Profile& p, const WheelView& v);
void DrawToast(std::vector<uint32_t>& out, int w, int h, const std::string& text, const Style& st);
void DrawKeyboard(std::vector<uint32_t>& out, int w, int h, const KeyboardView& kb, const Style& st);
void DrawWrist(std::vector<uint32_t>& out, int size, const Profile& p, const WristStyle& w, const WristView& v);
void DrawFinger(std::vector<uint32_t>& out, int w, int h, const Style& st);
void DrawCursor(std::vector<uint32_t>& out, int size, int level, const Style& st);
void DrawPointerDot(std::vector<uint32_t>& out, int size, const Style& st);
void DrawBeam(std::vector<uint32_t>& out, int w, int h, const Style& st);
void DrawShutdown();
// Buttons in a grid (see ListLayout); labels may hold a second line after '\n'.
void DrawList(std::vector<uint32_t>& out, int w, int h, const std::string& title, const std::vector<std::string>& labels, int cols,
              int hover, int pressed, const Style& st);
