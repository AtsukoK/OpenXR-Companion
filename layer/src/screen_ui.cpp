// Mirrored desktop screen, engine side: where it is (wrist, in front, following the view), the switch
// actions, pointing at it to move and click the mouse, and the bar + picker shown under / in front of it.
// The layer captures the picture and draws it (overlay.cpp).
#include "engine.h"
#include "keys.h"
#include <algorithm>

// ------------------------------------------------------------ mirrored screen
static const char* PlaceName(const std::string& p) {
    return p == "wrist_left" ? "left wrist" : p == "wrist_right" ? "right wrist" : p == "front" ? "in front of you" : "following your view";
}

void Engine::ScreenAction(const Action& a) {
    const std::string& m = a.mode;
    auto turnOn = [&] {
        if (!a.place.empty()) screen.place = a.place;
        if (a.monitor >= 0) { screen.monitor = a.monitor; screen.source = "monitor"; }
        if (!a.window.empty()) { screen.source = "window"; screen.windowMatch = a.window; screen.windowStep = 0; screen.windowAsk++; }
        screen.on = true; screenPlaceFront = true;
        Toast(std::string("Screen on (") + PlaceName(screen.place) + ")", 1.5);
    };
    auto turnOff = [&] { screen.on = false; Toast("Screen off", 1.2); };
    if (m == "on") turnOn();
    else if (m == "off") turnOff();
    else if (m == "next_monitor" || m == "prev_monitor") {
        int n = std::max(1, screen.monitorCount);
        int cur = screen.monitor == 0 ? screen.primaryIndex : screen.monitor;
        cur = m == "next_monitor" ? cur % n + 1 : (cur + n - 2) % n + 1;
        if (screen.source == "window") cur = screen.monitor == 0 ? screen.primaryIndex : screen.monitor; // back to monitors: start where it was
        screen.monitor = cur; screen.source = "monitor"; screen.choiceChanged = true;
        if (!screen.on) { screen.on = true; screenPlaceFront = true; }
        Toast("Screen: monitor " + std::to_string(cur) + " of " + std::to_string(n), 1.5);
    } else if (m == "next_window" || m == "prev_window") {
        screen.source = "window"; screen.windowStep = m == "next_window" ? 1 : -1; screen.windowMatch.clear(); screen.windowAsk++; screen.choiceChanged = true;
        if (!screen.on) { screen.on = true; screenPlaceFront = true; }
        // the layer shows the window's name once it has picked it
    } else if (m == "choose") {
        if (!screen.on) { screen.on = true; screenPlaceFront = true; }
        OpenPicker();
    } else if (m == "next_fps") {
        const auto& st = prof->screen.fpsSteps;
        size_t i = 0;
        while (i < st.size() && st[i] != screen.fps) i++;
        screen.fps = st.empty() ? 30 : st[(i + 1) % st.size()]; // not in the list: starts at the first
        Toast(screen.fps ? "Screen: " + std::to_string(screen.fps) + " fps" : std::string("Screen: every frame"), 1.5);
    } else if (m == "next_place") {
        static const char* order[] = { "wrist_left", "wrist_right", "front", "head" };
        int i = 0; while (i < 4 && screen.place != order[i]) i++;
        screen.place = order[(i + 1) % 4];
        screen.on = true; screenPlaceFront = true;
        Toast(std::string("Screen: ") + PlaceName(screen.place), 1.5);
    } else { // toggle: the same button with another place moves it there instead of turning it off
        if (screen.on && (a.place.empty() || a.place == screen.place)) turnOff(); else turnOn();
    }
    Log("screen %s: on %d place %s monitor %d fps %d", m.c_str(), screen.on, screen.place.c_str(), screen.monitor, screen.fps);
}

void Engine::UpdateScreen() {
    screen.poseValid = false;
    bool wasShown = screen.shown;
    screen.shown = screen.on || adjust; // adjust mode shows it so it can be moved
    if (screen.shown && !wasShown) screenPlaceFront = true;
    if (grab.target == GrabTarget::Screen && !screen.shown) grab = GrabRun{};
    if (!screen.shown) { ApplyDock(); ScreenInteract(); return; }
    ScreenStyle& S = prof->screen;
    ScreenPose();
    if (screen.poseValid && screen.place != "front" && grab.target != GrabTarget::Screen) {
        float dt = (float)std::clamp(now - lastNow, 0.0, 0.25);
        screen.pose = screenSmooth.Apply(screen.pose, dt, S.smoothing);
    } else screenSmooth.Reset();
    ApplyDock(); // docked: the screen sits at the keyboard (or the keyboard at the screen) before pointing is checked
    ScreenInteract();
    if (!adjust || !screen.poseValid || (grab.Active() && grab.target != GrabTarget::Screen)) return;
    if (kb.visible && prof->dock.mode != DockMode::Apart) return; // docked: Adjust dock (on the keyboard's bar) places it

    // Adjust mode: a grip near the screen grabs it; it keeps the hand's angle, the stick resizes.
    bool onWrist = screen.place == "wrist_left" || screen.place == "wrist_right";
    int wh = screen.place == "wrist_left" ? 0 : 1;
    Vec3 c = V(screen.pose.position);
    if (!grab.Active()) {
        for (int g = 0; g < 2; g++) {
            if (onWrist && g == wh) continue; // the arm it sits on can't grab it
            const HandRaw& hr = fr.h[g];
            if (!hr.gripValid || !cur[g][C_GRIP][F_PRESS] || prev[g][C_GRIP][F_PRESS]) continue;
            if (Len(V(hr.gripPose.position) - c) < screen.widthM * 0.6f + 0.08f) { StartGrab(GrabTarget::Screen, g, screen.pose); break; }
        }
        return;
    }
    const HandRaw& hr = fr.h[grab.hand];
    if (!cur[grab.hand][C_GRIP][F_PRESS] || !hr.gripValid) {
        if (prof->haptics) haptics.push_back({ grab.hand, prof->hapticAmp * 0.6f, 0.015f });
        grab = GrabRun{};
        return;
    }
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    auto dz = [](float v) { return std::fabs(v) < 0.25f ? 0.f : v; };
    XrPosef obj = GrabbedPose();
    XrPosef frame;
    if (onWrist) { if (!fr.h[wh].gripValid) return; frame = fr.h[wh].gripPose; }
    else if (screen.place == "head") frame = fr.head;
    else frame = screenFrontFrame;
    S.poses[screen.place] = { true, PMul(PInv(frame), obj) };
    if (onWrist) S.wristWidth = std::clamp(S.wristWidth * (1.f + dz(hr.sy) * dt * 0.8f), 0.04f, 0.6f);
    else S.width = std::clamp(S.width * (1.f + dz(hr.sy) * dt * 0.8f), 0.1f, 4.f);
    ScreenPose();
}

void Engine::ScreenPose() {
    screen.poseValid = false;
    const ScreenStyle& S = prof->screen;
    if (screenPlaceFront && fr.headValid) screenFrontFrame = YawFrame(fr.head);
    auto fp = S.poses.find(screen.place);
    if (fp != S.poses.end() && fp->second.set) { // placed by hand in adjust mode
        bool onWrist = screen.place == "wrist_left" || screen.place == "wrist_right";
        int wh = screen.place == "wrist_left" ? 0 : 1;
        if (onWrist) { if (!fr.h[wh].gripValid) return; screen.pose = PMul(fr.h[wh].gripPose, fp->second.p); }
        else if (screen.place == "head") { if (!fr.headValid) return; screen.pose = PMul(fr.head, fp->second.p); }
        else { if (screenPlaceFront) { if (!fr.headValid) return; screenPlaceFront = false; } screen.pose = PMul(screenFrontFrame, fp->second.p); }
        screen.widthM = onWrist ? S.wristWidth : S.width;
        screen.poseValid = true;
        return;
    }
    if (screen.place == "wrist_left" || screen.place == "wrist_right") {
        int h = screen.place == "wrist_left" ? 0 : 1;
        const HandRaw& hr = fr.h[h];
        if (!hr.gripValid) return;
        const XrQuaternionf& q = hr.gripPose.orientation;
        Vec3 ax = Rotate(q, { 1, 0, 0 }), Z = Rotate(q, { 0, 0, 1 });
        Vec3 palmOut = h == 0 ? ax : ax * -1.f;
        Vec3 n = S.face == "inner" ? palmOut : palmOut * -1.f;
        // Landscape along the forearm, read with the arm held across the body (like a watch); rotate_deg turns it.
        Vec3 xAlong = h == 0 ? Z * -1.f : Z;
        Vec3 yAlong = Cross(n, xAlong);
        float r = S.rotateDeg * 3.14159265f / 180.f;
        Vec3 y = Norm(yAlong * std::cos(r) + Cross(n, yAlong) * std::sin(r));
        Vec3 x = Cross(y, n);
        Vec3 c = V(hr.gripPose.position) + Z * S.towardElbow + n * S.lift + Norm(Cross(Z, n)) * S.side;
        screen.pose = { QuatFromBasis(x, y, n), X(c) };
        screen.widthM = S.wristWidth;
        screen.poseValid = true;
        return;
    }
    if (!fr.headValid) return;
    const XrPosef& hp = fr.head;
    if (screen.place == "head") {
        Vec3 fwd = Rotate(hp.orientation, { 0, 0, -1 }), up = Rotate(hp.orientation, { 0, 1, 0 });
        screen.pose = { hp.orientation, X(V(hp.position) + fwd * S.distance - up * S.below) };
        screen.widthM = S.width; screen.poseValid = true;
        return;
    }
    // front: placed in front of you (level, facing you) when shown, then stays there
    if (screenPlaceFront) {
        Vec3 f = Rotate(hp.orientation, { 0, 0, -1 }); f.y = 0;
        if (Len(f) < 1e-3f) f = { 0, 0, -1 };
        f = Norm(f);
        Vec3 eye = V(hp.position);
        Vec3 c = eye + f * S.distance; c.y -= S.below;
        Vec3 zf = f * -1.f, yUp{ 0, 1, 0 }, xr = Cross(yUp, zf);
        screenFrontPose = { QuatFromBasis(xr, yUp, zf), X(c) };
        screenPlaceFront = false;
    }
    screen.pose = screenFrontPose; screen.widthM = S.width; screen.poseValid = true;
}

// Ray from hand h's pointer against the screen; u, v = 0..1 across it (v down), t = distance.
bool Engine::ScreenRay(int h, float& u, float& v, float& t) const {
    const HandRaw& hr = fr.h[h];
    if (!hr.aimValid || !screen.poseValid) return false;
    Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
    Vec3 c = V(screen.pose.position), n = Rotate(screen.pose.orientation, { 0, 0, 1 });
    float den = Dot(d, n);
    if (std::fabs(den) < 1e-4f) return false;
    t = Dot(c - o, n) / den;
    if (t <= 0.01f || t > 5.f) return false;
    Vec3 l = RotateInv(screen.pose.orientation, (o + d * t) - c);
    float w = screen.widthM, hgt = screen.widthM * screen.aspect;
    u = l.x / w + 0.5f; v = 0.5f - l.y / hgt;
    return true;
}

// Point at the screen to move the mouse; trigger = left button, A/X = right button, stick = scroll.
void Engine::ScreenInteract() {
    const ScreenStyle& S = prof->screen;
    bool dockedToKb = kb.visible && prof->dock.mode == DockMode::ScreenOnKeys; // it sits above the keyboard then, not on the arm
    bool onWrist = !dockedToKb && (screen.place == "wrist_left" || screen.place == "wrist_right");
    int wh = screen.place == "wrist_left" ? 0 : 1;
    screen.interactive = S.interact && screen.on && screen.poseValid && !adjust && !wr.open && !(kb.visible && kbOpenTime > now - 0.25) && screen.monW > 0; // works next to the open keyboard too
    ScreenUi();
    if (uiHand >= 0 || screen.pickOpen) screen.interactive = false; // the bar or the picker has the laser
    int hand = -1; float hu = 0, hv = 0;
    if (screen.interactive) {
        int order[2] = { screen.hoverHand >= 0 ? screen.hoverHand : 1, screen.hoverHand >= 0 ? 1 - screen.hoverHand : 0 };
        for (int g : order) {
            if (onWrist && g == wh) continue; // the arm it sits on can't point at it
            if (kb.visible && kb.hover[g] >= 0 && !(g == screen.hoverHand && (mouseL || mouseR))) continue; // pointing at a key: the keyboard has it
            float u, v, t;
            if (!ScreenRay(g, u, v, t)) continue;
            bool held = g == screen.hoverHand && (mouseL || mouseR); // dragging keeps going past the edge
            if (held || (u >= 0 && u <= 1 && v >= 0 && v <= 1)) { hand = g; hu = std::clamp(u, 0.f, 1.f); hv = std::clamp(v, 0.f, 1.f); break; }
        }
    }
    auto release = [&] {
        if (mouseL) { Keys::MouseButton(0, false); mouseL = false; }
        if (mouseR) { Keys::MouseButton(1, false); mouseR = false; }
    };
    if (hand >= 0 && hand != screen.hoverHand) release();
    screen.hoverHand = hand;
    if (hand < 0) {
        release();
        lastMx = lastMy = -1; wheelAcc = 0;
        if (screenClicked && kb.visible) screenLeftAt = -1; // typing into the desktop: keep its focus
        else if (screenClicked) {
            if (screenLeftAt < 0) screenLeftAt = now;
            else if (now - screenLeftAt > 0.4) { if (S.refocus) Keys::FocusGameNow(); screenClicked = false; screenLeftAt = -1; }
        }
        return;
    }
    screenLeftAt = -1;
    int x = screen.monX + std::min(screen.monW - 1, (int)(hu * screen.monW));
    int y = screen.monY + std::min(screen.monH - 1, (int)(hv * screen.monH));
    if (x != lastMx || y != lastMy) { Keys::MouseTo(x, y); lastMx = x; lastMy = y; }
    // buttons: press only on a fresh press while pointing, so a held game trigger doesn't click
    bool trig = cur[hand][C_TRIGGER][F_PRESS], trigEdge = trig && !prev[hand][C_TRIGGER][F_PRESS];
    bool b1 = cur[hand][C_BTN1][F_PRESS], b1Edge = b1 && !prev[hand][C_BTN1][F_PRESS];
    if ((trigEdge || b1Edge) && screen.source == "window" && screen.hwnd) Keys::BringToFront(screen.hwnd);
    if (trigEdge && !mouseL) { Keys::MouseButton(0, true); mouseL = true; screenClicked = true; if (prof->haptics) haptics.push_back({ hand, prof->hapticAmp, 0.02f }); }
    if (!trig && mouseL) { Keys::MouseButton(0, false); mouseL = false; }
    if (b1Edge && !mouseR) { Keys::MouseButton(1, true); mouseR = true; screenClicked = true; if (prof->haptics) haptics.push_back({ hand, prof->hapticAmp, 0.02f }); }
    if (!b1 && mouseR) { Keys::MouseButton(1, false); mouseR = false; }
    float sy = fr.h[hand].sy;
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    if (std::fabs(sy) > 0.3f) wheelAcc += sy * dt * 10.f; else wheelAcc = 0; // up to 10 notches a second
    while (wheelAcc >= 1.f) { Keys::MouseWheel(120); wheelAcc -= 1.f; }
    while (wheelAcc <= -1.f) { Keys::MouseWheel(-120); wheelAcc += 1.f; }
}

bool Engine::PanelRay(int h, const XrPosef& pose, float w, float hgt, float& u, float& v) const {
    const HandRaw& hr = fr.h[h];
    if (!hr.aimValid) return false;
    Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
    Vec3 c = V(pose.position), n = Rotate(pose.orientation, { 0, 0, 1 });
    float den = Dot(d, n);
    if (std::fabs(den) < 1e-4f) return false;
    float t = Dot(c - o, n) / den;
    if (t <= 0.01f || t > 5.f) return false;
    Vec3 l = RotateInv(pose.orientation, (o + d * t) - c);
    u = l.x / w + 0.5f; v = 0.5f - l.y / hgt;
    return u >= 0 && u <= 1 && v >= 0 && v <= 1;
}

void Engine::OpenPicker() {
    screen.pickOpen = true; pickPage = 0; pickMode = PickerMode::Source;
    screen.wantChoices = true; // the layer fills the list
    BuildPicker();
}

// The picker's cells for the current page: choices, then page buttons and Close.
static const char* kPlaces[4] = { "wrist_left", "wrist_right", "front", "head" };
static const char* kPlaceNames[4] = { "Left wrist", "Right wrist", "In front of me\nstays where it appears", "Following my view" };

void Engine::BuildPicker() {
    if (pickMode == PickerMode::Place) { // where to put the screen
        pickMap.clear(); screen.pickLabels.clear();
        for (int i = 0; i < 4; i++) {
            pickMap.push_back(i);
            screen.pickLabels.push_back((screen.place == kPlaces[i] ? "\xE2\x80\xA2 " : "") + std::string(kPlaceNames[i]));
        }
        pickMap.push_back(-3); screen.pickLabels.push_back("Cancel");
        screen.pickTitle = "Move the screen to";
        screen.pickHover = -1; screen.uiVersion++;
        return;
    }
    const int perPage = 12;
    int n = (int)screen.choices.size();
    int pages = std::max(1, (n + perPage - 1) / perPage);
    pickPage = std::clamp(pickPage, 0, pages - 1);
    pickMap.clear(); screen.pickLabels.clear();
    for (int i = pickPage * perPage; i < std::min(n, (pickPage + 1) * perPage); i++) {
        pickMap.push_back(i);
        screen.pickLabels.push_back((screen.choices[i].current ? "\xE2\x80\xA2 " : "") + screen.choices[i].label);
    }
    if (pickPage > 0) { pickMap.push_back(-1); screen.pickLabels.push_back("\xE2\x80\xB9 Previous"); }
    if (pickPage < pages - 1) { pickMap.push_back(-2); screen.pickLabels.push_back("More \xE2\x80\xBA"); }
    pickMap.push_back(-3); screen.pickLabels.push_back("Close");
    screen.pickTitle = n ? "Show on the screen" + (pages > 1 ? " (" + std::to_string(pickPage + 1) + "/" + std::to_string(pages) + ")" : std::string()) : "Looking for monitors and windows...";
    screen.pickHover = -1;
    screen.uiVersion++;
}

void Engine::PickerActivate(int cell) {
    if (cell < 0 || cell >= (int)pickMap.size()) return;
    int k = pickMap[cell];
    if (pickMode == PickerMode::Place) {
        screen.pickOpen = false; screen.uiVersion++;
        if (k >= 0 && screen.place != kPlaces[k]) {
            screen.place = kPlaces[k]; screenPlaceFront = true;
            Toast(std::string("Screen: ") + kPlaceNames[k], 1.5);
        }
        return;
    }
    if (k == -1) { pickPage--; BuildPicker(); return; }
    if (k == -2) { pickPage++; BuildPicker(); return; }
    screen.pickOpen = false; screen.uiVersion++;
    if (k == -3) return;
    const ScreenChoice& c = screen.choices[k];
    if (c.hwnd) { screen.source = "window"; screen.hwnd = c.hwnd; screen.windowStep = 0; screen.windowMatch.clear(); screen.windowAsk++; }
    else { screen.source = "monitor"; screen.monitor = c.monitor; Toast("Screen: " + c.label.substr(0, c.label.find('\n')), 1.5); }
    screen.choiceChanged = true; // remembered as the default from now on
    Log("screen picked: %s", c.label.c_str());
}

void Engine::BarActivate(ScreenBarButton button) {
    Action a; a.type = ActType::Screen;
    if (adjust) { Action d; d.type = ActType::Adjust; RunAction(d, -1, -1, true); return; } // the bar is just "Done"
    switch (button) {
    case ScreenBarButton::Choose: if (screen.pickOpen && pickMode == PickerMode::Source) { screen.pickOpen = false; screen.uiVersion++; } else OpenPicker(); return;
    case ScreenBarButton::FrameRate: a.mode = "next_fps"; break;
    case ScreenBarButton::Move:
        if (screen.pickOpen && pickMode == PickerMode::Place) { screen.pickOpen = false; screen.uiVersion++; return; }
        screen.pickOpen = true; pickMode = PickerMode::Place; BuildPicker(); return;
    case ScreenBarButton::Adjust: { // adjust positions: move the screen, wheel and wrist panels by hand
        screen.pickOpen = false; screen.uiVersion++;
        Action d; d.type = ActType::Adjust; RunAction(d, -1, -1, true);
        return;
    }
    case ScreenBarButton::Keyboard: // typing goes to the mirrored window: bring it up first
        screen.pickOpen = false; screen.uiVersion++;
        if (screen.source == "window" && screen.hwnd) Keys::BringToFront(screen.hwnd);
        screenClicked = true; // so the game gets its window back once you're done
        OpenKeyboard(-1);
        return;
    case ScreenBarButton::Hide: a.mode = "off"; screen.pickOpen = false; break;
    }
    ScreenAction(a);
}

// Bar under the screen while you point at it, and the picker in front of it.
void Engine::ScreenUi() {
    uiHand = -1;
    // In adjust mode the bar stays as one "Done" button (the screen shows then even while off).
    bool live = screen.shown && screen.poseValid && (adjust || (screen.on && !wr.open)) && !kb.visible && prof->screen.interact;
    if (!live) {
        if (screen.barVisible || screen.pickOpen) { screen.barVisible = screen.pickOpen = false; screen.uiVersion++; }
        return;
    }
    if (screen.choicesVersion != seenChoices) { seenChoices = screen.choicesVersion; if (screen.pickOpen) BuildPicker(); }
    // where they go: the bar just under the screen, the picker a little in front of it
    float sw = screen.widthM, sh = screen.widthM * screen.aspect;
    screen.barW = std::max(sw, 0.26f) * prof->screen.barScale; screen.barH = screen.barW / (adjust ? 4.f : 8.5f);
    if (adjust && screen.pickOpen) { screen.pickOpen = false; screen.uiVersion++; }
    Vec3 up = Rotate(screen.pose.orientation, { 0, 1, 0 }), nrm = Rotate(screen.pose.orientation, { 0, 0, 1 });
    screen.barPose = { screen.pose.orientation, X(V(screen.pose.position) - up * (sh / 2 + screen.barH / 2 + 0.006f) + nrm * 0.002f) };
    screen.pickW = std::max(sw * 0.9f, 0.36f); screen.pickH = screen.pickW * 0.95f;
    screen.pickPose = { screen.pose.orientation, X(V(screen.pose.position) + nrm * 0.03f) };
    std::string fps = screen.fps ? std::to_string(screen.fps) + " fps" : std::string("max fps");
    bool listOpen = screen.pickOpen && pickMode == PickerMode::Source, moveOpen = screen.pickOpen && pickMode == PickerMode::Place;
    std::vector<std::string> bar = adjust ? std::vector<std::string>{ "Done adjusting" }
        : std::vector<std::string>{ listOpen ? "Close list" : "Choose...", "Frame rate\n" + fps, moveOpen ? "Cancel move" : "Move...", "Keyboard", "Adjust", "Hide" };
    if (bar != screen.barLabels) { screen.barLabels = bar; screen.uiVersion++; }

    bool onWrist = screen.place == "wrist_left" || screen.place == "wrist_right";
    int wh = screen.place == "wrist_left" ? 0 : 1;
    int barHover = -1, pickHover = -1;
    bool pointedAt = false;
    for (int g = 0; g < 2; g++) {
        if (onWrist && g == wh) continue;
        float u, v;
        if (screen.pickOpen && PanelRay(g, screen.pickPose, screen.pickW, screen.pickH, u, v)) {
            std::vector<Cell> cells; Cell hd;
            ListLayout((int)screen.pickLabels.size(), screen.pickCols, screen.pickH / screen.pickW, true, cells, hd);
            for (int i = 0; i < (int)cells.size(); i++) if (u >= cells[i].x && u < cells[i].x + cells[i].w && v >= cells[i].y && v < cells[i].y + cells[i].h) pickHover = i;
            uiHand = g; pointedAt = true; break;
        }
        if (PanelRay(g, screen.barPose, screen.barW, screen.barH, u, v)) {
            std::vector<Cell> cells; Cell hd;
            ListLayout((int)screen.barLabels.size(), (int)screen.barLabels.size(), screen.barH / screen.barW, false, cells, hd);
            for (int i = 0; i < (int)cells.size(); i++) if (u >= cells[i].x && u < cells[i].x + cells[i].w && v >= cells[i].y && v < cells[i].y + cells[i].h) barHover = i;
            uiHand = g; pointedAt = true; break;
        }
        float t;
        if (ScreenRay(g, u, v, t) && u > -0.1f && u < 1.1f && v > -0.1f && v < 1.3f) pointedAt = true;
    }
    if (pointedAt) barUntil = now + 2.0;
    bool barVis = now < barUntil || screen.pickOpen || adjust;
    if (barVis != screen.barVisible) { screen.barVisible = barVis; screen.uiVersion++; }
    if (!screen.barVisible) barHover = -1;
    if (barHover != screen.barHover || pickHover != screen.pickHover) {
        if (prof->haptics && uiHand >= 0 && (barHover >= 0 || pickHover >= 0)) haptics.push_back({ uiHand, prof->hapticAmp * 0.4f, 0.01f });
        screen.barHover = barHover; screen.pickHover = pickHover; screen.uiVersion++;
    }
    if (now > uiFlashUntil && (screen.barPressed >= 0 || screen.pickPressed >= 0)) { screen.barPressed = screen.pickPressed = -1; screen.uiVersion++; }
    if (uiHand < 0) return;
    bool press = cur[uiHand][C_TRIGGER][F_PRESS] && !prev[uiHand][C_TRIGGER][F_PRESS];
    if (!press) return;
    if (prof->haptics) haptics.push_back({ uiHand, prof->hapticAmp, 0.025f });
    uiFlashUntil = now + 0.15;
    if (pickHover >= 0) { screen.pickPressed = pickHover; screen.uiVersion++; PickerActivate(pickHover); }
    else if (barHover >= 0) { screen.barPressed = barHover; screen.uiVersion++; BarActivate((ScreenBarButton)barHover); }
}
