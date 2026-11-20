#pragma once
#include "common.h"
#include <functional>
#include <map>
#include <d3d11_1.h>
#include <dxgi1_2.h>

// Headset mirror: a desktop window showing one eye of what the headset shows, the game's picture
// WITH our own panels (wheel, wrist panels, screen, lasers...) drawn in where they really are.
// The game's own mirror can't show them: they are separate layers the runtime adds later.
// D3D11 games only for now.
class HeadsetMirror {
public:
    // Swapchain bookkeeping (the game's and ours), always on: it's cheap and needed the moment the mirror opens.
    void OnCreate(XrSwapchain sc, const XrSwapchainCreateInfo* ci);
    void OnImages(XrSwapchain sc, const XrSwapchainImageBaseHeader* imgs, uint32_t n);
    void OnAcquire(XrSwapchain sc, uint32_t idx);
    void OnRelease(XrSwapchain sc);
    void OnDestroy(XrSwapchain sc);

    // Called in xrEndFrame with every layer that will be submitted (game's + ours), on the game's thread.
    using LocateFn = std::function<bool(XrSpace space, XrSpace base, XrTime t, XrPosef& out)>;
    void Frame(ID3D11Device* dev, ID3D11DeviceContext* ctx, const std::vector<const XrCompositionLayerBaseHeader*>& layers, XrTime t,
               const LocateFn& locate);

    void SetOn(bool on); // opens / closes the window
    bool On() const { return on; }
    bool ClosedByUser();    // the window's X was clicked since the last call
    int eye = 1;            // 0 left, 1 right
    float smoothing = 0.5f; // steady the picture against head shake (0 = off .. 1 = strongest)
    bool onTop = false;     // keep the window above other windows (it never takes focus either way)
    bool topApplied = false;
    float zoom = 1.3f; // 1 = the whole eye picture, more = cropped in like a normal camera
    int fps = 60;      // most frames a second the window updates (0 = every headset frame)
    void Shutdown();   // release GPU things (session ending)
    bool grab = false; // tests: copy the next finished picture into grabbed (BGRA)
    std::vector<uint32_t> grabbed;
    int grabW = 0, grabH = 0;
    HWND Window() const { return hwnd; }

private:
    struct Sw {
        int64_t format = 0;
        uint32_t w = 0, h = 0, arraySize = 1, mips = 1, samples = 1;
        std::vector<ID3D11Texture2D*> imgs;
        std::vector<uint32_t> pending;
        int last = -1;
    };
    std::mutex mx;
    std::map<uint64_t, Sw> sws;
    bool on = false;
    double lastFrame = 0;
    // window (its own thread so the game's message loop doesn't matter)
    void StartWindow();
    void StopWindow();
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    HANDLE thread = nullptr;
    volatile HWND hwnd = nullptr;
    volatile LONG clientW = 1280, clientH = 720, sized = 0, userClosed = 0;
    // GPU
    ID3D11Device* dev = nullptr;
    IDXGISwapChain1* chain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11Buffer* cb = nullptr;
    ID3D11SamplerState* samp = nullptr;
    ID3D11BlendState *blendOpaque = nullptr, *blendPremul = nullptr;
    ID3D11RasterizerState* rast = nullptr;
    ID3DDeviceContextState* state = nullptr;
    ID3D11Texture2D* eyeCopy = nullptr;
    bool eyeMips = false; // eyeCopy has a mip chain (made each frame)
    ID3D11ShaderResourceView* eyeSrv = nullptr;
    D3D11_TEXTURE2D_DESC eyeDesc{};
    int64_t eyeFmt = 0;
    std::map<ID3D11Texture2D*, ID3D11ShaderResourceView*> srvs;
    bool failed = false;
    PoseSmoother camSmooth;
    XrTime lastT = 0;
    bool InitGpu(ID3D11Device* d);
    void ReleaseGpu();
    ID3D11ShaderResourceView* Srv(ID3D11Texture2D* t, int64_t fmt);
};
