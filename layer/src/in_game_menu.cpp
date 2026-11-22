// The in-game settings menu: opening, pages, pointing, the stick, capturing a button or key, saving. See in_game_menu.h.
#include "in_game_menu.h"
#include "engine.h"
#include "menu_pages.h"
#include <algorithm>

void InGameMenu::Toggle() {
    if (IsOpen())
        Close();
    else
        Open();
}

bool InGameMenu::IsOpen() const {
    return e.menuView.open;
}

void InGameMenu::Open() {
    if (!e.fr.headValid || !e.prof) return;
    stack_.clear();
    stack_.push_back([this] { return MenuPages::Home(e, *this); });
    e.menuView = MenuView{};
    e.menuView.open = true;
    Place();
    Refresh();
    Log("in-game menu open");
}

void InGameMenu::Close() {
    if (!IsOpen()) return;
    e.menuView.open = false;
    e.menuView.version++;
    run_.inputDone = nullptr;
    if (run_.saveAt > 0) run_.saveAt = e.now; // save right away
    Log("in-game menu closed");
}

// In front of you, a little below eye level, facing your eyes.
void InGameMenu::Place() {
    XrPosef frame = YawFrame(e.fr.head);
    XrPosef local{ { 0, 0, 0, 1 }, { 0, -0.12f, -0.6f } };
    e.menuView.pose = PMul(frame, local);
    e.menuView.pose.orientation = BillboardTo(V(e.menuView.pose.position), V(e.fr.head.position));
}

void InGameMenu::Push(MenuPageBuilder page) {
    stack_.push_back(std::move(page));
    e.menuView.first = 0;
    Refresh();
}

void InGameMenu::Back() {
    if (stack_.size() <= 1) return;
    stack_.pop_back();
    e.menuView.first = 0;
    Refresh();
}

void InGameMenu::Refresh() {
    if (!IsOpen() || stack_.empty()) return;
    page_ = stack_.back()();
    BuildView();
}

void InGameMenu::Changed() {
    run_.saveAt = e.now + 1.0; // written a moment after the last change
    Refresh();
}

void InGameMenu::BuildView() {
    MenuView& v = e.menuView;
    std::vector<MenuViewRow> rows;
    for (const MenuRow& r : page_.rows) {
        MenuViewRow vr;
        vr.kind = r.kind;
        vr.label = r.label;
        vr.detail = r.detail;
        vr.value = r.ValueText();
        if (r.kind == MenuRowKind::Toggle) vr.isOn = r.isOn && r.isOn();
        if (r.kind == MenuRowKind::Number && r.value) {
            float x = r.value();
            vr.canLess = x > r.min + r.step * 0.01f;
            vr.canMore = x < r.max - r.step * 0.01f;
        }
        if (r.kind == MenuRowKind::Choice && r.chosen) {
            int i = r.chosen();
            vr.canLess = i > 0;
            vr.canMore = i < (int)r.options.size() - 1;
        }
        rows.push_back(std::move(vr));
    }
    std::string status = run_.inputDone ? "Press the button you want, or push the stick (on either controller)" : "";
    bool same = rows.size() == v.rows.size() && v.title == page_.title && v.status == status && v.canBack == (stack_.size() > 1);
    for (size_t i = 0; same && i < rows.size(); i++) {
        const MenuViewRow &a = rows[i], &b = v.rows[i];
        same = a.label == b.label && a.detail == b.detail && a.value == b.value && a.isOn == b.isOn && a.canLess == b.canLess &&
               a.canMore == b.canMore && a.kind == b.kind;
    }
    if (same) return;
    v.rows = std::move(rows);
    v.title = page_.title;
    v.status = status;
    v.canBack = stack_.size() > 1;
    v.first = std::clamp(v.first, 0, std::max(0, (int)v.rows.size() - MenuView::kVisibleRows));
    v.version++;
}

// - / + (direction -1 / +1) on a number or a choice.
void InGameMenu::Step(int row, int direction) {
    if (row < 0 || row >= (int)page_.rows.size()) return;
    MenuRow& r = page_.rows[row];
    if (r.kind == MenuRowKind::Number && r.value && r.set) {
        float v = std::clamp(r.value() + direction * r.step, r.min, r.max);
        r.set(std::round(v / r.step) * r.step); // on the step grid, so 0.1 steps don't drift
        Changed();
    } else if (r.kind == MenuRowKind::Choice && r.chosen && r.choose) {
        int i = std::clamp(r.chosen() + direction, 0, (int)r.options.size() - 1);
        if (i != r.chosen()) {
            r.choose(i);
            Changed();
        }
    }
}

void InGameMenu::Activate(const MenuHit& hit, bool repeat) {
    MenuView& v = e.menuView;
    switch (hit.part) {
    case MenuPart::Less:
        Step(hit.row, -1);
        return;
    case MenuPart::More:
        Step(hit.row, +1);
        return;
    case MenuPart::ScrollUp:
        v.first = std::max(0, v.first - 1);
        v.version++;
        return;
    case MenuPart::ScrollDown:
        v.first = std::min(std::max(0, (int)v.rows.size() - MenuView::kVisibleRows), v.first + 1);
        v.version++;
        return;
    default:
        break;
    }
    if (repeat) return; // only - / + and scrolling repeat while held
    switch (hit.part) {
    case MenuPart::Back:
        Back();
        return;
    case MenuPart::Close:
        Close();
        return;
    case MenuPart::Row: {
        if (hit.row < 0 || hit.row >= (int)page_.rows.size()) return;
        MenuRow& r = page_.rows[hit.row];
        if (r.kind == MenuRowKind::Toggle && r.isOn && r.setOn) {
            r.setOn(!r.isOn());
            Changed();
        } else if (r.kind == MenuRowKind::Choice && r.chosen && r.choose) { // pressing the name cycles round
            int n = (int)r.options.size();
            if (n > 0) r.choose((r.chosen() + 1) % n);
            Changed();
        } else if ((r.kind == MenuRowKind::Link || r.kind == MenuRowKind::Button) && r.press) {
            auto press = r.press; // the page may be rebuilt by it
            press();
        }
        return;
    }
    default:
        return;
    }
}

// The laser on the panel: hover, trigger presses (held - / + and scroll arrows repeat).
void InGameMenu::UpdatePointing() {
    MenuView& v = e.menuView;
    std::vector<MenuBox> boxes;
    MenuLayout(v, boxes);
    MenuHit hover;
    run_.uiHand = -1;
    for (int g = 0; g < 2 && run_.uiHand < 0; g++) {
        float u, w;
        if (!e.screenMirror.PanelRay(g, v.pose, v.w, v.h, u, w)) continue;
        run_.uiHand = g;
        for (const MenuBox& b : boxes)
            if (b.Contains(u, w) && b.hit.part != MenuPart::None) hover = b.hit;
    }
    if (hover != v.hover) {
        if (e.prof->haptics && run_.uiHand >= 0 && hover.part != MenuPart::None)
            e.haptics.push_back({ run_.uiHand, e.prof->hapticAmp * 0.4f, 0.01f });
        v.hover = hover;
        v.version++;
    }
    if (e.now > run_.flashUntil && v.pressed.part != MenuPart::None) {
        v.pressed = MenuHit{};
        v.version++;
    }
    if (run_.uiHand < 0 || hover.part == MenuPart::None || run_.inputDone) {
        run_.repeatHit = MenuHit{};
        return;
    }
    int g = run_.uiHand;
    bool down = e.cur[g][C_TRIGGER][F_PRESS], press = down && !e.prev[g][C_TRIGGER][F_PRESS];
    bool repeats = hover.part == MenuPart::Less || hover.part == MenuPart::More || hover.part == MenuPart::ScrollUp ||
                   hover.part == MenuPart::ScrollDown;
    if (press) {
        if (e.prof->haptics) e.haptics.push_back({ g, e.prof->hapticAmp, 0.025f });
        run_.flashUntil = e.now + 0.15;
        v.pressed = hover;
        v.version++;
        Activate(hover, false);
        run_.repeatHit = repeats ? hover : MenuHit{};
        run_.repeatAt = e.now + 0.45;
    } else if (down && hover == run_.repeatHit && e.now >= run_.repeatAt) {
        Activate(hover, true);
        run_.repeatAt = e.now + 0.09;
    } else if (!down)
        run_.repeatHit = MenuHit{};
}

// The pointing hand's stick: up / down scrolls, left / right moves the number or choice pointed at.
void InGameMenu::UpdateStick(float dt) {
    int g = run_.uiHand;
    if (g < 0 || run_.inputDone) return;
    MenuView& v = e.menuView;
    float sx = e.fr.h[g].sx, sy = e.fr.h[g].sy;
    if (std::fabs(sy) > 0.6f && std::fabs(sy) > std::fabs(sx) && e.now >= run_.scrollAt) {
        int max = std::max(0, (int)v.rows.size() - MenuView::kVisibleRows);
        int first = std::clamp(v.first + (sy < 0 ? 1 : -1), 0, max);
        if (first != v.first) {
            v.first = first;
            v.version++;
        }
        run_.scrollAt = e.now + 0.18;
        return;
    }
    int row = v.hover.row;
    bool onValue = row >= 0 && row < (int)page_.rows.size() &&
                   (page_.rows[row].kind == MenuRowKind::Number || page_.rows[row].kind == MenuRowKind::Choice);
    if (!onValue || std::fabs(sx) < 0.3f) {
        run_.stickCarry = 0;
        return;
    }
    // 6 steps a second at full push
    run_.stickCarry += (std::fabs(sx) - 0.3f) / 0.7f * 6.f * dt * (sx > 0 ? 1.f : -1.f);
    while (std::fabs(run_.stickCarry) >= 1.f) {
        int dir = run_.stickCarry > 0 ? 1 : -1;
        run_.stickCarry -= dir;
        Step(row, dir);
    }
}

void InGameMenu::CaptureInput(std::function<void(const InputId&)> done) {
    run_.inputDone = std::move(done);
    run_.inputArmed = false;
    BuildView();
}

void InGameMenu::CaptureKey(std::function<void(const std::string&)> done) {
    e.keyboard.CaptureNextKey(std::move(done));
}

// Waiting for a button: first every trigger is let go (the one that asked), then the next press or stick push.
// Touch alone doesn't count: a touch comes before every press. The binding page can switch the way to touch.
void InGameMenu::UpdateInputCapture() {
    if (!run_.inputDone) return;
    if (!run_.inputArmed) {
        run_.inputArmed = !e.cur[0][C_TRIGGER][F_PRESS] && !e.cur[1][C_TRIGGER][F_PRESS];
        return;
    }
    const int feats[] = { F_PRESS, F_UP, F_DOWN, F_LEFT, F_RIGHT };
    for (int h = 0; h < 2; h++)
        for (int c = 0; c < C_COUNT; c++)
            for (int f : feats) {
                bool isStickWay = f != F_PRESS;
                if (isStickWay && c != C_STICK) continue;
                if (c == C_THUMBREST && f == F_PRESS) continue; // a thumbrest press is only a touch
                if (!e.cur[h][c][f] || e.prev[h][c][f]) continue;
                InputId id;
                id.hand = h;
                id.comp = c;
                id.feat = f;
                auto done = std::move(run_.inputDone);
                run_.inputDone = nullptr;
                if (e.prof->haptics) e.haptics.push_back({ h, e.prof->hapticAmp, 0.04f });
                done(id);
                Refresh();
                return;
            }
}

void InGameMenu::SaveWhenDue() {
    if (run_.saveAt > 0 && e.now >= run_.saveAt) {
        run_.saveAt = -1;
        e.menuSaveRequested = true;
    }
}

void InGameMenu::Update() {
    SaveWhenDue();
    if (!IsOpen()) {
        run_.uiHand = -1;
        return;
    }
    if (e.kb.visible) { // typing a key for a binding (or the keyboard is open for something else)
        run_.uiHand = -1;
        return;
    }
    float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.1);
    UpdateInputCapture();
    UpdatePointing();
    UpdateStick(dt);
}
