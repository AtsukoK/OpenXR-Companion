// In-game VR keyboard: point with either controller, trigger to type.
#include "engine.h"
#include "keys.h"
#include <algorithm>

static void BuildLayout(std::vector<KbKey>& keys, bool forBinding);

const std::vector<KbKey>& KbLayout(float& W, float& Hh, bool forBinding) {
    static std::vector<KbKey> typing, binding;
    W = 15.f;
    Hh = forBinding ? 8.f : 6.f;
    auto& keys = forBinding ? binding : typing;
    if (keys.empty()) BuildLayout(keys, forBinding);
    return keys;
}

static void BuildLayout(std::vector<KbKey>& keys, bool forBinding) {
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
    if (!forBinding) return;
    // Picking what a binding does: the mouse, more keys, and a button the game should see instead.
    row();
    add("Left click", "", "lmb", 1.75f);
    add("Right click", "", "rmb", 1.75f);
    add("Middle", "", "mmb", 1.5f);
    add("Mouse 4", "", "mouse4", 1.5f);
    add("Mouse 5", "", "mouse5", 1.5f);
    add("Wheel \u2191", "", "wheelup", 1.5f);
    add("Wheel \u2193", "", "wheeldown", 1.5f);
    add("Controller button", "", "", 4, KB_CONTROLLER);
    row();
    add("Home", "", "Home", 1.5f);
    add("End", "", "End", 1.5f);
    add("Page \u2191", "", "PageUp", 1.5f);
    add("Page \u2193", "", "PageDown", 1.5f);
    add("Insert", "", "Insert", 1.5f);
    add("Print", "", "PrintScreen", 1.5f);
    add("Pause", "", "Pause", 1.5f);
    add("Win", "", "LWin", 1.5f);
    add("Menu key", "", "Apps", 3);
}

void VrKeyboard::Open(int bi) {
    if (e.kb.visible) {
        Close();
        return;
    } // opener again = close
    if (e.wr.open) e.CloseWheel();
    float uw, uh;
    KbLayout(uw, uh, e.kb.binding);
    e.kb = KeyboardView{};
    e.kb.visible = true;
    e.kb.widthM = e.prof->kb.width;
    e.kb.heightM = e.prof->kb.width * uh / uw;
    Vec3 head = V(e.fr.head.position);
    run_.frame = YawFrame(e.fr.head);
    if (e.prof->kb.pose.set)
        e.kb.pose = PMul(run_.frame, e.prof->kb.pose.p); // placed by hand in adjust mode
    else {
        Vec3 f = Rotate(e.fr.head.orientation, { 0, 0, -1 });
        f.y = 0;
        f = Norm(f);
        Vec3 c = head + f * e.prof->kb.distance - Vec3{ 0, e.prof->kb.below, 0 };
        e.kb.pose.position = X(c);
        e.kb.pose.orientation = BillboardTo(c, head); // faces the eyes, so it tilts up like a desk
    }
    run_.openTime = e.now;
    run_.opener = bi;
    run_.repeatKey = -1;
    Log("keyboard open");
    e.kb.version++;
}

void VrKeyboard::CaptureNextKey(std::function<void(const std::string&)> done, std::function<void()> controllerButton) {
    if (!e.kb.visible) Open(-1);
    run_.capture = std::move(done);
    run_.captureController = std::move(controllerButton);
    e.kb.binding = true; // the binding layout: taller, with the mouse and "Controller button"
    float uw, uh;
    KbLayout(uw, uh, true);
    e.kb.heightM = e.kb.widthM * uh / uw;
    if (e.menuView.open) LayOnMenu();
    e.kb.hover[0] = e.kb.hover[1] = -1;
    e.kb.version++;
    e.Toast(run_.captureController ? "Pick a key, a mouse button, or Controller button" : "Press the key you want on the keyboard", 3.0);
}

void VrKeyboard::LayOnMenu() {
    float uw, uh;
    KbLayout(uw, uh, e.kb.binding);
    MenuView& m = e.menuView;
    float w = std::min(m.w * 0.94f, m.h * 0.8f * uw / uh);
    e.kb.widthM = w;
    e.kb.heightM = w * uh / uw;
    Vec3 nrm = Rotate(m.pose.orientation, { 0, 0, 1 }), up = Rotate(m.pose.orientation, { 0, 1, 0 });
    float shift = -m.h * 0.04f; // a little below the middle, under the title
    e.kb.pose = { m.pose.orientation, X(V(m.pose.position) + nrm * (0.006f * m.scale) + up * shift) };
    e.kb.inMenu = true;
    e.kb.barLabels.clear();
    e.kb.barVersion++;
    m.covered = true;
    m.version++;
}

void VrKeyboard::CaptureText(std::function<void(int, const std::string&)> onKey) {
    if (!e.kb.visible) Open(-1);
    run_.capture = nullptr;
    run_.captureController = nullptr;
    run_.text = std::move(onKey);
    e.kb.binding = false; // the typing layout
    float uw, uh;
    KbLayout(uw, uh, false);
    e.kb.heightM = e.kb.widthM * uh / uw;
    if (e.menuView.open) {
        LayOnMenu();
        e.menuView.covered = false; // the name being typed shows on the menu above the keys
        e.menuView.version++;
    }
    e.kb.hover[0] = e.kb.hover[1] = -1;
    e.kb.version++;
    e.Toast("Type the name, then Enter", 3.0);
}

void VrKeyboard::Close() {
    if (auto text = std::move(run_.text)) {
        run_.text = nullptr;
        text(3, "");
    }
    run_.capture = nullptr;
    run_.captureController = nullptr;
    e.kb.binding = false;
    if (e.kb.inMenu) {
        e.kb.inMenu = false;
        e.menuView.covered = false;
        e.menuView.version++;
    }
    if (e.kb.moving) {
        e.kb.moving = false;
        if (e.grab.target == GrabTarget::Keyboard) e.grab = Engine::GrabRun{};
        e.saveRequested = true;
    }
    e.kb.visible = false;
    e.kb.version++;
    run_.repeatKey = run_.repeatHand = -1;
    Log("keyboard closed");
}

int VrKeyboard::KeyAt(int h) const {
    const HandRaw& hr = e.fr.h[h];
    if (!hr.aimValid) return -1;
    Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
    Vec3 c = V(e.kb.pose.position), n = Rotate(e.kb.pose.orientation, { 0, 0, 1 });
    float den = Dot(d, n);
    if (std::fabs(den) < 1e-4f) return -1;
    float t = Dot(c - o, n) / den;
    if (t <= 0) return -1;
    Vec3 l = RotateInv(e.kb.pose.orientation, (o + d * t) - c);
    float uw, uh;
    const auto& keys = KbLayout(uw, uh, e.kb.binding);
    float u = (l.x / e.kb.widthM + 0.5f) * uw, v = (0.5f - l.y / e.kb.heightM) * uh;
    for (int i = 0; i < (int)keys.size(); i++) {
        const auto& k = keys[i];
        if (u >= k.x && u < k.x + k.w && v >= k.y && v < k.y + k.h) return i;
    }
    return -1;
}

void VrKeyboard::Press(int i, int hand) {
    float uw, uh;
    const auto& keys = KbLayout(uw, uh, e.kb.binding);
    const KbKey& k = keys[i];
    if (e.prof->haptics) e.haptics.push_back({ hand, std::min(1.f, e.prof->hapticAmp * 1.4f), 0.02f });
    e.kb.pressed = i;
    run_.pressFlashUntil = e.now + 0.12;
    switch (k.kind) {
    case KB_SHIFT:
        e.kb.shift = !e.kb.shift;
        break;
    case KB_CAPS:
        e.kb.caps = !e.kb.caps;
        break;
    case KB_CTRL:
        e.kb.ctrl = !e.kb.ctrl;
        break;
    case KB_ALT:
        e.kb.alt = !e.kb.alt;
        break;
    case KB_CLOSE:
        Close();
        return;
    case KB_MOVE:
        ToggleMove();
        break;
    case KB_CONTROLLER: { // picking a binding: a controller button the game should see, picked by pressing it
        auto pick = std::move(run_.captureController);
        if (!pick) break;
        run_.capture = nullptr;
        run_.captureController = nullptr;
        e.kb.shift = e.kb.ctrl = e.kb.alt = false;
        Close();
        pick();
        return;
    }
    default: {
        std::string combo;
        if (e.kb.ctrl) combo += "Ctrl+";
        if (e.kb.alt) combo += "Alt+";
        bool upper = k.letter ? (e.kb.shift != e.kb.caps) : e.kb.shift;
        if (upper) combo += "Shift+";
        combo += k.combo;
        if (run_.text) { // typing a name for the menu
            bool upperCase = k.letter ? (e.kb.shift != e.kb.caps) : e.kb.shift;
            std::string ch = k.combo == "Space" ? " " : (upperCase ? k.shiftLabel : k.label);
            int kind = k.combo == "Backspace" ? 1 : k.combo == "Enter" ? 2 : 0;
            bool printable = kind != 0 || k.combo == "Space" || (ch.size() == 1 && (unsigned char)ch[0] >= 32);
            e.kb.shift = false;
            e.kb.version++;
            if (!printable) break; // Esc, Tab, F-keys: nothing to type
            if (kind == 2) { // Enter: done, the keyboard goes
                auto text = std::move(run_.text);
                run_.text = nullptr;
                Close();
                text(2, "");
                return;
            }
            auto text = run_.text;
            text(kind, ch);
            return;
        }
        if (run_.capture) { // asked for by the menu: hand it over instead of typing it
            auto done = std::move(run_.capture);
            run_.capture = nullptr;
            run_.captureController = nullptr;
            e.kb.shift = e.kb.ctrl = e.kb.alt = false;
            Close();
            done(combo);
            return;
        }
        Keys::Tap(combo, 25);
        e.kb.shift = e.kb.ctrl = e.kb.alt = false; // one-shot modifiers
        if (k.combo == "Enter" && e.prof->kb.closeOnEnter) {
            Close();
            return;
        }
    }
    }
    e.kb.version++;
}

// Adjust mode: grab the keyboard with a grip and put it anywhere; the grabbing hand's stick changes its width.
void VrKeyboard::UpdatePlacing() {
    if (e.kb.inMenu) return; // the menu's keyboard stays on the menu
    bool placing = e.adjust || e.kb.moving;
    if (e.grab.target == GrabTarget::Keyboard && (!placing || !e.kb.visible)) e.grab = Engine::GrabRun{};
    if (!placing || !e.kb.visible || (e.grab.Active() && e.grab.target != GrabTarget::Keyboard)) return;
    if (e.prof->dock.mode == DockMode::KeysUnderScreen && e.screen.shown) return; // docked under the screen: Adjust dock places it
    if (!e.grab.Active()) {
        for (int h = 0; h < 2; h++) {
            const HandRaw& hr = e.fr.h[h];
            if (!hr.gripValid || !e.cur[h][C_GRIP][F_PRESS] || e.prev[h][C_GRIP][F_PRESS]) continue;
            Vec3 l = RotateInv(e.kb.pose.orientation, V(hr.gripPose.position) - V(e.kb.pose.position));
            if (std::fabs(l.x) < e.kb.widthM / 2 + 0.06f && std::fabs(l.y) < e.kb.heightM / 2 + 0.06f && std::fabs(l.z) < 0.12f) {
                e.StartGrab(GrabTarget::Keyboard, h, e.kb.pose);
                return;
            }
        }
        return;
    }
    const HandRaw& hr = e.fr.h[e.grab.hand];
    if (!e.cur[e.grab.hand][C_GRIP][F_PRESS] || !hr.gripValid) {
        if (e.prof->haptics) e.haptics.push_back({ e.grab.hand, e.prof->hapticAmp * 0.6f, 0.015f });
        e.grab = Engine::GrabRun{};
        return;
    }
    e.kb.pose = e.GrabbedPose();
    e.prof->kb.pose = { true, PMul(PInv(run_.frame), e.kb.pose) };
    float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.1);
    if (std::fabs(hr.sy) > 0.2f) {
        float uw, uh;
        KbLayout(uw, uh, e.kb.binding);
        e.prof->kb.width = std::clamp(e.prof->kb.width * (1.f + hr.sy * dt * 0.8f), 0.2f, 1.5f);
        e.kb.widthM = e.prof->kb.width;
        e.kb.heightM = e.kb.widthM * uh / uw;
    }
    e.kb.version++;
}

void VrKeyboard::Update() {
    if (!e.kb.visible) return;
    if (e.adjust) { // being placed: no typing
        if (e.kb.hover[0] != -1 || e.kb.hover[1] != -1) {
            e.kb.hover[0] = e.kb.hover[1] = -1;
            e.kb.version++;
        }
        return;
    }
    for (int h = 0; h < 2; h++) {
        int hit = KeyAt(h);
        if (hit != e.kb.hover[h]) {
            e.kb.hover[h] = hit;
            e.kb.version++;
            if (hit >= 0 && e.prof->haptics) e.haptics.push_back({ h, e.prof->hapticAmp * 0.5f, 0.008f });
        }
    }
    if (e.kb.pressed >= 0 && e.now > run_.pressFlashUntil) {
        e.kb.pressed = -1;
        e.kb.version++;
    }
    int c = e.prof->sel.cancelComp; // cancel button on either hand closes, any time
    for (int h = 0; h < 2; h++)
        if (c >= 0 && e.cur[h][c][F_PRESS] && !e.prev[h][c][F_PRESS]) {
            Close();
            return;
        }
    if (e.now - run_.openTime < 0.25) return; // ignore the press that opened it
    float uw, uh;
    const auto& keys = KbLayout(uw, uh, e.kb.binding);
    for (int h = 0; h < 2; h++) {
        bool down = e.cur[h][C_TRIGGER][F_PRESS], edge = down && !e.prev[h][C_TRIGGER][F_PRESS];
        if (edge && e.kb.hover[h] >= 0) {
            int k = e.kb.hover[h];
            if (e.kb.moving && keys[k].kind != KB_MOVE) continue; // being moved: only Done works
            Press(k, h);
            if (!e.kb.visible) return;
            if (keys[k].repeat) {
                run_.repeatKey = k;
                run_.repeatHand = h;
                run_.nextRepeat = e.now + 0.45;
            }
        }
        if (run_.repeatHand == h && (!down || e.kb.hover[h] != run_.repeatKey)) run_.repeatKey = run_.repeatHand = -1;
    }
    if (run_.repeatKey >= 0 && e.now >= run_.nextRepeat) {
        Press(run_.repeatKey, run_.repeatHand);
        run_.nextRepeat = e.now + 0.06;
    }
}

// ---------------------------------------------------------------- docking with the mirrored screen
float VrKeyboard::DockScreenWidth() const {
    if (e.prof->dock.mode == DockMode::ScreenOnKeys) return e.prof->dock.width > 0 ? e.prof->dock.width : e.kb.widthM;
    return e.screen.widthM;
}
// Default: the screen's bottom edge 3 cm above the keyboard's top edge, turned 25 degrees more upright than the keys.
XrPosef VrKeyboard::DockRel() const {
    if (e.prof->dock.pose.set) return e.prof->dock.pose.p;
    float a = 25.f * 3.14159265f / 180.f * 0.5f;
    XrQuaternionf q{ std::sin(a), 0, 0, std::cos(a) };
    float sh = DockScreenWidth() * e.screen.aspect;
    Vec3 p = Vec3{ 0, e.kb.heightM / 2 + 0.03f, 0 } + Rotate(q, { 0, sh / 2, 0 });
    return { q, X(p) };
}

void VrKeyboard::ApplyDock() {
    if (e.kb.inMenu) return; // the menu's keyboard stays on the menu
    DockStyle& D = e.prof->dock;
    if (!e.kb.visible) {
        e.dockAdjust = false;
        if (e.grab.target == GrabTarget::Dock) e.grab = Engine::GrabRun{};
        return;
    }
    if (D.mode == DockMode::Apart || !e.screen.shown || !e.screen.poseValid) {
        if (e.grab.target == GrabTarget::Dock) e.grab = Engine::GrabRun{};
        return;
    }
    XrPosef rel = DockRel();
    if (D.mode == DockMode::ScreenOnKeys) {
        e.screen.pose = PMul(e.kb.pose, rel);
        e.screen.widthM = DockScreenWidth();
    } else
        e.kb.pose = PMul(e.screen.pose, PInv(rel));
    if (!e.dockAdjust || (e.grab.Active() && e.grab.target != GrabTarget::Dock)) return;

    // Adjust dock: grab the one that follows (the screen, or the keyboard) and put it where you like.
    if (!e.grab.Active()) {
        for (int h = 0; h < 2; h++) {
            const HandRaw& hr = e.fr.h[h];
            if (!hr.gripValid || !e.cur[h][C_GRIP][F_PRESS] || e.prev[h][C_GRIP][F_PRESS]) continue;
            Vec3 gp = V(hr.gripPose.position);
            bool isClose = false;
            if (D.mode == DockMode::ScreenOnKeys)
                isClose = Len(gp - V(e.screen.pose.position)) < e.screen.widthM * 0.6f + 0.08f;
            else {
                Vec3 l = RotateInv(e.kb.pose.orientation, gp - V(e.kb.pose.position));
                isClose = std::fabs(l.x) < e.kb.widthM / 2 + 0.06f && std::fabs(l.y) < e.kb.heightM / 2 + 0.06f && std::fabs(l.z) < 0.12f;
            }
            if (isClose) {
                e.StartGrab(GrabTarget::Dock, h, D.mode == DockMode::ScreenOnKeys ? e.screen.pose : e.kb.pose);
                return;
            }
        }
        return;
    }
    const HandRaw& hr = e.fr.h[e.grab.hand];
    if (!e.cur[e.grab.hand][C_GRIP][F_PRESS] || !hr.gripValid) {
        if (e.prof->haptics) e.haptics.push_back({ e.grab.hand, e.prof->hapticAmp * 0.6f, 0.015f });
        e.grab = Engine::GrabRun{};
        e.saveRequested = true;
        return;
    }
    XrPosef obj = e.GrabbedPose();
    float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.1);
    if (D.mode == DockMode::ScreenOnKeys) {
        if (std::fabs(hr.sy) > 0.25f) D.width = std::clamp(DockScreenWidth() * (1.f + hr.sy * dt * 0.8f), 0.1f, 3.f);
        e.screen.pose = obj;
        e.screen.widthM = DockScreenWidth();
        D.pose = { true, PMul(PInv(e.kb.pose), obj) };
    } else {
        e.kb.pose = obj;
        D.pose = { true, PMul(PInv(obj), e.screen.pose) };
    }
}

// Move key / bar button: grab the keyboard with a grip to place it, without Adjust mode.
void VrKeyboard::ToggleMove() {
    if (!e.kb.moving && e.prof->dock.mode == DockMode::KeysUnderScreen && e.screen.shown) {
        e.Toast("Docked under the screen: use Adjust dock to place it", 3.0);
        return;
    }
    e.kb.moving = !e.kb.moving;
    if (e.kb.moving) {
        if (e.dockAdjust) {
            e.dockAdjust = false;
            if (e.grab.target == GrabTarget::Dock) e.grab = Engine::GrabRun{};
        }
        e.Toast("Grab the keyboard with a grip to move it, stick = size. Press Done when finished", 4.0);
    } else {
        if (e.grab.target == GrabTarget::Keyboard) e.grab = Engine::GrabRun{};
        e.saveRequested = true;
        e.Toast("Keyboard position saved", 2.0);
    }
    e.kb.version++;
}

// The bar under the keyboard: dock the screen above the keys, the keys under the screen, adjust the dock, close.
void VrKeyboard::UpdateBar() {
    if (!e.kb.visible || e.kb.inMenu) return; // no docking bar on the menu's keyboard
    const DockStyle& D = e.prof->dock;
    std::vector<std::string> labels{ D.mode == DockMode::ScreenOnKeys ? "Undock\nscreen" : "Dock screen\nabove keys",
                                     D.mode == DockMode::KeysUnderScreen ? "Undock\nkeys" : "Dock keys\nunder screen",
                                     e.dockAdjust ? "Done\nadjusting" : "Adjust\ndock",
                                     e.kb.moving ? "Done\nadjusting" : "Adjust\nkeyboard", "Close\nkeyboard" };
    if (labels != e.kb.barLabels) {
        e.kb.barLabels = labels;
        e.kb.barVersion++;
    }
    e.kb.barW = e.kb.widthM * 0.9f;
    e.kb.barH = e.kb.barW / 10.8f;
    Vec3 up = Rotate(e.kb.pose.orientation, { 0, 1, 0 }), nrm = Rotate(e.kb.pose.orientation, { 0, 0, 1 });
    e.kb.barPose = { e.kb.pose.orientation, X(V(e.kb.pose.position) - up * (e.kb.heightM / 2 + e.kb.barH / 2 + 0.012f) + nrm * 0.002f) };
    int hover = -1, hand = -1;
    for (int h = 0; h < 2 && hover < 0; h++) {
        float u, v;
        if (!e.screenMirror.PanelRay(h, e.kb.barPose, e.kb.barW, e.kb.barH, u, v)) continue;
        int n = (int)e.kb.barLabels.size();
        int c = (int)(u * n);
        if (c >= 0 && c < n && v >= 0 && v <= 1) {
            hover = c;
            hand = h;
        }
    }
    if (hover != e.kb.barHover) {
        if (hover >= 0 && e.prof->haptics) e.haptics.push_back({ hand, e.prof->hapticAmp * 0.4f, 0.01f });
        e.kb.barHover = hover;
        e.kb.barVersion++;
    }
    if (e.kb.barPressed >= 0 && e.now > run_.barFlashUntil) {
        e.kb.barPressed = -1;
        e.kb.barVersion++;
    }
    if (hand < 0 || !(e.cur[hand][C_TRIGGER][F_PRESS] && !e.prev[hand][C_TRIGGER][F_PRESS])) return;
    if (e.prof->haptics) e.haptics.push_back({ hand, e.prof->hapticAmp, 0.025f });
    e.kb.barPressed = hover;
    run_.barFlashUntil = e.now + 0.15;
    e.kb.barVersion++;
    BarActivate((KbBarButton)hover);
}

void VrKeyboard::BarActivate(KbBarButton button) {
    DockStyle& D = e.prof->dock;
    auto screenOn = [&]() {
        if (!e.screen.on) {
            Action a;
            a.type = ActType::Screen;
            a.mode = ActMode::On;
            e.ScreenAction(a);
        }
    };
    switch (button) {
    case KbBarButton::DockScreen:
        D.mode = D.mode == DockMode::ScreenOnKeys ? DockMode::Apart : DockMode::ScreenOnKeys;
        if (D.mode != DockMode::Apart) screenOn();
        e.dockAdjust = false;
        e.saveRequested = true;
        e.Toast(D.mode != DockMode::Apart ? "Screen docked above the keyboard" : "Screen undocked", 2.0);
        break;
    case KbBarButton::DockKeys:
        D.mode = D.mode == DockMode::KeysUnderScreen ? DockMode::Apart : DockMode::KeysUnderScreen;
        if (D.mode != DockMode::Apart) screenOn();
        e.dockAdjust = false;
        e.saveRequested = true;
        e.Toast(D.mode != DockMode::Apart ? "Keyboard docked under the screen" : "Keyboard undocked", 2.0);
        break;
    case KbBarButton::AdjustDock:
        if (D.mode == DockMode::Apart) {
            e.Toast("Dock the screen or the keys first", 2.5);
            break;
        }
        e.dockAdjust = !e.dockAdjust;
        if (e.dockAdjust && e.kb.moving) {
            e.kb.moving = false;
            if (e.grab.target == GrabTarget::Keyboard) e.grab = Engine::GrabRun{};
        }
        if (e.dockAdjust)
            e.Toast(D.mode == DockMode::ScreenOnKeys ? "Grab the screen with a grip to place it (stick: size)"
                                                     : "Grab the keyboard with a grip to place it",
                    4.0);
        else {
            e.saveRequested = true;
            e.Toast("Dock saved", 2.0);
        }
        break;
    case KbBarButton::AdjustKeyboard:
        ToggleMove();
        break;
    case KbBarButton::Close:
        Close();
        break;
    }
}
