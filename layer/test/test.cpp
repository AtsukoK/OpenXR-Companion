#include <sstream>
#include <fstream>
#include "../src/panel.h"
#include "../src/keys.h"
#include "../src/screen.h"
#include <thread>
#include <cstdio>
#include <objidl.h>
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
using namespace Gdiplus;

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static void SavePng(const std::vector<uint32_t>& px, int w, int h, const wchar_t* path) {
    Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)px.data());
    CLSID png; UINT n = 0, sz = 0; GetImageEncodersSize(&n, &sz);
    std::vector<BYTE> buf(sz); auto* enc = (ImageCodecInfo*)buf.data(); GetImageEncoders(n, sz, enc);
    for (UINT i = 0; i < n; i++) if (wcscmp(enc[i].MimeType, L"image/png") == 0) png = enc[i].Clsid;
    bmp.Save(path, &png);
}

int main(int argc, char** argv) {
    // Always the app's built-in default profile, never the user's own bindings.
    std::string err;
    auto p = ParseProfileText(DefaultProfileJson(), err);
    CHECK(p != nullptr, ("built-in default profile parses " + err).c_str());
    if (!p) return 1;
    p->notifications = true;
    p->sel.method = "thumbstick"; // the tests drive the stick, whatever the user picked
    p->blockWhileOpen = "hand";
    {
        for (auto& b : p->bindings) if (b.action.type == ActType::Wheel) b.enabled = false;
        Binding ob; ob.name = "test wheel"; ob.chordText = "right.thumbrest";
        InputId r; ParseInput("right.thumbrest", r); ob.chord.push_back(r);
        ob.gesture = "double_tap"; ob.doubleTapMs = 400; ob.block = "none";
        ob.action.type = ActType::Wheel; ob.action.wheel = "right"; ob.action.confirm = "stick_return";
        p->bindings.insert(p->bindings.begin(), ob);
    }
    Engine e; e.SetProfile(p);
    Frame f; double t = 0;
    f.headValid = true; f.head.position = { 0, 1.6f, 0 };
    f.h[1].gripValid = true; f.h[1].gripPose.position = { 0.2f, 1.1f, -0.3f };
    auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };

    step(5);
    // double tap the right thumbrest
    f.h[1].rest = true; step(5); f.h[1].rest = false; step(5);
    CHECK(!e.view.visible, "single thumbrest tap does not open");
    f.h[1].rest = true; step(5);
    CHECK(e.view.visible, "double tap thumbrest opens wheel");
    CHECK(e.Blocked(1, C_STICK) && e.Blocked(1, C_TRIGGER), "right hand blocked while open");
    CHECK(!e.Blocked(0, C_TRIGGER), "left hand free while open");
    f.h[1].rest = false; step(3);
    CHECK(e.view.visible, "lifting the thumb keeps stick_return wheel open");
    // hover item 5 (More..., 6 items, up-left = 300 deg)
    int moreIdx = -1, nItems = (int)p->wheels["right"].items.size();
    for (int i = 0; i < nItems; i++) if (p->wheels["right"].items[i].action.type == ActType::Wheel) moreIdx = i;
    float ang = moreIdx * 6.2831853f / nItems;
    f.h[1].sx = std::sin(ang) * 0.95f; f.h[1].sy = std::cos(ang) * 0.95f; step(3);
    CHECK(moreIdx >= 0 && e.view.hovered == moreIdx, "stick toward the submenu item hovers it");
    std::vector<uint32_t> px;
    DrawWheel(px, p->style.texSize, *p, e.view);
    SavePng(px, p->style.texSize, p->style.texSize, L"wheel_main.png");
    f.h[1].sx = 0; f.h[1].sy = 0; step(3);
    CHECK(e.view.visible && e.view.wheel && e.view.wheel->name == "Right hand more (sub-wheel)", "stick return on submenu pushes 'more'");
    {
        auto& mw = p->wheels["Right hand more (sub-wheel)"].items; int bi = 0;
        for (int i = 0; i < (int)mw.size(); i++) if (mw[i].action.type == ActType::Back) bi = i;
        float a2 = bi * 6.2831853f / mw.size();
        f.h[1].sx = std::sin(a2) * 0.95f; f.h[1].sy = std::cos(a2) * 0.95f; step(3);
    }
    DrawWheel(px, p->style.texSize, *p, e.view);
    SavePng(px, p->style.texSize, p->style.texSize, L"wheel_more.png");
    f.h[1].sx = 0; f.h[1].sy = 0; step(3);
    CHECK(e.view.visible && e.view.wheel->name == "right", "selecting Back pops to the first wheel");
    f.h[1].b2 = true; step(2); f.h[1].b2 = false; step(2);
    CHECK(!e.view.visible, "B cancels wheel");
    CHECK(e.Blocked(1, C_BTN2) == false, "block released after B let go");

    // shift layer chord: left grip + left stick click, then A blocked & fires (A in layer)
    {   // the test brings its own layer binding, so the user's edits to default.json cannot break it
        Binding lb; lb.name = "test shift"; lb.chordText = "left.grip+left.thumbstick.click";
        InputId a, b; ParseInput("left.grip", a); ParseInput("left.thumbstick.click", b);
        lb.chord = { a, b }; lb.gesture = "while_held"; lb.block = "trigger";
        lb.action.type = ActType::Layer; lb.action.mode = "hold"; lb.action.layer = "testshift";
        p->bindings.push_back(lb);
        e.SetProfile(p);
    }
    f.h[0].grip = 1.f; step(3);
    CHECK(!e.Blocked(0, C_GRIP), "grip alone passes to game");
    f.h[0].sc = true; step(3);
    CHECK(e.Blocked(0, C_STICK), "layer trigger (stick click) blocked from game");
    CHECK(!e.Blocked(1, C_BTN1), "A not blocked before press");
    f.h[0].sc = false; step(3);
    f.h[0].grip = 0; step(10);
    CHECK(!e.Blocked(0, C_STICK), "stick unblocked after release");

    // ---------------- keyboard
    {
        Binding kbB; kbB.name = "kb"; kbB.chordText = "left.menu";
        InputId m; ParseInput("left.menu", m); kbB.chord.push_back(m);
        kbB.gesture = "press"; kbB.action.type = ActType::Keyboard;
        p->bindings.push_back(kbB);
        e.SetProfile(p);
        f.h[0].menu = true; step(2); f.h[0].menu = false; step(2);
        CHECK(e.kb.visible, "left menu opens keyboard");
        CHECK(e.Blocked(0, C_TRIGGER) && e.Blocked(1, C_TRIGGER), "both triggers hidden from game while typing");
        float uw, uh; const auto& keys = KbLayout(uw, uh);
        auto aimAt = [&](int hand, int key) {
            const KbKey& k = keys[key];
            float u = k.x + k.w / 2, v = k.y + k.h / 2;
            Vec3 local{ (u / uw - 0.5f) * e.kb.widthM, (0.5f - v / uh) * e.kb.heightM, 0 };
            Vec3 target = V(e.kb.pose.position) + Rotate(e.kb.pose.orientation, local);
            Vec3 from{ hand ? 0.15f : -0.15f, 1.25f, -0.15f };
            f.h[hand].aimValid = true;
            f.h[hand].aimPose.position = X(from);
            f.h[hand].aimPose.orientation = BillboardTo(target, from);
        };
        {
            int fk = 0; for (auto& k : keys) if (k.combo.size() >= 2 && k.combo[0] == 'F' && isdigit((unsigned char)k.combo[1])) fk++;
            CHECK(fk == 12 && keys[0].combo == "F1" && keys[11].combo == "F12", "keyboard has an F1-F12 row on top");
        }
        int shiftKey = -1, qKey = -1, closeKey = -1;
        for (int i = 0; i < (int)keys.size(); i++) {
            if (keys[i].kind == KB_SHIFT && shiftKey < 0) shiftKey = i;
            if (keys[i].combo == "q") qKey = i;
            if (keys[i].kind == KB_CLOSE) closeKey = i;
        }
        aimAt(1, shiftKey); aimAt(0, qKey); step(30);
        CHECK(e.kb.hover[1] == shiftKey, "right ray hovers Shift");
        CHECK(e.kb.hover[0] == qKey, "left ray hovers Q");
        {
            const KbKey& k = keys[qKey];
            Vec3 want = V(e.kb.pose.position) + Rotate(e.kb.pose.orientation, { ((k.x + k.w / 2) / uw - 0.5f) * e.kb.widthM, (0.5f - (k.y + k.h / 2) / uh) * e.kb.heightM, 0 });
            CHECK(e.pointer[0].on && e.pointer[1].on && Len(V(e.pointer[0].dotPose.position) - want) < 0.01f, "keyboard: both controllers show a pointer, the dot sits on the aimed key");
            CHECK(e.pointer[0].beamLen > 0.1f, "keyboard: the pointer beam reaches from the controller to the key");
        }
        f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        CHECK(e.kb.shift, "trigger on Shift latches shift");
        std::vector<uint32_t> kpx; int KW = 1500, KH = (int)(KW * uh / uw);
        DrawKeyboard(kpx, KW, KH, e.kb, p->style);
        SavePng(kpx, KW, KH, L"keyboard.png");
        aimAt(1, closeKey); step(3);
        f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(2);
        CHECK(!e.kb.visible, "Close key closes keyboard");
        f.h[0].menu = true; step(2); f.h[0].menu = false; step(2);
        f.h[1].b2 = true; step(2); f.h[1].b2 = false; step(2);
        CHECK(!e.kb.visible, "B closes keyboard");
    }

    // ---------------- wrist panel (left inner forearm, poked by the right hand)
    {
        Wheel tw; tw.name = "wristtest"; tw.title = "Wrist";
        const char* labels[] = { "Map", "Inventory", "Keyboard", "Save", "More" };
        for (int i = 0; i < 5; i++) {
            WheelItem it; it.label = labels[i];
            it.action.type = i == 2 ? ActType::Keyboard : ActType::Key; it.action.keys = "F13";
            if (i == 4) { it.action.type = ActType::Page; it.action.page = "second"; }
            tw.items.push_back(it);
        }
        Wheel second; second.name = "second"; second.title = "Second";
        { WheelItem it; it.label = "Esc"; it.action.type = ActType::Key; it.action.keys = "F13"; second.items.push_back(it); }
        p->wrist[0].pages["wristtest"] = tw; p->wrist[0].pages["second"] = second;
        p->wrist[0].enabled = true; p->wrist[0].start = "wristtest";
        p->wrist[0].face = "inner"; p->wrist[0].press = "both";
        e.SetProfile(p);
        f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} }; // looking straight ahead (-Z)
        // Left grip turned so the palm side faces the eyes, 0.4 m in front.
        f.h[0].gripValid = true;
        f.h[0].gripPose = XrPosef{ {0, -0.70710678f, 0, 0.70710678f}, {0, 1.6f, -0.4f} };
        f.h[1].aimValid = true; f.h[1].aimPose = XrPosef{ {0,0,0,1}, {0.3f, 1.2f, -0.3f} }; // right hand away
        step(5);
        CHECK(!e.wrist[0].visible, "wrist panel waits for the look to last");
        step(30);
        CHECK(e.wrist[0].visible, "turned wrist + look shows the wrist panel");
        std::vector<uint32_t> wpx; DrawWrist(wpx, 512, *p, p->wrist[0], e.wrist[0]);
        SavePng(wpx, 512, 512, L"wrist_panel.png");

        // Poke the third cell (Keyboard) from the front.
        std::vector<Cell> cells; Cell hd; WristLayout(5, p->wrist[0].columns, cells, hd);
        const Cell& c = cells[2];
        float u = c.x + c.w / 2, v = c.y + c.h / 2;
        auto tipAt = [&](float depth) {
            Vec3 l{ (u - 0.5f) * p->wrist[0].size, (0.5f - v) * p->wrist[0].size, depth + 0.015f };
            f.h[1].aimPose.orientation = e.wrist[0].pose.orientation; // pointing into the panel
            f.h[1].aimPose.position = X(V(e.wrist[0].pose.position) + Rotate(e.wrist[0].pose.orientation, l));
        };
        tipAt(0.03f); step(30);
        CHECK(e.wrist[0].hover == 2, "finger in front of a button hovers it");
        CHECK(!e.kb.visible, "hovering alone does not press");
        CHECK(e.wrist[0].cursor && e.wrist[0].cursorLevel > 0 && e.wrist[0].cursorLevel < 8, "touch cursor shows while the finger is close");
        {
            Vec3 lc = RotateInv(e.wrist[0].pose.orientation, V(e.wrist[0].cursorPose.position) - V(e.wrist[0].pose.position));
            CHECK(std::fabs(lc.x - (u - 0.5f) * p->wrist[0].size) < 0.002f && std::fabs(lc.y - (0.5f - v) * p->wrist[0].size) < 0.002f, "touch cursor sits under the fingertip");
            float sizeFar = e.wrist[0].cursorSize; tipAt(0.005f); step(1);
            CHECK(e.wrist[0].cursorSize < sizeFar && e.wrist[0].cursorLevel >= 7, "touch cursor shrinks as the finger closes in");
            std::vector<uint32_t> cpx; DrawCursor(cpx, 128, 4, p->style); SavePng(cpx, 128, 128, L"cursor_near.png");
            DrawCursor(cpx, 128, 8, p->style); SavePng(cpx, 128, 128, L"cursor_touch.png");
        }
        tipAt(-0.01f); step(2);
        CHECK(e.kb.visible, "poking through the button runs its action (keyboard)");
        CHECK(!e.wrist[0].visible, "wrist panel steps aside for the keyboard");
        f.h[1].b2 = true; step(2); f.h[1].b2 = false; step(2);

        // Laser at the More cell (index 4) + trigger opens the second page with a Back cell.
        step(40);
        {
            const Cell& m = cells[4];
            Vec3 l{ (m.x + m.w / 2 - 0.5f) * p->wrist[0].size, (0.5f - (m.y + m.h / 2)) * p->wrist[0].size, 0.25f };
            f.h[1].aimPose.orientation = e.wrist[0].pose.orientation;
            f.h[1].aimPose.position = X(V(e.wrist[0].pose.position) + Rotate(e.wrist[0].pose.orientation, l));
            step(3);
            f.h[1].trig = 1; step(2); f.h[1].trig = 0; step(3);
            CHECK(e.wrist[0].visible && e.wrist[0].wheel && e.wrist[0].wheel->name == "second" && e.wrist[0].nested, "page button opens its page, with Back");
        }

        // Looking away hides it; the back of the wrist does not show the inner panel.
        f.h[1].aimPose.position = { 0.3f, 1.2f, -0.3f };
        step(40);
        CHECK(e.wrist[0].visible, "panel back after the keyboard closes");
        f.head.orientation = { 0, 0.5f, 0, 0.8660254f }; // head turned 60 degrees left
        step(50);
        CHECK(!e.wrist[0].visible, "looking away hides the wrist panel");
        f.head.orientation = { 0, 0, 0, 1 };
        f.h[0].gripPose.orientation = { 0, 0.70710678f, 0, 0.70710678f }; // palm facing away
        step(50);
        CHECK(!e.wrist[0].visible, "arm turned the other way keeps it hidden");

        // Right forearm, mirroring the left panel's pages.
        p->wrist[1] = p->wrist[0]; p->wrist[1].pages.clear(); p->wrist[1].mirror = true;
        e.SetProfile(p);
        f.h[0].gripValid = false;
        f.h[0].aimValid = true; f.h[0].aimPose = XrPosef{ {0,0,0,1}, {-0.3f, 1.2f, -0.3f} };
        f.h[1].gripValid = true;
        f.h[1].gripPose = XrPosef{ {0, 0.70710678f, 0, 0.70710678f}, {0, 1.6f, -0.4f} }; // palm side toward the eyes
        step(40);
        CHECK(e.wrist[1].visible && e.wrist[1].wheel && e.wrist[1].wheel->name == "wristtest", "right forearm shows the mirrored left pages");
        CHECK(!e.wrist[0].visible, "left panel stays hidden meanwhile");

        // Right forearm with its own pages.
        Wheel own; own.name = "rmain"; own.title = "Right";
        { WheelItem it; it.label = "Jump"; it.action.type = ActType::Key; it.action.keys = "F13"; own.items.push_back(it); }
        p->wrist[1].mirror = false; p->wrist[1].pages["rmain"] = own; p->wrist[1].start = "rmain";
        e.SetProfile(p);
        step(40);
        CHECK(e.wrist[1].visible && e.wrist[1].wheel && e.wrist[1].wheel->name == "rmain", "right forearm shows its own pages");
        std::vector<uint32_t> rpx; DrawWrist(rpx, 512, *p, p->wrist[1], e.wrist[1]);
        SavePng(rpx, 512, 512, L"wrist_right.png");

        // Left forearm mirroring the right panel's own pages.
        p->wrist[0].mirror = true;
        e.SetProfile(p);
        f.h[1].gripValid = false;
        f.h[1].aimValid = true; f.h[1].aimPose = XrPosef{ {0,0,0,1}, {0.3f, 1.2f, -0.3f} };
        f.h[0].gripValid = true;
        f.h[0].gripPose = XrPosef{ {0, -0.70710678f, 0, 0.70710678f}, {0, 1.6f, -0.4f} };
        step(40);
        CHECK(e.wrist[0].visible && e.wrist[0].wheel && e.wrist[0].wheel->name == "rmain", "left forearm can mirror the right panel");
    }

    // Index-style opener: double touch of B. Pressing B twice fast (looting) must not open it.
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        for (auto& b : q->bindings) if (b.action.type == ActType::Wheel) b.enabled = false;
        Binding ob; ob.name = "touch wheel"; ob.chordText = "right.secondary.touch";
        InputId r; ParseInput("right.secondary.touch", r); ob.chord.push_back(r);
        ob.gesture = "double_tap"; ob.doubleTapMs = 400; ob.block = "none";
        ob.action.type = ActType::Wheel; ob.action.wheel = "right"; ob.action.confirm = "stick_return";
        q->bindings.insert(q->bindings.begin(), ob);
        Engine e; e.SetProfile(q);
        Frame f; double t = 0;
        f.headValid = true; f.head.position = { 0, 1.6f, 0 };
        f.h[1].gripValid = true; f.h[1].gripPose.position = { 0.2f, 1.1f, -0.3f };
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(5);
        for (int k = 0; k < 2; k++) {   // two quick real presses: touch first, then click
            f.h[1].b2t = true; step(1); f.h[1].b2 = true; step(3); f.h[1].b2 = false; f.h[1].b2t = false; step(4);
        }
        CHECK(!e.view.visible, "pressing B twice fast does not open the double-touch wheel");
        step(60);
        f.h[1].b2t = true; step(4); f.h[1].b2t = false; step(4); f.h[1].b2t = true; step(4);
        CHECK(e.view.visible, "touching B twice without pressing opens the wheel");
    }

    // Adjust mode: grab a wrist panel / the open wheel with the grip, move and resize it, then save.
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        Binding ab; ab.name = "adjust"; ab.chordText = "left.x";
        InputId ai; ParseInput("left.x", ai); ab.chord.push_back(ai);
        ab.gesture = "press"; ab.block = "trigger"; ab.action.type = ActType::Adjust;
        q->bindings.push_back(ab);
        float elbow0 = q->wrist[0].towardElbow, size0 = q->wrist[0].size, down0 = q->style.headDown, dist0 = q->style.distance, rad0 = q->style.radius;
        Engine e; e.SetProfile(q);
        Frame f; double t = 0;
        f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        // Left arm down and away from the face: normally no panel.
        f.h[0].gripValid = true; f.h[0].gripPose = XrPosef{ {0,0,0,1}, {-0.3f, 1.0f, -0.2f} };
        f.h[1].gripValid = f.h[1].aimValid = true; f.h[1].gripPose = XrPosef{ {0,0,0,1}, {0.3f, 1.0f, -0.2f} }; f.h[1].aimPose = f.h[1].gripPose;
        auto step = [&](int frames = 1) { for (int i = 0; i < frames; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step(30);
        CHECK(!e.wrist[0].visible, "adjust: panel hidden before adjust mode");
        f.h[0].b1 = true; step(2); f.h[0].b1 = false; step(3);
        CHECK(e.adjust && e.wrist[0].visible && e.wrist[0].adjust, "adjust: mode on shows the wrist panel without looking at it");
        // Right grip grabs the left panel and moves 4 cm along +Z of the left grip (toward the elbow).
        Vec3 pc = V(e.wrist[0].pose.position);
        f.h[1].gripPose.position = X(pc + Vec3{ 0.01f, 0, 0 }); f.h[1].aimPose = f.h[1].gripPose; step(2);
        f.h[1].grip = 1; step(2);
        CHECK(e.Blocked(1, C_GRIP), "adjust: the grabbing grip is hidden from the game");
        f.h[1].gripPose.position = X(V(f.h[1].gripPose.position) + Vec3{ 0, 0, 0.04f }); f.h[1].aimPose = f.h[1].gripPose;
        f.h[1].sy = 1; step(10); f.h[1].sy = 0;
        XrQuaternionf q0 = e.wrist[0].pose.orientation;
        f.h[1].gripPose.orientation = { 0, 0, std::sin(0.2618f), std::cos(0.2618f) }; step(3); // turn the grabbing hand 30 degrees
        XrQuaternionf dq = QMul(e.wrist[0].pose.orientation, QInv(q0));
        float turned = 2.f * std::acos(std::clamp(std::fabs(dq.w), 0.f, 1.f)) * 57.2958f;
        CHECK(turned > 27.f && turned < 33.f, ("adjust: the panel turns with the grabbing hand, turned " + std::to_string(turned)).c_str());
        f.h[1].gripPose.orientation = { 0, 0, 0, 1 }; step(3);
        {
            XrPosef rel = PMul(PInv(f.h[0].gripPose), e.wrist[0].pose);
            CHECK(q->wrist[0].pose.set && std::fabs(rel.position.z - (q->wrist[0].pose.p.position.z)) < 0.002f, "adjust: the panel's place is kept relative to its arm");
        }
        (void)elbow0;
        CHECK(q->wrist[0].size > size0, "adjust: stick up while holding makes the panel bigger");
        CHECK(std::fabs(V(e.wrist[0].pose.position).z - (pc.z + 0.04f)) < 0.003f, "adjust: the panel follows the hand");
        f.h[1].grip = 0; step(3);
        // Open the right wheel (double tap thumbrest), grab it with the left grip, lift it 5 cm.
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.3f };
        step(70);
        CHECK(e.view.visible && e.wrist[0].visible, "adjust: the main wheel shows by itself, next to the wrist panels");
        Vec3 wc = V(e.view.pose.position);
        f.h[0].gripPose.position = X(wc); step(2);
        f.h[0].grip = 1; step(2);
        f.h[0].gripPose.position = X(wc + Vec3{ 0, 0.05f, 0 }); f.h[0].sy = -1; step(10); f.h[0].sy = 0;
        CHECK(q->style.poses.count(q->style.anchor) && q->style.poses[q->style.anchor].set && std::fabs(V(e.view.pose.position).y - (wc.y + 0.05f)) < 0.004f,
              "adjust: dragging the wheel up raises it and keeps that spot");
        (void)down0; (void)dist0;
        CHECK(q->style.radius < rad0, "adjust: stick down while holding makes the wheel smaller");
        f.h[0].grip = 0; step(3);
        f.h[0].b1 = true; step(2); f.h[0].b1 = false; step(3);
        CHECK(!e.adjust && e.saveRequested && !e.view.visible, "adjust: using it again leaves adjust mode, closes the wheel and asks to save");
    }

    // Adjust mode saving: write the placement into a (temporary) profile file and read it back.
    {
        std::wstring path = ProfilesDir() + L"\\vro_save_test.json";
        { std::ofstream o(path, std::ios::binary); o << DefaultProfileJson(); }
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->sourceName = "vro_save_test.json";
        q->style.handUp = 0.17f; q->style.radius = 0.15f; q->wrist[0].towardElbow = 0.2f; q->wrist[0].side = -0.01f; q->wrist[1].size = 0.13f;
        q->wrist[0].textUp = "custom"; q->wrist[0].rotateDeg = 33.f;
        std::string serr;
        bool ok = SavePlacement(*q, serr);
        std::ifstream in(path, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); in.close();
        auto back = ParseProfileText(ss.str(), err);
        CHECK(ok && back && std::fabs(back->style.handUp - 0.17f) < 1e-4f && std::fabs(back->style.radius - 0.15f) < 1e-4f
              && std::fabs(back->wrist[0].towardElbow - 0.2f) < 1e-4f && std::fabs(back->wrist[0].side + 0.01f) < 1e-4f
              && std::fabs(back->wrist[1].size - 0.13f) < 1e-4f && back->wrist[0].textUp == "custom" && std::fabs(back->wrist[0].rotateDeg - 33.f) < 0.1f && back->wheels.size() == q->wheels.size(),
              ("adjust: positions are saved to the profile file and the rest is kept " + serr).c_str());
        DeleteFileW(path.c_str());
    }

    // ---------------- mirrored screen: actions
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.fpsSteps = { 15, 30, 60, 0 }; q->screen.fps = 30;
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head.orientation.w = 1; f.h[0].gripValid = true; f.h[0].gripPose.orientation.w = 1;
        double t = 0; auto step = [&] { t += 0.011; f.t = t; e.Update(f); };
        Action tg; tg.type = ActType::Screen; tg.mode = "toggle"; tg.place = "wrist_left";
        CHECK(!e.screen.on, "screen: off at start (start_on false)");
        e.ScreenAction(tg); step();
        CHECK(e.screen.on && e.screen.place == "wrist_left" && e.screen.poseValid && e.IsOn(tg), "screen: wrist button turns it on, on the left wrist");
        Action fr2 = tg; fr2.place = "front";
        e.ScreenAction(fr2); step();
        CHECK(e.screen.on && e.screen.place == "front" && e.screen.poseValid && !e.IsOn(tg) && e.IsOn(fr2), "screen: a button for another place moves it there");
        CHECK(e.screen.pose.position.z < -0.5f, "screen: front is placed in front of the head");
        e.ScreenAction(fr2); step();
        CHECK(!e.screen.on && !e.screen.poseValid, "screen: the same button turns it off");
        Action nf; nf.type = ActType::Screen; nf.mode = "next_fps";
        e.ScreenAction(nf); CHECK(e.screen.fps == 60, "screen: next frame rate 30 -> 60");
        e.ScreenAction(nf); CHECK(e.screen.fps == 0, "screen: next frame rate 60 -> every frame");
        e.ScreenAction(nf); CHECK(e.screen.fps == 15, "screen: next frame rate wraps to 15");
        e.screen.monitorCount = 3; e.screen.primaryIndex = 2; e.screen.monitor = 0;
        Action nm; nm.type = ActType::Screen; nm.mode = "next_monitor";
        e.ScreenAction(nm); CHECK(e.screen.monitor == 3 && e.screen.on, "screen: next monitor from the main one (2 of 3) is 3, and shows it");
        e.ScreenAction(nm); CHECK(e.screen.monitor == 1, "screen: next monitor wraps 3 -> 1");
        nm.mode = "prev_monitor"; e.ScreenAction(nm); CHECK(e.screen.monitor == 3, "screen: previous monitor wraps 1 -> 3");
        Action np; np.type = ActType::Screen; np.mode = "next_place";
        e.screen.place = "head"; e.ScreenAction(np); CHECK(e.screen.place == "wrist_left", "screen: next place cycles head -> left wrist");
        std::string perr;
        auto bad = ParseProfileText(R"({"bindings":[{"name":"x","input":"left.x","action":{"type":"screen","place":"ceiling"}}]})", perr);
        CHECK(!bad, "screen: unknown place is a profile error");
    }
    // ---------------- mirrored screen: adjust mode
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.place = "front"; q->screen.distance = 0.9f; q->screen.below = 0.05f; q->screen.width = 0.8f;
        q->wheels.clear(); // keep adjust mode from opening a wheel in the way
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head.orientation.w = 1; f.head.position = { 0, 1.6f, 0 };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose.orientation.w = f.h[h].aimPose.orientation.w = 1; }
        f.h[0].gripPose.position = { -0.3f, 1.0f, -0.2f };
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.2f };
        double t = 0; auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
        step();
        CHECK(!e.screen.shown, "screen adjust: hidden while off");
        Action adj; adj.type = ActType::Adjust;
        e.ScreenAction(Action{}); // no-op guard
        // turn adjust mode on through the engine's own action path
        Action on; on.type = ActType::Screen; on.mode = "off"; e.ScreenAction(on);
        q->bindings.clear();
        Binding b; b.name = "adj"; InputId i; ParseInput("left.menu", i); b.chord.push_back(i); b.chordText = "left.menu"; b.action = adj; b.block = "none";
        q->bindings.push_back(b); e.SetProfile(q);
        f.h[0].menu = true; step(); f.h[0].menu = false; step();
        CHECK(e.adjust && e.screen.shown && e.screen.poseValid, "screen adjust: shown in adjust mode even while off");
        Vec3 c = V(e.screen.pose.position);
        f.h[1].gripPose.position = X(c + Vec3{ 0.05f, 0, 0 }); step();
        f.h[1].grip = 1.f; step(2);
        f.h[1].gripPose.position = X(c + Vec3{ 0.05f, -0.2f, 0.3f }); f.h[1].sy = 1.f; step(10);
        f.h[1].grip = 0.f; f.h[1].sy = 0.f; step();
        Vec3 moved = V(e.screen.pose.position) - c;
        CHECK(q->screen.poses.count("front") && Len(moved - Vec3{ 0, -0.2f, 0.3f }) < 0.01f && q->screen.width > 0.8f,
              "screen adjust: grip moves it anywhere (x, y, z), stick makes it bigger");
        f.h[0].menu = true; step(); f.h[0].menu = false; step();
        CHECK(!e.adjust && !e.screen.shown && e.saveRequested, "screen adjust: finishing hides it again and saves");
    }
    // ---------------- mirrored screen: pointing at it works the desktop mouse
    {
        Keys::SetDryRun(true);
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.place = "front"; q->screen.distance = 1.0f; q->screen.below = 0.f; q->screen.width = 1.0f;
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose.orientation.w = f.h[h].aimPose.orientation.w = 1; }
        f.h[0].gripPose.position = f.h[0].aimPose.position = { -0.3f, 1.0f, -0.2f };
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.2f };
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f }; // straight at the middle of the screen
        double t = 0; auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
        e.screen.monX = -1920; e.screen.monY = 0; e.screen.monW = 1920; e.screen.monH = 1080; e.screen.aspect = 9.f / 16.f;
        Action on; on.type = ActType::Screen; on.mode = "on"; e.ScreenAction(on);
        step(2);
        int x, y, wh, fo; bool l, r; Keys::DryState(x, y, l, r, wh, fo);
        CHECK(e.screen.hoverHand == 1 && std::abs(x - (-960)) <= 2 && std::abs(y - 540) <= 2, ("screen use: pointing at the middle puts the mouse in the middle of that monitor (" + std::to_string(x) + ", " + std::to_string(y) + ")").c_str());
        CHECK(e.Blocked(1, C_TRIGGER) && e.Blocked(1, C_BTN1) && e.pointer[1].on, "screen use: the pointing hand's trigger and A go to the desktop, and the laser shows");
        f.h[1].trig = 1.f; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(l, "screen use: trigger holds the left mouse button");
        f.h[1].aimPose.position = { 0.25f, 1.6f, -0.2f }; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(l && x > -960 + 400, "screen use: dragging moves the mouse with the button held");
        f.h[1].trig = 0.f; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(!l, "screen use: letting go of the trigger lets go of the button");
        f.h[1].b1 = true; step(2); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(r, "screen use: A holds the right mouse button");
        f.h[1].b1 = false; step(2);
        f.h[1].sy = 1.f; step(20); f.h[1].sy = 0.f; step(); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(wh >= 120 * 1 && wh <= 120 * 4, ("screen use: stick up scrolls (" + std::to_string(wh / 120) + " notches)").c_str());
        f.h[1].aimPose.position = { 3.f, 1.6f, -0.2f }; step(60); Keys::DryState(x, y, l, r, wh, fo);
        CHECK(e.screen.hoverHand < 0 && fo == 1 && !e.Blocked(1, C_TRIGGER), "screen use: pointing away gives the game its window and buttons back");
        Keys::SetDryRun(false);
    }
    // ---------------- mirrored screen: windows instead of monitors
    {
        auto q = ParseProfileText(DefaultProfileJson(), err);
        Engine e; e.SetProfile(q);
        Action nw; nw.type = ActType::Screen; nw.mode = "next_window";
        uint64_t ask0 = e.screen.windowAsk;
        e.ScreenAction(nw);
        CHECK(e.screen.on && e.screen.source == "window" && e.screen.windowStep == 1 && e.screen.windowAsk == ask0 + 1, "screen window: next window switches to windows and asks for the next one");
        nw.mode = "prev_window"; e.ScreenAction(nw);
        CHECK(e.screen.windowStep == -1, "screen window: previous window asks for the one before");
        Action nm; nm.type = ActType::Screen; nm.mode = "next_monitor"; e.screen.monitorCount = 2;
        e.ScreenAction(nm);
        CHECK(e.screen.source == "monitor", "screen window: next monitor goes back to monitors");
        Action sw; sw.type = ActType::Screen; sw.mode = "on"; sw.window = "chrome";
        e.ScreenAction(sw);
        CHECK(e.screen.source == "window" && e.screen.windowMatch == "chrome", "screen window: a button can show a window by name");
        std::string perr;
        auto pj = ParseProfileText(R"({"screen":{"source":"window","window":"discord"}})", perr);
        CHECK(pj && pj->screen.source == "window" && pj->screen.window == "discord", "screen window: profile can start on a window by name");
    }
    // ---------------- mirrored screen: real window capture on this PC
    {
        auto wins = ScreenCapture::Windows();
        printf("windows found: %d\n", (int)wins.size());
        for (size_t i = 0; i < wins.size() && i < 12; i++) printf("  %s | %s\n", wins[i].exe.c_str(), wins[i].title.substr(0, 50).c_str());
        int pick = -1;
        for (int i = 0; i < (int)wins.size() && pick < 0; i++) { std::string x = wins[i].exe; for (auto& ch : x) ch = (char)tolower((unsigned char)ch); if (x.find("chrome") != std::string::npos || x.find("firefox") != std::string::npos || x.find("msedge") != std::string::npos) pick = i; }
        if (pick < 0 && !wins.empty()) pick = 0;
        if (pick >= 0) {
            ScreenCapture c; c.Configure(0, wins[pick].hwnd, 30, 1280, true, 1.f);
            std::vector<uint32_t> px; int w = 0, h = 0; bool got = false;
            for (int i = 0; i < 300 && !got; i++) { got = c.TakeFrame(px, w, h); if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            int n, prim, shown; std::string msg; uint64_t mv; c.Status(n, prim, shown, msg, mv);
            int rx, ry, rw, rh; c.Rect(rx, ry, rw, rh);
            char b[300]; snprintf(b, sizeof b, "screen window: capture '%s' -> %dx%d, window at %d,%d %dx%d (%s)", wins[pick].exe.c_str(), w, h, rx, ry, rw, rh, msg.c_str());
            CHECK(got && w > 0 && (int)px.size() == w * h && rw > 0, b);
            if (got && getenv("VRO_SCREEN_BMP")) {
                std::string fn = std::string(getenv("VRO_SCREEN_BMP")) + "win.bmp";
                FILE* fo = fopen(fn.c_str(), "wb");
                if (fo) {
                    BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
                    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32;
                    fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + w * h * 4;
                    fwrite(&fh, sizeof fh, 1, fo); fwrite(&ih, sizeof ih, 1, fo); fwrite(px.data(), 4, px.size(), fo); fclose(fo);
                }
            }
            c.Stop();
        }
    }
    // ---------------- mirrored screen: button bar and picker
    {
        Keys::SetDryRun(true);
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->screen.place = "front"; q->screen.distance = 1.0f; q->screen.below = 0.f; q->screen.width = 1.0f;
        Engine e; e.SetProfile(q);
        Frame f; f.headValid = true; f.head = XrPosef{ {0,0,0,1}, {0, 1.6f, 0} };
        for (int h = 0; h < 2; h++) { f.h[h].gripValid = f.h[h].aimValid = true; f.h[h].gripPose.orientation.w = f.h[h].aimPose.orientation.w = 1; }
        f.h[0].gripPose.position = f.h[0].aimPose.position = { -0.3f, 1.0f, -0.2f };
        f.h[1].gripPose.position = { 0.3f, 1.0f, -0.2f };
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f };
        double t = 0; auto step = [&](int n = 1) { for (int i = 0; i < n; i++) { t += 0.011; f.t = t; e.Update(f); } };
        e.screen.monX = 0; e.screen.monY = 0; e.screen.monW = 1920; e.screen.monH = 1080; e.screen.aspect = 9.f / 16.f;
        Action on; on.type = ActType::Screen; on.mode = "on"; e.ScreenAction(on);
        step(2);
        CHECK(e.screen.barVisible && e.screen.barLabels.size() == 5, "screen bar: pointing at the screen shows its button bar");
        auto aimAt = [&](const XrPosef& pose, float w, float h, const Cell& c) { // point the right laser at a cell's middle
            Vec3 local{ (c.x + c.w / 2 - 0.5f) * w, (0.5f - (c.y + c.h / 2)) * h, 0 };
            Vec3 p = V(pose.position) + Rotate(pose.orientation, local);
            f.h[1].aimPose.position = X(p + Vec3{ 0, 0, 0.5f });
        };
        std::vector<Cell> cells; Cell hd;
        ListLayout(5, 5, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[0]); step(2);
        CHECK(e.screen.barHover == 0 && e.screen.hoverHand < 0, "screen bar: the laser on 'Choose...' highlights it and doesn't move the mouse");
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(e.screen.pickOpen && e.screen.wantChoices, "screen bar: 'Choose...' opens the picker and asks for the list");
        e.screen.wantChoices = false;
        e.screen.choices.clear();
        { ScreenChoice c; c.label = "Monitor 1\n1920 x 1080"; c.monitor = 1; e.screen.choices.push_back(c); }
        for (int i = 0; i < 15; i++) { ScreenChoice c; c.label = "app" + std::to_string(i) + "\nWindow " + std::to_string(i); c.hwnd = 1000 + i; e.screen.choices.push_back(c); }
        e.screen.choicesVersion++; step(2);
        CHECK(e.screen.pickLabels.size() == 14 && e.screen.pickLabels[12] == "More \xE2\x80\xBA", "screen picker: 12 per page, then More and Close");
        ListLayout((int)e.screen.pickLabels.size(), e.screen.pickCols, e.screen.pickH / e.screen.pickW, true, cells, hd);
        aimAt(e.screen.pickPose, e.screen.pickW, e.screen.pickH, cells[3]); step(2);
        CHECK(e.screen.pickHover == 3, "screen picker: the laser highlights an entry");
        {
            std::vector<uint32_t> px; int W = 1000, H = (int)(1000 * e.screen.pickH / e.screen.pickW);
            DrawList(px, W, H, e.screen.pickTitle, e.screen.pickLabels, e.screen.pickCols, e.screen.pickHover, -1, q->style);
            SavePng(px, W, H, L"screen_picker.png");
            int BW = 1200, BH = (int)(1200 * e.screen.barH / e.screen.barW);
            DrawList(px, BW, BH, "", e.screen.barLabels, (int)e.screen.barLabels.size(), 1, -1, q->style);
            SavePng(px, BW, BH, L"screen_bar.png");
        }
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(!e.screen.pickOpen && e.screen.source == "window" && e.screen.hwnd == 1002, "screen picker: picking a window shows that window");
        // keyboard button
        f.h[1].aimPose.position = { 0.f, 1.6f, -0.2f }; step(2);
        ListLayout(5, 5, e.screen.barH / e.screen.barW, false, cells, hd);
        aimAt(e.screen.barPose, e.screen.barW, e.screen.barH, cells[3]); step(2);
        f.h[1].trig = 1.f; step(2); f.h[1].trig = 0.f; step(2);
        CHECK(e.kb.visible, "screen bar: 'Keyboard' opens the VR keyboard");
        f.h[1].aimPose.position = { 3.f, 1.6f, -0.2f }; step(80);
        int x, y, wh, fo; bool l, r; Keys::DryState(x, y, l, r, wh, fo);
        CHECK(fo == 0, "screen bar: the game doesn't take the focus back while the keyboard is up");
        Keys::SetDryRun(false);
    }
    // ---------------- the screen remembers what was picked in the headset
    {
        std::wstring path = ProfilesDir() + L"\\vro_choice_test.json";
        { std::ofstream o(path, std::ios::binary); o << DefaultProfileJson(); }
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->sourceName = "vro_choice_test.json";
        CHECK(q->screen.source == "monitor", "screen choice: the default is a monitor");
        q->screen.source = "window"; q->screen.window = "chrome"; q->screen.windowTitle = "Chat - Twitch";
        std::string serr; bool ok = SaveScreenChoice(*q, serr); if (!ok) printf("save error: %s\n", serr.c_str());
        std::ifstream in(path, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); in.close();
        auto back = ParseProfileText(ss.str(), err);
        CHECK(ok && back && back->screen.source == "window" && back->screen.window == "chrome" && back->screen.windowTitle == "Chat - Twitch" && back->wheels.size() == q->wheels.size(),
              "screen choice: a picked window is saved as the default and the rest of the profile is kept");
        Engine e; e.SetProfile(back);
        CHECK(e.screen.source == "window" && e.screen.windowMatch == "chrome" && e.screen.windowTitle == "Chat - Twitch", "screen choice: the next start asks for that same window");
        q->screen.source = "monitor"; q->screen.monitor = 2; SaveScreenChoice(*q, serr);
        std::ifstream in2(path, std::ios::binary); std::stringstream s2; s2 << in2.rdbuf(); in2.close();
        auto back2 = ParseProfileText(s2.str(), err);
        CHECK(back2 && back2->screen.source == "monitor" && back2->screen.monitor == 2, "screen choice: a picked monitor is saved as the default");
        Action nm; nm.type = ActType::Screen; nm.mode = "next_monitor"; e.screen.monitorCount = 2; e.ScreenAction(nm);
        CHECK(e.screen.choiceChanged, "screen choice: switching in the headset asks to remember it");
        DeleteFileW(path.c_str());
    }
    // ---------------- hand-placed positions are saved and read back
    {
        std::wstring path = ProfilesDir() + L"\\vro_pose_test.json";
        { std::ofstream o(path, std::ios::binary); o << DefaultProfileJson(); }
        auto q = ParseProfileText(DefaultProfileJson(), err);
        q->sourceName = "vro_pose_test.json";
        XrPosef p{ { 0, std::sin(0.3f), 0, std::cos(0.3f) }, { 0.1f, -0.2f, 0.3f } };
        q->wrist[1].pose = { true, p }; q->style.poses["head_locked"] = { true, p }; q->screen.poses["wrist_right"] = { true, p };
        std::string serr; bool ok = SavePlacement(*q, serr);
        std::ifstream in(path, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); in.close();
        auto back = ParseProfileText(ss.str(), err);
        auto same = [&](const FreePose& a) { return a.set && std::fabs(a.p.position.z - 0.3f) < 1e-3f && std::fabs(a.p.orientation.y - std::sin(0.3f)) < 1e-3f; };
        CHECK(ok && back && same(back->wrist[1].pose) && same(back->style.poses["head_locked"]) && same(back->screen.poses["wrist_right"]) && !back->wrist[0].pose.set,
              "hand-placed positions and angles of wheel, wrist panels and screen are saved and read back");
        DeleteFileW(path.c_str());
    }
    // ---------------- mirrored screen: real capture on this PC
    {
        auto mons = ScreenCapture::Monitors();
        printf("monitors found: %d\n", (int)mons.size());
        for (size_t i = 0; i < mons.size(); i++) printf("  %d: %dx%d at %d,%d%s\n", (int)i + 1, mons[i].w, mons[i].h, mons[i].x, mons[i].y, mons[i].primary ? " (main)" : "");
        for (int m = 0; m <= (int)mons.size() && !mons.empty(); m++) {
            ScreenCapture c; c.Configure(m, 0, 30, getenv("VRO_SCREEN_MAXW") ? atoi(getenv("VRO_SCREEN_MAXW")) : 1280, true, 1.f);
            std::vector<uint32_t> px; int w = 0, h = 0; bool got = false;
            for (int i = 0; i < 300 && !got; i++) { got = c.TakeFrame(px, w, h); if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            int n, prim, shown; std::string msg; uint64_t mv; c.Status(n, prim, shown, msg, mv);
            char b[256]; snprintf(b, sizeof b, "screen: capture monitor setting %d -> %dx%d (%s)", m, w, h, msg.c_str());
            CHECK(got && w > 0 && (int)px.size() == w * h, b);
            if (got && getenv("VRO_SCREEN_BMP")) { // look at what was captured
                std::string fn = std::string(getenv("VRO_SCREEN_BMP")) + std::to_string(m) + ".bmp";
                FILE* fo = fopen(fn.c_str(), "wb");
                if (fo) {
                    BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
                    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32;
                    fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + w * h * 4;
                    fwrite(&fh, sizeof fh, 1, fo); fwrite(&ih, sizeof ih, 1, fo); fwrite(px.data(), 4, px.size(), fo); fclose(fo);
                }
            }
            c.Stop();
        }
    }

    printf("\n%d failures\n", fails);
    Keys::Shutdown();
    DrawShutdown();
    return fails;
}
