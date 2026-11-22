#include "engine.h"
#include "keys.h"
#include <algorithm>

void Engine::SetProfile(std::shared_ptr<Profile> p) {
    Reset();
    prof = std::move(p);
    bs.assign(prof ? prof->bindings.size() : 0, BState{});
    if (prof) Keys::SetFocusWindow(prof->focusWindow);
    if (prof) { // the editor's screen settings apply on every (re)load; on/off survives a reload
        const ScreenStyle& S = prof->screen;
        screen.place = S.place;
        screen.monitor = S.monitor;
        screen.fps = S.fps;
        if (S.source == ScreenSource::Window && screen.source != ScreenSource::Window) {
            screen.source = ScreenSource::Window;
            screen.windowMatch = S.window;
            screen.windowTitle = S.windowTitle;
            screen.windowStep = 0;
            screen.windowAsk++;
        } else if (S.source == ScreenSource::Monitor)
            screen.source = ScreenSource::Monitor;
        if (!screenMirror.run_.init) {
            screen.on = S.startOn;
            screenMirror.run_.placeFront = screen.on;
            screenMirror.run_.init = true;
        } else if (screen.on)
            screenMirror.run_.placeFront = true;
        offs = prof->offsets; // controller offsets: the file's values (also after the editor saves)
        offsetsPanel.run_.sel = offs.linkHands ? 2 : (offsetsPanel.run_.sel == 2 ? 1 : offsetsPanel.run_.sel);
        offsetsPanel.run_.kind = offs.aimSame ? 2 : (offsetsPanel.run_.kind == 2 ? 0 : offsetsPanel.run_.kind);
    }
}

void Engine::Reset() {
    if (wr.open) CloseWheel();
    kb = KeyboardView{};
    keyboard.run_.repeatKey = keyboard.run_.repeatHand = -1;
    for (int h = 0; h < 2; h++) {
        wrist[h] = WristView{};
        wristPanels.run_[h] = {};
    }
    adjust = false;
    grab = GrabRun{};
    adjustClosedAt = -1;
    offView.open = false;
    offView.version++;
    offsetsPanel.run_.grabArmed = false;
    offGrabHand = -1;
    offsetsPanel.run_.uiHand = -1;
    offsetsPanel.run_.saveAt = -1;
    if (screenMirror.run_.mouseL) Keys::MouseButton(0, false);
    if (screenMirror.run_.mouseR) Keys::MouseButton(1, false);
    screenMirror.run_.mouseL = screenMirror.run_.mouseR = false;
    screen.hoverHand = -1;
    Keys::ReleaseAll();
    keyToggles.clear();
    xrToggles.clear();
    xrHolds.clear();
    pulses.clear();
    layers.clear();
    for (auto& b : bs)
        b = BState{};
    memset(blockOut, 0, sizeof(blockOut));
    for (auto& h : ovOut)
        for (auto& o : h)
            o = Override{};
}

bool Engine::IsOn(const Action& a) const {
    switch (a.type) {
    case ActType::Key:
        return a.mode == ActMode::Toggle && keyToggles.count(a.keys) > 0;
    case ActType::XrInput:
        return a.mode == ActMode::Toggle && xrToggles.count({ a.target.hand, a.target.comp }) > 0;
    case ActType::Layer:
        return layers.count(a.layer) > 0;
    case ActType::Screen:
        return (a.mode == ActMode::Toggle || a.mode == ActMode::On) && screen.on && (!a.place || *a.place == screen.place);
    case ActType::Mirror:
        return a.mode != ActMode::Off && mirrorOn;
    case ActType::Offsets:
        return (a.mode == ActMode::Panel && offView.open) || ((a.mode == ActMode::Enable || a.mode == ActMode::On) && offs.enabled);
    default:
        return false;
    }
}

void Engine::Toast(const std::string& s, double sec) {
    if (prof && !prof->notifications) return;
    toast = s;
    toastUntil = NowSec() + sec;
    toastVersion++;
}

void Engine::ComputeDown() {
    memcpy(prev, cur, sizeof(cur));
    memset(cur, 0, sizeof(cur));
    const Profile& p = *prof;
    for (int h = 0; h < 2; h++) {
        const HandRaw& r = fr.h[h];
        auto hyst = [&](int c, int f, bool cond_on, bool cond_hold) {
            cur[h][c][f] = prev[h][c][f] ? cond_hold : cond_on;
        };
        cur[h][C_BTN1][F_PRESS] = r.b1;
        cur[h][C_BTN1][F_TOUCH] = r.b1t || r.b1;
        cur[h][C_BTN2][F_PRESS] = r.b2;
        cur[h][C_BTN2][F_TOUCH] = r.b2t || r.b2;
        hyst(C_TRIGGER, F_PRESS, r.trig > p.pressOn, r.trig > p.pressOff);
        cur[h][C_TRIGGER][F_TOUCH] = r.trigT || cur[h][C_TRIGGER][F_PRESS];
        hyst(C_GRIP, F_PRESS, r.grip > p.pressOn, r.grip > p.pressOff);
        cur[h][C_GRIP][F_TOUCH] = cur[h][C_GRIP][F_PRESS];
        cur[h][C_STICK][F_PRESS] = r.sc;
        cur[h][C_STICK][F_TOUCH] = r.st || r.sc;
        // A direction also has to be within the hand's direction zone (when one is set), so a forward push that
        // leans a little sideways doesn't also count as left or right.
        const StickShape& zone = p.sticks[h];
        bool up = InDirectionZone(zone, r.sx, r.sy, 0, 1), down = InDirectionZone(zone, r.sx, r.sy, 0, -1);
        bool right = InDirectionZone(zone, r.sx, r.sy, 1, 0), left = InDirectionZone(zone, r.sx, r.sy, -1, 0);
        hyst(C_STICK, F_UP, up && r.sy > p.stickOn, up && r.sy > p.stickOff);
        hyst(C_STICK, F_DOWN, down && r.sy < -p.stickOn, down && r.sy < -p.stickOff);
        hyst(C_STICK, F_RIGHT, right && r.sx > p.stickOn, right && r.sx > p.stickOff);
        hyst(C_STICK, F_LEFT, left && r.sx < -p.stickOn, left && r.sx < -p.stickOff);
        cur[h][C_THUMBREST][F_TOUCH] = cur[h][C_THUMBREST][F_PRESS] = r.rest;
        cur[h][C_MENU][F_PRESS] = cur[h][C_MENU][F_TOUCH] = r.menu;
    }
}

bool Engine::CompActive(int h, int c) const {
    const HandRaw& r = fr.h[h];
    switch (c) {
    case C_TRIGGER:
        return r.trig > prof->pressOff * 0.5f;
    case C_GRIP:
        return r.grip > prof->pressOff * 0.5f;
    case C_STICK:
        return r.sc || std::sqrt(r.sx * r.sx + r.sy * r.sy) > 0.25f;
    case C_THUMBREST:
        return false;
    default:
        return cur[h][c][F_PRESS];
    }
}

bool Engine::LayersOk(const Binding& b) const {
    for (auto& l : b.layers)
        if (!layers.count(l)) return false;
    for (auto& l : b.notLayers)
        if (layers.count(l)) return false;
    return true;
}

Override Engine::MakeOv(const InputId& t) {
    Override o;
    o.on = true;
    switch (t.feat) {
    case F_PRESS:
        o.press = true;
        o.touch = true;
        o.value = 1;
        break;
    case F_TOUCH:
        o.touch = true;
        break;
    case F_UP:
        o.y = 1;
        o.touch = true;
        break;
    case F_DOWN:
        o.y = -1;
        o.touch = true;
        break;
    case F_LEFT:
        o.x = -1;
        o.touch = true;
        break;
    case F_RIGHT:
        o.x = 1;
        o.touch = true;
        break;
    }
    return o;
}

void Engine::SetLayer(const std::string& l, bool on) {
    bool had = layers.count(l) > 0;
    if (on == had) return;
    if (on)
        layers.insert(l);
    else
        layers.erase(l);
    Log("layer %s %s", l.c_str(), on ? "on" : "off");
    Toast("Layer " + l + (on ? " ON" : " off"), 1.2);
}

// Returns true when the action keeps running until EndAction (hold behaviour).
bool Engine::RunAction(const Action& a, int hand, int bi, bool fromWheel) {
    bool hold = (a.mode == ActMode::Hold) && !fromWheel;
    switch (a.type) {
    case ActType::Key:
        if (a.mode == ActMode::Toggle) {
            if (keyToggles.count(a.keys)) {
                keyToggles.erase(a.keys);
                Keys::Up(a.keys);
            } else {
                keyToggles.insert(a.keys);
                Keys::Down(a.keys);
            }
            return false;
        }
        if (hold) {
            Keys::Down(a.keys);
            return true;
        }
        Keys::Tap(a.keys, a.tapMs);
        return false;
    case ActType::Sequence:
        Keys::Sequence(a.steps);
        return false;
    case ActType::XrInput: {
        auto key = std::make_pair(a.target.hand, a.target.comp);
        if (a.mode == ActMode::Toggle) {
            if (xrToggles.count(key))
                xrToggles.erase(key);
            else
                xrToggles[key] = MakeOv(a.target);
            return false;
        }
        if (hold) {
            xrHolds[bi] = { key, MakeOv(a.target) };
            return true;
        }
        pulses.push_back({ a.target.hand, a.target.comp, MakeOv(a.target), now + std::max(a.tapMs, 30) / 1000.0 });
        return false;
    }
    case ActType::Layer:
        if (a.mode == ActMode::Hold && !fromWheel) {
            SetLayer(a.layer, true);
            return true;
        }
        if (a.mode == ActMode::On)
            SetLayer(a.layer, true);
        else if (a.mode == ActMode::Off)
            SetLayer(a.layer, false);
        else
            SetLayer(a.layer, !layers.count(a.layer));
        return false;
    case ActType::Wheel:
        if (fromWheel) {
            auto it = prof->wheels.find(a.wheel);
            if (it != prof->wheels.end()) PushWheel(&it->second);
            return false;
        }
        OpenWheel(a, bi, hand);
        return wr.open && wr.confirm == ConfirmMode::Release;
    case ActType::Back:
        if (wr.open) {
            if (wr.stack.size() > 1) {
                wr.stack.pop_back();
                wr.hovered = -1;
                Dirty();
            } else
                CloseWheel();
        }
        return false;
    case ActType::Close:
        if (wr.open) CloseWheel();
        return false;
    case ActType::Reload:
        reloadRequested = true;
        return false;
    case ActType::Keyboard:
        keyboard.Open(bi);
        return false;
    case ActType::Screen:
        ScreenAction(a);
        return false;
    case ActType::Offsets:
        OffsetsAction(a);
        return false;
    case ActType::Mirror:
        mirrorAsk = a.mode == ActMode::On ? MirrorRequest::On : a.mode == ActMode::Off ? MirrorRequest::Off : MirrorRequest::Toggle;
        return false;
    case ActType::Adjust:
        adjust = !adjust;
        grab = GrabRun{};
        if (adjust)
            Toast("Adjust: grip to move, stick to resize. Grab a fingertip to set where you poke", 4.0);
        else {
            saveRequested = true;
            Toast("Positions saved", 2.0);
            if (wr.open) CloseWheel();
        }
        Log("adjust mode %s", adjust ? "on" : "off");
        return false;
    default:
        return false;
    }
}

void Engine::EndAction(const Action& a, int bi) {
    switch (a.type) {
    case ActType::Key:
        Keys::Up(a.keys);
        break;
    case ActType::XrInput:
        xrHolds.erase(bi);
        break;
    case ActType::Layer:
        SetLayer(a.layer, false);
        break;
    case ActType::Wheel:
        if (wr.open && wr.opener == bi && wr.confirm == ConfirmMode::Release) {
            if (wr.hovered >= 0)
                Select(wr.hovered);
            else
                CloseWheel();
        }
        break;
    default:
        break;
    }
}

void Engine::Fire(int bi) {
    const Binding& b = prof->bindings[bi];
    BState& s = bs[bi];
    s.fired = true;
    Log("fire '%s' (%s)", b.name.c_str(), b.chordText.c_str());
    bool holds = RunAction(b.action, b.chord.back().hand, bi, false);
    bool wantsEnd = holds && (b.gesture == Gesture::Press || b.gesture == Gesture::Hold || b.gesture == Gesture::WhileHeld ||
                              b.gesture == Gesture::DoubleTapHold);
    if (wantsEnd)
        s.needEnd = true;
    else if (holds)
        EndAction(b.action, bi);
}

void Engine::EndB(int bi) {
    BState& s = bs[bi];
    if (s.needEnd) {
        s.needEnd = false;
        EndAction(prof->bindings[bi].action, bi);
    }
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
            if (b.gesture == Gesture::Tap && dur * 1000 < b.tapMs) {
                bool otherFired = false;
                for (int j = 0; j < (int)B.size(); j++)
                    if (j != i && bs[j].fired && bs[j].armTime == s.armTime && B[j].chord.back() == trig) otherFired = true;
                if (!otherFired) Fire(i);
            }
            if (b.gesture == Gesture::Hold && !s.fired && b.tapPassthrough)
                pulses.push_back({ trig.hand, trig.comp, MakeOv(trig), now + 0.08 });
            EndB(i);
            continue;
        }
        if (b.gesture == Gesture::Hold && !s.fired && dur * 1000 >= b.holdMs) Fire(i);
    }
    // A double tap on a .touch input only counts touches that never become a real press:
    // pressing the button (e.g. B twice fast to loot) cancels it, so it can't open a wheel.
    // (Thumbrest and menu report touch and press as the same thing, so they're left alone.)
    for (int i = 0; i < (int)B.size(); i++) {
        const InputId& t = B[i].chord.back();
        if (B[i].gesture == Gesture::DoubleTap && t.feat == F_TOUCH && t.comp != C_THUMBREST && t.comp != C_MENU &&
            cur[t.hand][t.comp][F_PRESS])
            bs[i].lastTap = -1;
    }
    // New presses: arm the most specific matching bindings.
    std::vector<int> cand;
    for (int i = 0; i < (int)B.size(); i++) {
        const Binding& b = B[i];
        if (!b.enabled || bs[i].armed) continue;
        const InputId& trig = b.chord.back();
        if (!Pressed(trig)) continue;
        if (wr.open && b.action.type != ActType::Wheel && b.action.type != ActType::Keyboard && b.action.type != ActType::Adjust) continue;
        if (kb.visible && b.action.type != ActType::Keyboard && b.action.type != ActType::Adjust)
            continue; // adjust: place the open keyboard
        if (!LayersOk(b)) continue;
        bool others = true;
        for (size_t k = 0; k + 1 < b.chord.size(); k++)
            if (!Down(b.chord[k])) others = false;
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
        s.armed = true;
        s.armTime = now;
        s.fired = false;
        if (b.gesture == Gesture::Press || b.gesture == Gesture::WhileHeld)
            Fire(i);
        else if (b.gesture == Gesture::DoubleTap || b.gesture == Gesture::DoubleTapHold) {
            if (s.lastTap >= 0 && (now - s.lastTap) * 1000 <= b.doubleTapMs) {
                s.lastTap = -1;
                Fire(i);
            } else
                s.lastTap = now;
        }
    }
}

Vec3 Engine::FingertipPos(int hand) const {
    const HandRaw& hr = fr.h[hand];
    const PointerStyle& P = prof->pointer;
    float inward = hand == 1 ? -P.tipInwardM : P.tipInwardM; // toward the middle: +x for the left hand, -x for the right
    return V(hr.aimPose.position) + Rotate(hr.aimPose.orientation, { inward, P.tipUpM, -P.tipForwardM });
}

// Laser pointers: for each controller, the nearest pointable surface it is aimed at.
void Engine::UpdatePointers() {
    for (int h = 0; h < 2; h++) {
        PointerView& P = pointer[h];
        P.on = false;
        P.finger = false;
        P.dot = true;
        const HandRaw& hr = fr.h[h];
        if (!hr.aimValid || !fr.headValid) continue;
        Vec3 o = V(hr.aimPose.position), d = Rotate(hr.aimPose.orientation, { 0, 0, -1 });
        Vec3 eye = V(fr.head.position);
        // A quad standing along the ray, turned to face the eye (used by the beam and the finger).
        auto alongRay = [&](Vec3 mid, Vec3 fallbackN) {
            Vec3 toEye = eye - mid;
            toEye = toEye - d * Dot(toEye, d);
            Vec3 z = Len(toEye) > 1e-4f ? Norm(toEye) : fallbackN;
            return XrPosef{ QuatFromBasis(Cross(d, z), d, z), X(mid) };
        };
        float best = 1e9f;
        XrQuaternionf bestQ{ 0, 0, 0, 1 };
        // Ray against a square/rect surface (half sizes in metres, with a little margin).
        auto test = [&](const XrPosef& pose, float hw, float hh) {
            Vec3 c = V(pose.position), n = Rotate(pose.orientation, { 0, 0, 1 });
            float den = Dot(d, n);
            if (std::fabs(den) < 1e-4f) return;
            float t = Dot(c - o, n) / den;
            if (t <= 0.01f || t > 3.f || t >= best) return;
            Vec3 l = RotateInv(pose.orientation, (o + d * t) - c);
            if (std::fabs(l.x) > hw * 1.1f || std::fabs(l.y) > hh * 1.1f) return;
            best = t;
            bestQ = pose.orientation;
        };
        if (wr.open && h == wr.selHand && (prof->sel.method == SelectMethod::Point || prof->sel.method == SelectMethod::Both))
            test(wr.pose, prof->style.radius, prof->style.radius);
        if (kb.visible) test(kb.pose, kb.widthM / 2, kb.heightM / 2);
        if (screen.interactive && screen.hoverHand == h)
            test(screen.pose, screen.widthM / 2 * 1.2f, screen.widthM * screen.aspect / 2 * 1.2f);
        if (screenMirror.run_.uiHand == h && screen.pickOpen) test(screen.pickPose, screen.pickW / 2, screen.pickH / 2);
        if (screenMirror.run_.uiHand == h && screen.barVisible) test(screen.barPose, screen.barW / 2, screen.barH / 2);
        if (offView.open && h != offGrabHand) test(offView.pose, offView.w / 2, offView.h / 2);
        int panel = 1 - h; // a hand points at the panel on the other arm
        const WristView& wv = wrist[panel];
        bool wristUp = wv.visible && grab.target != WristGrab(panel);
        float dPanel = wristUp ? Len(V(wv.pose.position) - o) : 1e9f;
        // The laser stays until the fingertip is close enough to take over (no gap where neither shows).
        bool pokeTookOver = wv.cursor && wv.touchDepth < WristView::kPokeTakeoverM;
        if (wristUp && prof->wrist[panel].press != PressMethod::Poke && !pokeTookOver) test(wv.pose, wv.sizeM / 2, wv.sizeM / 2);
        // Fingertip silhouette: where the poke tip is, so you can find your "finger" before it reaches the panel.
        // In Adjust mode both always show, so they can be grabbed and moved.
        if ((prof->pointer.finger && wristUp && dPanel < 0.5f) || adjust) { // on every panel: it also shows where the laser starts
            const float len = 0.028f;                                       // just the tip of the finger
            Vec3 tip = FingertipPos(h);                                     // the same point the wrist panels poke with
            P.finger = true;
            P.fingerLen = len;
            P.fingerW = 0.012f;
            P.fingerPose = alongRay(tip - d * (len * 0.5f), Rotate(wv.pose.orientation, { 0, 0, 1 }));
        }
        if (best > 1e8f) {
            // Guide laser: a wrist panel is up but this hand isn't aimed at it yet. Show a short beam
            // so the ray can be found and steered onto the panel. Hidden while the touch cursor shows.
            if (!(wristUp && prof->pointer.guide && prof->wrist[panel].press != PressMethod::Poke && !pokeTookOver && dPanel < 0.8f))
                continue;
            float len = std::clamp(dPanel * 1.3f, 0.12f, 0.6f);
            P.on = true;
            P.dot = false;
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
        if (b.block == BindingBlock::Trigger) {
            auto& t = b.chord.back();
            want[t.hand][t.comp] = true;
        } else if (b.block == BindingBlock::All)
            for (auto& t : b.chord)
                want[t.hand][t.comp] = true;
    }
    if (wr.open) {
        const BlockWhileOpen m = prof->blockWhileOpen;
        if (m == BlockWhileOpen::All) {
            for (auto& h : want)
                for (auto& c : h)
                    c = true;
        } else if (m == BlockWhileOpen::Hand) {
            for (int c = 0; c < C_COUNT; c++)
                want[wr.selHand][c] = want[wr.openerHand][c] = true;
        } else if (m == BlockWhileOpen::Minimal) {
            want[wr.selHand][C_STICK] = true;
            if (prof->sel.confirmComp >= 0) want[wr.selHand][prof->sel.confirmComp] = true;
            if (prof->sel.cancelComp >= 0) want[wr.selHand][prof->sel.cancelComp] = true;
        }
    }
    if (kb.visible)
        for (auto& h : want)
            for (auto& c : h)
                c = true;
    if (dockAdjust) {
        want[0][C_GRIP] = want[1][C_GRIP] = true;
        if (grab.hand >= 0) want[grab.hand][C_STICK] = true;
    }
    if (adjust) { // the grips move things around; the grabbing hand's stick resizes
        want[0][C_GRIP] = want[1][C_GRIP] = true;
        if (grab.hand >= 0) want[grab.hand][C_STICK] = true;
    }
    if (screen.hoverHand >= 0) {
        int h = screen.hoverHand;
        want[h][C_TRIGGER] = want[h][C_BTN1] = want[h][C_STICK] = true;
    }
    if (screenMirror.run_.uiHand >= 0) want[screenMirror.run_.uiHand][C_TRIGGER] = true;
    if (offsetsPanel.run_.uiHand >= 0) want[offsetsPanel.run_.uiHand][C_TRIGGER] = want[offsetsPanel.run_.uiHand][C_STICK] = true;
    if (offsetsPanel.run_.grabArmed || offGrabHand >= 0) want[0][C_GRIP] = want[1][C_GRIP] = true; // grab-to-set uses the grips
    for (int h = 0; h < 2; h++) // the pressing (other) hand's trigger belongs to the panel while over a button
        if (wrist[h].visible && wrist[h].hover >= 0) want[1 - h][C_TRIGGER] = true;
    for (int h = 0; h < 2; h++)
        for (int c = 0; c < C_COUNT; c++) {
            // Once blocked, stay blocked until the part is let go so the game never sees a half press.
            if (want[h][c])
                sticky[h][c] = true;
            else if (sticky[h][c] && !CompActive(h, c))
                sticky[h][c] = false;
            blockOut[h][c] = sticky[h][c];
        }
    for (auto& h : ovOut)
        for (auto& o : h)
            o = Override{};
    pulses.erase(std::remove_if(pulses.begin(), pulses.end(), [&](const Pulse& p) { return p.until < now; }), pulses.end());
    for (auto& p : pulses)
        ovOut[p.hand][p.comp] = p.o;
    for (auto& [k, o] : xrToggles)
        ovOut[k.first][k.second] = o;
    for (auto& [bi, ko] : xrHolds)
        ovOut[ko.first.first][ko.first.second] = ko.second;
}

void Engine::Update(const Frame& f) {
    if (!Active()) {
        memset(blockOut, 0, sizeof(blockOut));
        for (auto& h : ovOut)
            for (auto& o : h)
                o = Override{};
        return;
    }
    fr = f;
    now = f.t;
    // Pointer correction: turn the aim ray down and move its start toward the index finger.
    // Done once here so the wheel, keyboard, wrist panels and lasers all use the same ray.
    {
        const PointerStyle& P = prof->pointer;
        for (int h = 0; h < 2; h++) {
            HandRaw& hr = fr.h[h];
            bool on = P.correction == LaserCorrection::Always || (P.correction == LaserCorrection::Auto && hr.touchCtl);
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
            if (!hr.aimValid) {
                aimSmooth[h].Reset();
                continue;
            }
            hr.aimPose = aimSmooth[h].Apply(hr.aimPose, dt, P.smoothing);
        }
    }
    ComputeDown();
    ProcessBindings();
    UpdateWheel();
    UpdateAdjustFingertips(); // first: a grip on a fingertip wins over the panels around it
    UpdateAdjustWheel();
    UpdateAdjustFade();
    keyboard.UpdatePlacing();
    keyboard.Update();
    wristPanels.Update(0);
    wristPanels.Update(1);
    screenMirror.Update(); // places a docked screen/keyboard too, before the screen takes the pointer
    keyboard.UpdateBar();
    offsetsPanel.Update();
    ComputeBlocks();
    UpdatePointers();
    lastNow = now;

    view.visible = wr.open;
    if (wr.open) {
        const Wheel* w = wr.stack.back();
        if (view.wheel != w || view.hovered != wr.hovered) {
            view.wheel = w;
            view.hovered = wr.hovered;
            Dirty();
        }
        std::string crumb;
        for (size_t i = 0; i < wr.stack.size(); i++)
            crumb += (i ? " > " : "") + wr.stack[i]->title;
        view.crumb = crumb;
        view.pose = wr.pose;
        std::vector<bool> on;
        for (auto& it : w->items)
            on.push_back(IsOn(it.action));
        if (on != view.on) {
            view.on = std::move(on);
            Dirty();
        }
        view.size = prof->style.radius * 2.f;
    }
}

bool Engine::OffsetPose(int hand, int kind, XrPosef& out) const {
    return offsetsPanel.OffsetPose(hand, kind, out);
}

void Engine::OffsetsAction(const Action& a) {
    offsetsPanel.OffsetsAction(a);
}

void Engine::ScreenAction(const Action& a) {
    screenMirror.ScreenAction(a);
}
