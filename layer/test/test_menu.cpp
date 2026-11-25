// The in-game settings menu, driven like a player would: point at a row with the right controller, pull the trigger.
#include "../src/binding_checks.h"
#include "test_support.h"

namespace {
// The menu as it looks now, saved for checking by eye.
void SaveMenuPicture(const Engine& e, const Profile& p, const wchar_t* file) {
    int W = (int)(1100 * e.menuView.w / kMenuWidth), H = (int)(W * e.menuView.h / e.menuView.w); // as the overlay draws it
    std::vector<uint32_t> px;
    DrawMenu(px, W, H, e.menuView, p.style);
    SavePng(px, W, H, file);
}

struct MenuDriver {
    Engine& e;
    Frame& f;
    double& t;

    void Step(double secs = 0.05) {
        for (double end = t + secs; t < end;) {
            t += 0.011;
            f.t = t;
            e.Update(f);
        }
    }
    // Aim the right controller at a point on a panel (u right, v down, 0..1).
    void AimAt(const XrPosef& pose, float w, float h, float u, float v) {
        Vec3 target = V(pose.position) + Rotate(pose.orientation, { (u - 0.5f) * w, (0.5f - v) * h, 0 });
        Vec3 from{ 0.15f, 1.3f, -0.2f };
        f.h[1].aimValid = true;
        f.h[1].aimPose.position = X(from);
        f.h[1].aimPose.orientation = BillboardTo(target, from);
    }
    void PullTrigger() {
        f.h[1].trig = 1.f;
        Step();
        f.h[1].trig = 0.f;
        Step();
    }
    // The row whose label starts with this text, or -1.
    int Row(const std::string& label) const {
        for (int i = 0; i < (int)e.menuView.rows.size(); i++)
            if (e.menuView.rows[i].label.rfind(label, 0) == 0) return i;
        return -1;
    }
    // Point at a part (the row's own box, its - / +, or Back / Close) and press it. False when it isn't on the panel.
    bool Press(MenuPart part, int row = -1, int sub = -1) {
        std::vector<MenuBox> boxes;
        MenuLayout(e.menuView, boxes);
        for (const MenuBox& b : boxes) {
            if (b.hit.part != part || b.hit.row != row || b.hit.sub != sub) continue;
            AimAt(e.menuView.pose, e.menuView.w, e.menuView.h, b.x + b.w / 2, b.y + b.h / 2);
            Step();
            PullTrigger();
            return true;
        }
        return false;
    }
    bool PressRow(const std::string& label) { return Press(MenuPart::Row, Row(label)); }
    // A button in a row of buttons (Bar), by its label.
    bool PressItem(const std::string& label) {
        for (int i = 0; i < (int)e.menuView.rows.size(); i++)
            for (int k = 0; k < (int)e.menuView.rows[i].items.size(); k++)
                if (e.menuView.rows[i].items[k] == label) return Press(MenuPart::Row, i, k);
        return false;
    }
    // A part's tag on the overview.
    bool PressTag(int hand, int comp) {
        for (int i = 0; i < (int)e.menuView.rows.size(); i++)
            for (int k = 0; k < (int)e.menuView.rows[i].tags.size(); k++)
                if (e.menuView.rows[i].tags[k].hand == hand && e.menuView.rows[i].tags[k].comp == comp) return Press(MenuPart::Row, i, k);
        return false;
    }
};
} // namespace

void TestInGameMenu() {
    std::string err;
    auto p = ParseProfileText(DefaultProfileJson(), err);
    CHECK(p != nullptr, "menu: the built-in profile reads");
    if (!p) return;
    Binding opener;
    opener.name = "Settings menu";
    InputId menuButton;
    ParseInput("left.menu", menuButton);
    opener.chord = { menuButton };
    opener.chordText = "left.menu";
    opener.action.type = ActType::Menu;
    p->bindings.push_back(opener);
    size_t bindingsBefore = p->bindings.size();
    Engine e;
    e.SetProfile(p);
    Frame f;
    double t = 0;
    f.headValid = true;
    f.head.position = { 0, 1.6f, 0 };
    MenuDriver d{ e, f, t };
    d.Step();

    e.gameUses = { { 1, C_BTN1, false, "Jump" }, { 1, C_TRIGGER, false, "Fire" }, { 0, C_STICK, false, "Move" },
                   { 1, C_STICK, false, "Turn" }, { 0, C_TRIGGER, false, "Grab" } }; // what a game would bind
    f.h[0].menu = true;
    d.Step();
    f.h[0].menu = false;
    d.Step(0.15);
    CHECK(e.menuView.open && e.menuView.title.rfind("OpenXR Companion", 0) == 0, "menu: a settings_menu binding opens it on the home tiles");
    SaveMenuPicture(e, *p, L"menu_main.png");
    CHECK(d.PressItem("Controllers") && e.menuView.title == "Controller configuration" && e.menuView.w > kMenuWidth,
          "menu: the Controllers tile opens the overview, a wider panel");
    int overview = -1;
    for (int i = 0; i < (int)e.menuView.rows.size(); i++)
        if (e.menuView.rows[i].kind == MenuRowKind::Overview) overview = i;
    bool jumpShown = false, unusedShown = false;
    if (overview >= 0)
        for (const MenuTag& tg : e.menuView.rows[overview].tags) {
            jumpShown = jumpShown || (tg.hand == 1 && tg.comp == C_BTN1 && tg.title == "A" && tg.lines[0] == "Jump" && tg.lineKinds[0] == 0);
            unusedShown = unusedShown || (tg.hand == 1 && tg.comp == C_GRIP && tg.lines[0] == "Default");
        }
    CHECK(jumpShown && unusedShown, "menu: the overview tags each part with the game's own control, or 'Default'");
    SaveMenuPicture(e, *p, L"menu_overview.png");
    CHECK(d.PressTag(1, C_BTN1) && e.menuView.title == "Right A" && d.Row("In the game: Jump") >= 0 && d.Row("Press") >= 0 && d.Row("Touch") >= 0,
          "menu: pointing at a tag opens that part's page: the game's use and every way to bind it");
    SaveMenuPicture(e, *p, L"menu_part.png");
    CHECK(d.Press(MenuPart::Back) && e.menuView.title == "Controller configuration", "menu: Back returns to the overview");
    CHECK(d.PressItem("All bindings") && e.menuView.title.rfind("Edit bindings", 0) == 0, "menu: Edit bindings under the overview");
    CHECK(d.Row("Right controller bindings") >= 0 && d.Row("Right stick") >= 0 && d.Row("Controller offsets") >= 0,
          "menu: home links to bindings, sticks and controller offsets");
    SaveMenuPicture(e, *p, L"menu_home.png");

    // Live controllers: what is pressed now reaches the panel (a few times a second).
    CHECK(d.PressRow("Test controllers") && e.menuView.title == "Test controllers", "menu: the controller test page");
    f.h[1].b1 = true;  // right A pressed
    f.h[0].b2t = true; // left Y only touched
    f.h[0].trig = 0.6f;
    f.h[1].sx = 0.5f;
    f.h[1].sy = 0.8f;
    d.Step(0.15);
    const LiveHand &liveL = e.menuView.live[0], &liveR = e.menuView.live[1];
    CHECK(liveR.press[C_BTN1] && liveL.touch[C_BTN2] && !liveL.press[C_BTN2] && std::fabs(liveL.trigger - 0.6f) < 0.01f &&
              std::fabs(liveR.stickY - 0.8f) < 0.01f,
          "menu: the test page shows what is pressed, touched and pushed right now");
    SaveMenuPicture(e, *p, L"menu_test_controllers.png");
    // The same page with Steam Frame controllers: right X and left D-pad up pressed too.
    f.h[0].kind = f.h[1].kind = ControllerKind::SteamFrame;
    f.h[1].more[C_BTN3] = f.h[0].more[C_DPAD_UP] = true;
    d.Step(0.15);
    CHECK(e.menuView.live[1].press[C_BTN3] && e.menuView.live[0].press[C_DPAD_UP], "menu: the test page shows the Frame's X and D-pad");
    SaveMenuPicture(e, *p, L"menu_test_frame.png");
    f.h[0].kind = f.h[1].kind = ControllerKind::Unknown;
    f.h[1].more[C_BTN3] = f.h[0].more[C_DPAD_UP] = false;
    f.h[1].b1 = false;
    f.h[0].b2t = false;
    f.h[0].trig = 0.f;
    f.h[1].sx = f.h[1].sy = 0.f;
    d.Step(0.15);
    CHECK(!e.menuView.live[1].press[C_BTN1], "menu: and lets go with you");
    d.Press(MenuPart::Back);

    // Stick options: + five times on the inner deadzone.
    CHECK(d.PressRow("Right stick") && e.menuView.title == "Right stick", "menu: pointing at a link and pulling the trigger opens it");
    CHECK(d.PressRow("Deadzones") && e.menuView.title == "Right stick: deadzones", "menu: the stick's deadzones on their own page");
    int inner = d.Row("Inner deadzone");
    CHECK(inner >= 0 && e.menuView.rows[inner].value == "off" && !e.menuView.rows[inner].canLess, "menu: a number at its minimum shows 'off', - is greyed");
    for (int i = 0; i < 5; i++)
        d.Press(MenuPart::More, inner);
    CHECK(std::fabs(p->sticks[1].innerDeadzone - 0.05f) < 1e-4f && e.menuView.rows[inner].value == "5 %",
          "menu: + raises the right stick's inner deadzone, 1 % a press, shown in %");
    f.h[1].sx = 0.3f; // the stick a little forward right, for the picture
    f.h[1].sy = 0.6f;
    d.Step(0.15);
    CHECK(!e.menuSaveRequested, "menu: not saved on every press");
    SaveMenuPicture(e, *p, L"menu_stick.png");
    f.h[1].sx = f.h[1].sy = 0.f;
    d.Step(0.1);
    d.Step(1.2);
    CHECK(e.menuSaveRequested, "menu: saved a moment after the last change");
    e.menuSaveRequested = false;
    f.h[1].sy = -1.f; // the stick being tested doesn't scroll or change the page
    d.Step(0.5);
    CHECK(e.menuView.first == 0 && std::fabs(p->sticks[1].innerDeadzone - 0.05f) < 1e-4f, "menu: the sticks don't scroll a stick page");
    f.h[1].sy = 0.f;
    d.Step(0.1);
    d.Press(MenuPart::Back);
    CHECK(d.Press(MenuPart::Back) && e.menuView.title.rfind("Edit bindings", 0) == 0, "menu: Back returns to the home page");

    // A new binding: press the button, then type the key on the VR keyboard.
    CHECK(d.PressRow("Right controller bindings") && e.menuView.title == "Right bindings", "menu: the right controller's bindings");
    d.PressItem("+ New binding");
    CHECK(!e.menuView.status.empty(), "menu: asking for a button says so on the panel");
    SaveMenuPicture(e, *p, L"menu_capture.png");
    f.h[1].b1 = true; // right A
    d.Step();
    f.h[1].b1 = false;
    d.Step();
    CHECK(p->bindings.size() == bindingsBefore + 1 && InputName(p->bindings.back().chord.back()) == "right.a",
          "menu: the next button press becomes the new binding's button");
    CHECK(e.menuView.title == "What happens" && !e.kb.visible, "menu: then 'What happens' opens for the new binding");
    {
        int W = 1100, H = (int)(1100 * e.menuView.h / e.menuView.w);
        std::vector<uint32_t> px;
        DrawMenu(px, W, H, e.menuView, p->style);
        SavePng(px, W, H, L"menu_what_happens.png");
    }
    d.PressRow("Press a key or mouse button");
    CHECK(e.kb.visible && e.menuView.title == "Right A", "menu: then the VR keyboard opens for its key, on the binding's page");
    float uw, uh;
    const auto& keys = KbLayout(uw, uh, true);
    CHECK(e.kb.binding, "menu: the keyboard shows the binding layout (mouse buttons, Controller button)");
    {
        int W = 1400, H = (int)(1400 * uh / uw);
        std::vector<uint32_t> px;
        DrawKeyboard(px, W, H, e.kb, p->style);
        SavePng(px, W, H, L"keyboard_binding.png");
    }
    int qKey = -1;
    for (int i = 0; i < (int)keys.size(); i++)
        if (keys[i].combo == "q") qKey = i;
    const KbKey& k = keys[qKey];
    d.Step(0.4); // presses right after the keyboard opens are ignored
    d.AimAt(e.kb.pose, e.kb.widthM, e.kb.heightM, (k.x + k.w / 2) / uw, (k.y + k.h / 2) / uh);
    d.Step();
    d.PullTrigger();
    CHECK(p->bindings.back().action.keys == "q" && !e.kb.visible, "menu: the key pressed on the VR keyboard becomes the binding's key, not typed");
    CHECK(d.Row("3  What happens: Key: q (hold)") >= 0 && p->bindings.back().name == "Right A: q", "menu: the page and the name show the new key");

    // What it does, again: a mouse button, then a controller button the game should see.
    auto pressKey = [&](const char* combo, int kind) {
        const auto& ks = KbLayout(uw, uh, true);
        int at = -1;
        for (int i = 0; i < (int)ks.size(); i++)
            if (ks[i].kind == kind && (kind != KB_KEY || ks[i].combo == combo)) at = i;
        if (at < 0) return false;
        d.Step(0.4);
        d.AimAt(e.kb.pose, e.kb.widthM, e.kb.heightM, (ks[at].x + ks[at].w / 2) / uw, (ks[at].y + ks[at].h / 2) / uh);
        d.Step();
        d.PullTrigger();
        return true;
    };
    CHECK(d.PressRow("3  What happens: Key: q (hold)") && e.menuView.title == "What happens" && d.PressRow("Press a key or mouse button") && e.kb.visible && e.kb.binding,
          "menu: What happens > Press a key opens the binding keyboard");
    CHECK(pressKey("rmb", KB_KEY) && p->bindings.back().action.keys == "rmb" && p->bindings.back().action.type == ActType::Key,
          "menu: a mouse button on the binding keyboard becomes what it does");
    d.PressRow("3  What happens: Key: rmb (hold)");
    d.PressRow("Press a key or mouse button");
    {
        // the keyboard lies on the menu's pane: same facing, just in front, inside its edges; the menu is dimmed under it
        Vec3 off = RotateInv(e.menuView.pose.orientation, V(e.kb.pose.position) - V(e.menuView.pose.position));
        float facing = Dot(Rotate(e.kb.pose.orientation, { 0, 0, 1 }), Rotate(e.menuView.pose.orientation, { 0, 0, 1 }));
        CHECK(e.kb.inMenu && e.menuView.covered && facing > 0.999f && off.z > 0 && off.z < 0.02f &&
                  std::fabs(off.x) + e.kb.widthM / 2 <= e.menuView.w / 2 + 1e-4f && std::fabs(off.y) + e.kb.heightM / 2 <= e.menuView.h / 2 + 1e-4f,
              "menu: the binding keyboard lies inside the menu's pane");
    }
    CHECK(pressKey("", KB_CONTROLLER) && !e.kb.visible && !e.menuView.status.empty(), "menu: Controller button asks for a button");
    f.h[1].b2 = true; // right B: the button the game should see
    d.Step();
    f.h[1].b2 = false;
    d.Step();
    const Binding& asButton = p->bindings.back();
    CHECK(asButton.action.type == ActType::XrInput && InputName(asButton.action.target) == "right.b" && asButton.action.mode == ActMode::Hold &&
              d.Row("3  What happens: Controller: Right B (hold)") >= 0,
          "menu: the pressed controller button becomes what the game sees");

    int way = d.Row("Press or touch");
    CHECK(way >= 0 && !e.menuView.rows[way].canLess && e.menuView.rows[way].canMore && e.menuView.rows[way].value == "Press",
          "menu: a choice on its first option: - greyed, + not");
    int when = d.Row("1  How");
    d.Press(MenuPart::More, when);
    CHECK(p->bindings.back().gesture == Gesture::Hold && p->bindings.back().block == BindingBlock::Trigger && p->bindings.back().tapPassthrough,
          "menu: > on How steps to Hold, and a quick tap is let through to the game (set for you)");
    d.Press(MenuPart::More, d.Row("1  How"));
    CHECK(p->bindings.back().gesture == Gesture::DoubleTap && p->bindings.back().block == BindingBlock::None,
          "menu: Double-tap leaves the button to the game too");
    if (!p->wheels.empty())
    {
        const auto& [wname, wh] = *p->wheels.begin();
        std::string title = wh.title.empty() ? wname : wh.title;
        d.PressRow("3  What happens: " + std::string("Controller: Right B (hold)"));
        CHECK(e.menuView.title == "What happens" && d.PressRow("Wheel '" + title + "'") && p->bindings.back().action.type == ActType::Wheel &&
                  p->bindings.back().action.wheel == wname && e.menuView.title != "What happens",
              "menu: What happens > a wheel makes the binding open it, and goes back");
    }
    e.menuView.first = std::min(d.Row("Enabled"), MenuMaxFirst(e.menuView)); // further down the page now: scroll to it
    d.PressRow("Enabled");
    CHECK(!p->bindings.back().enabled, "menu: the On switch turns the binding off");

    // A picture of this page, for checking by eye.
    {
        int W = 1100, H = (int)(1100 * e.menuView.h / e.menuView.w);
        std::vector<uint32_t> px;
        DrawMenu(px, W, H, e.menuView, p->style);
        bool drawn = px.size() == (size_t)W * H && std::any_of(px.begin(), px.end(), [](uint32_t c) { return (c >> 24) != 0; });
        CHECK(drawn, "menu: the panel draws");
        SavePng(px, W, H, L"menu_binding.png");
    }

    e.menuView.first = 0;
    CHECK(d.PressItem("Delete binding") && p->bindings.size() == bindingsBefore && e.menuView.title == "Right bindings",
          "menu: Delete removes the binding and goes back to the list");
    {
        int W = 1100, H = (int)(1100 * e.menuView.h / e.menuView.w);
        std::vector<uint32_t> px;
        DrawMenu(px, W, H, e.menuView, p->style);
        SavePng(px, W, H, L"menu_bindings.png");
    }
    CHECK(d.Press(MenuPart::Close) && !e.menuView.open, "menu: the X closes it");
}

// The hub pages: wheels, wrist panels, sticks, more settings. Pictures of each for review (layer/build/menu_hub_*.png).
// "May need your attention" in the headset: the tester's left stick setup (a Hold letting short presses through, a
// double-tap keeping the button from the game) is marked in the list and explained on the binding's page.
void TestMenuWarnings() {
    std::string err;
    auto p = ParseProfileText(R"({ "bindings": [
      { "name": "Menu", "input": "left.menu", "gesture": "press", "action": { "type": "settings_menu" } },
      { "name": "Hold L3 = LCtrl", "input": "left.thumbstick", "gesture": "hold", "block": "trigger", "tap_passthrough": true,
        "action": { "type": "key", "keys": "LCtrl" } },
      { "name": "Double L3 = wait", "input": "left.thumbstick", "gesture": "double_tap", "block": "trigger",
        "action": { "type": "xr_input", "target": "left.thumbstick" } },
      { "name": "Fine", "input": "left.x", "gesture": "press", "action": { "type": "key", "keys": "F8" } } ] })", err);
    CHECK(p != nullptr, ("warnings: profile reads " + err).c_str());
    if (!p) return;
    auto w1 = CheckBinding(*p, 1, true), w2 = CheckBinding(*p, 2, true), w3 = CheckBinding(*p, 3, true);
    auto has = [](const std::vector<BindingWarning>& w, const char* t) {
        return std::any_of(w.begin(), w.end(), [&](const BindingWarning& x) { return x.title == t; });
    };
    CHECK(has(w1, "Short presses get past another binding") && has(w2, "Single presses still reach the game") && w3.empty(),
          "warnings: the short-press leak is found on both bindings, the fine one has none");
    CHECK(has(w2, "Movement pauses while you click"), "warnings: a stick click kept from the game is pointed out");
    Engine e;
    e.SetProfile(p);
    Frame f;
    double t = 0;
    f.headValid = true;
    f.head.position = { 0, 1.6f, 0 };
    MenuDriver d{ e, f, t };
    d.Step();
    Action open;
    open.type = ActType::Menu;
    e.MenuRunAction(open);
    d.Step(0.15);
    CHECK(d.PressItem("Controllers") && d.PressItem("All bindings") && d.PressRow("Left controller bindings"), "warnings: to the left bindings");
    int row = d.Row("\u26A0 Double L3");
    CHECK(row >= 0 && e.menuView.rows[row].isWarn && d.Row("Fine") >= 0 && !e.menuView.rows[d.Row("Fine")].isWarn,
          "warnings: the list marks the flagged binding, not the fine one");
    SaveMenuPicture(e, *p, L"menu_warn_list.png");
    CHECK(d.PressRow("\u26A0 Double L3") && d.Row("\u26A0 Single presses still reach the game") >= 0, "warnings: the binding's page explains it");
    SaveMenuPicture(e, *p, L"menu_warn_binding.png");
}

void TestMenuHub() {
    std::string err;
    auto p = ParseProfileText(DefaultProfileJson(), err);
    CHECK(p != nullptr, "hub: the built-in profile reads");
    if (!p) return;
    Engine e;
    e.SetProfile(p);
    Frame f;
    double t = 0;
    f.headValid = true;
    f.head.position = { 0, 1.6f, 0 };
    MenuDriver d{ e, f, t };
    d.Step();
    Action open;
    open.type = ActType::Menu;
    e.MenuRunAction(open);
    d.Step(0.15);
    CHECK(e.menuView.open && d.PressItem("Radial wheels") && e.menuView.title == "Radial wheels", "hub: the wheels tile");
    SaveMenuPicture(e, *p, L"menu_hub_1_wheels.png");
    // the wheel the default opens from the right thumb rest (its name and title change with the default profile)
    std::string rw;
    for (auto& b : p->bindings)
        if (rw.empty() && b.action.type == ActType::Wheel && !b.chord.empty() && b.chord.back().hand == 1 && b.chord.back().comp == C_THUMBREST)
            rw = b.action.wheel;
    std::string rwTitle = p->wheels.count(rw) && !p->wheels[rw].title.empty() ? p->wheels[rw].title : rw;
    CHECK(d.PressItem(rwTitle) && e.menuView.title == "Wheel: " + rwTitle, ("hub: a wheel tile opens its page with the picture ('" + rwTitle + "')").c_str());
    SaveMenuPicture(e, *p, L"menu_hub_1b_wheel_right.png");
    int pv = -1;
    for (int i = 0; i < (int)e.menuView.rows.size(); i++)
        if (e.menuView.rows[i].kind == MenuRowKind::Preview) pv = i;
    CHECK(pv >= 0 && d.Press(MenuPart::Row, pv, 2) && e.menuView.title.rfind("Button: ", 0) == 0, "hub: pointing at a slice opens that button");
    SaveMenuPicture(e, *p, L"menu_hub_1c_slice_button.png");
    d.Press(MenuPart::Back);
    d.Press(MenuPart::Back);
    size_t wheelsBefore = p->wheels.size();
    CHECK(d.PressItem("+ New wheel") && p->wheels.size() == wheelsBefore + 1 && e.menuView.title == "Wheel: New wheel",
          "hub: + New wheel makes one and opens it");
    CHECK(d.PressItem("+ Add a button") && e.menuView.title == "What it does", "hub: + Add a button goes to What it does");
    SaveMenuPicture(e, *p, L"menu_hub_3_what_it_does.png");
    // the keyboard row is further down: scroll to it
    for (int i = 0; i < 20 && d.Row("VR keyboard") >= 0 && d.Row("VR keyboard") >= e.menuView.first + 7; i++)
        d.Press(MenuPart::ScrollDown);
    CHECK(d.PressRow("VR keyboard") && e.menuView.title == "Button: Keyboard", "hub: picking an action names a new button after it");
    SaveMenuPicture(e, *p, L"menu_hub_4_button.png");
    CHECK(d.PressRow("1  Label") && e.kb.visible && e.kb.inMenu, "hub: Label opens the keyboard on the menu");
    float uw, uh;
    const auto& keys = KbLayout(uw, uh, false);
    auto typeKey = [&](const std::string& combo) {
        for (int i = 0; i < (int)keys.size(); i++)
            if (keys[i].combo == combo) {
                float u = (keys[i].x + keys[i].w / 2) / uw, v = (keys[i].y + keys[i].h / 2) / uh;
                d.AimAt(e.kb.pose, e.kb.widthM, e.kb.heightM, u, v);
                d.Step(0.3);
                d.PullTrigger();
                d.Step(0.1);
                return;
            }
    };
    for (int i = 0; i < 8; i++)
        typeKey("Backspace");
    typeKey("k");
    typeKey("b");
    SaveMenuPicture(e, *p, L"menu_hub_5_typing.png");
    typeKey("Enter");
    d.Step(0.3);
    Wheel* made = nullptr;
    for (auto& [n, w] : p->wheels)
        if (w.title == "New wheel") made = &w;
    std::string label = made && !made->items.empty() ? made->items[0].label : "none";
    CHECK(!e.kb.visible && label == "kb", ("hub: typing renames the button (now '" + label + "')").c_str());
    d.Step(0.3);
    CHECK(d.Press(MenuPart::Back) && e.menuView.title == "Wheel: New wheel", "hub: back to the wheel");
    SaveMenuPicture(e, *p, L"menu_hub_2_wheel.png");
    CHECK(d.Press(MenuPart::Back) && d.PressRow("Look and how you pick"), "hub: wheel look page");
    SaveMenuPicture(e, *p, L"menu_hub_6_wheel_look.png");
    d.Press(MenuPart::Back);
    d.Press(MenuPart::Back);
    CHECK(d.PressItem("Wrist panels") && e.menuView.title == "Wrist panels", "hub: wrist panels tile");
    SaveMenuPicture(e, *p, L"menu_hub_7_wrist.png");
    CHECK(d.PressItem("Left forearm") && e.menuView.title == "Left forearm panel", "hub: left forearm page");
    SaveMenuPicture(e, *p, L"menu_hub_8_wrist_left.png");
    int pageRow = -1;
    for (int i = 0; i < (int)e.menuView.rows.size(); i++)
        if (e.menuView.rows[i].label.find("(shown first)") != std::string::npos) pageRow = i;
    CHECK(pageRow >= 0 && d.Press(MenuPart::Row, pageRow) && e.menuView.title.rfind("Left wrist: ", 0) == 0, "hub: a wrist page");
    SaveMenuPicture(e, *p, L"menu_hub_9_wrist_page.png");
    d.Press(MenuPart::Back);
    d.Press(MenuPart::Back);
    d.Press(MenuPart::Back);
    CHECK(d.PressItem("Sticks") && e.menuView.title == "Sticks", "hub: sticks tile");
    SaveMenuPicture(e, *p, L"menu_hub_10_sticks.png");
    d.Press(MenuPart::Back);
    CHECK(d.PressItem("More settings") && e.menuView.title == "More settings", "hub: more settings tile");
    SaveMenuPicture(e, *p, L"menu_hub_11_more.png");
}

// What the menu saves: the menu's own fields, and every other field of a binding left as it was.
void TestMenuSave() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"vro_menu_save_test";
    std::error_code fec;
    std::filesystem::remove_all(dir, fec);
    _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", dir.c_str());
    std::filesystem::create_directories(ProfilesDir(), fec);
    std::wstring file = ProfilesDir() + L"\\menu_test.json";
    {
        std::ofstream o(file, std::ios::binary);
        o << R"JSON({ "bindings": [
          { "name": "Jump", "input": "right.a", "gesture": "press", "layers": "shift", "tap_ms": 222,
            "action": { "type": "key", "keys": "Space", "tap_ms": 90 } },
          { "name": "Wheel", "input": "right.thumbrest", "gesture": "double_tap", "action": { "type": "key", "keys": "F2" } }
        ] })JSON";
    }
    std::string err;
    std::ifstream in(file, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    in.close();
    auto p = ParseProfileText(ss.str(), err);
    CHECK(p != nullptr, ("menu save: the test profile reads " + err).c_str());
    if (!p) return;
    p->sourceName = "menu_test.json";
    p->bindings[0].action.keys = "F";
    p->bindings[0].action.mode = ActMode::Hold;
    p->bindings[0].enabled = false;
    p->bindings.erase(p->bindings.begin() + 1); // deleted in the headset
    Binding added = p->bindings[0];
    added.name = "New";
    added.sourceIndex = -1;
    added.layers.clear();
    added.action.type = ActType::XrInput; // made a controller button in the headset
    ParseInput("left.trigger", added.action.target);
    p->bindings.push_back(added);
    p->sticks[1].innerDeadzone = 0.12f;
    bool ok = SaveMenuEdits(*p, err);
    std::ifstream back(file, std::ios::binary);
    std::stringstream bs;
    bs << back.rdbuf();
    auto q = ParseProfileText(bs.str(), err);
    CHECK(ok && q && q->bindings.size() == 2, ("menu save: written, deleted binding gone, new one added " + err).c_str());
    if (!q || q->bindings.size() != 2) return;
    CHECK(q->bindings[0].action.keys == "F" && q->bindings[0].action.mode == ActMode::Hold && !q->bindings[0].enabled,
          "menu save: the menu's changes are in the file");
    CHECK(q->bindings[0].layers.size() == 1 && q->bindings[0].tapMs == 222 && q->bindings[0].action.tapMs == 90,
          "menu save: fields the menu doesn't edit (layer, tap times) are kept");
    CHECK(q->bindings[1].action.type == ActType::XrInput && InputName(q->bindings[1].action.target) == "left.trigger" &&
              q->bindings[1].action.keys.empty(),
          "menu save: a binding made a controller button saves as xr_input with its target (no key left over)");
    CHECK(q->sticks[1].innerDeadzone == 0.12f && q->sticks[0].innerDeadzone == 0.f, "menu save: stick options saved per hand");
    CHECK(p->bindings[1].sourceIndex == 1, "menu save: after saving, the new binding knows its place in the file");
    back.close();
    {
        std::ofstream o(file, std::ios::binary);
        o << R"JSON({ "bindings": [ { "name": "W", "input": "right.b", "action": { "type": "key", "keys": "F" } } ],
          "wheels": { "main": { "title": "Main", "items": [
            { "label": "Off one", "enabled": false, "action": { "type": "key", "keys": "1" } },
            { "label": "Jump", "color": "#FF0000", "action": { "type": "key", "keys": "Space", "tap_ms": 77 } } ] } } })JSON";
    }
    std::ifstream in2(file, std::ios::binary);
    std::stringstream s2;
    s2 << in2.rdbuf();
    in2.close();
    auto r = ParseProfileText(s2.str(), err);
    CHECK(r != nullptr, "menu save: wheel profile reads");
    if (!r) return;
    r->sourceName = "menu_test.json";
    r->wheels["main"].items[0].label = "Hop";
    Wheel nw;
    nw.name = "wheel_1";
    nw.title = "Tools";
    WheelItem kbItem;
    kbItem.label = "Keyboard";
    kbItem.action.type = ActType::Keyboard;
    nw.items.push_back(kbItem);
    r->wheels["wheel_1"] = nw;
    r->bindings[0].action = Action{};
    r->bindings[0].action.type = ActType::Wheel;
    r->bindings[0].action.wheel = "wheel_1";
    r->wrist[0].columns = 2;
    ok = SaveMenuEdits(*r, err);
    std::ifstream back2(file, std::ios::binary);
    std::stringstream b2;
    b2 << back2.rdbuf();
    back2.close();
    std::string saved = b2.str();
    auto q2 = ParseProfileText(saved, err);
    CHECK(ok && q2 && q2->wheels.count("wheel_1") && q2->wheels["wheel_1"].items[0].action.type == ActType::Keyboard,
          ("menu save: a wheel made in the headset is written " + err).c_str());
    CHECK(q2 && q2->bindings[0].action.type == ActType::Wheel && q2->bindings[0].action.wheel == "wheel_1",
          "menu save: a binding set to open a wheel saves as a wheel action (was lost before)");
    CHECK(q2 && q2->wheels["main"].items.size() == 1 && q2->wheels["main"].items[0].label == "Hop" &&
              q2->wheels["main"].items[0].action.tapMs == 77 && saved.find("Off one") != std::string::npos && saved.find("FF0000") != std::string::npos,
          "menu save: wheel items keep their other fields, and switched-off items stay in the file");
    CHECK(q2 && q2->wrist[0].columns == 2, "menu save: wrist panel settings saved");
    _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", L"");
    std::filesystem::remove_all(dir, fec);
}
