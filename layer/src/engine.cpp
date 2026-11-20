#include "engine.h"
#include "keys.h"
#include <algorithm>

static constexpr float kPi = 3.14159265f;

void Engine::SetProfile(std::shared_ptr<Profile> p) {
    Reset();
    prof = std::move(p);
    bs.assign(prof ? prof->bindings.size() : 0, BState{});
    if (prof) Keys::SetFocusWindow(prof->focusWindow);
    if (prof) { // the editor's screen settings apply on every (re)load; on/off survives a reload
        const ScreenStyle& S = prof->screen;
        screen.place = S.place; screen.monitor = S.monitor; screen.fps = S.fps;
        if (S.source == "window" && screen.source != "window") { screen.source = "window"; screen.windowMatch = S.window; screen.windowTitle = S.windowTitle; screen.windowStep = 0; screen.windowAsk++; }
        else if (S.source == "monitor") screen.source = "monitor";
        if (!screenInit) { screen.on = S.startOn; screenPlaceFront = screen.on; screenInit = true; }
        else if (screen.on) screenPlaceFront = true;
        offs = prof->offsets; // controller offsets: the file's values (also after the editor saves)
        offSel = offs.linkHands ? 2 : (offSel == 2 ? 1 : offSel);
        offKind = offs.aimSame ? 2 : (offKind == 2 ? 0 : offKind);
    }
}

void Engine::Reset() {
    if (wr.open) CloseWheel();
    kb = KeyboardView{}; kbRepeatKey = kbRepeatHand = -1;
    for (int h = 0; h < 2; h++) { wrist[h] = WristView{}; wrun[h] = WristRun{}; }
    adjust = false; grab = GrabRun{}; adjustClosedAt = -1;
    offView.open = false; offView.version++; offGrabArmed = false; offGrabHand = -1; offUiHand = -1; offSaveAt = -1;
    if (mouseL) Keys::MouseButton(0, false);
    if (mouseR) Keys::MouseButton(1, false);
    mouseL = mouseR = false; screen.hoverHand = -1;
    Keys::ReleaseAll();
    keyToggles.clear();
    xrToggles.clear(); xrHolds.clear(); pulses.clear();
    layers.clear();
    for (auto& b : bs) b = BState{};
    memset(blockOut, 0, sizeof(blockOut));
    for (auto& h : ovOut) for (auto& o : h) o = Override{};
}

bool Engine::IsOn(const Action& a) const {
    switch (a.type) {
    case ActType::Key: return a.mode == "toggle" && keyToggles.count(a.keys) > 0;
    case ActType::XrInput: return a.mode == "toggle" && xrToggles.count({ a.target.hand, a.target.comp }) > 0;
    case ActType::Layer: return layers.count(a.layer) > 0;
    case ActType::Screen: return (a.mode == "toggle" || a.mode == "on") && screen.on && (a.place.empty() || a.place == screen.place);
    case ActType::Mirror: return a.mode != "off" && mirrorOn;
    case ActType::Offsets: return (a.mode == "panel" && offView.open) || ((a.mode == "enable" || a.mode == "on") && offs.enabled);
    default: return false;
    }
}

void Engine::Toast(const std::string& s, double sec) {
    if (prof && !prof->notifications) return;
    toast = s; toastUntil = NowSec() + sec; toastVersion++;
}

void Engine::ComputeDown() {
    memcpy(prev, cur, sizeof(cur));
    memset(cur, 0, sizeof(cur));
    const Profile& p = *prof;
    for (int h = 0; h < 2; h++) {
        const HandRaw& r = fr.h[h];
        auto hyst = [&](int c, int f, bool cond_on, bool cond_hold) { cur[h][c][f] = prev[h][c][f] ? cond_hold : cond_on; };
        cur[h][C_BTN1][F_PRESS] = r.b1; cur[h][C_BTN1][F_TOUCH] = r.b1t || r.b1;
        cur[h][C_BTN2][F_PRESS] = r.b2; cur[h][C_BTN2][F_TOUCH] = r.b2t || r.b2;
        hyst(C_TRIGGER, F_PRESS, r.trig > p.pressOn, r.trig > p.pressOff);
        cur[h][C_TRIGGER][F_TOUCH] = r.trigT || cur[h][C_TRIGGER][F_PRESS];
        hyst(C_GRIP, F_PRESS, r.grip > p.pressOn, r.grip > p.pressOff);
        cur[h][C_GRIP][F_TOUCH] = cur[h][C_GRIP][F_PRESS];
        cur[h][C_STICK][F_PRESS] = r.sc;
        cur[h][C_STICK][F_TOUCH] = r.st || r.sc;
        hyst(C_STICK, F_UP, r.sy > p.stickOn, r.sy > p.stickOff);
        hyst(C_STICK, F_DOWN, r.sy < -p.stickOn, r.sy < -p.stickOff);
        hyst(C_STICK, F_RIGHT, r.sx > p.stickOn, r.sx > p.stickOff);
        hyst(C_STICK, F_LEFT, r.sx < -p.stickOn, r.sx < -p.stickOff);
        cur[h][C_THUMBREST][F_TOUCH] = cur[h][C_THUMBREST][F_PRESS] = r.rest;
        cur[h][C_MENU][F_PRESS] = cur[h][C_MENU][F_TOUCH] = r.menu;
    }
}

bool Engine::CompActive(int h, int c) const {
    const HandRaw& r = fr.h[h];
    switch (c) {
    case C_TRIGGER: return r.trig > prof->pressOff * 0.5f;
    case C_GRIP: return r.grip > prof->pressOff * 0.5f;
    case C_STICK: return r.sc || std::sqrt(r.sx * r.sx + r.sy * r.sy) > 0.25f;
    case C_THUMBREST: return false;
    default: return cur[h][c][F_PRESS];
    }
}

bool Engine::LayersOk(const Binding& b) const {
    for (auto& l : b.layers) if (!layers.count(l)) return false;
    for (auto& l : b.notLayers) if (layers.count(l)) return false;
    return true;
}

Override Engine::MakeOv(const InputId& t) {
    Override o; o.on = true;
    switch (t.feat) {
    case F_PRESS: o.press = true; o.touch = true; o.value = 1; break;
    case F_TOUCH: o.touch = true; break;
    case F_UP: o.y = 1; o.touch = true; break;
    case F_DOWN: o.y = -1; o.touch = true; break;
    case F_LEFT: o.x = -1; o.touch = true; break;
    case F_RIGHT: o.x = 1; o.touch = true; break;
    }
    return o;
}

void Engine::SetLayer(const std::string& l, bool on) {
    bool had = layers.count(l) > 0;
    if (on == had) return;
    if (on) layers.insert(l); else layers.erase(l);
    Log("layer %s %s", l.c_str(), on ? "on" : "off");
    Toast("Layer " + l + (on ? " ON" : " off"), 1.2);
}

// Returns true when the action keeps running until EndAction (hold behaviour).
bool Engine::RunAction(const Action& a, int hand, int bi, bool fromWheel) {
    bool hold = (a.mode == "hold") && !fromWheel;
    switch (a.type) {
    case ActType::Key:
        if (a.mode == "toggle") {
            if (keyToggles.count(a.keys)) { keyToggles.erase(a.keys); Keys::Up(a.keys); }
            else { keyToggles.insert(a.keys); Keys::Down(a.keys); }
            return false;
        }
        if (hold) { Keys::Down(a.keys); return true; }
        Keys::Tap(a.keys, a.tapMs);
        return false;
    case ActType::Sequence:
        Keys::Sequence(a.steps);
        return false;
    case ActType::XrInput: {
        auto key = std::make_pair(a.target.hand, a.target.comp);
        if (a.mode == "toggle") {
            if (xrToggles.count(key)) xrToggles.erase(key); else xrToggles[key] = MakeOv(a.target);
            return false;
        }
        if (hold) { xrHolds[bi] = { key, MakeOv(a.target) }; return true; }
        pulses.push_back({ a.target.hand, a.target.comp, MakeOv(a.target), now + std::max(a.tapMs, 30) / 1000.0 });
        return false;
    }
    case ActType::Layer:
        if (a.mode == "hold" && !fromWheel) { SetLayer(a.layer, true); return true; }
        if (a.mode == "on") SetLayer(a.layer, true);
        else if (a.mode == "off") SetLayer(a.layer, false);
        else SetLayer(a.layer, !layers.count(a.layer));
        return false;
    case ActType::Wheel:
        if (fromWheel) { auto it = prof->wheels.find(a.wheel); if (it != prof->wheels.end()) PushWheel(&it->second); return false; }
        OpenWheel(a, bi, hand);
        return wr.open && wr.confirm == "release";
    case ActType::Back:
        if (wr.open) { if (wr.stack.size() > 1) { wr.stack.pop_back(); wr.hovered = -1; Dirty(); } else CloseWheel(); }
        return false;
    case ActType::Close:
        if (wr.open) CloseWheel();
        return false;
    case ActType::Reload:
        reloadRequested = true;
        return false;
    case ActType::Keyboard:
        OpenKeyboard(bi);
        return false;
    case ActType::Screen:
        ScreenAction(a);
        return false;
    case ActType::Offsets:
        OffsetsAction(a);
        return false;
    case ActType::Mirror:
        mirrorAsk = a.mode == "on" ? 2 : a.mode == "off" ? 3 : 1;
        return false;
    case ActType::Adjust:
        adjust = !adjust; grab = GrabRun{};
        if (adjust) Toast("Adjust: grip to move, stick to resize", 4.0);
        else { saveRequested = true; Toast("Positions saved", 2.0); if (wr.open) CloseWheel(); }
        Log("adjust mode %s", adjust ? "on" : "off");
        return false;
    default:
        return false;
    }
}

void Engine::EndAction(const Action& a, int bi) {
    switch (a.type) {
    case ActType::Key: Keys::Up(a.keys); break;
    case ActType::XrInput: xrHolds.erase(bi); break;
    case ActType::Layer: SetLayer(a.layer, false); break;
    case ActType::Wheel:
        if (wr.open && wr.opener == bi && wr.confirm == "release") {
            if (wr.hovered >= 0) Select(wr.hovered); else CloseWheel();
        }
        break;
    default: break;
    }
}

void Engine::Fire(int bi) {
    const Binding& b = prof->bindings[bi];
    BState& s = bs[bi];
    s.fired = true;
    Log("fire '%s' (%s)", b.name.c_str(), b.chordText.c_str());
    bool holds = RunAction(b.action, b.chord.back().hand, bi, false);
    bool wantsEnd = holds && (b.gesture == "press" || b.gesture == "hold" || b.gesture == "while_held");
    if (wantsEnd) s.needEnd = true;
    else if (holds) EndAction(b.action, bi);
}

void Engine::EndB(int bi) {
    BState& s = bs[bi];
    if (s.needEnd) { s.needEnd = false; EndAction(prof->bindings[bi].action, bi); }
}

void Engine::ProcessBindings() {
    auto& B = prof->bindings;
    // Releases and holds of armed bindings.
    for (int i = 0; i < (int)B.size(); i++) {
        BState& s = bs[i];
        if (!s.armed) continue;
        const Binding& b = B[i];
        const InputId& trig = b.chord.back();
        double dur = now - s.armTime;
        if (!Down(trig)) {
            s.armed = false;
            if (b.gesture == "tap" && dur * 1000 < b.tapMs) {
                bool otherFired = false;
                for (int j = 0; j < (int)B.size(); j++)
                    if (j != i && bs[j].fired && bs[j].armTime == s.armTime && B[j].chord.back() == trig) otherFired = true;
                if (!otherFired) Fire(i);
            }
            if (b.gesture == "hold" && !s.fired && b.tapPassthrough)
                pulses.push_back({ trig.hand, trig.comp, MakeOv(trig), now + 0.08 });
            EndB(i);
            continue;
        }
        if (b.gesture == "hold" && !s.fired && dur * 1000 >= b.holdMs) Fire(i);
    }
    // A double tap on a .touch input only counts touches that never become a real press:
    // pressing the button (e.g. B twice fast to loot) cancels it, so it can't open a wheel.
    // (Thumbrest and menu report touch and press as the same thing, so they're left alone.)
    for (int i = 0; i < (int)B.size(); i++) {
        const InputId& t = B[i].chord.back();
        if (B[i].gesture == "double_tap" && t.feat == F_TOUCH && t.comp != C_THUMBREST && t.comp != C_MENU
            && cur[t.hand][t.comp][F_PRESS]) bs[i].lastTap = -1;
    }
    // New presses: arm the most specific matching bindings.
    std::vector<int> cand;
    for (int i = 0; i < (int)B.size(); i++) {
        const Binding& b = B[i];
        if (!b.enabled || bs[i].armed) continue;
        const InputId& trig = b.chord.back();
        if (!Pressed(trig)) continue;
        if (wr.open && b.action.type != ActType::Wheel && b.action.type != ActType::Keyboard && b.action.type != ActType::Adjust) continue;
        if (kb.visible && b.action.type != ActType::Keyboard && b.action.type != ActType::Adjust) continue; // adjust: place the open keyboard
        if (!LayersOk(b)) continue;
        bool others = true;
        for (size_t k = 0; k + 1 < b.chord.size(); k++) if (!Down(b.chord[k])) others = false;
        if (others) cand.push_back(i);
    }
    // Group by trigger, keep max specificity per trigger.
    for (int i : cand) {
        const Binding& b = B[i];
        int spec = (int)(b.chord.size() + b.layers.size());
        bool best = true;
        for (int j : cand) {
            const Binding& o = B[j];
            if (o.chord.back() == b.chord.back() && (int)(o.chord.size() + o.layers.size()) > spec) best = false;
        }
        if (!best) continue;
        BState& s = bs[i];
        s.armed = true; s.armTime = now; s.fired = false;
        if (b.gesture == "press" || b.gesture == "while_held") Fire(i);
        else if (b.gesture == "double_tap") {
            if (s.lastTap >= 0 && (now - s.lastTap) * 1000 <= b.doubleTapMs) { s.lastTap = -1; Fire(i); }
            else s.lastTap = now;
        }
    }
}

// Adjust mode, wheel: grab the open wheel with the other hand's grip and move it; its stick resizes.
void Engine::UpdateAdjustWheel() {
    if (grab.target == 2 && (!adjust || !wr.open)) grab = GrabRun{};
    // Adjust mode shows the main wheel by itself (again shortly after it is closed), following the hand.
    if (adjust && wr.open) adjustClosedAt = -1;
    else if (adjust && !kb.visible && prof->style.anchor.rfind("hand", 0) == 0) { // wheels in front of you aren't brought out: they got in the way (open one with its button to place it)
        if (adjustClosedAt < 0) adjustClosedAt = now;
        if (now - adjustClosedAt > 0.6 && !prof->wheels.empty()) {
            int hand = fr.h[1].gripValid ? 1 : 0;
            Action a; a.type = ActType::Wheel; a.confirm = "button";
            a.wheel = prof->wheels.count(hand ? "right" : "left") ? (hand ? "right" : "left") : prof->wheels.begin()->first;
            for (auto& b : prof->bindings) // prefer the wheel this hand really opens
                if (b.enabled && b.action.type == ActType::Wheel && !b.chord.empty() && b.chord.back().hand == hand && prof->wheels.count(b.action.wheel)) { a.wheel = b.action.wheel; break; }
            OpenWheel(a, -1, hand);
            wr.locked = false;
            adjustClosedAt = -1;
        }
    }
    if (!adjust || !wr.open || (grab.target >= 0 && grab.target != 2)) return;
    int g = 1 - wr.openerHand;
    const HandRaw& hr = fr.h[g];
    Style& st = prof->style;
    Vec3 c = V(wr.pose.position);
    if (grab.target < 0) {
        bool edge = cur[g][C_GRIP][F_PRESS] && !prev[g][C_GRIP][F_PRESS];
        if (edge && hr.gripValid && Len(V(hr.gripPose.position) - c) < st.radius + 0.08f) StartGrab(2, g, wr.pose);
        return;
    }
    if (!cur[g][C_GRIP][F_PRESS] || !hr.gripValid) {
        grab = GrabRun{};
        if (prof->haptics) haptics.push_back({ g, prof->hapticAmp * 0.6f, 0.015f });
        return;
    }
    XrPosef obj = GrabbedPose(); // keeps the hand's angle: tilt and turn it however you like
    st.poses[st.anchor] = { true, PMul(PInv(WheelFrame(wr.openerHand)), obj) };
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    st.radius = std::clamp(st.radius * (1.f + hr.sy * dt * 0.8f), 0.05f, 0.4f);
    wr.pose = obj;
}

void Engine::StartGrab(int target, int hand, const XrPosef& obj) {
    grab = GrabRun{}; grab.target = target; grab.hand = hand;
    grab.rel = PMul(PInv(fr.h[hand].gripPose), obj);
    if (prof->haptics) haptics.push_back({ hand, prof->hapticAmp, 0.02f });
}
XrPosef Engine::GrabbedPose() const { return PMul(fr.h[grab.hand].gripPose, grab.rel); }

// What a hand-placed wheel is attached to: the hand's position (hand anchors) or the head, turned like the head.
XrPosef Engine::WheelFrame(int hand) const {
    XrPosef f = YawFrame(fr.head);
    const Style& st = prof->style;
    if ((st.anchor == "hand" || st.anchor == "hand_locked") && fr.h[hand].gripValid) f.position = fr.h[hand].gripPose.position;
    return f;
}

XrPosef Engine::WheelPose(int hand) const {
    const Style& st = prof->style;
    auto fp = st.poses.find(st.anchor);
    if (fp != st.poses.end() && fp->second.set) return PMul(WheelFrame(hand), fp->second.p);
    Vec3 head = V(fr.head.position);
    Vec3 center;
    bool handAnchor = st.anchor == "hand" || st.anchor == "hand_locked";
    if (handAnchor && fr.h[hand].gripValid) {
        Vec3 g = V(fr.h[hand].gripPose.position);
        Vec3 toward = head - g; toward.y = 0; toward = Norm(toward);
        center = g + Vec3{ 0, st.handUp, 0 } + toward * st.handToward;
    } else {
        Vec3 f = Rotate(fr.head.orientation, { 0, 0, -1 });
        f.y = 0; f = Norm(f);
        center = head + f * st.distance - Vec3{ 0, st.headDown, 0 };
    }
    XrPosef p;
    p.position = X(center);
    p.orientation = BillboardTo(center, head);
    return p;
}

void Engine::OpenWheel(const Action& a, int bi, int hand) {
    auto it = prof->wheels.find(a.wheel);
    if (it == prof->wheels.end()) return;
    if (wr.open) { CloseWheel(); return; } // pressing the opener again closes it
    wr = WheelRun{};
    wr.open = true;
    wr.stack.push_back(&it->second);
    wr.opener = bi;
    wr.openerHand = hand;
    const Selection& sel = prof->sel;
    wr.selHand = sel.hand == "left" ? 0 : sel.hand == "right" ? 1 : hand;
    wr.confirm = a.confirm;
    if (wr.confirm == "auto" && bi < 0) wr.confirm = "button"; // opened from a wrist button
    else if (wr.confirm == "auto") {
        const Binding& b = prof->bindings[bi];
        const InputId& t = b.chord.back();
        if (t.feat == F_TOUCH) wr.confirm = "stick_return";
        else if (b.gesture == "while_held" || b.gesture == "hold") wr.confirm = "release";
        else wr.confirm = "button";
    }
    wr.openTime = now;
    wr.pose = WheelPose(hand);
    wr.locked = prof->style.anchor == "hand_locked" || prof->style.anchor == "head_locked";
    // If the stick is already pushed, wait for it to centre first.
    float m = std::hypot(fr.h[wr.selHand].sx, fr.h[wr.selHand].sy);
    wr.stickOut = false;
    if (m > sel.deadzone) wr.hoverSince = -1; // marker: ignore until centred
    Log("wheel '%s' open, confirm=%s", a.wheel.c_str(), wr.confirm.c_str());
    Dirty();
}

void Engine::PushWheel(const Wheel* w) {
    wr.stack.push_back(w);
    wr.hovered = -1;
    wr.hoverSince = now;
    Dirty();
}

void Engine::CloseWheel() {
    wr.open = false;
    wr.stack.clear();
    wr.hovered = -1;
    view.visible = false;
    Dirty();
}

void Engine::Select(int idx) {
    const Wheel* w = wr.stack.back();
    if (idx < 0 || idx >= (int)w->items.size()) return;
    const WheelItem& it = w->items[idx];
    Log("select '%s'", it.label.c_str());
    if (prof->haptics) haptics.push_back({ wr.selHand, std::min(1.f, prof->hapticAmp * 1.6f), 0.04f });
    if (it.action.type == ActType::Wheel || it.action.type == ActType::Back) { RunAction(it.action, wr.selHand, -1, true); return; }
    if (adjust && it.action.type != ActType::Adjust && it.action.type != ActType::Close && it.action.type != ActType::Keyboard) return; // adjust mode: finishing, or the keyboard to place it
    RunAction(it.action, wr.selHand, -1, true);
    if (wr.open && (prof->sel.closeAfterSelect || it.action.type == ActType::Close)) CloseWheel();
    else Dirty();
}

static int Sector(float x, float y, int n) {
    if (n <= 0) return -1;
    float ang = std::atan2(x, y) * 180.f / kPi; // 0 = up, clockwise
    float seg = 360.f / n;
    float a = std::fmod(ang + seg / 2 + 720.f, 360.f);
    return std::min(n - 1, (int)(a / seg));
}

void Engine::UpdateWheel() {
    if (!wr.open) return;
    const Selection& sel = prof->sel;
    const Wheel* w = wr.stack.back();
    int n = (int)w->items.size();
    if (!wr.locked) wr.pose = WheelPose(wr.openerHand);

    const HandRaw& hr = fr.h[wr.selHand];
    float mag = std::hypot(hr.sx, hr.sy);
    bool out = mag > sel.deadzone;
    if (wr.hoverSince < 0) { if (mag < sel.deadzone * 0.6f) wr.hoverSince = now; out = false; }
    bool useStick = sel.method != "point";
    bool usePoint = sel.method == "point" || sel.method == "both";
    int newHover = wr.hovered;

    if (useStick && out) newHover = Sector(hr.sx, hr.sy, n);
    if (usePoint && hr.aimValid && !(useStick && out)) {
        Vec3 o = V(hr.aimPose.position);
        Vec3 d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
        Vec3 c = V(wr.pose.position);
        Vec3 nrm = Rotate(wr.pose.orientation, { 0, 0, 1 });
        float den = Dot(d, nrm);
        newHover = -1;
        if (std::fabs(den) > 1e-4f) {
            float t = Dot(c - o, nrm) / den;
            if (t > 0) {
                Vec3 local = RotateInv(wr.pose.orientation, (o + d * t) - c);
                float r = std::hypot(local.x, local.y) / prof->style.radius;
                if (r > prof->style.innerRatio && r < 1.25f) newHover = Sector(local.x, local.y, n);
            }
        }
    }

    // Stick-return confirm: pick the hovered item when the stick comes back to the centre.
    if (useStick && wr.confirm == "stick_return" && wr.stickOut && !out && wr.hovered >= 0) {
        wr.stickOut = false;
        Select(wr.hovered);
        return;
    }
    wr.stickOut = out;

    if (newHover != wr.hovered) {
        wr.hovered = newHover;
        wr.hoverSince = now;
        if (newHover >= 0 && prof->haptics) haptics.push_back({ wr.selHand, prof->hapticAmp, 0.015f });
        Dirty();
    }
    // Release-confirm wheels open submenus by resting on them.
    if (wr.confirm == "release" && wr.hovered >= 0 && sel.submenuDwellMs > 0 &&
        w->items[wr.hovered].action.type == ActType::Wheel && (now - wr.hoverSince) * 1000 >= sel.submenuDwellMs) {
        Select(wr.hovered);
        return;
    }
    if (sel.confirmComp >= 0 && cur[wr.selHand][sel.confirmComp][F_PRESS] && !prev[wr.selHand][sel.confirmComp][F_PRESS]) {
        if (wr.hovered >= 0) { Select(wr.hovered); return; }
    }
    if (sel.cancelComp >= 0 && cur[wr.selHand][sel.cancelComp][F_PRESS] && !prev[wr.selHand][sel.cancelComp][F_PRESS]) {
        if (wr.stack.size() > 1) { wr.stack.pop_back(); wr.hovered = -1; Dirty(); }
        else CloseWheel();
        return;
    }
    if (sel.autoCloseMs > 0 && (now - wr.openTime) * 1000 > sel.autoCloseMs) CloseWheel();
}

// Laser pointers: for each controller, the nearest pointable surface it is aimed at.
void Engine::UpdatePointers() {
    for (int h = 0; h < 2; h++) {
        PointerView& P = pointer[h]; P.on = false; P.finger = false; P.dot = true;
        const HandRaw& hr = fr.h[h];
        if (!hr.aimValid || !fr.headValid) continue;
        Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
        Vec3 eye = V(fr.head.position);
        // A quad standing along the ray, turned to face the eye (used by the beam and the finger).
        auto alongRay = [&](Vec3 mid, Vec3 fallbackN) {
            Vec3 toEye = eye - mid; toEye = toEye - d * Dot(toEye, d);
            Vec3 z = Len(toEye) > 1e-4f ? Norm(toEye) : fallbackN;
            return XrPosef{ QuatFromBasis(Cross(d, z), d, z), X(mid) };
        };
        float best = 1e9f; XrQuaternionf bestQ{ 0,0,0,1 };
        // Ray against a square/rect surface (half sizes in metres, with a little margin).
        auto test = [&](const XrPosef& pose, float hw, float hh) {
            Vec3 c = V(pose.position), n = Rotate(pose.orientation, { 0, 0, 1 });
            float den = Dot(d, n);
            if (std::fabs(den) < 1e-4f) return;
            float t = Dot(c - o, n) / den;
            if (t <= 0.01f || t > 3.f || t >= best) return;
            Vec3 l = RotateInv(pose.orientation, (o + d * t) - c);
            if (std::fabs(l.x) > hw * 1.1f || std::fabs(l.y) > hh * 1.1f) return;
            best = t; bestQ = pose.orientation;
        };
        if (wr.open && h == wr.selHand && (prof->sel.method == "point" || prof->sel.method == "both"))
            test(wr.pose, prof->style.radius, prof->style.radius);
        if (kb.visible) test(kb.pose, kb.widthM / 2, kb.heightM / 2);
        if (screen.interactive && screen.hoverHand == h) test(screen.pose, screen.widthM / 2 * 1.2f, screen.widthM * screen.aspect / 2 * 1.2f);
        if (uiHand == h && screen.pickOpen) test(screen.pickPose, screen.pickW / 2, screen.pickH / 2);
        if (uiHand == h && screen.barVisible) test(screen.barPose, screen.barW / 2, screen.barH / 2);
        if (offView.open && h != offGrabHand) test(offView.pose, offView.w / 2, offView.h / 2);
        int panel = 1 - h; // a hand points at the panel on the other arm
        const WristView& wv = wrist[panel];
        bool wristUp = wv.visible && grab.target != panel;
        float dPanel = wristUp ? Len(V(wv.pose.position) - o) : 1e9f;
        // The laser stays until the touch cursor actually takes over (no gap where neither shows).
        if (wristUp && prof->wrist[panel].press != "poke" && !wv.cursor)
            test(wv.pose, wv.sizeM / 2, wv.sizeM / 2);
        // Fingertip silhouette: where the poke tip is, so you can find your "finger" before it reaches the panel.
        if (wristUp && prof->pointer.finger && dPanel < 0.5f) { // on every panel: it also shows where the laser starts
            const float len = 0.09f;
            Vec3 tip = o + d * 0.015f; // same tip as the poke test in UpdateWrist
            P.finger = true; P.fingerLen = len; P.fingerW = 0.024f;
            P.fingerPose = alongRay(tip - d * (len * 0.5f), Rotate(wv.pose.orientation, { 0, 0, 1 }));
        }
        if (best > 1e8f) {
            // Guide laser: a wrist panel is up but this hand isn't aimed at it yet. Show a short beam
            // so the ray can be found and steered onto the panel. Hidden while the touch cursor shows.
            if (!(wristUp && prof->pointer.guide && prof->wrist[panel].press != "poke" && !wv.cursor && dPanel < 0.8f)) continue;
            float len = std::clamp(dPanel * 1.3f, 0.12f, 0.6f);
            P.on = true; P.dot = false;
            P.beamPose = alongRay(o + d * (len * 0.5f), d * -1.f);
            P.beamLen = len;
            continue;
        }
        Vec3 hit = o + d * best;
        Vec3 n = Rotate(bestQ, { 0, 0, 1 });
        P.on = true;
        P.dotPose = { bestQ, X(hit + n * 0.003f) };
        P.dotSize = 0.008f + 0.006f * best;
        P.beamPose = alongRay((o + hit) * 0.5f, n);
        P.beamLen = best;
    }
}

void Engine::ComputeBlocks() {
    bool want[2][C_COUNT]{};
    for (int i = 0; i < (int)bs.size(); i++) {
        if (!bs[i].armed) continue;
        const Binding& b = prof->bindings[i];
        if (b.block == "trigger") { auto& t = b.chord.back(); want[t.hand][t.comp] = true; }
        else if (b.block == "all") for (auto& t : b.chord) want[t.hand][t.comp] = true;
    }
    if (wr.open) {
        const std::string& m = prof->blockWhileOpen;
        if (m == "all") { for (auto& h : want) for (auto& c : h) c = true; }
        else if (m == "hand") { for (int c = 0; c < C_COUNT; c++) want[wr.selHand][c] = want[wr.openerHand][c] = true; }
        else if (m == "minimal") {
            want[wr.selHand][C_STICK] = true;
            if (prof->sel.confirmComp >= 0) want[wr.selHand][prof->sel.confirmComp] = true;
            if (prof->sel.cancelComp >= 0) want[wr.selHand][prof->sel.cancelComp] = true;
        }
    }
    if (kb.visible) for (auto& h : want) for (auto& c : h) c = true;
    if (dockAdjust) { want[0][C_GRIP] = want[1][C_GRIP] = true; if (grab.hand >= 0) want[grab.hand][C_STICK] = true; }
    if (adjust) { // the grips move things around; the grabbing hand's stick resizes
        want[0][C_GRIP] = want[1][C_GRIP] = true;
        if (grab.hand >= 0) want[grab.hand][C_STICK] = true;
    }
    if (screen.hoverHand >= 0) { int h = screen.hoverHand; want[h][C_TRIGGER] = want[h][C_BTN1] = want[h][C_STICK] = true; }
    if (uiHand >= 0) want[uiHand][C_TRIGGER] = true;
    if (offUiHand >= 0) want[offUiHand][C_TRIGGER] = want[offUiHand][C_STICK] = true;
    if (offGrabArmed || offGrabHand >= 0) want[0][C_GRIP] = want[1][C_GRIP] = true; // grab-to-set uses the grips
    for (int h = 0; h < 2; h++) // the pressing (other) hand's trigger belongs to the panel while over a button
        if (wrist[h].visible && wrist[h].hover >= 0) want[1 - h][C_TRIGGER] = true;
    for (int h = 0; h < 2; h++)
        for (int c = 0; c < C_COUNT; c++) {
            // Once blocked, stay blocked until the part is let go so the game never sees a half press.
            if (want[h][c]) sticky[h][c] = true;
            else if (sticky[h][c] && !CompActive(h, c)) sticky[h][c] = false;
            blockOut[h][c] = sticky[h][c];
        }
    for (auto& h : ovOut) for (auto& o : h) o = Override{};
    pulses.erase(std::remove_if(pulses.begin(), pulses.end(), [&](const Pulse& p) { return p.until < now; }), pulses.end());
    for (auto& p : pulses) ovOut[p.hand][p.comp] = p.o;
    for (auto& [k, o] : xrToggles) ovOut[k.first][k.second] = o;
    for (auto& [bi, ko] : xrHolds) ovOut[ko.first.first][ko.first.second] = ko.second;
}

// Adjust mode: the wheel turns see-through so it doesn't block the wrist panels or the screen you are placing.
// Solid again when a hand comes near it (to grab it) or while it is held; fainter still while something else is held.
void Engine::UpdateAdjustFade() {
    float want = 1.f;
    if (adjust && view.visible) {
        if (grab.target == 2) want = 1.f;
        else if (grab.target >= 0) want = 0.12f;
        else {
            float d = 1e9f;
            Vec3 c = V(view.pose.position);
            for (int h = 0; h < 2; h++) // the hand that would grab it (the opening hand carries the wheel along)
                if (fr.h[h].gripValid && !(wr.open && h == wr.openerHand)) d = std::min(d, Len(V(fr.h[h].gripPose.position) - c));
            float edge = view.size * 0.5f;                       // from the wheel's rim
            float t = std::clamp((d - edge - 0.08f) / 0.15f, 0.f, 1.f); // solid within 8 cm of the rim, faint 23 cm away
            want = 1.f - 0.7f * t;
        }
    }
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    float k = std::min(1.f, dt * 12.f); // about a tenth of a second to fade
    view.alpha += (want - view.alpha) * (dt > 0 ? k : 1.f);
    if (std::fabs(view.alpha - want) < 0.01f) view.alpha = want;
    if (std::fabs(view.alpha - wheelAlphaDrawn) > 0.04f || (view.alpha != wheelAlphaDrawn && view.alpha == want)) {
        wheelAlphaDrawn = view.alpha; view.version++;
    }
}

void Engine::Update(const Frame& f) {
    if (!Active()) { memset(blockOut, 0, sizeof(blockOut)); for (auto& h : ovOut) for (auto& o : h) o = Override{}; return; }
    fr = f;
    now = f.t;
    // Pointer correction: turn the aim ray down and move its start toward the index finger.
    // Done once here so the wheel, keyboard, wrist panels and lasers all use the same ray.
    {
        const PointerStyle& P = prof->pointer;
        for (int h = 0; h < 2; h++) {
            HandRaw& hr = fr.h[h];
            bool on = P.correction == "always" || (P.correction == "auto" && hr.touchCtl);
            if (!on || !hr.aimValid) continue;
            XrQuaternionf q = hr.aimPose.orientation;
            float a = -P.pitchDeg * 3.14159265f / 180.f * 0.5f; // about local +X; negative = down
            float s = std::sin(a), w = std::cos(a);
            XrQuaternionf r{ q.w * s + q.x * w, q.y * w + q.z * s, q.z * w - q.y * s, q.w * w - q.x * s }; // q * (s,0,0,w)
            Vec3 pos = V(hr.aimPose.position) + Rotate(q, { 0, -P.downM, 0 }) + Rotate(r, { 0, 0, -P.forwardM });
            hr.aimPose = { r, X(pos) };
        }
        // Steady the laser for shaky hands (wheel, keyboard, wrist buttons, screen mouse all use it).
        float dt = (float)std::clamp(f.t - lastNow, 0.0, 0.25);
        for (int h = 0; h < 2; h++) {
            HandRaw& hr = fr.h[h];
            if (!hr.aimValid) { aimSmooth[h].Reset(); continue; }
            hr.aimPose = aimSmooth[h].Apply(hr.aimPose, dt, P.smoothing);
        }
    }
    ComputeDown();
    ProcessBindings();
    UpdateWheel();
    UpdateAdjustWheel();
    UpdateAdjustFade();
    UpdateAdjustKeyboard();
    UpdateKeyboard();
    UpdateWrist(0);
    UpdateWrist(1);
    UpdateScreen(); // places a docked screen/keyboard too, before the screen takes the pointer
    UpdateKbBar();
    UpdateOffsets();
    ComputeBlocks();
    UpdatePointers();
    lastNow = now;

    view.visible = wr.open;
    if (wr.open) {
        const Wheel* w = wr.stack.back();
        if (view.wheel != w || view.hovered != wr.hovered) { view.wheel = w; view.hovered = wr.hovered; Dirty(); }
        std::string crumb;
        for (size_t i = 0; i < wr.stack.size(); i++) crumb += (i ? " > " : "") + wr.stack[i]->title;
        view.crumb = crumb;
        view.pose = wr.pose;
        std::vector<bool> on;
        for (auto& it : w->items) on.push_back(IsOn(it.action));
        if (on != view.on) { view.on = std::move(on); Dirty(); }
        view.size = prof->style.radius * 2.f;
    }
}

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
    if (grab.target == 3 && !screen.shown) grab = GrabRun{};
    if (!screen.shown) { ApplyDock(); ScreenInteract(); return; }
    ScreenStyle& S = prof->screen;
    ScreenPose();
    if (screen.poseValid && screen.place != "front" && grab.target != 3) {
        float dt = (float)std::clamp(now - lastNow, 0.0, 0.25);
        screen.pose = screenSmooth.Apply(screen.pose, dt, S.smoothing);
    } else screenSmooth.Reset();
    ApplyDock(); // docked: the screen sits at the keyboard (or the keyboard at the screen) before pointing is checked
    ScreenInteract();
    if (!adjust || !screen.poseValid || (grab.target >= 0 && grab.target != 3)) return;
    if (kb.visible && prof->dock.mode != 0) return; // docked: Adjust dock (on the keyboard's bar) places it

    // Adjust mode: a grip near the screen grabs it; it keeps the hand's angle, the stick resizes.
    bool onWrist = screen.place == "wrist_left" || screen.place == "wrist_right";
    int wh = screen.place == "wrist_left" ? 0 : 1;
    Vec3 c = V(screen.pose.position);
    if (grab.target < 0) {
        for (int g = 0; g < 2; g++) {
            if (onWrist && g == wh) continue; // the arm it sits on can't grab it
            const HandRaw& hr = fr.h[g];
            if (!hr.gripValid || !cur[g][C_GRIP][F_PRESS] || prev[g][C_GRIP][F_PRESS]) continue;
            if (Len(V(hr.gripPose.position) - c) < screen.widthM * 0.6f + 0.08f) { StartGrab(3, g, screen.pose); break; }
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
    bool dockedToKb = kb.visible && prof->dock.mode == 1; // it sits above the keyboard then, not on the arm
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
    screen.pickOpen = true; pickPage = 0; pickMode = 0;
    screen.wantChoices = true; // the layer fills the list
    BuildPicker();
}

// The picker's cells for the current page: choices, then page buttons and Close.
static const char* kPlaces[4] = { "wrist_left", "wrist_right", "front", "head" };
static const char* kPlaceNames[4] = { "Left wrist", "Right wrist", "In front of me\nstays where it appears", "Following my view" };

void Engine::BuildPicker() {
    if (pickMode == 1) { // where to put the screen
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
    if (pickMode == 1) {
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

void Engine::BarActivate(int cell) {
    Action a; a.type = ActType::Screen;
    if (adjust) { Action d; d.type = ActType::Adjust; RunAction(d, -1, -1, true); return; } // the bar is just "Done"
    switch (cell) {
    case 0: if (screen.pickOpen && pickMode == 0) { screen.pickOpen = false; screen.uiVersion++; } else OpenPicker(); return;
    case 1: a.mode = "next_fps"; break;
    case 2:
        if (screen.pickOpen && pickMode == 1) { screen.pickOpen = false; screen.uiVersion++; return; }
        screen.pickOpen = true; pickMode = 1; BuildPicker(); return;
    case 4: { // adjust positions: move the screen, wheel and wrist panels by hand
        screen.pickOpen = false; screen.uiVersion++;
        Action d; d.type = ActType::Adjust; RunAction(d, -1, -1, true);
        return;
    }
    case 3: // typing goes to the mirrored window: bring it up first
        screen.pickOpen = false; screen.uiVersion++;
        if (screen.source == "window" && screen.hwnd) Keys::BringToFront(screen.hwnd);
        screenClicked = true; // so the game gets its window back once you're done
        OpenKeyboard(-1);
        return;
    default: a.mode = "off"; screen.pickOpen = false; break;
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
    bool listOpen = screen.pickOpen && pickMode == 0, moveOpen = screen.pickOpen && pickMode == 1;
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
    else if (barHover >= 0) { screen.barPressed = barHover; screen.uiVersion++; BarActivate(barHover); }
}
