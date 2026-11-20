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

bool OffsetsPanel::OffsetPose(int hand, int kind, XrPosef& out) const {
    if (!e.prof || !e.offs.enabled) return false;
    HandOffset eff = e.offs.Effective(hand, kind);
    if (eff.Zero()) return false;
    out = { EulerQ(eff), { eff.x, eff.y, eff.z } };
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
HandOffset& OffsetsPanel::Edit(int hand, int kind) {
    int h = e.offs.linkHands ? 1 : hand;
    int k = e.offs.aimSame ? 0 : kind;
    return e.offs.o[h][k];
}

void OffsetsPanel::Changed() {
    run_.saveAt = e.now + 1.0; // written a moment after the last change
    if (e.offView.open) Build();
}

// ------------------------------------------------------------ actions
void OffsetsPanel::OffsetsAction(const Action& a) {
    const ActMode m = a.mode;
    if (m == ActMode::Panel) {
        if (e.offView.open) {
            e.offView.open = false;
            run_.grabArmed = false;
            e.offView.version++;
            if (run_.saveAt > 0) run_.saveAt = e.now;
        } else
            Open();
        return;
    }
    if (m == ActMode::Enable || m == ActMode::On || m == ActMode::Off) {
        e.offs.enabled = m == ActMode::On ? true : m == ActMode::Off ? false : !e.offs.enabled;
        e.Toast(e.offs.enabled ? "Controller offsets ON" : "Controller offsets OFF (game sees the controllers as they are)", 2.0);
    } else if (m == ActMode::Reset) {
        for (auto& h : e.offs.o)
            for (auto& o : h)
                o = HandOffset{};
        e.Toast("Controller offsets reset", 2.0);
    } else if (m == ActMode::Grab) {
        run_.grabArmed = true;
        run_.grabArmedAt = e.now;
        e.Toast("Hold grip on a controller, turn it to where it feels right, let go", 5.0);
        return;
    }
    Changed();
}

void OffsetsPanel::Open() {
    if (!e.fr.headValid) return;
    e.offView.open = true;
    XrPosef frame = YawFrame(e.fr.head);
    // a little below eye level, turned to face the eyes exactly (the old fixed tilt leaned the top toward you)
    XrPosef local{ { 0, 0, 0, 1 }, { 0, -0.18f, -0.55f } };
    e.offView.pose = PMul(frame, local);
    e.offView.pose.orientation = BillboardTo(V(e.offView.pose.position), V(e.fr.head.position));
    e.offView.w = 0.42f;
    e.offView.h = 0.52f;
    e.offView.cols = 3;
    run_.repeatCell = -1;
    Build();
}

void OffsetsPanel::Build() {
    std::vector<std::string> L;
    L.push_back(e.offs.linkHands ? "Hand\nBoth (mirrored)" : run_.sel == 0 ? "Hand\nLeft only" : "Hand\nRight only");
    L.push_back(e.offs.aimSame ? "Pose\nGrip + aim" : run_.kind == 1 ? "Pose\nAim (game laser)" : "Pose\nGrip (hand, guns)");
    L.push_back(e.offs.enabled ? "Offsets ON\n(press: off)" : "Offsets OFF\n(press: on)");
    int hand = e.offs.linkHands ? 1 : run_.sel, kind = e.offs.aimSame ? 0 : (run_.kind == 1 ? 1 : 0);
    const HandOffset& o = e.offs.linkHands ? e.offs.o[1][kind] : e.offs.o[hand][kind];
    for (int f = 0; f < 6; f++) {
        bool deg = f < 3;
        L.push_back("\xE2\x88\x92"); // minus sign
        L.push_back(std::string(kFieldName[f]) + "\n" + Fmt(FieldGet(o, f), deg));
        L.push_back("+");
    }
    char st[48];
    snprintf(st, sizeof(st), "Step\n%g\xC2\xB0 / %g cm", kStepDeg[run_.step], kStepCm[run_.step]);
    L.push_back(st);
    L.push_back(e.offGrabHand >= 0 ? "Turning...\nlet go to keep"
                : run_.grabArmed   ? "Now hold grip\non a controller"
                                   : "Grab to set\n(turn by hand)");
    char sm[48];
    if (e.offs.smoothing <= 0.001f)
        snprintf(sm, sizeof(sm), "Steady hands\noff");
    else
        snprintf(sm, sizeof(sm), "Steady hands\n%.2f", e.offs.smoothing);
    L.push_back(sm);
    L.push_back(e.offs.linkHands ? "Reset\n(both hands)" : run_.sel == 0 ? "Reset\nleft hand" : "Reset\nright hand");
    L.push_back("Reset all");
    L.push_back("Done");
    std::string title = "Controller offsets" + (e.prof && !e.prof->sourceName.empty() ? " - " + e.prof->sourceName : std::string());
    if (L != e.offView.labels || title != e.offView.title) {
        e.offView.labels = std::move(L);
        e.offView.title = title;
        e.offView.version++;
    }
}

void OffsetsPanel::PressCell(int cell, bool repeat) {
    float sd = kStepDeg[run_.step], sc = kStepCm[run_.step];
    if (cell >= 3 && cell < 21) {
        int f = (cell - 3) / 3, c = (cell - 3) % 3;
        int hand = e.offs.linkHands ? 1 : run_.sel, kind = e.offs.aimSame ? 0 : (run_.kind == 1 ? 1 : 0);
        HandOffset& o = Edit(hand, kind);
        float v = FieldGet(o, f), step = f < 3 ? sd : sc;
        if (c == 0)
            FieldSet(o, f, v - step);
        else if (c == 2)
            FieldSet(o, f, v + step);
        else if (!repeat)
            FieldSet(o, f, 0.f); // the middle cell resets that one value
        Changed();
        return;
    }
    if (repeat) return;
    switch (cell) {
    case 0: // Both (linked) -> Right -> Left -> Both
        if (e.offs.linkHands) {
            e.offs.linkHands = false;
            run_.sel = 1;
            for (int k = 0; k < 2; k++)
                e.offs.o[0][k] = e.offs.o[1][k].Mirrored();
        } else if (run_.sel == 1)
            run_.sel = 0;
        else {
            e.offs.linkHands = true;
            run_.sel = 2;
        }
        break;
    case 1: // Grip + aim -> Grip -> Aim -> Grip + aim
        if (e.offs.aimSame) {
            e.offs.aimSame = false;
            run_.kind = 0;
            for (int h = 0; h < 2; h++)
                e.offs.o[h][1] = e.offs.o[h][0];
        } else if (run_.kind == 0)
            run_.kind = 1;
        else {
            e.offs.aimSame = true;
            run_.kind = 2;
        }
        break;
    case 2:
        e.offs.enabled = !e.offs.enabled;
        break;
    case 21:
        run_.step = (run_.step + 1) % 3;
        Build();
        return;
    case 22:
        run_.grabArmed = !run_.grabArmed && e.offGrabHand < 0;
        run_.grabArmedAt = e.now;
        Build();
        return;
    case 23: {
        float s = e.offs.smoothing;
        e.offs.smoothing = s < 0.2f ? 0.25f : s < 0.45f ? 0.5f : s < 0.7f ? 0.75f : s < 0.95f ? 1.f : 0.f;
        break;
    }
    case 24: {
        int hand = e.offs.linkHands ? 1 : run_.sel;
        for (int k = 0; k < 2; k++)
            e.offs.o[hand][k] = HandOffset{};
        e.Toast("Offsets reset", 1.5);
        break;
    }
    case 25:
        for (auto& h : e.offs.o)
            for (auto& o : h)
                o = HandOffset{};
        e.Toast("All offsets reset", 1.5);
        break;
    case 26:
        e.offView.open = false;
        run_.grabArmed = false;
        e.offView.version++;
        run_.saveAt = run_.saveAt > 0 ? e.now : -1;
        return;
    default:
        return;
    }
    Changed();
}

// ------------------------------------------------------------ per frame
void OffsetsPanel::Update() {
    float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.1);
    // Grab to set: while the grip is held, the game's hand keeps the angle it had when you grabbed;
    // you turn the real controller to where it feels right, and on letting go that difference is kept.
    // Armed by mistake (the button sits next to Steady hands): give up after 5 s without a grip,
    // so a later grab in the game can't quietly turn the offsets (and the grips go back to the game).
    if (run_.grabArmed && e.offGrabHand < 0 && e.now - run_.grabArmedAt > 5.0) {
        run_.grabArmed = false;
        Build();
        e.Toast("Grab to set: cancelled (no grip within 5 s)", 2.0);
    }
    if (run_.grabArmed && e.offGrabHand < 0) {
        for (int h = 0; h < 2 && e.offGrabHand < 0; h++) {
            if (!e.offs.linkHands && run_.sel != h) continue;
            if (!(e.cur[h][C_GRIP][F_PRESS] && !e.prev[h][C_GRIP][F_PRESS]) || !e.fr.h[h].gripValid) continue;
            e.offGrabHand = h;
            run_.grabArmed = false;
            run_.grabStart = e.fr.h[h].gripPose.orientation;
            for (int k = 0; k < 2; k++)
                run_.grabFrom[k] = e.offs.Effective(h, k);
            if (!e.offs.enabled) {
                e.offs.enabled = true;
            }
            if (e.prof->haptics) e.haptics.push_back({ h, e.prof->hapticAmp, 0.04f });
            e.Toast("Turn the controller, let go of grip to keep it", 3.0);
            if (e.offView.open) Build();
        }
    }
    if (e.offGrabHand >= 0) {
        int h = e.offGrabHand;
        if (!e.cur[h][C_GRIP][F_PRESS]) {
            e.offGrabHand = -1;
            int k = e.offs.aimSame ? 0 : (run_.kind == 1 ? 1 : 0);
            HandOffset eff = e.offs.Effective(h, k);
            char b[128];
            snprintf(b, sizeof(b), "Kept: pitch %+.1f, yaw %+.1f, roll %+.1f", eff.pitch, eff.yaw, eff.roll);
            e.Toast(b, 3.0);
            if (e.prof->haptics) e.haptics.push_back({ h, e.prof->hapticAmp, 0.04f });
            Changed();
        } else if (e.fr.h[h].gripValid) {
            XrQuaternionf rel = QMul(QInv(e.fr.h[h].gripPose.orientation), run_.grabStart);
            int kinds[2], nk = 0;
            if (e.offs.aimSame)
                kinds[nk++] = 0;
            else
                kinds[nk++] = run_.kind == 1 ? 1 : 0;
            for (int i = 0; i < nk; i++) {
                int k = kinds[i];
                HandOffset n = run_.grabFrom[k];
                QEuler(QMul(rel, EulerQ(run_.grabFrom[k])), n);
                // stored form: with linked hands the right hand's values (the left is its mirror)
                HandOffset& dst = Edit(h, k);
                if (e.offs.linkHands && h == 0) n = n.Mirrored();
                dst.pitch = n.pitch;
                dst.yaw = n.yaw;
                dst.roll = n.roll;
            }
            if (e.offView.open) Build();
        }
    }
    if (run_.saveAt > 0 && e.now >= run_.saveAt && e.offGrabHand < 0) {
        run_.saveAt = -1;
        e.offsSaveRequested = true;
    }

    run_.uiHand = -1;
    if (!e.offView.open) return;
    if (e.wr.open || e.kb.visible) return;
    int hover = -1;
    std::vector<Cell> cells;
    Cell hd;
    ListLayout((int)e.offView.labels.size(), e.offView.cols, e.offView.h / e.offView.w, true, cells, hd);
    for (int g = 0; g < 2 && run_.uiHand < 0; g++) {
        if (g == e.offGrabHand) continue;
        float u, v;
        if (!e.PanelRay(g, e.offView.pose, e.offView.w, e.offView.h, u, v)) continue;
        run_.uiHand = g;
        for (int i = 0; i < (int)cells.size(); i++)
            if (u >= cells[i].x && u < cells[i].x + cells[i].w && v >= cells[i].y && v < cells[i].y + cells[i].h) hover = i;
    }
    if (hover != e.offView.hover) {
        if (e.prof->haptics && run_.uiHand >= 0 && hover >= 0) e.haptics.push_back({ run_.uiHand, e.prof->hapticAmp * 0.4f, 0.01f });
        e.offView.hover = hover;
        e.offView.version++;
    }
    if (e.now > run_.flashUntil && e.offView.pressed >= 0) {
        e.offView.pressed = -1;
        e.offView.version++;
    }
    if (run_.uiHand < 0 || hover < 0) {
        run_.repeatCell = -1;
        return;
    }
    int g = run_.uiHand;
    bool down = e.cur[g][C_TRIGGER][F_PRESS], press = down && !e.prev[g][C_TRIGGER][F_PRESS];
    bool isStep = hover >= 3 && hover < 21 && (hover - 3) % 3 != 1;
    if (press) {
        if (e.prof->haptics) e.haptics.push_back({ g, e.prof->hapticAmp, 0.025f });
        run_.flashUntil = e.now + 0.15;
        e.offView.pressed = hover;
        e.offView.version++;
        PressCell(hover, false);
        run_.repeatCell = isStep ? hover : -1;
        run_.repeatAt = e.now + 0.45;
    } else if (down && hover == run_.repeatCell && e.now >= run_.repeatAt) { // held: keeps stepping
        PressCell(hover, true);
        run_.repeatAt = e.now + 0.07;
    } else if (!down)
        run_.repeatCell = -1;
    // The stick works as a slider on the row you point at: push right to raise, left to lower.
    if (hover >= 3 && hover < 21) {
        float sx = e.fr.h[g].sx;
        if (std::fabs(sx) > 0.25f) {
            int f = (hover - 3) / 3;
            int hand = e.offs.linkHands ? 1 : run_.sel, kind = e.offs.aimSame ? 0 : (run_.kind == 1 ? 1 : 0);
            HandOffset& o = Edit(hand, kind);
            float rate = (f < 3 ? kStepDeg[run_.step] : kStepCm[run_.step]) * 8.f; // 8 steps a second at full push
            float s = (std::fabs(sx) - 0.25f) / 0.75f * (sx > 0 ? 1.f : -1.f);
            FieldSet(o, f, FieldGet(o, f) + s * rate * dt);
            Changed();
        }
    }
}
