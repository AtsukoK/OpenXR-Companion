// Mirrored desktop screen, engine side: where it is (wrist, in front, following the view), the switch
// actions, pointing at it to move and click the mouse, and the bar + picker shown under / in front of it.
// The layer captures the picture and draws it (overlay.cpp).
#include "engine.h"
#include "keys.h"
#include <algorithm>

// ------------------------------------------------------------ mirrored screen
static const char* PlaceName(ScreenPlace p) {
    switch (p) {
    case ScreenPlace::WristLeft:
        return "left wrist";
    case ScreenPlace::WristRight:
        return "right wrist";
    case ScreenPlace::Front:
        return "in front of you";
    case ScreenPlace::Head:
        return "following your view";
    }
    return "";
}

void ScreenMirror::ScreenAction(const Action& a) {
    const ActMode m = a.mode;
    auto turnOn = [&] {
        if (a.place) e.screen.place = *a.place;
        if (a.monitor >= 0) {
            e.screen.monitor = a.monitor;
            e.screen.source = ScreenSource::Monitor;
        }
        if (!a.window.empty()) {
            e.screen.source = ScreenSource::Window;
            e.screen.windowMatch = a.window;
            e.screen.windowStep = 0;
            e.screen.windowAsk++;
        }
        e.screen.on = true;
        run_.placeFront = true;
        e.Toast(std::string("Screen on (") + PlaceName(e.screen.place) + ")", 1.5);
    };
    auto turnOff = [&] {
        e.screen.on = false;
        e.Toast("Screen off", 1.2);
    };
    if (m == ActMode::On)
        turnOn();
    else if (m == ActMode::Off)
        turnOff();
    else if (m == ActMode::NextMonitor || m == ActMode::PrevMonitor) {
        int n = std::max(1, e.screen.monitorCount);
        int cur = e.screen.monitor == 0 ? e.screen.primaryIndex : e.screen.monitor;
        cur = m == ActMode::NextMonitor ? cur % n + 1 : (cur + n - 2) % n + 1;
        if (e.screen.source == ScreenSource::Window)
            cur = e.screen.monitor == 0 ? e.screen.primaryIndex : e.screen.monitor; // back to monitors: start where it was
        e.screen.monitor = cur;
        e.screen.source = ScreenSource::Monitor;
        e.screen.choiceChanged = true;
        if (!e.screen.on) {
            e.screen.on = true;
            run_.placeFront = true;
        }
        e.Toast("Screen: monitor " + std::to_string(cur) + " of " + std::to_string(n), 1.5);
    } else if (m == ActMode::NextWindow || m == ActMode::PrevWindow) {
        e.screen.source = ScreenSource::Window;
        e.screen.windowStep = m == ActMode::NextWindow ? 1 : -1;
        e.screen.windowMatch.clear();
        e.screen.windowAsk++;
        e.screen.choiceChanged = true;
        if (!e.screen.on) {
            e.screen.on = true;
            run_.placeFront = true;
        }
        // the layer shows the window's name once it has picked it
    } else if (m == ActMode::Choose) {
        if (!e.screen.on) {
            e.screen.on = true;
            run_.placeFront = true;
        }
        OpenPicker();
    } else if (m == ActMode::NextFps) {
        const auto& st = e.prof->screen.fpsSteps;
        size_t i = 0;
        while (i < st.size() && st[i] != e.screen.fps)
            i++;
        e.screen.fps = st.empty() ? 30 : st[(i + 1) % st.size()]; // not in the list: starts at the first
        e.Toast(e.screen.fps ? "Screen: " + std::to_string(e.screen.fps) + " fps" : std::string("Screen: every frame"), 1.5);
    } else if (m == ActMode::NextPlace) {
        const int n = (int)std::size(kScreenPlaces);
        int i = 0;
        while (i < n && e.screen.place != kScreenPlaces[i])
            i++;
        e.screen.place = kScreenPlaces[(i + 1) % n];
        e.screen.on = true;
        run_.placeFront = true;
        e.Toast(std::string("Screen: ") + PlaceName(e.screen.place), 1.5);
    } else { // toggle: the same button with another place moves it there instead of turning it off
        if (e.screen.on && (!a.place || *a.place == e.screen.place))
            turnOff();
        else
            turnOn();
    }
    Log("e.screen %s: on %d place %s monitor %d fps %d", ActModeKey(m), e.screen.on, ScreenPlaceKey(e.screen.place), e.screen.monitor,
        e.screen.fps);
}

void ScreenMirror::Update() {
    e.screen.poseValid = false;
    bool wasShown = e.screen.shown;
    e.screen.shown = e.screen.on || e.adjust; // adjust mode shows it so it can be moved
    if (e.screen.shown && !wasShown) run_.placeFront = true;
    if (e.grab.target == GrabTarget::Screen && !e.screen.shown) e.grab = Engine::GrabRun{};
    if (!e.screen.shown) {
        e.keyboard.ApplyDock();
        Interact();
        return;
    }
    ScreenStyle& S = e.prof->screen;
    UpdatePose();
    if (e.screen.poseValid && e.screen.place != ScreenPlace::Front && e.grab.target != GrabTarget::Screen) {
        float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.25);
        e.screen.pose = e.screenSmooth.Apply(e.screen.pose, dt, S.smoothing);
    } else
        e.screenSmooth.Reset();
    e.keyboard.ApplyDock(); // docked: the screen sits at the keyboard (or the keyboard at the screen) before pointing is checked
    Interact();
    if (!e.adjust || !e.screen.poseValid || (e.grab.Active() && e.grab.target != GrabTarget::Screen)) return;
    if (e.kb.visible && e.prof->dock.mode != DockMode::Apart) return; // docked: Adjust dock (on the keyboard's bar) places it

    // Adjust mode: a grip near the screen grabs it; it keeps the hand's angle, the stick resizes.
    bool onWrist = IsWristPlace(e.screen.place);
    int wh = WristHandOf(e.screen.place);
    Vec3 c = V(e.screen.pose.position);
    if (!e.grab.Active()) {
        for (int g = 0; g < 2; g++) {
            if (onWrist && g == wh) continue; // the arm it sits on can't grab it
            const HandRaw& hr = e.fr.h[g];
            if (!hr.gripValid || !e.cur[g][C_GRIP][F_PRESS] || e.prev[g][C_GRIP][F_PRESS]) continue;
            if (Len(V(hr.gripPose.position) - c) < e.screen.widthM * 0.6f + 0.08f) {
                e.StartGrab(GrabTarget::Screen, g, e.screen.pose);
                break;
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
    float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.1);
    auto dz = [](float v) {
        return std::fabs(v) < 0.25f ? 0.f : v;
    };
    XrPosef obj = e.GrabbedPose();
    XrPosef frame;
    if (onWrist) {
        if (!e.fr.h[wh].gripValid) return;
        frame = e.fr.h[wh].gripPose;
    } else if (e.screen.place == ScreenPlace::Head)
        frame = e.fr.head;
    else
        frame = run_.frontFrame;
    S.poses[ScreenPlaceKey(e.screen.place)] = { true, PMul(PInv(frame), obj) };
    if (onWrist)
        S.wristWidth = std::clamp(S.wristWidth * (1.f + dz(hr.sy) * dt * 0.8f), 0.04f, 0.6f);
    else
        S.width = std::clamp(S.width * (1.f + dz(hr.sy) * dt * 0.8f), 0.1f, 4.f);
    UpdatePose();
}

void ScreenMirror::UpdatePose() {
    e.screen.poseValid = false;
    const ScreenStyle& S = e.prof->screen;
    if (run_.placeFront && e.fr.headValid) run_.frontFrame = YawFrame(e.fr.head);
    auto fp = S.poses.find(ScreenPlaceKey(e.screen.place));
    if (fp != S.poses.end() && fp->second.set) { // placed by hand in adjust mode
        bool onWrist = IsWristPlace(e.screen.place);
        int wh = WristHandOf(e.screen.place);
        if (onWrist) {
            if (!e.fr.h[wh].gripValid) return;
            e.screen.pose = PMul(e.fr.h[wh].gripPose, fp->second.p);
        } else if (e.screen.place == ScreenPlace::Head) {
            if (!e.fr.headValid) return;
            e.screen.pose = PMul(e.fr.head, fp->second.p);
        } else {
            if (run_.placeFront) {
                if (!e.fr.headValid) return;
                run_.placeFront = false;
            }
            e.screen.pose = PMul(run_.frontFrame, fp->second.p);
        }
        e.screen.widthM = onWrist ? S.wristWidth : S.width;
        e.screen.poseValid = true;
        return;
    }
    if (IsWristPlace(e.screen.place)) {
        int h = WristHandOf(e.screen.place);
        const HandRaw& hr = e.fr.h[h];
        if (!hr.gripValid) return;
        const XrQuaternionf& q = hr.gripPose.orientation;
        Vec3 ax = Rotate(q, { 1, 0, 0 }), Z = Rotate(q, { 0, 0, 1 });
        Vec3 palmOut = h == 0 ? ax : ax * -1.f;
        Vec3 n = S.face == ArmSide::Inner ? palmOut : palmOut * -1.f;
        // Landscape along the forearm, read with the arm held across the body (like a watch); rotate_deg turns it.
        Vec3 xAlong = h == 0 ? Z * -1.f : Z;
        Vec3 yAlong = Cross(n, xAlong);
        float r = S.rotateDeg * 3.14159265f / 180.f;
        Vec3 y = Norm(yAlong * std::cos(r) + Cross(n, yAlong) * std::sin(r));
        Vec3 x = Cross(y, n);
        Vec3 c = V(hr.gripPose.position) + Z * S.towardElbow + n * S.lift + Norm(Cross(Z, n)) * S.side;
        e.screen.pose = { QuatFromBasis(x, y, n), X(c) };
        e.screen.widthM = S.wristWidth;
        e.screen.poseValid = true;
        return;
    }
    if (!e.fr.headValid) return;
    const XrPosef& hp = e.fr.head;
    if (e.screen.place == ScreenPlace::Head) {
        Vec3 fwd = Rotate(hp.orientation, { 0, 0, -1 }), up = Rotate(hp.orientation, { 0, 1, 0 });
        e.screen.pose = { hp.orientation, X(V(hp.position) + fwd * S.distance - up * S.below) };
        e.screen.widthM = S.width;
        e.screen.poseValid = true;
        return;
    }
    // front: placed in front of you (level, facing you) when shown, then stays there
    if (run_.placeFront) {
        Vec3 f = Rotate(hp.orientation, { 0, 0, -1 });
        f.y = 0;
        if (Len(f) < 1e-3f) f = { 0, 0, -1 };
        f = Norm(f);
        Vec3 eye = V(hp.position);
        Vec3 c = eye + f * S.distance;
        c.y -= S.below;
        Vec3 zf = f * -1.f, yUp{ 0, 1, 0 }, xr = Cross(yUp, zf);
        run_.frontPose = { QuatFromBasis(xr, yUp, zf), X(c) };
        run_.placeFront = false;
    }
    e.screen.pose = run_.frontPose;
    e.screen.widthM = S.width;
    e.screen.poseValid = true;
}

// Ray from hand h's pointer against the screen; u, v = 0..1 across it (v down), t = distance.
bool ScreenMirror::Ray(int h, float& u, float& v, float& t) const {
    const HandRaw& hr = e.fr.h[h];
    if (!hr.aimValid || !e.screen.poseValid) return false;
    Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
    Vec3 c = V(e.screen.pose.position), n = Rotate(e.screen.pose.orientation, { 0, 0, 1 });
    float den = Dot(d, n);
    if (std::fabs(den) < 1e-4f) return false;
    t = Dot(c - o, n) / den;
    if (t <= 0.01f || t > 5.f) return false;
    Vec3 l = RotateInv(e.screen.pose.orientation, (o + d * t) - c);
    float w = e.screen.widthM, hgt = e.screen.widthM * e.screen.aspect;
    u = l.x / w + 0.5f;
    v = 0.5f - l.y / hgt;
    return true;
}

// Point at the screen to move the mouse; trigger = left button, A/X = right button, stick = scroll.
void ScreenMirror::Interact() {
    const ScreenStyle& S = e.prof->screen;
    bool dockedToKb = e.kb.visible && e.prof->dock.mode == DockMode::ScreenOnKeys; // it sits above the keyboard then, not on the arm
    bool onWrist = !dockedToKb && (IsWristPlace(e.screen.place));
    int wh = WristHandOf(e.screen.place);
    e.screen.interactive = S.interact && e.screen.on && e.screen.poseValid && !e.adjust && !e.wr.open &&
                           !(e.kb.visible && e.keyboard.OpenedAt() > e.now - 0.25) &&
                           e.screen.monW > 0; // works next to the open keyboard too
    UpdateUi();
    if (run_.uiHand >= 0 || e.screen.pickOpen) e.screen.interactive = false; // the bar or the picker has the laser
    int hand = -1;
    float hu = 0, hv = 0;
    if (e.screen.interactive) {
        int order[2] = { e.screen.hoverHand >= 0 ? e.screen.hoverHand : 1, e.screen.hoverHand >= 0 ? 1 - e.screen.hoverHand : 0 };
        for (int g : order) {
            if (onWrist && g == wh) continue; // the arm it sits on can't point at it
            if (e.kb.visible && e.kb.hover[g] >= 0 && !(g == e.screen.hoverHand && (run_.mouseL || run_.mouseR)))
                continue; // pointing at a key: the keyboard has it
            float u, v, t;
            if (!Ray(g, u, v, t)) continue;
            bool held = g == e.screen.hoverHand && (run_.mouseL || run_.mouseR); // dragging keeps going past the edge
            if (held || (u >= 0 && u <= 1 && v >= 0 && v <= 1)) {
                hand = g;
                hu = std::clamp(u, 0.f, 1.f);
                hv = std::clamp(v, 0.f, 1.f);
                break;
            }
        }
    }
    auto release = [&] {
        if (run_.mouseL) {
            Keys::MouseButton(0, false);
            run_.mouseL = false;
        }
        if (run_.mouseR) {
            Keys::MouseButton(1, false);
            run_.mouseR = false;
        }
    };
    if (hand >= 0 && hand != e.screen.hoverHand) release();
    e.screen.hoverHand = hand;
    if (hand < 0) {
        release();
        run_.lastMx = run_.lastMy = -1;
        run_.wheelAcc = 0;
        if (run_.clicked && e.kb.visible)
            run_.leftAt = -1; // typing into the desktop: keep its focus
        else if (run_.clicked) {
            if (run_.leftAt < 0)
                run_.leftAt = e.now;
            else if (e.now - run_.leftAt > 0.4) {
                if (S.refocus) Keys::FocusGameNow();
                run_.clicked = false;
                run_.leftAt = -1;
            }
        }
        return;
    }
    run_.leftAt = -1;
    int x = e.screen.monX + std::min(e.screen.monW - 1, (int)(hu * e.screen.monW));
    int y = e.screen.monY + std::min(e.screen.monH - 1, (int)(hv * e.screen.monH));
    if (x != run_.lastMx || y != run_.lastMy) {
        Keys::MouseTo(x, y);
        run_.lastMx = x;
        run_.lastMy = y;
    }
    // buttons: press only on a fresh press while pointing, so a held game trigger doesn't click
    bool trig = e.cur[hand][C_TRIGGER][F_PRESS], trigEdge = trig && !e.prev[hand][C_TRIGGER][F_PRESS];
    bool b1 = e.cur[hand][C_BTN1][F_PRESS], b1Edge = b1 && !e.prev[hand][C_BTN1][F_PRESS];
    if ((trigEdge || b1Edge) && e.screen.source == ScreenSource::Window && e.screen.hwnd) Keys::BringToFront(e.screen.hwnd);
    if (trigEdge && !run_.mouseL) {
        Keys::MouseButton(0, true);
        run_.mouseL = true;
        run_.clicked = true;
        if (e.prof->haptics) e.haptics.push_back({ hand, e.prof->hapticAmp, 0.02f });
    }
    if (!trig && run_.mouseL) {
        Keys::MouseButton(0, false);
        run_.mouseL = false;
    }
    if (b1Edge && !run_.mouseR) {
        Keys::MouseButton(1, true);
        run_.mouseR = true;
        run_.clicked = true;
        if (e.prof->haptics) e.haptics.push_back({ hand, e.prof->hapticAmp, 0.02f });
    }
    if (!b1 && run_.mouseR) {
        Keys::MouseButton(1, false);
        run_.mouseR = false;
    }
    float sy = e.fr.h[hand].sy;
    float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.1);
    if (std::fabs(sy) > 0.3f)
        run_.wheelAcc += sy * dt * 10.f;
    else
        run_.wheelAcc = 0; // up to 10 notches a second
    while (run_.wheelAcc >= 1.f) {
        Keys::MouseWheel(120);
        run_.wheelAcc -= 1.f;
    }
    while (run_.wheelAcc <= -1.f) {
        Keys::MouseWheel(-120);
        run_.wheelAcc += 1.f;
    }
}

bool ScreenMirror::PanelRay(int h, const XrPosef& pose, float w, float hgt, float& u, float& v) const {
    const HandRaw& hr = e.fr.h[h];
    if (!hr.aimValid) return false;
    Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
    Vec3 c = V(pose.position), n = Rotate(pose.orientation, { 0, 0, 1 });
    float den = Dot(d, n);
    if (std::fabs(den) < 1e-4f) return false;
    float t = Dot(c - o, n) / den;
    if (t <= 0.01f || t > 5.f) return false;
    Vec3 l = RotateInv(pose.orientation, (o + d * t) - c);
    u = l.x / w + 0.5f;
    v = 0.5f - l.y / hgt;
    return u >= 0 && u <= 1 && v >= 0 && v <= 1;
}

void ScreenMirror::OpenPicker() {
    e.screen.pickOpen = true;
    run_.pickPage = 0;
    run_.pickMode = PickerMode::Source;
    e.screen.wantChoices = true; // the layer fills the list
    BuildPicker();
}

// The picker's cells for the current page: choices, then page buttons and Close.
// Picker labels, in the order of kScreenPlaces.
static const char* kPlaceNames[4] = { "Left wrist", "Right wrist", "In front of me\nstays where it appears", "Following my view" };

void ScreenMirror::BuildPicker() {
    if (run_.pickMode == PickerMode::Place) { // where to put the screen
        run_.pickMap.clear();
        e.screen.pickLabels.clear();
        for (int i = 0; i < 4; i++) {
            run_.pickMap.push_back(i);
            e.screen.pickLabels.push_back((e.screen.place == kScreenPlaces[i] ? "\xE2\x80\xA2 " : "") + std::string(kPlaceNames[i]));
        }
        run_.pickMap.push_back(-3);
        e.screen.pickLabels.push_back("Cancel");
        e.screen.pickTitle = "Move the e.screen to";
        e.screen.pickHover = -1;
        e.screen.uiVersion++;
        return;
    }
    const int perPage = 12;
    int n = (int)e.screen.choices.size();
    int pages = std::max(1, (n + perPage - 1) / perPage);
    run_.pickPage = std::clamp(run_.pickPage, 0, pages - 1);
    run_.pickMap.clear();
    e.screen.pickLabels.clear();
    for (int i = run_.pickPage * perPage; i < std::min(n, (run_.pickPage + 1) * perPage); i++) {
        run_.pickMap.push_back(i);
        e.screen.pickLabels.push_back((e.screen.choices[i].current ? "\xE2\x80\xA2 " : "") + e.screen.choices[i].label);
    }
    if (run_.pickPage > 0) {
        run_.pickMap.push_back(-1);
        e.screen.pickLabels.push_back("\xE2\x80\xB9 Previous");
    }
    if (run_.pickPage < pages - 1) {
        run_.pickMap.push_back(-2);
        e.screen.pickLabels.push_back("More \xE2\x80\xBA");
    }
    run_.pickMap.push_back(-3);
    e.screen.pickLabels.push_back("Close");
    e.screen.pickTitle = n ? "Show on the e.screen" +
                                 (pages > 1 ? " (" + std::to_string(run_.pickPage + 1) + "/" + std::to_string(pages) + ")" : std::string())
                           : "Looking for monitors and windows...";
    e.screen.pickHover = -1;
    e.screen.uiVersion++;
}

void ScreenMirror::PickerActivate(int cell) {
    if (cell < 0 || cell >= (int)run_.pickMap.size()) return;
    int k = run_.pickMap[cell];
    if (run_.pickMode == PickerMode::Place) {
        e.screen.pickOpen = false;
        e.screen.uiVersion++;
        if (k >= 0 && e.screen.place != kScreenPlaces[k]) {
            e.screen.place = kScreenPlaces[k];
            run_.placeFront = true;
            e.Toast(std::string("Screen: ") + kPlaceNames[k], 1.5);
        }
        return;
    }
    if (k == -1) {
        run_.pickPage--;
        BuildPicker();
        return;
    }
    if (k == -2) {
        run_.pickPage++;
        BuildPicker();
        return;
    }
    e.screen.pickOpen = false;
    e.screen.uiVersion++;
    if (k == -3) return;
    const ScreenChoice& c = e.screen.choices[k];
    if (c.hwnd) {
        e.screen.source = ScreenSource::Window;
        e.screen.hwnd = c.hwnd;
        e.screen.windowStep = 0;
        e.screen.windowMatch.clear();
        e.screen.windowAsk++;
    } else {
        e.screen.source = ScreenSource::Monitor;
        e.screen.monitor = c.monitor;
        e.Toast("Screen: " + c.label.substr(0, c.label.find('\n')), 1.5);
    }
    e.screen.choiceChanged = true; // remembered as the default from now on
    Log("e.screen picked: %s", c.label.c_str());
}

void ScreenMirror::BarActivate(ScreenBarButton button) {
    Action a;
    a.type = ActType::Screen;
    if (e.adjust) {
        Action d;
        d.type = ActType::Adjust;
        e.RunAction(d, -1, -1, true);
        return;
    } // the bar is just "Done"
    switch (button) {
    case ScreenBarButton::Choose:
        if (e.screen.pickOpen && run_.pickMode == PickerMode::Source) {
            e.screen.pickOpen = false;
            e.screen.uiVersion++;
        } else
            OpenPicker();
        return;
    case ScreenBarButton::FrameRate:
        a.mode = ActMode::NextFps;
        break;
    case ScreenBarButton::Move:
        if (e.screen.pickOpen && run_.pickMode == PickerMode::Place) {
            e.screen.pickOpen = false;
            e.screen.uiVersion++;
            return;
        }
        e.screen.pickOpen = true;
        run_.pickMode = PickerMode::Place;
        BuildPicker();
        return;
    case ScreenBarButton::Adjust: { // adjust positions: move the screen, wheel and wrist panels by hand
        e.screen.pickOpen = false;
        e.screen.uiVersion++;
        Action d;
        d.type = ActType::Adjust;
        e.RunAction(d, -1, -1, true);
        return;
    }
    case ScreenBarButton::Keyboard: // typing goes to the mirrored window: bring it up first
        e.screen.pickOpen = false;
        e.screen.uiVersion++;
        if (e.screen.source == ScreenSource::Window && e.screen.hwnd) Keys::BringToFront(e.screen.hwnd);
        run_.clicked = true; // so the game gets its window back once you're done
        e.keyboard.Open(-1);
        return;
    case ScreenBarButton::Hide:
        a.mode = ActMode::Off;
        e.screen.pickOpen = false;
        break;
    }
    ScreenAction(a);
}

// Bar under the screen while you point at it, and the picker in front of it.
void ScreenMirror::UpdateUi() {
    run_.uiHand = -1;
    // In adjust mode the bar stays as one "Done" button (the screen shows then even while off).
    bool live =
        e.screen.shown && e.screen.poseValid && (e.adjust || (e.screen.on && !e.wr.open)) && !e.kb.visible && e.prof->screen.interact;
    if (!live) {
        if (e.screen.barVisible || e.screen.pickOpen) {
            e.screen.barVisible = e.screen.pickOpen = false;
            e.screen.uiVersion++;
        }
        return;
    }
    if (e.screen.choicesVersion != run_.seenChoices) {
        run_.seenChoices = e.screen.choicesVersion;
        if (e.screen.pickOpen) BuildPicker();
    }
    // where they go: the bar just under the screen, the picker a little in front of it
    float sw = e.screen.widthM, sh = e.screen.widthM * e.screen.aspect;
    e.screen.barW = std::max(sw, 0.26f) * e.prof->screen.barScale;
    e.screen.barH = e.screen.barW / (e.adjust ? 4.f : 8.5f);
    if (e.adjust && e.screen.pickOpen) {
        e.screen.pickOpen = false;
        e.screen.uiVersion++;
    }
    Vec3 up = Rotate(e.screen.pose.orientation, { 0, 1, 0 }), nrm = Rotate(e.screen.pose.orientation, { 0, 0, 1 });
    e.screen.barPose = { e.screen.pose.orientation,
                         X(V(e.screen.pose.position) - up * (sh / 2 + e.screen.barH / 2 + 0.006f) + nrm * 0.002f) };
    e.screen.pickW = std::max(sw * 0.9f, 0.36f);
    e.screen.pickH = e.screen.pickW * 0.95f;
    e.screen.pickPose = { e.screen.pose.orientation, X(V(e.screen.pose.position) + nrm * 0.03f) };
    std::string fps = e.screen.fps ? std::to_string(e.screen.fps) + " fps" : std::string("max fps");
    bool listOpen = e.screen.pickOpen && run_.pickMode == PickerMode::Source,
         moveOpen = e.screen.pickOpen && run_.pickMode == PickerMode::Place;
    std::vector<std::string> bar =
        e.adjust ? std::vector<std::string>{ "Done adjusting" } : std::vector<std::string>{ listOpen ? "Close list" : "Choose...",
                                                                                            "Frame rate\n" + fps,
                                                                                            moveOpen ? "Cancel move" : "Move...",
                                                                                            "Keyboard",
                                                                                            "Adjust",
                                                                                            "Hide" };
    if (bar != e.screen.barLabels) {
        e.screen.barLabels = bar;
        e.screen.uiVersion++;
    }

    bool onWrist = IsWristPlace(e.screen.place);
    int wh = WristHandOf(e.screen.place);
    int barHover = -1, pickHover = -1;
    bool pointedAt = false;
    for (int g = 0; g < 2; g++) {
        if (onWrist && g == wh) continue;
        float u, v;
        if (e.screen.pickOpen && PanelRay(g, e.screen.pickPose, e.screen.pickW, e.screen.pickH, u, v)) {
            std::vector<Cell> cells;
            Cell hd;
            ListLayout((int)e.screen.pickLabels.size(), e.screen.pickCols, e.screen.pickH / e.screen.pickW, true, cells, hd);
            for (int i = 0; i < (int)cells.size(); i++)
                if (u >= cells[i].x && u < cells[i].x + cells[i].w && v >= cells[i].y && v < cells[i].y + cells[i].h) pickHover = i;
            run_.uiHand = g;
            pointedAt = true;
            break;
        }
        if (PanelRay(g, e.screen.barPose, e.screen.barW, e.screen.barH, u, v)) {
            std::vector<Cell> cells;
            Cell hd;
            ListLayout((int)e.screen.barLabels.size(), (int)e.screen.barLabels.size(), e.screen.barH / e.screen.barW, false, cells, hd);
            for (int i = 0; i < (int)cells.size(); i++)
                if (u >= cells[i].x && u < cells[i].x + cells[i].w && v >= cells[i].y && v < cells[i].y + cells[i].h) barHover = i;
            run_.uiHand = g;
            pointedAt = true;
            break;
        }
        float t;
        if (Ray(g, u, v, t) && u > -0.1f && u < 1.1f && v > -0.1f && v < 1.3f) pointedAt = true;
    }
    if (pointedAt) run_.barUntil = e.now + 2.0;
    bool barVis = e.now < run_.barUntil || e.screen.pickOpen || e.adjust;
    if (barVis != e.screen.barVisible) {
        e.screen.barVisible = barVis;
        e.screen.uiVersion++;
    }
    if (!e.screen.barVisible) barHover = -1;
    if (barHover != e.screen.barHover || pickHover != e.screen.pickHover) {
        if (e.prof->haptics && run_.uiHand >= 0 && (barHover >= 0 || pickHover >= 0))
            e.haptics.push_back({ run_.uiHand, e.prof->hapticAmp * 0.4f, 0.01f });
        e.screen.barHover = barHover;
        e.screen.pickHover = pickHover;
        e.screen.uiVersion++;
    }
    if (e.now > run_.uiFlashUntil && (e.screen.barPressed >= 0 || e.screen.pickPressed >= 0)) {
        e.screen.barPressed = e.screen.pickPressed = -1;
        e.screen.uiVersion++;
    }
    if (run_.uiHand < 0) return;
    bool press = e.cur[run_.uiHand][C_TRIGGER][F_PRESS] && !e.prev[run_.uiHand][C_TRIGGER][F_PRESS];
    if (!press) return;
    if (e.prof->haptics) e.haptics.push_back({ run_.uiHand, e.prof->hapticAmp, 0.025f });
    run_.uiFlashUntil = e.now + 0.15;
    if (pickHover >= 0) {
        e.screen.pickPressed = pickHover;
        e.screen.uiVersion++;
        PickerActivate(pickHover);
    } else if (barHover >= 0) {
        e.screen.barPressed = barHover;
        e.screen.uiVersion++;
        BarActivate((ScreenBarButton)barHover);
    }
}
