// In-game VR keyboard: point with either controller, trigger to type.
#include "engine.h"
#include "keys.h"
#include <algorithm>

const std::vector<KbKey>& KbLayout(float& W, float& Hh) {
    static std::vector<KbKey> keys;
    W = 15.f;
    Hh = 6.f;
    if (!keys.empty()) return keys;
    float x = 0, y = 0;
    auto add = [&](std::string label, std::string shiftLabel, std::string combo, float w, int kind = KB_KEY, bool letter = false,
                   bool repeat = false) {
        if (shiftLabel.empty()) shiftLabel = letter ? std::string(1, (char)toupper(label[0])) : label;
        keys.push_back({ label, shiftLabel, combo, x, y, w, 1.f, kind, letter, repeat });
        x += w;
    };
    auto row = [&] {
        x = 0;
        y += 1;
    };
    for (int i = 1; i <= 12; i++)
        add("F" + std::to_string(i), "", "F" + std::to_string(i), 1.25f); // function keys
    row();
    add("Esc", "", "Esc", 1);
    const std::string digits = "1234567890", shifted = "!@#$%^&*()";
    for (int i = 0; i < 10; i++)
        add(std::string(1, digits[i]), std::string(1, shifted[i]), std::string(1, digits[i]), 1);
    add("-", "_", "minus", 1);
    add("=", "+", "equals", 1);
    add("← Bksp", "", "Backspace", 2, KB_KEY, false, true);
    row();
    add("Tab", "", "Tab", 1.5f);
    for (const char* c : { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p" })
        add(c, "", c, 1, KB_KEY, true);
    add("[", "{", "lbracket", 1);
    add("]", "}", "rbracket", 1);
    add("\\", "|", "backslash", 1.5f);
    row();
    add("Caps", "", "", 1.75f, KB_CAPS);
    for (const char* c : { "a", "s", "d", "f", "g", "h", "j", "k", "l" })
        add(c, "", c, 1, KB_KEY, true);
    add(";", ":", "semicolon", 1);
    add("'", "\"", "quote", 1);
    add("Enter", "", "Enter", 2.25f);
    row();
    add("Shift", "", "", 2.25f, KB_SHIFT);
    for (const char* c : { "z", "x", "c", "v", "b", "n", "m" })
        add(c, "", c, 1, KB_KEY, true);
    add(",", "<", "comma", 1);
    add(".", ">", "period", 1);
    add("/", "?", "slash", 1);
    add("Shift", "", "", 2.75f, KB_SHIFT);
    row();
    add("Ctrl", "", "", 1.5f, KB_CTRL);
    add("Alt", "", "", 1.5f, KB_ALT);
    add("Space", "", "Space", 3.5f, KB_KEY, false, true);
    add("Del", "", "Delete", 1, KB_KEY, false, true);
    add("←", "", "Left", 1, KB_KEY, false, true);
    add("↑", "", "Up", 1, KB_KEY, false, true);
    add("↓", "", "Down", 1, KB_KEY, false, true);
    add("→", "", "Right", 1, KB_KEY, false, true);
    add("Move", "", "", 1.5f, KB_MOVE);
    add("Close", "", "", 2, KB_CLOSE);
    return keys;
}

void Engine::OpenKeyboard(int bi) {
    if (kb.visible) {
        CloseKeyboard();
        return;
    } // opener again = close
    if (wr.open) CloseWheel();
    float uw, uh;
    KbLayout(uw, uh);
    kb = KeyboardView{};
    kb.visible = true;
    kb.widthM = prof->kb.width;
    kb.heightM = prof->kb.width * uh / uw;
    Vec3 head = V(fr.head.position);
    kbRun.frame = YawFrame(fr.head);
    if (prof->kb.pose.set)
        kb.pose = PMul(kbRun.frame, prof->kb.pose.p); // placed by hand in adjust mode
    else {
        Vec3 f = Rotate(fr.head.orientation, { 0, 0, -1 });
        f.y = 0;
        f = Norm(f);
        Vec3 c = head + f * prof->kb.distance - Vec3{ 0, prof->kb.below, 0 };
        kb.pose.position = X(c);
        kb.pose.orientation = BillboardTo(c, head); // faces the eyes, so it tilts up like a desk
    }
    kbRun.openTime = now;
    kbRun.opener = bi;
    kbRun.repeatKey = -1;
    Log("keyboard open");
    kb.version++;
}

void Engine::CloseKeyboard() {
    if (kb.moving) {
        kb.moving = false;
        if (grab.target == GrabTarget::Keyboard) grab = GrabRun{};
        saveRequested = true;
    }
    kb.visible = false;
    kb.version++;
    kbRun.repeatKey = kbRun.repeatHand = -1;
    Log("keyboard closed");
}

int Engine::KbHit(int h) const {
    const HandRaw& hr = fr.h[h];
    if (!hr.aimValid) return -1;
    Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
    Vec3 c = V(kb.pose.position), n = Rotate(kb.pose.orientation, { 0, 0, 1 });
    float den = Dot(d, n);
    if (std::fabs(den) < 1e-4f) return -1;
    float t = Dot(c - o, n) / den;
    if (t <= 0) return -1;
    Vec3 l = RotateInv(kb.pose.orientation, (o + d * t) - c);
    float uw, uh;
    const auto& keys = KbLayout(uw, uh);
    float u = (l.x / kb.widthM + 0.5f) * uw, v = (0.5f - l.y / kb.heightM) * uh;
    for (int i = 0; i < (int)keys.size(); i++) {
        const auto& k = keys[i];
        if (u >= k.x && u < k.x + k.w && v >= k.y && v < k.y + k.h) return i;
    }
    return -1;
}

void Engine::KbPress(int i, int hand) {
    float uw, uh;
    const auto& keys = KbLayout(uw, uh);
    const KbKey& k = keys[i];
    if (prof->haptics) haptics.push_back({ hand, std::min(1.f, prof->hapticAmp * 1.4f), 0.02f });
    kb.pressed = i;
    kbRun.pressFlashUntil = now + 0.12;
    switch (k.kind) {
    case KB_SHIFT:
        kb.shift = !kb.shift;
        break;
    case KB_CAPS:
        kb.caps = !kb.caps;
        break;
    case KB_CTRL:
        kb.ctrl = !kb.ctrl;
        break;
    case KB_ALT:
        kb.alt = !kb.alt;
        break;
    case KB_CLOSE:
        CloseKeyboard();
        return;
    case KB_MOVE:
        KbToggleMove();
        break;
    default: {
        std::string combo;
        if (kb.ctrl) combo += "Ctrl+";
        if (kb.alt) combo += "Alt+";
        bool upper = k.letter ? (kb.shift != kb.caps) : kb.shift;
        if (upper) combo += "Shift+";
        combo += k.combo;
        Keys::Tap(combo, 25);
        kb.shift = kb.ctrl = kb.alt = false; // one-shot modifiers
        if (k.combo == "Enter" && prof->kb.closeOnEnter) {
            CloseKeyboard();
            return;
        }
    }
    }
    kb.version++;
}

// Adjust mode: grab the keyboard with a grip and put it anywhere; the grabbing hand's stick changes its width.
void Engine::UpdateAdjustKeyboard() {
    bool placing = adjust || kb.moving;
    if (grab.target == GrabTarget::Keyboard && (!placing || !kb.visible)) grab = GrabRun{};
    if (!placing || !kb.visible || (grab.Active() && grab.target != GrabTarget::Keyboard)) return;
    if (prof->dock.mode == DockMode::KeysUnderScreen && screen.shown) return; // docked under the screen: Adjust dock places it
    if (!grab.Active()) {
        for (int h = 0; h < 2; h++) {
            const HandRaw& hr = fr.h[h];
            if (!hr.gripValid || !cur[h][C_GRIP][F_PRESS] || prev[h][C_GRIP][F_PRESS]) continue;
            Vec3 l = RotateInv(kb.pose.orientation, V(hr.gripPose.position) - V(kb.pose.position));
            if (std::fabs(l.x) < kb.widthM / 2 + 0.06f && std::fabs(l.y) < kb.heightM / 2 + 0.06f && std::fabs(l.z) < 0.12f) {
                StartGrab(GrabTarget::Keyboard, h, kb.pose);
                return;
            }
        }
        return;
    }
    const HandRaw& hr = fr.h[grab.hand];
    if (!cur[grab.hand][C_GRIP][F_PRESS] || !hr.gripValid) {
        if (prof->haptics) haptics.push_back({ grab.hand, prof->hapticAmp * 0.6f, 0.015f });
        grab = GrabRun{};
        return;
    }
    kb.pose = GrabbedPose();
    prof->kb.pose = { true, PMul(PInv(kbRun.frame), kb.pose) };
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    if (std::fabs(hr.sy) > 0.2f) {
        float uw, uh;
        KbLayout(uw, uh);
        prof->kb.width = std::clamp(prof->kb.width * (1.f + hr.sy * dt * 0.8f), 0.2f, 1.5f);
        kb.widthM = prof->kb.width;
        kb.heightM = kb.widthM * uh / uw;
    }
    kb.version++;
}

void Engine::UpdateKeyboard() {
    if (!kb.visible) return;
    if (adjust) { // being placed: no typing
        if (kb.hover[0] != -1 || kb.hover[1] != -1) {
            kb.hover[0] = kb.hover[1] = -1;
            kb.version++;
        }
        return;
    }
    for (int h = 0; h < 2; h++) {
        int hit = KbHit(h);
        if (hit != kb.hover[h]) {
            kb.hover[h] = hit;
            kb.version++;
            if (hit >= 0 && prof->haptics) haptics.push_back({ h, prof->hapticAmp * 0.5f, 0.008f });
        }
    }
    if (kb.pressed >= 0 && now > kbRun.pressFlashUntil) {
        kb.pressed = -1;
        kb.version++;
    }
    int c = prof->sel.cancelComp; // cancel button on either hand closes, any time
    for (int h = 0; h < 2; h++)
        if (c >= 0 && cur[h][c][F_PRESS] && !prev[h][c][F_PRESS]) {
            CloseKeyboard();
            return;
        }
    if (now - kbRun.openTime < 0.25) return; // ignore the press that opened it
    float uw, uh;
    const auto& keys = KbLayout(uw, uh);
    for (int h = 0; h < 2; h++) {
        bool down = cur[h][C_TRIGGER][F_PRESS], edge = down && !prev[h][C_TRIGGER][F_PRESS];
        if (edge && kb.hover[h] >= 0) {
            int k = kb.hover[h];
            if (kb.moving && keys[k].kind != KB_MOVE) continue; // being moved: only Done works
            KbPress(k, h);
            if (!kb.visible) return;
            if (keys[k].repeat) {
                kbRun.repeatKey = k;
                kbRun.repeatHand = h;
                kbRun.nextRepeat = now + 0.45;
            }
        }
        if (kbRun.repeatHand == h && (!down || kb.hover[h] != kbRun.repeatKey)) kbRun.repeatKey = kbRun.repeatHand = -1;
    }
    if (kbRun.repeatKey >= 0 && now >= kbRun.nextRepeat) {
        KbPress(kbRun.repeatKey, kbRun.repeatHand);
        kbRun.nextRepeat = now + 0.06;
    }
}

// ---------------------------------------------------------------- docking with the mirrored screen
float Engine::DockScreenWidth() const {
    if (prof->dock.mode == DockMode::ScreenOnKeys) return prof->dock.width > 0 ? prof->dock.width : kb.widthM;
    return screen.widthM;
}
// Default: the screen's bottom edge 3 cm above the keyboard's top edge, turned 25 degrees more upright than the keys.
XrPosef Engine::DockRel() const {
    if (prof->dock.pose.set) return prof->dock.pose.p;
    float a = 25.f * 3.14159265f / 180.f * 0.5f;
    XrQuaternionf q{ std::sin(a), 0, 0, std::cos(a) };
    float sh = DockScreenWidth() * screen.aspect;
    Vec3 p = Vec3{ 0, kb.heightM / 2 + 0.03f, 0 } + Rotate(q, { 0, sh / 2, 0 });
    return { q, X(p) };
}

void Engine::ApplyDock() {
    DockStyle& D = prof->dock;
    if (!kb.visible) {
        dockAdjust = false;
        if (grab.target == GrabTarget::Dock) grab = GrabRun{};
        return;
    }
    if (D.mode == DockMode::Apart || !screen.shown || !screen.poseValid) {
        if (grab.target == GrabTarget::Dock) grab = GrabRun{};
        return;
    }
    XrPosef rel = DockRel();
    if (D.mode == DockMode::ScreenOnKeys) {
        screen.pose = PMul(kb.pose, rel);
        screen.widthM = DockScreenWidth();
    } else
        kb.pose = PMul(screen.pose, PInv(rel));
    if (!dockAdjust || (grab.Active() && grab.target != GrabTarget::Dock)) return;

    // Adjust dock: grab the one that follows (the screen, or the keyboard) and put it where you like.
    if (!grab.Active()) {
        for (int h = 0; h < 2; h++) {
            const HandRaw& hr = fr.h[h];
            if (!hr.gripValid || !cur[h][C_GRIP][F_PRESS] || prev[h][C_GRIP][F_PRESS]) continue;
            Vec3 gp = V(hr.gripPose.position);
            bool isClose = false;
            if (D.mode == DockMode::ScreenOnKeys)
                isClose = Len(gp - V(screen.pose.position)) < screen.widthM * 0.6f + 0.08f;
            else {
                Vec3 l = RotateInv(kb.pose.orientation, gp - V(kb.pose.position));
                isClose = std::fabs(l.x) < kb.widthM / 2 + 0.06f && std::fabs(l.y) < kb.heightM / 2 + 0.06f && std::fabs(l.z) < 0.12f;
            }
            if (isClose) {
                StartGrab(GrabTarget::Dock, h, D.mode == DockMode::ScreenOnKeys ? screen.pose : kb.pose);
                return;
            }
        }
        return;
    }
    const HandRaw& hr = fr.h[grab.hand];
    if (!cur[grab.hand][C_GRIP][F_PRESS] || !hr.gripValid) {
        if (prof->haptics) haptics.push_back({ grab.hand, prof->hapticAmp * 0.6f, 0.015f });
        grab = GrabRun{};
        saveRequested = true;
        return;
    }
    XrPosef obj = GrabbedPose();
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    if (D.mode == DockMode::ScreenOnKeys) {
        if (std::fabs(hr.sy) > 0.25f) D.width = std::clamp(DockScreenWidth() * (1.f + hr.sy * dt * 0.8f), 0.1f, 3.f);
        screen.pose = obj;
        screen.widthM = DockScreenWidth();
        D.pose = { true, PMul(PInv(kb.pose), obj) };
    } else {
        kb.pose = obj;
        D.pose = { true, PMul(PInv(obj), screen.pose) };
    }
}

// Move key / bar button: grab the keyboard with a grip to place it, without Adjust mode.
void Engine::KbToggleMove() {
    if (!kb.moving && prof->dock.mode == DockMode::KeysUnderScreen && screen.shown) {
        Toast("Docked under the screen: use Adjust dock to place it", 3.0);
        return;
    }
    kb.moving = !kb.moving;
    if (kb.moving) {
        if (dockAdjust) {
            dockAdjust = false;
            if (grab.target == GrabTarget::Dock) grab = GrabRun{};
        }
        Toast("Grab the keyboard with a grip to move it, stick = size. Press Done when finished", 4.0);
    } else {
        if (grab.target == GrabTarget::Keyboard) grab = GrabRun{};
        saveRequested = true;
        Toast("Keyboard position saved", 2.0);
    }
    kb.version++;
}

// The bar under the keyboard: dock the screen above the keys, the keys under the screen, adjust the dock, close.
void Engine::UpdateKbBar() {
    if (!kb.visible) return;
    const DockStyle& D = prof->dock;
    std::vector<std::string> labels{ D.mode == DockMode::ScreenOnKeys ? "Undock\nscreen" : "Dock screen\nabove keys",
                                     D.mode == DockMode::KeysUnderScreen ? "Undock\nkeys" : "Dock keys\nunder screen",
                                     dockAdjust ? "Done\nadjusting" : "Adjust\ndock", kb.moving ? "Done\nadjusting" : "Adjust\nkeyboard",
                                     "Close\nkeyboard" };
    if (labels != kb.barLabels) {
        kb.barLabels = labels;
        kb.barVersion++;
    }
    kb.barW = kb.widthM * 0.9f;
    kb.barH = kb.barW / 10.8f;
    Vec3 up = Rotate(kb.pose.orientation, { 0, 1, 0 }), nrm = Rotate(kb.pose.orientation, { 0, 0, 1 });
    kb.barPose = { kb.pose.orientation, X(V(kb.pose.position) - up * (kb.heightM / 2 + kb.barH / 2 + 0.012f) + nrm * 0.002f) };
    int hover = -1, hand = -1;
    for (int h = 0; h < 2 && hover < 0; h++) {
        float u, v;
        if (!PanelRay(h, kb.barPose, kb.barW, kb.barH, u, v)) continue;
        int n = (int)kb.barLabels.size();
        int c = (int)(u * n);
        if (c >= 0 && c < n && v >= 0 && v <= 1) {
            hover = c;
            hand = h;
        }
    }
    if (hover != kb.barHover) {
        if (hover >= 0 && prof->haptics) haptics.push_back({ hand, prof->hapticAmp * 0.4f, 0.01f });
        kb.barHover = hover;
        kb.barVersion++;
    }
    if (kb.barPressed >= 0 && now > kbRun.barFlashUntil) {
        kb.barPressed = -1;
        kb.barVersion++;
    }
    if (hand < 0 || !(cur[hand][C_TRIGGER][F_PRESS] && !prev[hand][C_TRIGGER][F_PRESS])) return;
    if (prof->haptics) haptics.push_back({ hand, prof->hapticAmp, 0.025f });
    kb.barPressed = hover;
    kbRun.barFlashUntil = now + 0.15;
    kb.barVersion++;
    KbBarActivate((KbBarButton)hover);
}

void Engine::KbBarActivate(KbBarButton button) {
    DockStyle& D = prof->dock;
    auto screenOn = [&]() {
        if (!screen.on) {
            Action a;
            a.type = ActType::Screen;
            a.mode = ActMode::On;
            ScreenAction(a);
        }
    };
    switch (button) {
    case KbBarButton::DockScreen:
        D.mode = D.mode == DockMode::ScreenOnKeys ? DockMode::Apart : DockMode::ScreenOnKeys;
        if (D.mode != DockMode::Apart) screenOn();
        dockAdjust = false;
        saveRequested = true;
        Toast(D.mode != DockMode::Apart ? "Screen docked above the keyboard" : "Screen undocked", 2.0);
        break;
    case KbBarButton::DockKeys:
        D.mode = D.mode == DockMode::KeysUnderScreen ? DockMode::Apart : DockMode::KeysUnderScreen;
        if (D.mode != DockMode::Apart) screenOn();
        dockAdjust = false;
        saveRequested = true;
        Toast(D.mode != DockMode::Apart ? "Keyboard docked under the screen" : "Keyboard undocked", 2.0);
        break;
    case KbBarButton::AdjustDock:
        if (D.mode == DockMode::Apart) {
            Toast("Dock the screen or the keys first", 2.5);
            break;
        }
        dockAdjust = !dockAdjust;
        if (dockAdjust && kb.moving) {
            kb.moving = false;
            if (grab.target == GrabTarget::Keyboard) grab = GrabRun{};
        }
        if (dockAdjust)
            Toast(D.mode == DockMode::ScreenOnKeys ? "Grab the screen with a grip to place it (stick: size)"
                                                   : "Grab the keyboard with a grip to place it",
                  4.0);
        else {
            saveRequested = true;
            Toast("Dock saved", 2.0);
        }
        break;
    case KbBarButton::AdjustKeyboard:
        KbToggleMove();
        break;
    case KbBarButton::Close:
        CloseKeyboard();
        break;
    }
}
