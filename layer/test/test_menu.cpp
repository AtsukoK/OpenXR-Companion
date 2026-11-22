// The in-game settings menu, driven like a player would: point at a row with the right controller, pull the trigger.
#include "test_support.h"

namespace {
// The menu as it looks now, saved for checking by eye.
void SaveMenuPicture(const Engine& e, const Profile& p, const wchar_t* file) {
    int W = 1100, H = (int)(1100 * e.menuView.h / e.menuView.w);
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
    bool Press(MenuPart part, int row = -1) {
        std::vector<MenuBox> boxes;
        MenuLayout(e.menuView, boxes);
        for (const MenuBox& b : boxes) {
            if (b.hit.part != part || b.hit.row != row) continue;
            AimAt(e.menuView.pose, e.menuView.w, e.menuView.h, b.x + b.w / 2, b.y + b.h / 2);
            Step();
            PullTrigger();
            return true;
        }
        return false;
    }
    bool PressRow(const std::string& label) { return Press(MenuPart::Row, Row(label)); }
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

    f.h[0].menu = true;
    d.Step();
    f.h[0].menu = false;
    d.Step();
    CHECK(e.menuView.open && e.menuView.title.rfind("Settings", 0) == 0, "menu: a settings_menu binding opens it on the home page");
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
    f.h[1].b1 = false;
    f.h[0].b2t = false;
    f.h[0].trig = 0.f;
    f.h[1].sx = f.h[1].sy = 0.f;
    d.Step(0.15);
    CHECK(!e.menuView.live[1].press[C_BTN1], "menu: and lets go with you");
    d.Press(MenuPart::Back);

    // Stick options: + five times on the inner deadzone.
    CHECK(d.PressRow("Right stick") && e.menuView.title == "Right stick", "menu: pointing at a link and pulling the trigger opens it");
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
    CHECK(d.Press(MenuPart::Back) && e.menuView.title.rfind("Settings", 0) == 0, "menu: Back returns to the home page");

    // A new binding: press the button, then type the key on the VR keyboard.
    CHECK(d.PressRow("Right controller bindings") && e.menuView.title == "Right bindings", "menu: the right controller's bindings");
    d.PressRow("+ New binding");
    CHECK(!e.menuView.status.empty(), "menu: asking for a button says so on the panel");
    f.h[1].b1 = true; // right A
    d.Step();
    f.h[1].b1 = false;
    d.Step();
    CHECK(p->bindings.size() == bindingsBefore + 1 && InputName(p->bindings.back().chord.back()) == "right.a",
          "menu: the next button press becomes the new binding's button");
    CHECK(e.kb.visible && e.menuView.title == "New binding", "menu: then the VR keyboard opens for its key, on the binding's page");
    float uw, uh;
    const auto& keys = KbLayout(uw, uh);
    int qKey = -1;
    for (int i = 0; i < (int)keys.size(); i++)
        if (keys[i].combo == "q") qKey = i;
    const KbKey& k = keys[qKey];
    d.Step(0.4); // presses right after the keyboard opens are ignored
    d.AimAt(e.kb.pose, e.kb.widthM, e.kb.heightM, (k.x + k.w / 2) / uw, (k.y + k.h / 2) / uh);
    d.Step();
    d.PullTrigger();
    CHECK(p->bindings.back().action.keys == "q" && !e.kb.visible, "menu: the key pressed on the VR keyboard becomes the binding's key, not typed");
    CHECK(d.Row("Key: q") >= 0 && p->bindings.back().name == "Right A: q", "menu: the page and the name show the new key");

    int way = d.Row("Way");
    CHECK(way >= 0 && !e.menuView.rows[way].canLess && e.menuView.rows[way].canMore && e.menuView.rows[way].value == "Press",
          "menu: a choice on its first option: - greyed, + not");
    int when = d.Row("When");
    d.Press(MenuPart::More, when);
    CHECK(p->bindings.back().gesture == Gesture::Tap, "menu: > on 'When' steps to the next way (quick tap)");
    d.PressRow("On");
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

    CHECK(d.PressRow("Delete this binding") && p->bindings.size() == bindingsBefore && e.menuView.title == "Right bindings",
          "menu: Delete removes the binding and goes back to the list");
    {
        int W = 1100, H = (int)(1100 * e.menuView.h / e.menuView.w);
        std::vector<uint32_t> px;
        DrawMenu(px, W, H, e.menuView, p->style);
        SavePng(px, W, H, L"menu_bindings.png");
    }
    CHECK(d.Press(MenuPart::Close) && !e.menuView.open, "menu: the X closes it");
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
    CHECK(q->sticks[1].innerDeadzone == 0.12f && q->sticks[0].innerDeadzone == 0.f, "menu save: stick options saved per hand");
    CHECK(p->bindings[1].sourceIndex == 1, "menu save: after saving, the new binding knows its place in the file");
    _wputenv_s(L"OPENXR_COMPANION_TEST_DATA", L"");
    std::filesystem::remove_all(dir, fec);
}
