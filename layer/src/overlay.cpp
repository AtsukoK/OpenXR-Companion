#include "overlay.h"
#include <cctype>

namespace {
// Panel picture sizes in pixels.
constexpr int kKeyboardPx = 1500; // width; height follows the layout
constexpr int kBarPx = 1200;      // bars under the screen and the keyboard (width)
constexpr int kPickerPx = 1000;   // screen picker (width)
constexpr int kOffsetsPx = 900;   // controller offsets panel (width)
constexpr int kWristPx = 512;
constexpr int kCursorPx = 128;
constexpr int kDotPx = 64;
constexpr int kBeamW = 16, kBeamH = 256;
constexpr int kFingerW = 64, kFingerH = 150;
constexpr int kToastW = 1024, kToastH = 128;

constexpr float kBeamWidthM = 0.0035f;
const XrPosef kToastPose{ { 0, 0, 0, 1 }, { 0, -0.14f, -0.6f } }; // in the head space: a little below the eyes
constexpr XrExtent2Df kToastSize{ 0.32f, 0.04f };

std::string Lower(std::string s) {
    for (auto& c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

std::string WithoutExtension(std::string exe) {
    size_t dot = exe.rfind('.');
    return dot == std::string::npos ? exe : exe.substr(0, dot);
}

std::string Shorten(const std::string& s, size_t max) {
    return s.size() > max ? s.substr(0, max - 3) + "..." : s;
}

// Premultiplied pixels made see-through by `alpha` (every channel scaled).
void FadePixels(std::vector<uint32_t>& px, float alpha) {
    uint32_t a = (uint32_t)(std::clamp(alpha, 0.f, 1.f) * 256.f);
    for (auto& p : px) {
        uint32_t rb = ((p & 0x00FF00FFu) * a >> 8) & 0x00FF00FFu;
        uint32_t ga = (((p >> 8) & 0x00FF00FFu) * a) & 0xFF00FF00u;
        p = rb | ga;
    }
}
} // namespace

// ---------------------------------------------------------------- OverlayQuad

bool OverlayQuad::Ensure(const OverlayContext& c, int w, int h) {
    if (panel_.sc && (panel_.w != w || panel_.h != h)) Destroy(c);
    if (!panel_.sc) panel_.Create(c.session, *c.gfx, *c.swap, w, h);
    return panel_.sc != XR_NULL_HANDLE;
}

void OverlayQuad::Redraw(const OverlayContext& c, uint64_t version, std::vector<uint32_t>& scratch,
                         const std::function<void(std::vector<uint32_t>&)>& draw) {
    if (!panel_.sc || version == drawn_) return;
    draw(scratch);
    if (panel_.Upload(*c.gfx, *c.swap, scratch)) drawn_ = version;
}

void OverlayQuad::SubmitAs(XrCompositionLayerQuad& q, LayerList& layers, XrSpace space, const XrPosef& pose, XrExtent2Df size) {
    if (!panel_.ready) return;
    q = XrCompositionLayerQuad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
    q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    q.space = space;
    q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    q.subImage.swapchain = panel_.sc;
    q.subImage.imageRect = { { 0, 0 }, { panel_.w, panel_.h } };
    q.pose = pose;
    q.size = size;
    layers.push_back((const XrCompositionLayerBaseHeader*)&q);
}

void OverlayQuad::Destroy(const OverlayContext& c) {
    if (c.swap) panel_.Destroy(*c.swap);
    drawn_ = kNothing;
}

// ---------------------------------------------------------------- OverlayRenderer

void OverlayRenderer::Render(const OverlayContext& c, LayerList& layers) {
    // back to front: the screen under the menus, pointers on top, toasts last
    RenderScreen(c, layers);
    RenderWheel(c, layers);
    RenderKeyboard(c, layers);
    RenderWrists(c, layers);
    RenderOffsets(c, layers);
    RenderPointers(c, layers);
    RenderToast(c, layers);
}

void OverlayRenderer::InvalidateAll() {
    for (OverlayQuad* q :
         { &screenBar_, &screenPicker_, &wheel_, &keyboard_, &keyboardBar_, &offsets_, &pointerDot_, &pointerBeam_, &finger_, &toast_ })
        q->Invalidate();
    for (int h = 0; h < 2; h++) {
        wrist_[h].Invalidate();
        cursor_[h].Invalidate();
    }
}

void OverlayRenderer::Destroy(const OverlayContext& c) {
    StopScreen(c);
    for (OverlayQuad* q : { &wheel_, &keyboard_, &keyboardBar_, &offsets_, &pointerDot_, &pointerBeam_, &finger_, &toast_ })
        q->Destroy(c);
    for (int h = 0; h < 2; h++) {
        wrist_[h].Destroy(c);
        cursor_[h].Destroy(c);
    }
}

void OverlayRenderer::StopScreen(const OverlayContext& c) {
    capture_.Stop();
    screen_.Destroy(c);
    screenBar_.Destroy(c);
    screenPicker_.Destroy(c);
    screenPixels_.clear();
    screenPixels_.shrink_to_fit();
}

// ---------------------------------------------------------------- mirrored screen

// The engine asked for another window (next / previous / by name), or the shown one closed.
void OverlayRenderer::UpdateScreenSource(ScreenView& sv) {
    bool asked = sv.windowAsk != windowAskSeen_;
    if (sv.source != ScreenSource::Window) return;
    if (!asked && IsWindow((HWND)sv.hwnd) && !capture_.WindowGone()) return;
    windowAskSeen_ = sv.windowAsk;

    auto wins = ScreenCapture::Windows();
    int current = -1;
    for (int i = 0; i < (int)wins.size(); i++)
        if (wins[i].hwnd == sv.hwnd) current = i;

    int pick = -1;
    if (!sv.windowTitle.empty()) { // the window picked last time, if it's still open with the same title
        for (int i = 0; i < (int)wins.size() && pick < 0; i++)
            if (wins[i].title == sv.windowTitle) pick = i;
    }
    if (pick < 0 && !sv.windowMatch.empty()) {
        std::string match = Lower(sv.windowMatch);
        for (int i = 0; i < (int)wins.size() && pick < 0; i++) {
            if (Lower(wins[i].title).find(match) != std::string::npos || Lower(wins[i].exe).find(match) != std::string::npos) pick = i;
        }
        if (pick < 0) eng_.Toast("Screen: no open window matches '" + sv.windowMatch + "'", 3.0);
    } else if (pick < 0 && sv.windowMatch.empty() && !wins.empty()) {
        int n = (int)wins.size();
        int step = sv.windowStep;
        pick = current < 0 ? (step < 0 ? n - 1 : 0) : ((current + step) % n + n) % n;
    }
    sv.windowStep = 0;
    sv.windowMatch.clear();
    sv.windowTitle.clear();

    if (pick < 0) { // nothing to show: back to the monitor
        if (wins.empty()) eng_.Toast("Screen: no window to show", 2.5);
        sv.source = ScreenSource::Monitor;
        sv.hwnd = 0;
        return;
    }
    const auto& w = wins[pick];
    bool changed = w.hwnd != sv.hwnd;
    sv.hwnd = w.hwnd;
    if (asked || changed) eng_.Toast("Screen: " + Shorten(w.title, 48), 2.0);
    Log("screen window: %s (%s)", w.title.c_str(), w.exe.c_str());
    pickedExe_ = WithoutExtension(Lower(w.exe));
    pickedTitle_ = w.title;
}

// The picker's list: monitors first, then open windows.
void OverlayRenderer::FillScreenChoices(ScreenView& sv) {
    std::vector<ScreenChoice> choices;
    auto monitors = ScreenCapture::Monitors();
    int currentMonitor = sv.monitor == 0 ? sv.primaryIndex : sv.monitor;
    for (int i = 0; i < (int)monitors.size(); i++) {
        ScreenChoice c;
        c.monitor = i + 1;
        c.label = "Monitor " + std::to_string(i + 1) + (monitors[i].primary ? " (main)" : "") + "\n" + std::to_string(monitors[i].w) +
                  " x " + std::to_string(monitors[i].h);
        c.current = sv.source != ScreenSource::Window && currentMonitor == i + 1;
        choices.push_back(c);
    }
    for (auto& w : ScreenCapture::Windows()) {
        ScreenChoice c;
        c.hwnd = w.hwnd;
        c.label = WithoutExtension(w.exe) + "\n" + Shorten(w.title, 60);
        c.current = sv.source == ScreenSource::Window && sv.hwnd == w.hwnd;
        choices.push_back(c);
    }
    sv.choices = std::move(choices);
    sv.choicesVersion++;
}

void OverlayRenderer::RenderScreen(const OverlayContext& c, LayerList& layers) {
    ScreenView& sv = eng_.screen;
    if (!sv.shown || !c.local) {
        if (capture_.Running() || screen_.Exists()) StopScreen(c);
        return;
    }
    const ScreenStyle& style = eng_.prof->screen;

    UpdateScreenSource(sv);
    // Picked in the headset: that monitor / window is the default from now on.
    if (sv.choiceChanged && sv.windowAsk == windowAskSeen_) {
        sv.choiceChanged = false;
        ScreenStyle& saved = eng_.prof->screen;
        saved.source = sv.source;
        if (sv.source == ScreenSource::Window) {
            saved.window = pickedExe_;
            saved.windowTitle = pickedTitle_;
        } else {
            saved.monitor = sv.monitor;
        }
        std::string err;
        if (SaveScreenChoice(*eng_.prof, err)) {
            if (onProfileSaved) onProfileSaved();
            Log("screen: remembered %s",
                sv.source == ScreenSource::Window ? pickedTitle_.c_str() : ("monitor " + std::to_string(sv.monitor)).c_str());
        } else {
            Log("screen: could not remember the choice: %s", err.c_str());
        }
    }
    if (sv.source != ScreenSource::Window) sv.hwnd = 0;
    if (sv.wantChoices) {
        sv.wantChoices = false;
        FillScreenChoices(sv);
    }

    // capture: configured every frame, frames arrive from its own thread
    int maxWidth = style.maxPx > 0 ? style.maxPx : (sizeof(void*) == 4 ? 1280 : 1920); // 32-bit games are short on memory
    capture_.Configure(sv.monitor, sv.hwnd, sv.fps, maxWidth, style.cursor, style.opacity);
    int monitors, primary, shown;
    std::string message;
    uint64_t messageVersion;
    capture_.Status(monitors, primary, shown, message, messageVersion);
    if (monitors > 0) {
        sv.monitorCount = monitors;
        sv.primaryIndex = primary;
    }
    capture_.Rect(sv.monX, sv.monY, sv.monW, sv.monH);
    if (screen_.Width() > 0) sv.aspect = (float)screen_.Height() / screen_.Width();
    if (messageVersion != captureMessageVersion_) {
        captureMessageVersion_ = messageVersion;
        if (message.find("can't") != std::string::npos || message.find("no monitor") != std::string::npos)
            eng_.Toast("Screen: " + message, 4.0);
    }
    int fw, fh;
    if (capture_.TakeFrame(screenPixels_, fw, fh) && screen_.Ensure(c, fw, fh)) screen_.Raw().Upload(*c.gfx, *c.swap, screenPixels_);

    // a quad shows from both sides: hide the screen and its bars when the head is behind it
    bool seenFromFront = true;
    if (headValid_) {
        Vec3 facing = Rotate(sv.pose.orientation, { 0, 0, 1 });
        seenFromFront = Dot(facing, V(head_.position) - V(sv.pose.position)) > 0.f;
    }
    if (!sv.poseValid || !seenFromFront) return;
    if (screen_.Width() > 0) screen_.Submit(layers, c.local, sv.pose, { sv.widthM, sv.widthM * screen_.Height() / screen_.Width() });

    const Style& st = eng_.prof->style;
    if (sv.barVisible) {
        int h = (int)(kBarPx * sv.barH / sv.barW);
        if (screenBar_.Ensure(c, kBarPx, h)) {
            screenBar_.Redraw(c, sv.uiVersion, scratch_, [&](std::vector<uint32_t>& px) {
                DrawList(px, kBarPx, h, "", sv.barLabels, (int)sv.barLabels.size(), sv.barHover, sv.barPressed, st);
            });
        }
        screenBar_.Submit(layers, c.local, sv.barPose, { sv.barW, sv.barH });
    }
    if (sv.pickOpen) {
        int h = (int)(kPickerPx * sv.pickH / sv.pickW);
        if (screenPicker_.Ensure(c, kPickerPx, h)) {
            screenPicker_.Redraw(c, sv.uiVersion, scratch_, [&](std::vector<uint32_t>& px) {
                DrawList(px, kPickerPx, h, sv.pickTitle, sv.pickLabels, sv.pickCols, sv.pickHover, sv.pickPressed, st);
            });
        }
        screenPicker_.Submit(layers, c.local, sv.pickPose, { sv.pickW, sv.pickH });
    }
}

// ---------------------------------------------------------------- wheel, keyboard, wrist panels

void OverlayRenderer::RenderWheel(const OverlayContext& c, LayerList& layers) {
    const WheelView& v = eng_.view;
    if (!v.visible || !c.local) return;
    const Profile& p = *eng_.prof;
    int size = p.style.texSize;
    if (!wheel_.Ensure(c, size, size)) return;
    wheel_.Redraw(c, v.version, scratch_, [&](std::vector<uint32_t>& px) {
        DrawWheel(px, size, p, v);
        if (v.alpha < 0.999f) FadePixels(px, v.alpha); // see-through while placing other things
    });
    wheel_.Submit(layers, c.local, v.pose, { v.size, v.size });
}

void OverlayRenderer::RenderKeyboard(const OverlayContext& c, LayerList& layers) {
    const KeyboardView& kb = eng_.kb;
    if (!kb.visible || !c.local) return;
    const Style& st = eng_.prof->style;

    float unitsW, unitsH;
    KbLayout(unitsW, unitsH);
    const int h = (int)(kKeyboardPx * unitsH / unitsW);
    if (keyboard_.Ensure(c, kKeyboardPx, h))
        keyboard_.Redraw(c, kb.version, scratch_, [&](std::vector<uint32_t>& px) { DrawKeyboard(px, kKeyboardPx, h, kb, st); });
    keyboard_.Submit(layers, c.local, kb.pose, { kb.widthM, kb.heightM });

    // the docking bar under it
    const int barH = std::max(40, (int)(kBarPx * kb.barH / kb.barW));
    if (keyboardBar_.Ensure(c, kBarPx, barH) && !kb.barLabels.empty()) {
        keyboardBar_.Redraw(c, kb.barVersion, scratch_, [&](std::vector<uint32_t>& px) {
            DrawList(px, kBarPx, barH, "", kb.barLabels, (int)kb.barLabels.size(), kb.barHover, kb.barPressed, st);
        });
    }
    keyboardBar_.Submit(layers, c.local, kb.barPose, { kb.barW, kb.barH });
}

void OverlayRenderer::RenderWrists(const OverlayContext& c, LayerList& layers) {
    if (!c.local) return;
    const Profile& p = *eng_.prof;
    for (int h = 0; h < 2; h++) {
        const WristView& wv = eng_.wrist[h];
        if (!wv.visible) continue;
        if (wrist_[h].Ensure(c, kWristPx, kWristPx))
            wrist_[h].Redraw(c, wv.version, scratch_, [&](std::vector<uint32_t>& px) { DrawWrist(px, kWristPx, p, p.wrist[h], wv); });
        wrist_[h].Submit(layers, c.local, wv.pose, { wv.sizeM, wv.sizeM });

        // touch cursor on top of the panel: redrawn when its look changes, moved every frame
        if (!wrist_[h].Ready() || !wv.cursor) continue;
        if (cursor_[h].Ensure(c, kCursorPx, kCursorPx)) {
            cursor_[h].Redraw(c, (uint64_t)wv.cursorLevel, scratch_,
                              [&](std::vector<uint32_t>& px) { DrawCursor(px, kCursorPx, wv.cursorLevel, p.style); });
        }
        cursor_[h].Submit(layers, c.local, wv.cursorPose, { wv.cursorSize, wv.cursorSize });
    }
}

void OverlayRenderer::RenderOffsets(const OverlayContext& c, LayerList& layers) {
    const OffsetView& ov = eng_.offView;
    if (!ov.open || !c.local) {
        if (offsets_.Exists()) offsets_.Destroy(c);
        return;
    }
    const int h = (int)(kOffsetsPx * ov.h / ov.w);
    if (offsets_.Ensure(c, kOffsetsPx, h)) {
        offsets_.Redraw(c, ov.version, scratch_, [&](std::vector<uint32_t>& px) {
            DrawList(px, kOffsetsPx, h, ov.title, ov.labels, ov.cols, ov.hover, ov.pressed, eng_.prof->style);
        });
    }
    offsets_.Submit(layers, c.local, ov.pose, { ov.w, ov.h });
}

// ---------------------------------------------------------------- pointers, fingertips, toasts

// Laser pointers, on top of everything: a beam from the controller and a dot where it hits.
void OverlayRenderer::RenderPointers(const OverlayContext& c, LayerList& layers) {
    if (!c.local) return;
    const Style& st = eng_.prof->style;
    const uint64_t kDrawnOnce = 1; // the pictures never change; they are redrawn only after InvalidateAll

    if (eng_.pointer[0].on || eng_.pointer[1].on) {
        if (pointerDot_.Ensure(c, kDotPx, kDotPx))
            pointerDot_.Redraw(c, kDrawnOnce, scratch_, [&](std::vector<uint32_t>& px) { DrawPointerDot(px, kDotPx, st); });
        if (pointerBeam_.Ensure(c, kBeamW, kBeamH))
            pointerBeam_.Redraw(c, kDrawnOnce, scratch_, [&](std::vector<uint32_t>& px) { DrawBeam(px, kBeamW, kBeamH, st); });
        for (int h = 0; h < 2; h++) {
            const PointerView& p = eng_.pointer[h];
            if (!p.on || !pointerDot_.Ready() || !pointerBeam_.Ready()) continue;
            pointerBeam_.SubmitAs(beamQuad_[h], layers, c.local, p.beamPose, { kBeamWidthM, p.beamLen });
            if (p.dot) pointerDot_.SubmitAs(dotQuad_[h], layers, c.local, p.dotPose, { p.dotSize, p.dotSize });
        }
    }
    // fingertip silhouettes near wrist panels
    if (eng_.pointer[0].finger || eng_.pointer[1].finger) {
        if (finger_.Ensure(c, kFingerW, kFingerH))
            finger_.Redraw(c, kDrawnOnce, scratch_, [&](std::vector<uint32_t>& px) { DrawFinger(px, kFingerW, kFingerH, st); });
        for (int h = 0; h < 2; h++) {
            const PointerView& p = eng_.pointer[h];
            if (p.finger) finger_.SubmitAs(fingerQuad_[h], layers, c.local, p.fingerPose, { p.fingerW, p.fingerLen });
        }
    }
}

void OverlayRenderer::RenderToast(const OverlayContext& c, LayerList& layers) {
    if (NowSec() >= eng_.toastUntil || !c.view) return;
    if (toast_.Ensure(c, kToastW, kToastH)) {
        toast_.Redraw(c, eng_.toastVersion, scratch_,
                      [&](std::vector<uint32_t>& px) { DrawToast(px, kToastW, kToastH, eng_.toast, eng_.prof->style); });
    }
    toast_.Submit(layers, c.view, kToastPose, kToastSize);
}
