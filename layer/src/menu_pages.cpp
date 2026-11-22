// The in-game menu's pages: see menu_pages.h.
#include "menu_pages.h"
#include "engine.h"
#include "in_game_menu.h"
#include <algorithm>

namespace MenuPages {

std::string InputWords(const InputId& id) {
    if (!id.valid()) return "?";
    static const char* rightNames[C_COUNT] = { "A",  "B",      "trigger",    "grip",         "thumbstick",   "thumbrest",  "menu",
                                               "X",  "Y",      "bumper",     "D-pad up",     "D-pad down",   "D-pad left", "D-pad right" };
    static const char* leftNames[C_COUNT] = { "X",  "Y",      "trigger",    "grip",         "thumbstick",   "thumbrest",  "menu",
                                              "X",  "Y",      "bumper",     "D-pad up",     "D-pad down",   "D-pad left", "D-pad right" };
    std::string s = std::string(id.hand == 0 ? "Left " : "Right ") + (id.hand == 0 ? leftNames[id.comp] : rightNames[id.comp]);
    switch (id.feat) {
    case F_TOUCH:
        return id.comp == C_THUMBREST ? s : s + " (touch)";
    case F_UP:
        return s + ": up";
    case F_DOWN:
        return s + ": down";
    case F_LEFT:
        return s + ": left";
    case F_RIGHT:
        return s + ": right";
    default:
        return s;
    }
}

static const std::vector<std::string> kGestureWords = { "Press", "Quick tap", "Hold", "Double-tap", "While held", "Double-tap and hold" };
static const Gesture kGestures[] = { Gesture::Press, Gesture::Tap, Gesture::Hold, Gesture::DoubleTap, Gesture::WhileHeld, Gesture::DoubleTapHold };

static int GestureIndex(Gesture g) {
    for (int i = 0; i < (int)std::size(kGestures); i++)
        if (kGestures[i] == g) return i;
    return 0;
}

// What a binding does, short: "Space", "Space (held)", "Opens wheel 'right'".
static std::string ActionWords(const Action& a) {
    std::string mode = a.mode == ActMode::Hold ? " (held)" : a.mode == ActMode::Toggle ? " (on / off)" : "";
    switch (a.type) {
    case ActType::Key:
        return (a.keys.empty() ? "no key yet" : a.keys) + mode;
    case ActType::Sequence:
        return "a macro";
    case ActType::Wheel:
        return "opens wheel '" + a.wheel + "'";
    case ActType::Keyboard:
        return "opens the VR keyboard";
    case ActType::Layer:
        return "layer '" + a.layer + "'" + mode;
    case ActType::XrInput:
        return "game sees " + InputWords(a.target) + mode;
    case ActType::None:
        return "off in the game";
    default:
        return "an action set in the editor";
    }
}

static std::string BindingSummary(const Binding& b) {
    std::string s = InputWords(b.chord.back()) + ", " + kGestureWords[GestureIndex(b.gesture)] + ": " + ActionWords(b.action);
    if (!b.layers.empty()) s += "  (layer " + b.layers[0] + ")";
    return s;
}

// The binding's last input (the one that fires it) set to a new one; a combo keeps its other buttons.
static void SetFiringInput(Binding& b, const InputId& id) {
    if (b.chord.empty())
        b.chord.push_back(id);
    else
        b.chord.back() = id;
    b.chordText.clear();
    for (size_t i = 0; i < b.chord.size(); i++)
        b.chordText += (i ? "+" : "") + InputName(b.chord[i]);
}

// What the binding does, from the binding keyboard: a key or mouse button, or a controller button the game sees.
static void SetKeyAction(Binding& b, const std::string& keys) {
    bool wasKey = b.action.type == ActType::Key;
    b.action.type = ActType::Key;
    b.action.keys = keys;
    if (!wasKey) b.action.mode = ActMode::Hold; // held with the button: works for keys, mouse buttons and the wheel
    b.name = InputWords(b.chord.back()) + ": " + keys;
}
static void SetButtonAction(Binding& b, const InputId& target) {
    bool wasButton = b.action.type == ActType::XrInput;
    b.action.type = ActType::XrInput;
    b.action.target = target;
    if (!wasButton) b.action.mode = ActMode::Hold;
    b.name = InputWords(b.chord.back()) + ": " + InputWords(target);
}

static bool HasTouch(int comp) {
    return comp == C_BTN1 || comp == C_BTN2 || comp == C_TRIGGER || comp == C_STICK;
}

MenuPage Home(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Settings" + (e.prof->sourceName.empty() ? std::string() : " - " + e.prof->sourceName);
    auto countOn = [&](int hand) {
        int n = 0;
        for (const Binding& b : e.prof->bindings)
            if (b.enabled && b.chord.back().hand == hand) n++;
        return std::to_string(n) + " on";
    };
    p.rows.push_back(MenuRow::Link("Test controllers", "See what each button, the triggers and the sticks do right now",
                                   [&e, &m] { m.Push([&e, &m] { return TestControllers(e, m); }); }));
    p.rows.push_back(MenuRow::Heading("Controller buttons"));
    for (int hand : { 1, 0 })
        p.rows.push_back(MenuRow::Link(hand ? "Right controller bindings" : "Left controller bindings", countOn(hand),
                                       [&e, &m, hand] { m.Push([&e, &m, hand] { return ControllerBindings(e, m, hand); }); }));
    p.rows.push_back(MenuRow::Heading("Sticks"));
    for (int hand : { 1, 0 }) {
        const StickShape& s = e.prof->sticks[hand];
        std::string now = s.ChangesGame() || s.directionAngleDeg > 0 ? "deadzones / direction zone set" : "all off";
        p.rows.push_back(MenuRow::Link(hand ? "Right stick" : "Left stick", now,
                                       [&e, &m, hand] { m.Push([&e, &m, hand] { return StickOptions(e, m, hand); }); }));
    }
    p.rows.push_back(MenuRow::Heading("More"));
    p.rows.push_back(MenuRow::Link("Controller offsets", "How the game's hands and guns point (opens its own panel)", [&e, &m] {
        m.Close();
        Action a;
        a.type = ActType::Offsets;
        a.mode = ActMode::Panel;
        e.OffsetsAction(a);
    }));
    return p;
}

MenuPage ControllerBindings(Engine& e, InGameMenu& m, int hand) {
    MenuPage p;
    p.title = hand ? "Right bindings" : "Left bindings";
    p.rows.push_back(MenuRow::Live(LiveKind::Controller, hand, 3));
    p.rows.push_back(MenuRow::Button("+ New binding", "Press the button you want, then pick a key, a mouse button or a controller button",
                                     [&e, &m, hand] {
        m.CaptureInput([&e, &m](const InputId& id) {
            Binding b;
            b.name = "New binding";
            SetFiringInput(b, id);
            b.gesture = Gesture::Press;
            b.block = BindingBlock::Trigger;
            b.action.type = ActType::Key;
            b.action.mode = ActMode::Hold;
            int index = 0;
            e.EditBindings([&](std::vector<Binding>& list) {
                list.push_back(b);
                index = (int)list.size() - 1;
            });
            m.Changed();
            m.Push([&e, &m, index] { return BindingDetail(e, m, index); });
            PickAction(e, m, index);
        });
    }));
    int shown = 0;
    for (int i = 0; i < (int)e.prof->bindings.size(); i++) {
        const Binding& b = e.prof->bindings[i];
        if (b.chord.back().hand != hand) continue;
        shown++;
        p.rows.push_back(MenuRow::Link(b.name + (b.enabled ? "" : "  (off)"), BindingSummary(b),
                                       [&e, &m, i] { m.Push([&e, &m, i] { return BindingDetail(e, m, i); }); }));
    }
    if (shown == 0) p.rows.push_back(MenuRow::Note("No bindings on this controller yet"));
    return p;
}

// Opens the binding keyboard for binding `index`: a key or mouse button makes it press that, its "Controller button"
// key then asks for the controller button the game should see instead.
void PickAction(Engine& e, InGameMenu& m, int index) {
    auto change = [&e, &m, index](std::function<void(Binding&)> f) {
        e.EditBindings([&](std::vector<Binding>& list) {
            if (index < (int)list.size()) f(list[index]);
        });
        m.Changed();
    };
    m.CaptureAction([change](const std::string& keys) { change([keys](Binding& x) { SetKeyAction(x, keys); }); },
                    [change](const InputId& target) { change([target](Binding& x) { SetButtonAction(x, target); }); });
}

MenuPage BindingDetail(Engine& e, InGameMenu& m, int index) {
    MenuPage p;
    if (index < 0 || index >= (int)e.prof->bindings.size()) {
        p.title = "Binding";
        p.rows.push_back(MenuRow::Note("This binding is gone"));
        return p;
    }
    // Every row edits the binding through the engine (EditBindings), which ends whatever it was doing first.
    auto edit = [&e, &m, index](std::function<void(Binding&)> change) {
        e.EditBindings([&](std::vector<Binding>& list) {
            if (index < (int)list.size()) change(list[index]);
        });
        m.Changed();
    };
    auto get = [&e, index]() -> const Binding& { return e.prof->bindings[index]; };
    const Binding& b = get();
    p.title = b.name;
    p.rows.push_back(MenuRow::Toggle("On", "", [get] { return get().enabled; }, [edit](bool on) { edit([on](Binding& x) { x.enabled = on; }); }));

    InputId firing = b.chord.back();
    p.rows.push_back(MenuRow::Button("Button: " + InputWords(firing) + (b.chord.size() > 1 ? " (in a combo)" : ""),
                                     "Press here, then press the button you want", [&m, edit] {
                                         m.CaptureInput([edit](const InputId& id) { edit([id](Binding& x) { SetFiringInput(x, id); }); });
                                     }));
    if (HasTouch(firing.comp) && (firing.feat == F_PRESS || firing.feat == F_TOUCH))
        p.rows.push_back(MenuRow::Choice("Way", "Touch = resting a finger on it, without pressing", { "Press", "Touch" },
                                         [get] { return get().chord.back().feat == F_TOUCH ? 1 : 0; },
                                         [edit](int i) {
                                             edit([i](Binding& x) {
                                                 InputId id = x.chord.back();
                                                 id.feat = i == 1 ? F_TOUCH : F_PRESS;
                                                 SetFiringInput(x, id);
                                                 if (i == 1) x.block = BindingBlock::None; // hiding a touch would hide its presses
                                             });
                                         }));
    p.rows.push_back(MenuRow::Choice("When", "", kGestureWords, [get] { return GestureIndex(get().gesture); },
                                     [edit](int i) { edit([i](Binding& x) { x.gesture = kGestures[i]; }); }));
    if (b.gesture == Gesture::Hold) {
        MenuRow hold = MenuRow::Number("Hold for", "", [get] { return (float)get().holdMs; },
                                       [edit](float v) { edit([v](Binding& x) { x.holdMs = (int)v; }); }, 100, 2000, 50);
        hold.unit = "ms";
        p.rows.push_back(hold);
    }
    bool isKey = b.action.type == ActType::Key, isButton = b.action.type == ActType::XrInput;
    p.rows.push_back(MenuRow::Button("Does: " + ActionWords(b.action),
                                     isKey || isButton ? "Press here, then pick a key, a mouse button or Controller button on the VR keyboard"
                                                       : "Set in the editor. Press here to make it a key, mouse button or controller button instead",
                                     [&e, &m, index] { PickAction(e, m, index); }));
    if (isKey || isButton) {
        static const ActMode modes[] = { ActMode::Tap, ActMode::Hold, ActMode::Toggle };
        p.rows.push_back(MenuRow::Choice(isKey ? "The key is" : "The game's button is", "", { "Tapped", "Held", "On / off" },
                                         [get] {
                                             ActMode md = get().action.mode;
                                             return md == ActMode::Hold ? 1 : md == ActMode::Toggle ? 2 : 0;
                                         },
                                         [edit](int i) { edit([i](Binding& x) { x.action.mode = modes[i]; }); }));
    }
    p.rows.push_back(MenuRow::Choice("Game sees the button", "While this binding is in use", { "No", "Yes" },
                                     [get] { return get().block == BindingBlock::None ? 1 : 0; },
                                     [edit](int i) { edit([i](Binding& x) { x.block = i ? BindingBlock::None : BindingBlock::Trigger; }); }));
    MenuRow remove = MenuRow::Button("Delete this binding", "", [&e, &m, index] {
        e.EditBindings([index](std::vector<Binding>& list) {
            if (index < (int)list.size()) list.erase(list.begin() + index);
        });
        m.Changed();
        m.Back();
    });
    remove.isDanger = true;
    p.rows.push_back(remove);
    return p;
}

MenuPage TestControllers(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Test controllers";
    p.rows.push_back(MenuRow::Live(LiveKind::BothControllers, 1, 4));
    p.rows.push_back(MenuRow::Note("Press, touch and push things: they light up here", "A ring = touched, filled = pressed. The white dot is the stick."));
    for (int hand : { 1, 0 })
        p.rows.push_back(MenuRow::Link(hand ? "Right stick" : "Left stick", "Its deadzones, with the stick live",
                                       [&e, &m, hand] { m.Push([&e, &m, hand] { return StickOptions(e, m, hand); }); }));
    return p;
}

MenuPage StickOptions(Engine& e, InGameMenu& m, int hand) {
    MenuPage p;
    p.title = hand ? "Right stick" : "Left stick";
    StickShape* s = &e.prof->sticks[hand];
    auto share = [&m](const char* label, const char* detail, float* field) {
        MenuRow r = MenuRow::Number(label, detail, [field] { return *field; }, [field, &m](float v) { *field = v; }, 0, 0.9f, 0.01f);
        r.scale = 100;
        r.unit = "%";
        r.atMin = "off";
        return r;
    };
    p.rows.push_back(MenuRow::Live(LiveKind::Stick, hand, 3));
    p.rows.push_back(MenuRow::Heading("What the game sees"));
    p.rows.push_back(share("Inner deadzone", "Small pushes count as none (stops drift)", &s->innerDeadzone));
    p.rows.push_back(share("Outer deadzone", "Near the edge already counts as full", &s->outerDeadzone));
    p.rows.push_back(MenuRow::Heading("Your stick bindings"));
    MenuRow zone = MenuRow::Number("Direction zone", "Directions only count close to straight",
                                   [s] { return s->directionAngleDeg; }, [s](float v) { s->directionAngleDeg = v; }, 0, 45, 5);
    zone.unit = "\xC2\xB0";
    zone.atMin = "off";
    p.rows.push_back(zone);
    MenuRow pressAt = MenuRow::Number("Direction counts at", "How far to push (both sticks)", [&e] { return e.prof->stickOn; },
                                      [&e](float v) {
                                          e.prof->stickOn = v;
                                          e.prof->stickOff = std::max(0.1f, v - 0.2f); // let go a little below it
                                      },
                                      0.3f, 0.95f, 0.05f);
    pressAt.scale = 100;
    pressAt.unit = "%";
    p.rows.push_back(pressAt);
    return p;
}

} // namespace MenuPages
