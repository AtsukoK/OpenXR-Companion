#pragma once
// Everything the layer shows in the headset on top of the game: the mirrored screen (with its bar and
// picker), the wheel, the VR keyboard (with its bar), the wrist panels and touch cursors, the
// controller offsets panel, laser pointers, fingertips and toasts.
//
// The engine decides what is visible and where; this draws each element into its own swapchain
// (only when its look changed) and adds a quad layer for it to the game's frame.
#include "panel.h"
#include "screen.h"
#include <functional>

// What drawing needs from the session.
struct OverlayContext {
    XrSession session = XR_NULL_HANDLE;
    Gfx* gfx = nullptr;
    const XrFns* swap = nullptr;
    XrSpace local = XR_NULL_HANDLE; // world-locked elements
    XrSpace view = XR_NULL_HANDLE;  // head-locked elements (toasts)
};

using LayerList = std::vector<const XrCompositionLayerBaseHeader*>;

// One element drawn into its own swapchain and shown as a quad layer.
class OverlayQuad {
public:
    // Makes sure the swapchain exists at w x h (made again if the size changed).
    bool Ensure(const OverlayContext& c, int w, int h);
    // Redraws when `version` differs from the one on it; `draw` fills the pixels (premultiplied BGRA).
    void Redraw(const OverlayContext& c, uint64_t version, std::vector<uint32_t>& scratch,
                const std::function<void(std::vector<uint32_t>&)>& draw);
    // Adds the quad to the frame (once an image has been uploaded).
    void Submit(LayerList& layers, XrSpace space, const XrPosef& pose, XrExtent2Df size) { SubmitAs(quad_, layers, space, pose, size); }
    // The same picture shown more than once (one quad per copy, kept by the caller until xrEndFrame).
    void SubmitAs(XrCompositionLayerQuad& quad, LayerList& layers, XrSpace space, const XrPosef& pose, XrExtent2Df size);
    void Invalidate() { drawn_ = kNothing; }
    void Destroy(const OverlayContext& c);

    bool Exists() const { return panel_.sc != XR_NULL_HANDLE; }
    bool Ready() const { return panel_.ready; }
    int Width() const { return panel_.w; }
    int Height() const { return panel_.h; }
    Panel& Raw() { return panel_; }

private:
    static constexpr uint64_t kNothing = ~0ull;
    Panel panel_;
    uint64_t drawn_ = kNothing;
    XrCompositionLayerQuad quad_{ XR_TYPE_COMPOSITION_LAYER_QUAD };
};

class OverlayRenderer {
public:
    explicit OverlayRenderer(Engine& engine) : eng_(engine) {}

    // Draws what is visible and appends its layers. Called from xrEndFrame with the layer's lock held.
    void Render(const OverlayContext& c, LayerList& layers);
    // The head in the local space, for hiding the screen when seen from behind.
    void SetHead(const XrPosef& head, bool valid) { head_ = head; headValid_ = valid; }
    // A new profile: redraw everything with its style.
    void InvalidateAll();
    // The session ends: free every swapchain and stop capturing.
    void Destroy(const OverlayContext& c);

    // Called after the layer wrote the profile itself (so it isn't reloaded as an outside change).
    std::function<void()> onProfileSaved;

private:
    void UpdateScreenSource(ScreenView& sv);
    void FillScreenChoices(ScreenView& sv);
    void RenderScreen(const OverlayContext& c, LayerList& layers);
    void RenderWheel(const OverlayContext& c, LayerList& layers);
    void RenderKeyboard(const OverlayContext& c, LayerList& layers);
    void RenderWrists(const OverlayContext& c, LayerList& layers);
    void RenderOffsets(const OverlayContext& c, LayerList& layers);
    void RenderPointers(const OverlayContext& c, LayerList& layers);
    void RenderToast(const OverlayContext& c, LayerList& layers);
    void StopScreen(const OverlayContext& c);

    Engine& eng_;
    std::vector<uint32_t> scratch_; // shared pixel buffer for redraws
    XrPosef head_{ {0,0,0,1}, {0,0,0} };
    bool headValid_ = false;

    // mirrored screen
    OverlayQuad screen_, screenBar_, screenPicker_;
    ScreenCapture capture_;
    std::vector<uint32_t> screenPixels_;
    uint64_t captureMessageVersion_ = 0;
    uint64_t windowAskSeen_ = 0;
    std::string pickedExe_, pickedTitle_; // the window picked last, remembered as the default

    OverlayQuad wheel_;
    OverlayQuad keyboard_, keyboardBar_;
    OverlayQuad wrist_[2], cursor_[2];
    OverlayQuad offsets_;
    OverlayQuad pointerDot_, pointerBeam_, finger_;
    OverlayQuad toast_;
    // pointers and fingertips use one picture for both hands, each hand has its own quad
    XrCompositionLayerQuad beamQuad_[2]{}, dotQuad_[2]{}, fingerQuad_[2]{};
};
