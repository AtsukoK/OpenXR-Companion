// Adjust mode (Calibrate Positions): grab wheels, wrist panels, the screen and the keyboard with a grip
// and place them by hand. Grabbed things keep the pose they had in the hand.
#include "engine.h"
#include "keys.h"
#include <algorithm>
#include <fstream>
#include <sstream>

// ---------------------------------------------------------------- guided setup of positions
// The editor's "Set up my positions at the start of the next game" writes setup_positions.json { "pending": true }.
// A few seconds after the headset is tracked, adjust mode turns on by itself and a guide card walks through each
// thing to place: grab it with a grip, let go, A = next, B = skip. At the end the positions are saved (like leaving
// adjust mode) and the file says { "pending": false }, so it runs once.
enum SetupStep { S_LeftWrist, S_RightWrist, S_Wheel, S_Screen, S_Keyboard, S_Fingertips };

std::wstring Engine::SetupFile() { return DataDir() + L"\\setup_positions.json"; }

static bool SetupPending() {
    std::ifstream in(Engine::SetupFile(), std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string t = ss.str();
    t.erase(std::remove_if(t.begin(), t.end(), [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }), t.end());
    return t.find("\"pending\":true") != std::string::npos;
}

static bool SetupGrabMatches(int id, GrabTarget g) {
    switch (id) {
    case S_LeftWrist: return g == GrabTarget::LeftWrist;
    case S_RightWrist: return g == GrabTarget::RightWrist;
    case S_Wheel: return g == GrabTarget::Wheel;
    case S_Screen: return g == GrabTarget::Screen;
    case S_Keyboard: return g == GrabTarget::Keyboard;
    case S_Fingertips: return g == GrabTarget::LeftFingertip || g == GrabTarget::RightFingertip;
    }
    return false;
}

void Engine::SetupShow() {
    if (setup.step < 0 || setup.step >= (int)setup.steps.size()) return;
    int id = setup.steps[setup.step];
    const char* body = "";
    switch (id) {
    case S_LeftWrist:
        body = "LEFT WRIST PANEL: hold your left arm up and look at it. Grab the panel with your RIGHT grip and put it where "
               "you like it. Thumbstick up / down while holding it: size.";
        break;
    case S_RightWrist:
        body = "RIGHT WRIST PANEL: hold your right arm up and look at it. Grab the panel with your LEFT grip and put it where "
               "you like it. Thumbstick up / down while holding it: size.";
        break;
    case S_Wheel:
        body = "RADIAL WHEEL: it shows at your hand. Grab it with the other hand's grip and move it where it's easy to reach. "
               "Thumbstick up / down while holding it: size.";
        break;
    case S_Screen:
        body = "SCREEN MIRROR: your desktop shows here. Grab the screen with a grip and put it where you'd like to read it. "
               "Thumbstick up / down while holding it: size.";
        break;
    case S_Keyboard:
        body = "VR KEYBOARD: grab the keyboard with a grip and put it where typing is comfortable. Thumbstick up / down while "
               "holding it: width.";
        break;
    case S_Fingertips:
        body = "FINGERTIPS: a small fingertip shows on each controller. Grab one with the OTHER hand's grip and put it where "
               "your real fingertip is, then the other one. Then touching panel buttons lands right.";
        break;
    }
    guideText = "Set up your positions   (" + std::to_string(setup.step + 1) + " of " + std::to_string(setup.steps.size()) + ")\n" +
                body + "\n" +
                (setup.placed ? "\xE2\x9C\x93 Placed!   A: next   (grab it again to change it)   B: skip"
                              : "A: next   B: skip this one   (the Calibrate button ends it early)");
    guideVersion++;
}

void Engine::SetupFinish(bool completed) {
    setup.step = -1;
    setup.steps.clear();
    guideText.clear();
    guideVersion++;
    if (kb.visible) keyboard.Close();
    if (adjust) {
        adjust = false;
        grab = GrabRun{};
        if (wr.open) CloseWheel();
    }
    saveRequested = true; // keep what was placed
    {
        std::ofstream o(SetupFile(), std::ios::binary | std::ios::trunc);
        o << "{ \"pending\": false }";
    }
    // shown even with notifications off: the user is waiting for it
    toast = completed ? "All set: your positions are saved" : "Setup ended: your positions so far are saved";
    toastUntil = NowSec() + 3.5;
    toastVersion++;
    Log("setup of positions %s", completed ? "done" : "ended early");
}

void Engine::UpdateSetup() {
    if (!setup.checked) {
        if (!fr.headValid) { setup.trackedSince = -1; return; }
        if (setup.trackedSince < 0) setup.trackedSince = now;
        if (now - setup.trackedSince < 4.0) return; // let the game settle first
        setup.checked = true;
        if (!SetupPending()) return;
        setup.steps.clear();
        if (prof->wrist[0].enabled) setup.steps.push_back(S_LeftWrist);
        if (prof->wrist[1].enabled) setup.steps.push_back(S_RightWrist);
        if (!prof->wheels.empty()) setup.steps.push_back(S_Wheel);
        setup.steps.push_back(S_Screen);
        setup.steps.push_back(S_Keyboard);
        setup.steps.push_back(S_Fingertips);
        if (menu.IsOpen()) menu.Close();
        if (wr.open) CloseWheel();
        adjust = true;
        grab = GrabRun{};
        setup.step = 0;
        setup.placed = false;
        setup.prevGrab = GrabTarget::None;
        Log("setup of positions: started (%d steps)", (int)setup.steps.size());
        SetupShow();
        return;
    }
    if (setup.step < 0) return;
    if (!adjust) { SetupFinish(false); return; } // ended with the Calibrate button
    int id = setup.steps[setup.step];
    if (SetupGrabMatches(id, setup.prevGrab) && !grab.Active() && !setup.placed) {
        setup.placed = true;
        SetupShow();
    }
    setup.prevGrab = grab.target;
    auto edge = [&](int h, int c) { return cur[h][c][F_PRESS] && !prev[h][c][F_PRESS]; };
    bool next = edge(0, C_BTN1) || edge(1, C_BTN1), skip = edge(0, C_BTN2) || edge(1, C_BTN2);
    if (!next && !skip) return;
    if (id == S_Keyboard && kb.visible) keyboard.Close();
    setup.step++;
    setup.placed = false;
    setup.prevGrab = GrabTarget::None;
    if (setup.step >= (int)setup.steps.size()) { SetupFinish(true); return; }
    if (setup.steps[setup.step] == S_Keyboard && !kb.visible) keyboard.Open(-1);
    SetupShow();
}

// Adjust mode, wheel: grab the open wheel with the other hand's grip and move it; its stick resizes.
void Engine::UpdateAdjustWheel() {
    if (grab.target == GrabTarget::Wheel && (!adjust || !wr.open)) grab = GrabRun{};
    // Adjust mode shows the main wheel by itself (again shortly after it is closed), following the hand.
    if (adjust && wr.open)
        adjustClosedAt = -1;
    else if (adjust && !kb.visible &&
             IsHandAnchor(
                 prof->style
                     .anchor)) { // wheels in front of you aren't brought out: they got in the way (open one with its button to place it)
        if (adjustClosedAt < 0) adjustClosedAt = now;
        if (now - adjustClosedAt > 0.6 && !prof->wheels.empty()) {
            int hand = fr.h[1].gripValid ? 1 : 0;
            Action a;
            a.type = ActType::Wheel;
            a.confirm = ConfirmMode::Button;
            a.wheel = prof->wheels.count(hand ? "right" : "left") ? (hand ? "right" : "left") : prof->wheels.begin()->first;
            for (auto& b : prof->bindings) // prefer the wheel this hand really opens
                if (b.enabled && b.action.type == ActType::Wheel && !b.chord.empty() && b.chord.back().hand == hand &&
                    prof->wheels.count(b.action.wheel)) {
                    a.wheel = b.action.wheel;
                    break;
                }
            OpenWheel(a, -1, hand);
            wr.locked = false;
            adjustClosedAt = -1;
        }
    }
    if (!adjust || !wr.open || (grab.Active() && grab.target != GrabTarget::Wheel)) return;
    int g = 1 - wr.openerHand;
    const HandRaw& hr = fr.h[g];
    Style& st = prof->style;
    Vec3 c = V(wr.pose.position);
    if (!grab.Active()) {
        bool edge = cur[g][C_GRIP][F_PRESS] && !prev[g][C_GRIP][F_PRESS];
        if (edge && hr.gripValid && Len(V(hr.gripPose.position) - c) < st.radius + 0.08f) StartGrab(GrabTarget::Wheel, g, wr.pose);
        return;
    }
    if (!cur[g][C_GRIP][F_PRESS] || !hr.gripValid) {
        grab = GrabRun{};
        if (prof->haptics) haptics.push_back({ g, prof->hapticAmp * 0.6f, 0.015f });
        return;
    }
    XrPosef obj = GrabbedPose(); // keeps the hand's angle: tilt and turn it however you like
    st.poses[EnumKey(kWheelAnchorNames, st.anchor)] = { true, PMul(PInv(WheelFrame(wr.openerHand)), obj) };
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    st.radius = std::clamp(st.radius * (1.f + hr.sy * dt * 0.8f), 0.05f, 0.4f);
    wr.pose = obj;
}

// Adjust mode, fingertips: both poking fingertips show; grab one with the other hand's grip and put it where your
// real fingertip is. It is stored relative to the laser start (forward, up, toward the middle) and saved on leaving.
void Engine::UpdateAdjustFingertips() {
    bool holding = grab.target == GrabTarget::LeftFingertip || grab.target == GrabTarget::RightFingertip;
    if (!adjust) {
        if (holding) grab = GrabRun{};
        return;
    }
    const float kGrabRangeM = 0.05f;
    if (!grab.Active()) {
        for (int g = 0; g < 2; g++) {
            int t = 1 - g; // the grabbing hand takes the other hand's fingertip
            const HandRaw& gh = fr.h[g];
            bool edge = cur[g][C_GRIP][F_PRESS] && !prev[g][C_GRIP][F_PRESS];
            if (!edge || !gh.gripValid || !fr.h[t].aimValid) continue;
            Vec3 tip = FingertipPos(t);
            if (Len(V(gh.gripPose.position) - tip) > kGrabRangeM) continue;
            StartGrab(FingertipGrab(t), g, { fr.h[t].aimPose.orientation, X(tip) });
            if (prof->haptics) haptics.push_back({ g, prof->hapticAmp * 0.6f, 0.015f });
            return;
        }
        return;
    }
    if (!holding) return;
    int g = grab.hand, t = grab.target == GrabTarget::LeftFingertip ? 0 : 1;
    if (!cur[g][C_GRIP][F_PRESS] || !fr.h[g].gripValid || !fr.h[t].aimValid) {
        grab = GrabRun{};
        if (prof->haptics) haptics.push_back({ g, prof->hapticAmp * 0.6f, 0.015f });
        return;
    }
    // where the fingertip is held, in the pointing hand's laser frame
    const XrPosef& aim = fr.h[t].aimPose;
    Vec3 local = RotateInv(aim.orientation, V(GrabbedPose().position) - V(aim.position));
    PointerStyle& P = prof->pointer;
    P.tipForwardM = std::clamp(-local.z, -0.1f, 0.15f);
    P.tipUpM = std::clamp(local.y, -0.1f, 0.1f);
    P.tipInwardM = std::clamp(t == 1 ? -local.x : local.x, -0.1f, 0.1f);
}

void Engine::StartGrab(GrabTarget target, int hand, const XrPosef& obj) {
    grab = GrabRun{};
    grab.target = target;
    grab.hand = hand;
    grab.rel = PMul(PInv(fr.h[hand].gripPose), obj);
    if (prof->haptics) haptics.push_back({ hand, prof->hapticAmp, 0.02f });
}
XrPosef Engine::GrabbedPose() const {
    return PMul(fr.h[grab.hand].gripPose, grab.rel);
}

// Adjust mode: the wheel turns see-through so it doesn't block the wrist panels or the screen you are placing.
// Solid again when a hand comes near it (to grab it) or while it is held; fainter still while something else is held.
void Engine::UpdateAdjustFade() {
    float want = 1.f;
    if (adjust && view.visible) {
        if (grab.target == GrabTarget::Wheel)
            want = 1.f;
        else if (grab.Active())
            want = 0.12f;
        else {
            float d = 1e9f;
            Vec3 c = V(view.pose.position);
            for (int h = 0; h < 2; h++) // the hand that would grab it (the opening hand carries the wheel along)
                if (fr.h[h].gripValid && !(wr.open && h == wr.openerHand)) d = std::min(d, Len(V(fr.h[h].gripPose.position) - c));
            float edge = view.size * 0.5f;                              // from the wheel's rim
            float t = std::clamp((d - edge - 0.08f) / 0.15f, 0.f, 1.f); // solid within 8 cm of the rim, faint 23 cm away
            want = 1.f - 0.7f * t;
        }
    }
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    float k = std::min(1.f, dt * 12.f); // about a tenth of a second to fade
    view.alpha += (want - view.alpha) * (dt > 0 ? k : 1.f);
    if (std::fabs(view.alpha - want) < 0.01f) view.alpha = want;
    if (std::fabs(view.alpha - wheelAlphaDrawn) > 0.04f || (view.alpha != wheelAlphaDrawn && view.alpha == want)) {
        wheelAlphaDrawn = view.alpha;
        view.version++;
    }
}
