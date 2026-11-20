// Controller offsets for the game: a panel in front of you with +/- buttons (and the stick as a
// slider), grab-to-set (hold grip, turn the controller to where it feels right, let go) and resets.
// The layer applies the result to every controller pose the game asks for (layer.cpp, LocateSpace).
#include "engine.h"
#include <algorithm>

static constexpr float kDeg = 3.14159265f / 180.f;

static XrQuaternionf AxisQ(float x, float y, float z, float deg) {
    float a = deg * kDeg * 0.5f, s = std::sin(a);
    return { x * s, y * s, z * s, std::cos(a) };
}
// yaw about +Y, then pitch about +X, then roll about +Z (all in the controller's frame)
static XrQuaternionf EulerQ(const HandOffset& o) {
    return QMul(QMul(AxisQ(0, 1, 0, o.yaw), AxisQ(1, 0, 0, o.pitch)), AxisQ(0, 0, 1, o.roll));
}
static void QEuler(const XrQuaternionf& q, HandOffset& o) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float m12 = 2 * (y * z - w * x), m02 = 2 * (x * z + w * y), m22 = 1 - 2 * (x * x + y * y);
    float m10 = 2 * (x * y + w * z), m11 = 1 - 2 * (x * x + z * z);
    o.pitch = std::asin(std::clamp(-m12, -1.f, 1.f)) / kDeg;
    o.yaw = std::atan2(m02, m22) / kDeg;
    o.roll = std::atan2(m10, m11) / kDeg;
    auto r = [](float v) {
        return std::round(v * 10.f) / 10.f;
    };
    o.pitch = r(o.pitch);
    o.yaw = r(o.yaw);
    o.roll = r(o.roll);
}

bool Engine::OffsetPose(int hand, int kind, XrPosef& out) const {
    if (!prof || !offs.enabled) return false;
    HandOffset e = offs.Effective(hand, kind);
    if (e.Zero()) return false;
    out = { EulerQ(e), { e.x, e.y, e.z } };
    return true;
}

// ------------------------------------------------------------ fields
static const char* kFieldName[6] = { "Pitch (tip up +)", "Yaw (turn left +)", "Roll (roll left +)",
                                     "Up / down",        "Right / left",      "Forward / back" };
static const float kStepDeg[3] = { 0.5f, 1.f, 5.f }, kStepCm[3] = { 0.2f, 0.5f, 1.f };
// value shown for field f (degrees or cm), forward is -z
static float FieldGet(const HandOffset& o, int f) {
    switch (f) {
    case 0:
        return o.pitch;
    case 1:
        return o.yaw;
    case 2:
        return o.roll;
    case 3:
        return o.y * 100.f;
    case 4:
        return o.x * 100.f;
    default:
        return -o.z * 100.f;
    }
}
static void FieldSet(HandOffset& o, int f, float v) {
    switch (f) {
    case 0:
        o.pitch = std::clamp(v, -90.f, 90.f);
        break;
    case 1:
        o.yaw = std::clamp(v, -90.f, 90.f);
        break;
    case 2:
        o.roll = std::clamp(v, -180.f, 180.f);
        break;
    case 3:
        o.y = std::clamp(v, -30.f, 30.f) / 100.f;
        break;
    case 4:
        o.x = std::clamp(v, -30.f, 30.f) / 100.f;
        break;
    default:
        o.z = -std::clamp(v, -30.f, 30.f) / 100.f;
        break;
    }
}
static std::string Fmt(float v, bool deg) {
    char b[32];
    if (std::fabs(v) < 0.05f) v = 0;
    snprintf(b, sizeof(b), deg ? "%+.1f\xC2\xB0" : "%+.1f cm", v);
    return b;
}

// The stored offset the panel edits: with both hands linked that is the right hand's.
HandOffset& Engine::EditOffset(int hand, int kind) {
    int h = offs.linkHands ? 1 : hand;
    int k = offs.aimSame ? 0 : kind;
    return offs.o[h][k];
}

void Engine::OffsetsChanged() {
    off.saveAt = now + 1.0; // written a moment after the last change
    if (offView.open) BuildOffsetPanel();
}

// ------------------------------------------------------------ actions
void Engine::OffsetsAction(const Action& a) {
    const ActMode m = a.mode;
    if (m == ActMode::Panel) {
        if (offView.open) {
            offView.open = false;
            off.grabArmed = false;
            offView.version++;
            if (off.saveAt > 0) off.saveAt = now;
        } else
            OpenOffsets();
        return;
    }
    if (m == ActMode::Enable || m == ActMode::On || m == ActMode::Off) {
        offs.enabled = m == ActMode::On ? true : m == ActMode::Off ? false : !offs.enabled;
        Toast(offs.enabled ? "Controller offsets ON" : "Controller offsets OFF (game sees the controllers as they are)", 2.0);
    } else if (m == ActMode::Reset) {
        for (auto& h : offs.o)
            for (auto& o : h)
                o = HandOffset{};
        Toast("Controller offsets reset", 2.0);
    } else if (m == ActMode::Grab) {
        off.grabArmed = true;
        off.grabArmedAt = now;
        Toast("Hold grip on a controller, turn it to where it feels right, let go", 5.0);
        return;
    }
    OffsetsChanged();
}

void Engine::OpenOffsets() {
    if (!fr.headValid) return;
    offView.open = true;
    XrPosef frame = YawFrame(fr.head);
    // a little below eye level, turned to face the eyes exactly (the old fixed tilt leaned the top toward you)
    XrPosef local{ { 0, 0, 0, 1 }, { 0, -0.18f, -0.55f } };
    offView.pose = PMul(frame, local);
    offView.pose.orientation = BillboardTo(V(offView.pose.position), V(fr.head.position));
    offView.w = 0.42f;
    offView.h = 0.52f;
    offView.cols = 3;
    off.repeatCell = -1;
    BuildOffsetPanel();
}

void Engine::BuildOffsetPanel() {
    std::vector<std::string> L;
    L.push_back(offs.linkHands ? "Hand\nBoth (mirrored)" : off.sel == 0 ? "Hand\nLeft only" : "Hand\nRight only");
    L.push_back(offs.aimSame ? "Pose\nGrip + aim" : off.kind == 1 ? "Pose\nAim (game laser)" : "Pose\nGrip (hand, guns)");
    L.push_back(offs.enabled ? "Offsets ON\n(press: off)" : "Offsets OFF\n(press: on)");
    int hand = offs.linkHands ? 1 : off.sel, kind = offs.aimSame ? 0 : (off.kind == 1 ? 1 : 0);
    const HandOffset& o = offs.linkHands ? offs.o[1][kind] : offs.o[hand][kind];
    for (int f = 0; f < 6; f++) {
        bool deg = f < 3;
        L.push_back("\xE2\x88\x92"); // minus sign
        L.push_back(std::string(kFieldName[f]) + "\n" + Fmt(FieldGet(o, f), deg));
        L.push_back("+");
    }
    char st[48];
    snprintf(st, sizeof(st), "Step\n%g\xC2\xB0 / %g cm", kStepDeg[off.step], kStepCm[off.step]);
    L.push_back(st);
    L.push_back(offGrabHand >= 0 ? "Turning...\nlet go to keep"
                : off.grabArmed  ? "Now hold grip\non a controller"
                                 : "Grab to set\n(turn by hand)");
    char sm[48];
    if (offs.smoothing <= 0.001f)
        snprintf(sm, sizeof(sm), "Steady hands\noff");
    else
        snprintf(sm, sizeof(sm), "Steady hands\n%.2f", offs.smoothing);
    L.push_back(sm);
    L.push_back(offs.linkHands ? "Reset\n(both hands)" : off.sel == 0 ? "Reset\nleft hand" : "Reset\nright hand");
    L.push_back("Reset all");
    L.push_back("Done");
    std::string title = "Controller offsets" + (prof && !prof->sourceName.empty() ? " - " + prof->sourceName : std::string());
    if (L != offView.labels || title != offView.title) {
        offView.labels = std::move(L);
        offView.title = title;
        offView.version++;
    }
}

void Engine::OffsetCell(int cell, bool repeat) {
    float sd = kStepDeg[off.step], sc = kStepCm[off.step];
    if (cell >= 3 && cell < 21) {
        int f = (cell - 3) / 3, c = (cell - 3) % 3;
        int hand = offs.linkHands ? 1 : off.sel, kind = offs.aimSame ? 0 : (off.kind == 1 ? 1 : 0);
        HandOffset& o = EditOffset(hand, kind);
        float v = FieldGet(o, f), step = f < 3 ? sd : sc;
        if (c == 0)
            FieldSet(o, f, v - step);
        else if (c == 2)
            FieldSet(o, f, v + step);
        else if (!repeat)
            FieldSet(o, f, 0.f); // the middle cell resets that one value
        OffsetsChanged();
        return;
    }
    if (repeat) return;
    switch (cell) {
    case 0: // Both (linked) -> Right -> Left -> Both
        if (offs.linkHands) {
            offs.linkHands = false;
            off.sel = 1;
            for (int k = 0; k < 2; k++)
                offs.o[0][k] = offs.o[1][k].Mirrored();
        } else if (off.sel == 1)
            off.sel = 0;
        else {
            offs.linkHands = true;
            off.sel = 2;
        }
        break;
    case 1: // Grip + aim -> Grip -> Aim -> Grip + aim
        if (offs.aimSame) {
            offs.aimSame = false;
            off.kind = 0;
            for (int h = 0; h < 2; h++)
                offs.o[h][1] = offs.o[h][0];
        } else if (off.kind == 0)
            off.kind = 1;
        else {
            offs.aimSame = true;
            off.kind = 2;
        }
        break;
    case 2:
        offs.enabled = !offs.enabled;
        break;
    case 21:
        off.step = (off.step + 1) % 3;
        BuildOffsetPanel();
        return;
    case 22:
        off.grabArmed = !off.grabArmed && offGrabHand < 0;
        off.grabArmedAt = now;
        BuildOffsetPanel();
        return;
    case 23: {
        float s = offs.smoothing;
        offs.smoothing = s < 0.2f ? 0.25f : s < 0.45f ? 0.5f : s < 0.7f ? 0.75f : s < 0.95f ? 1.f : 0.f;
        break;
    }
    case 24: {
        int hand = offs.linkHands ? 1 : off.sel;
        for (int k = 0; k < 2; k++)
            offs.o[hand][k] = HandOffset{};
        Toast("Offsets reset", 1.5);
        break;
    }
    case 25:
        for (auto& h : offs.o)
            for (auto& o : h)
                o = HandOffset{};
        Toast("All offsets reset", 1.5);
        break;
    case 26:
        offView.open = false;
        off.grabArmed = false;
        offView.version++;
        off.saveAt = off.saveAt > 0 ? now : -1;
        return;
    default:
        return;
    }
    OffsetsChanged();
}

// ------------------------------------------------------------ per frame
void Engine::UpdateOffsets() {
    float dt = (float)std::clamp(now - lastNow, 0.0, 0.1);
    // Grab to set: while the grip is held, the game's hand keeps the angle it had when you grabbed;
    // you turn the real controller to where it feels right, and on letting go that difference is kept.
    // Armed by mistake (the button sits next to Steady hands): give up after 5 s without a grip,
    // so a later grab in the game can't quietly turn the offsets (and the grips go back to the game).
    if (off.grabArmed && offGrabHand < 0 && now - off.grabArmedAt > 5.0) {
        off.grabArmed = false;
        BuildOffsetPanel();
        Toast("Grab to set: cancelled (no grip within 5 s)", 2.0);
    }
    if (off.grabArmed && offGrabHand < 0) {
        for (int h = 0; h < 2 && offGrabHand < 0; h++) {
            if (!offs.linkHands && off.sel != h) continue;
            if (!(cur[h][C_GRIP][F_PRESS] && !prev[h][C_GRIP][F_PRESS]) || !fr.h[h].gripValid) continue;
            offGrabHand = h;
            off.grabArmed = false;
            off.grabStart = fr.h[h].gripPose.orientation;
            for (int k = 0; k < 2; k++)
                off.grabFrom[k] = offs.Effective(h, k);
            if (!offs.enabled) {
                offs.enabled = true;
            }
            if (prof->haptics) haptics.push_back({ h, prof->hapticAmp, 0.04f });
            Toast("Turn the controller, let go of grip to keep it", 3.0);
            if (offView.open) BuildOffsetPanel();
        }
    }
    if (offGrabHand >= 0) {
        int h = offGrabHand;
        if (!cur[h][C_GRIP][F_PRESS]) {
            offGrabHand = -1;
            int k = offs.aimSame ? 0 : (off.kind == 1 ? 1 : 0);
            HandOffset e = offs.Effective(h, k);
            char b[128];
            snprintf(b, sizeof(b), "Kept: pitch %+.1f, yaw %+.1f, roll %+.1f", e.pitch, e.yaw, e.roll);
            Toast(b, 3.0);
            if (prof->haptics) haptics.push_back({ h, prof->hapticAmp, 0.04f });
            OffsetsChanged();
        } else if (fr.h[h].gripValid) {
            XrQuaternionf rel = QMul(QInv(fr.h[h].gripPose.orientation), off.grabStart);
            int kinds[2], nk = 0;
            if (offs.aimSame)
                kinds[nk++] = 0;
            else
                kinds[nk++] = off.kind == 1 ? 1 : 0;
            for (int i = 0; i < nk; i++) {
                int k = kinds[i];
                HandOffset n = off.grabFrom[k];
                QEuler(QMul(rel, EulerQ(off.grabFrom[k])), n);
                // stored form: with linked hands the right hand's values (the left is its mirror)
                HandOffset& dst = EditOffset(h, k);
                if (offs.linkHands && h == 0) n = n.Mirrored();
                dst.pitch = n.pitch;
                dst.yaw = n.yaw;
                dst.roll = n.roll;
            }
            if (offView.open) BuildOffsetPanel();
        }
    }
    if (off.saveAt > 0 && now >= off.saveAt && offGrabHand < 0) {
        off.saveAt = -1;
        offsSaveRequested = true;
    }

    off.uiHand = -1;
    if (!offView.open) return;
    if (wr.open || kb.visible) return;
    int hover = -1;
    std::vector<Cell> cells;
    Cell hd;
    ListLayout((int)offView.labels.size(), offView.cols, offView.h / offView.w, true, cells, hd);
    for (int g = 0; g < 2 && off.uiHand < 0; g++) {
        if (g == offGrabHand) continue;
        float u, v;
        if (!PanelRay(g, offView.pose, offView.w, offView.h, u, v)) continue;
        off.uiHand = g;
        for (int i = 0; i < (int)cells.size(); i++)
            if (u >= cells[i].x && u < cells[i].x + cells[i].w && v >= cells[i].y && v < cells[i].y + cells[i].h) hover = i;
    }
    if (hover != offView.hover) {
        if (prof->haptics && off.uiHand >= 0 && hover >= 0) haptics.push_back({ off.uiHand, prof->hapticAmp * 0.4f, 0.01f });
        offView.hover = hover;
        offView.version++;
    }
    if (now > off.flashUntil && offView.pressed >= 0) {
        offView.pressed = -1;
        offView.version++;
    }
    if (off.uiHand < 0 || hover < 0) {
        off.repeatCell = -1;
        return;
    }
    int g = off.uiHand;
    bool down = cur[g][C_TRIGGER][F_PRESS], press = down && !prev[g][C_TRIGGER][F_PRESS];
    bool isStep = hover >= 3 && hover < 21 && (hover - 3) % 3 != 1;
    if (press) {
        if (prof->haptics) haptics.push_back({ g, prof->hapticAmp, 0.025f });
        off.flashUntil = now + 0.15;
        offView.pressed = hover;
        offView.version++;
        OffsetCell(hover, false);
        off.repeatCell = isStep ? hover : -1;
        off.repeatAt = now + 0.45;
    } else if (down && hover == off.repeatCell && now >= off.repeatAt) { // held: keeps stepping
        OffsetCell(hover, true);
        off.repeatAt = now + 0.07;
    } else if (!down)
        off.repeatCell = -1;
    // The stick works as a slider on the row you point at: push right to raise, left to lower.
    if (hover >= 3 && hover < 21) {
        float sx = fr.h[g].sx;
        if (std::fabs(sx) > 0.25f) {
            int f = (hover - 3) / 3;
            int hand = offs.linkHands ? 1 : off.sel, kind = offs.aimSame ? 0 : (off.kind == 1 ? 1 : 0);
            HandOffset& o = EditOffset(hand, kind);
            float rate = (f < 3 ? kStepDeg[off.step] : kStepCm[off.step]) * 8.f; // 8 steps a second at full push
            float s = (std::fabs(sx) - 0.25f) / 0.75f * (sx > 0 ? 1.f : -1.f);
            FieldSet(o, f, FieldGet(o, f) + s * rate * dt);
            OffsetsChanged();
        }
    }
}
