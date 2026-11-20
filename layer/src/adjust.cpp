// Adjust mode (Calibrate Positions): grab wheels, wrist panels, the screen and the keyboard with a grip
// and place them by hand. Grabbed things keep the pose they had in the hand.
#include "engine.h"
#include "keys.h"
#include <algorithm>

// Adjust mode, wheel: grab the open wheel with the other hand's grip and move it; its stick resizes.
void Engine::UpdateAdjustWheel() {
    if (grab.target == GrabTarget::Wheel && (!adjust || !wr.open)) grab = GrabRun{};
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
    st.poses[st.anchor] = { true, PMul(PInv(WheelFrame(wr.openerHand)), obj) };
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    st.radius = std::clamp(st.radius * (1.f + hr.sy * dt * 0.8f), 0.05f, 0.4f);
    wr.pose = obj;
}

void Engine::StartGrab(GrabTarget target, int hand, const XrPosef& obj) {
    grab = GrabRun{}; grab.target = target; grab.hand = hand;
    grab.rel = PMul(PInv(fr.h[hand].gripPose), obj);
    if (prof->haptics) haptics.push_back({ hand, prof->hapticAmp, 0.02f });
}
XrPosef Engine::GrabbedPose() const { return PMul(fr.h[grab.hand].gripPose, grab.rel); }

// Adjust mode: the wheel turns see-through so it doesn't block the wrist panels or the screen you are placing.
// Solid again when a hand comes near it (to grab it) or while it is held; fainter still while something else is held.
void Engine::UpdateAdjustFade() {
    float want = 1.f;
    if (adjust && view.visible) {
        if (grab.target == GrabTarget::Wheel) want = 1.f;
        else if (grab.Active()) want = 0.12f;
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

