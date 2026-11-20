// The radial wheel: opening it (at the hand or in front of you), hovering by stick or pointing,
// sub-wheels, selecting an item and closing.
#include "engine.h"
#include "keys.h"
#include <algorithm>

static constexpr float kPi = 3.14159265f;

// What a hand-placed wheel is attached to: the hand's position (hand anchors) or the head, turned like the head.
XrPosef Engine::WheelFrame(int hand) const {
    XrPosef f = YawFrame(fr.head);
    const Style& st = prof->style;
    if (IsHandAnchor(st.anchor) && fr.h[hand].gripValid) f.position = fr.h[hand].gripPose.position;
    return f;
}

XrPosef Engine::WheelPose(int hand) const {
    const Style& st = prof->style;
    auto fp = st.poses.find(EnumKey(kWheelAnchorNames, st.anchor));
    if (fp != st.poses.end() && fp->second.set) return PMul(WheelFrame(hand), fp->second.p);
    Vec3 head = V(fr.head.position);
    Vec3 center;
    bool handAnchor = IsHandAnchor(st.anchor);
    if (handAnchor && fr.h[hand].gripValid) {
        Vec3 g = V(fr.h[hand].gripPose.position);
        Vec3 toward = head - g;
        toward.y = 0;
        toward = Norm(toward);
        center = g + Vec3{ 0, st.handUp, 0 } + toward * st.handToward;
    } else {
        Vec3 f = Rotate(fr.head.orientation, { 0, 0, -1 });
        f.y = 0;
        f = Norm(f);
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
    if (wr.open) {
        CloseWheel();
        return;
    } // pressing the opener again closes it
    wr = WheelRun{};
    wr.open = true;
    wr.stack.push_back(&it->second);
    wr.opener = bi;
    wr.openerHand = hand;
    const Selection& sel = prof->sel;
    wr.selHand = sel.hand == SelectHand::Left ? 0 : sel.hand == SelectHand::Right ? 1 : hand;
    wr.confirm = a.confirm;
    if (wr.confirm == ConfirmMode::Auto && bi < 0)
        wr.confirm = ConfirmMode::Button; // opened from a wrist button
    else if (wr.confirm == ConfirmMode::Auto) {
        const Binding& b = prof->bindings[bi];
        const InputId& t = b.chord.back();
        if (t.feat == F_TOUCH)
            wr.confirm = ConfirmMode::StickReturn;
        else if (b.gesture == Gesture::WhileHeld || b.gesture == Gesture::Hold)
            wr.confirm = ConfirmMode::Release;
        else
            wr.confirm = ConfirmMode::Button;
    }
    wr.openTime = now;
    wr.pose = WheelPose(hand);
    wr.locked = IsLockedAnchor(prof->style.anchor);
    // If the stick is already pushed, wait for it to centre first.
    float m = std::hypot(fr.h[wr.selHand].sx, fr.h[wr.selHand].sy);
    wr.stickOut = false;
    if (m > sel.deadzone) wr.hoverSince = -1; // marker: ignore until centred
    Log("wheel '%s' open, confirm=%s", a.wheel.c_str(), EnumKey(kConfirmModeNames, wr.confirm));
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
    if (it.action.type == ActType::Wheel || it.action.type == ActType::Back) {
        RunAction(it.action, wr.selHand, -1, true);
        return;
    }
    if (adjust && it.action.type != ActType::Adjust && it.action.type != ActType::Close && it.action.type != ActType::Keyboard)
        return; // adjust mode: finishing, or the keyboard to place it
    RunAction(it.action, wr.selHand, -1, true);
    if (wr.open && (prof->sel.closeAfterSelect || it.action.type == ActType::Close))
        CloseWheel();
    else
        Dirty();
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
    if (wr.hoverSince < 0) {
        if (mag < sel.deadzone * 0.6f) wr.hoverSince = now;
        out = false;
    }
    bool useStick = sel.method != SelectMethod::Point;
    bool usePoint = sel.method == SelectMethod::Point || sel.method == SelectMethod::Both;
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
    if (useStick && wr.confirm == ConfirmMode::StickReturn && wr.stickOut && !out && wr.hovered >= 0) {
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
    if (wr.confirm == ConfirmMode::Release && wr.hovered >= 0 && sel.submenuDwellMs > 0 &&
        w->items[wr.hovered].action.type == ActType::Wheel && (now - wr.hoverSince) * 1000 >= sel.submenuDwellMs) {
        Select(wr.hovered);
        return;
    }
    if (sel.confirmComp >= 0 && cur[wr.selHand][sel.confirmComp][F_PRESS] && !prev[wr.selHand][sel.confirmComp][F_PRESS]) {
        if (wr.hovered >= 0) {
            Select(wr.hovered);
            return;
        }
    }
    if (sel.cancelComp >= 0 && cur[wr.selHand][sel.cancelComp][F_PRESS] && !prev[wr.selHand][sel.cancelComp][F_PRESS]) {
        if (wr.stack.size() > 1) {
            wr.stack.pop_back();
            wr.hovered = -1;
            Dirty();
        } else
            CloseWheel();
        return;
    }
    if (sel.autoCloseMs > 0 && (now - wr.openTime) * 1000 > sel.autoCloseMs) CloseWheel();
}
