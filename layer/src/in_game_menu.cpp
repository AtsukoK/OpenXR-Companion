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
    stack_.push_back([this] { return MenuPages::Main(e, *this); });
    e.menuView = MenuView{};
    e.menuView.open = true;
    Place();
    Refresh();
    Log("in-game menu open");
}

void InGameMenu::Close() {
    if (!IsOpen()) return;
    if (e.kb.inMenu) e.keyboard.Close(); // its keyboard goes with it
    e.menuView.open = false;
    e.menuView.version++;
    run_.inputDone = nullptr;
    if (run_.saveAt > 0) run_.saveAt = e.now; // save right away
    Log("in-game menu closed");
}

// Toward the front of the playspace (where it has one; else where you look), a little below eye level, facing you.
void InGameMenu::Place() {
    anchor_ = YawFrame(e.fr.head);
    if (e.fr.stageValid) {
        XrPosef st = e.fr.stage;
        st.position = e.fr.head.position;
        anchor_.orientation = YawFrame(st).orientation; // the playspace's forward, turned only left/right
    }
    placedAt_ = 0;
    PlaceAt(page_.distance > 0 ? page_.distance : kMenuDistance);
}

void InGameMenu::PlaceAt(float distance) {
    if (distance == placedAt_) return;
    placedAt_ = distance;
    XrPosef local{ { 0, 0, 0, 1 }, { 0, -0.2f * distance, -distance } }; // the same angle below the eyes at any distance
    e.menuView.pose = PMul(anchor_, local);
    e.menuView.pose.orientation = BillboardTo(V(e.menuView.pose.position), V(anchor_.position));
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

void InGameMenu::SaveNow() {
    run_.saveAt = -1;
    e.menuSaveRequested = true;
    e.menuSaveSay = true;
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
        vr.isDanger = r.isDanger;
        vr.isWarn = r.isWarn;
        vr.live = r.live;
        vr.liveHand = r.liveHand;
        vr.span = r.span;
        vr.tags = r.tags;
        vr.items = r.items;
        vr.itemDetails = r.itemDetails;
        vr.cols = r.cols;
        vr.previews = r.previews;
        vr.previewWrist = r.previewWrist;
        vr.previewCols = r.previewCols;
        vr.previewSelected = r.previewSelected;
        for (const Wheel& w : r.previews) {
            vr.previewSig += w.title + "|";
            for (const WheelItem& it : w.items)
                vr.previewSig += it.label + "/" + it.icon + "/" + std::to_string(it.color) + "/" + std::to_string((int)it.action.type) + ";";
            vr.previewSig += "#";
        }
        if (r.kind == MenuRowKind::Toggle) vr.isOn = r.isOn && r.isOn();
        if (r.kind == MenuRowKind::Number && r.value) {
            float x = r.value();
            vr.canLess = x > r.min + r.step * 0.01f;
            vr.canMore = x < r.max - r.step * 0.01f;
            vr.fraction = r.max > r.min ? std::clamp((x - r.min) / (r.max - r.min), 0.f, 1.f) : 0.f;
        }
        if (r.kind == MenuRowKind::Choice && r.chosen) {
            int i = r.chosen();
            vr.canLess = i > 0;
            vr.canMore = i < (int)r.options.size() - 1;
        }
        rows.push_back(std::move(vr));
    }
    std::string status = run_.inputDone ? "Press the button you want, or push the stick (on either controller)"
                         : isTyping_    ? "Type, then Enter. Close the keyboard to cancel"
                                        : "";
    std::string title = isTyping_ ? typingWhat_ + ": " + typing_ + "_" : page_.title;
    bool same = rows.size() == v.rows.size() && v.title == title && v.status == status && v.canBack == (stack_.size() > 1);
    for (size_t i = 0; same && i < rows.size(); i++) {
        const MenuViewRow &a = rows[i], &b = v.rows[i];
        same = a.label == b.label && a.detail == b.detail && a.value == b.value && a.isOn == b.isOn && a.canLess == b.canLess &&
               a.canMore == b.canMore && a.kind == b.kind && a.fraction == b.fraction && a.tags == b.tags && a.items == b.items && a.itemDetails == b.itemDetails && a.previewSig == b.previewSig && a.previewSelected == b.previewSelected && a.previewCols == b.previewCols;
    }
    float scale = page_.scale > 0 ? page_.scale : 1.f;
    int nRows = page_.shownRows > 0 ? page_.shownRows : kMenuListRows;
    float width = (page_.width > 0 ? page_.width : kMenuListWidth) * scale;
    PlaceAt(page_.distance > 0 ? page_.distance : kMenuDistance);
    same = same && v.w == width && v.scale == scale && v.stickFree == page_.stickFree && v.visibleRows == nRows;
    if (same) return;
    v.w = width;
    v.h = kMenuHeight * MenuHeightFactor(nRows) * scale;
    v.visibleRows = nRows;
    v.scale = scale;
    v.stickFree = page_.stickFree;
    v.rows = std::move(rows);
    v.title = title;
    v.status = status;
    v.canBack = stack_.size() > 1;
    v.first = std::clamp(v.first, 0, MenuMaxFirst(v));
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
        v.first = std::min(MenuMaxFirst(v), v.first + 1);
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
        } else if (r.kind == MenuRowKind::Overview && r.openTag && hit.sub >= 0 && hit.sub < (int)r.tags.size()) {
            auto open = r.openTag;
            MenuTag tag = r.tags[hit.sub];
            open(tag.hand, tag.comp);
        } else if (r.kind == MenuRowKind::Preview && hit.sub >= 0) {
            int n = r.previews.empty() ? 0 : (int)r.previews[0].items.size();
            if (hit.sub < n) {
                if (auto open = r.openItem) open(hit.sub);
            } else if (hit.sub - n < (int)r.itemPress.size() && r.itemPress[hit.sub - n]) {
                auto press = r.itemPress[hit.sub - n];
                press();
            }
        } else if ((r.kind == MenuRowKind::Bar || r.kind == MenuRowKind::Tiles) && hit.sub >= 0 && hit.sub < (int)r.itemPress.size() && r.itemPress[hit.sub]) {
            auto press = r.itemPress[hit.sub];
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
    // the pull that pressed a key on the menu's keyboard (and closed it) doesn't also press what lies under that key
    if (kbHeldTrigger_ && (e.cur[0][C_TRIGGER][F_PRESS] || e.cur[1][C_TRIGGER][F_PRESS])) {
        if (v.hover.part != MenuPart::None) {
            v.hover = MenuHit{};
            v.version++;
        }
        run_.repeatHit = MenuHit{};
        return;
    }
    kbHeldTrigger_ = false;
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
    if (g < 0 || run_.inputDone || page_.stickFree) return;
    MenuView& v = e.menuView;
    float sx = e.fr.h[g].sx, sy = e.fr.h[g].sy;
    if (std::fabs(sy) > 0.6f && std::fabs(sy) > std::fabs(sx) && e.now >= run_.scrollAt) {
        int max = MenuMaxFirst(v);
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

void InGameMenu::AskText(const std::string& what, const std::string& start, std::function<void(const std::string&)> done) {
    typing_ = start;
    typingWhat_ = what;
    isTyping_ = true;
    BuildView();
    e.keyboard.CaptureText([this, done = std::move(done)](int kind, const std::string& ch) {
        if (kind == 0 && typing_.size() < 40)
            typing_ += ch;
        else if (kind == 1 && !typing_.empty()) {
            do typing_.pop_back(); // a whole UTF-8 character
            while (!typing_.empty() && ((unsigned char)typing_.back() & 0xC0) == 0x80);
        }
        if (kind >= 2) {
            isTyping_ = false;
            std::string text = typing_;
            while (!text.empty() && text.back() == ' ')
                text.pop_back();
            if (kind == 2 && !text.empty()) done(text);
            Refresh();
            return;
        }
        BuildView();
    });
}

void InGameMenu::CaptureAction(std::function<void(const std::string&)> key, std::function<void(const InputId&)> button) {
    e.keyboard.CaptureNextKey(std::move(key), [this, button = std::move(button)] {
        e.Toast("Press the controller button the game should see", 3.0);
        CaptureInput(button);
    });
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

// Live rows: copy what the controllers do now into the view, at most 20 times a second and only when it changed
// (each change redraws the whole panel).
void InGameMenu::UpdateLive() {
    MenuView& v = e.menuView;
    bool hasLive = std::any_of(v.rows.begin(), v.rows.end(),
                               [](const MenuViewRow& r) { return r.kind == MenuRowKind::Live || r.kind == MenuRowKind::Overview; });
    if (!hasLive || e.now < run_.liveAt) return;
    run_.liveAt = e.now + 0.05;
    bool changed = v.stickOn != e.prof->stickOn;
    v.stickOn = e.prof->stickOn;
    for (int h = 0; h < 2; h++) {
        const HandRaw& r = e.fr.h[h];
        LiveHand now;
        now.kind = r.kind;
        for (int c = 0; c < C_COUNT; c++) {
            now.press[c] = e.cur[h][c][F_PRESS];
            now.touch[c] = e.cur[h][c][F_TOUCH];
        }
        now.trigger = r.trig;
        now.grip = r.grip;
        now.stickX = r.sx;
        now.stickY = r.sy;
        now.shape = e.StickShapeOf(h);
        if (!(now == v.live[h])) {
            v.live[h] = now;
            changed = true;
        }
    }
    if (changed) v.version++;
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
        kbWasOnMenu_ = e.kb.inMenu;
        return;
    }
    if (kbWasOnMenu_) { // it just closed: wait until the trigger that closed it is let go
        kbWasOnMenu_ = false;
        kbHeldTrigger_ = true;
    }
    float dt = (float)std::clamp(e.now - e.lastNow, 0.0, 0.1);
    UpdateLive();
    UpdateInputCapture();
    UpdatePointing();
    UpdateStick(dt);
}
