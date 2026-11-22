// The in-game menu's pages: see menu_pages.h.
#include "menu_pages.h"
#include "engine.h"
#include "controller_silhouettes.h"
#include "in_game_menu.h"
#include <algorithm>

namespace MenuPages {

std::string InputWords(const InputId& id) {
    if (!id.valid()) return "?";
    static const char* rightNames[C_COUNT] = { "A",  "B",      "trigger",    "grip",         "thumbstick",   "thumbrest",  "menu",
                                               "X",  "Y",      "bumper",     "D-pad up",     "D-pad down",   "D-pad left", "D-pad right",
                                               "trackpad" };
    static const char* leftNames[C_COUNT] = { "X",  "Y",      "trigger",    "grip",         "thumbstick",   "thumbrest",  "menu",
                                              "X",  "Y",      "bumper",     "D-pad up",     "D-pad down",   "D-pad left", "D-pad right",
                                              "trackpad" };
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
    case F_FULL:
        return s + (id.comp == C_GRIP ? " (full squeeze)" : " (full pull)");
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

std::string PartName(int hand, int comp, ControllerKind kind) {
    static const char* names[C_COUNT] = { "A",  "B",      "Trigger",  "Grip",       "Thumbstick", "Thumbrest",  "Menu",
                                          "X",  "Y",      "Bumper",   "D-pad up",   "D-pad down", "D-pad left", "D-pad right",
                                          "Trackpad" };
    if (comp < 0 || comp >= C_COUNT) return "?";
    bool xy = hand == 0 && kind != ControllerKind::Index && kind != ControllerKind::SteamFrame;
    if (comp == C_BTN1 && xy) return "X";
    if (comp == C_BTN2 && xy) return "Y";
    if (comp == C_MENU && hand == 0 && kind == ControllerKind::SteamFrame) return "View";
    return names[comp];
}

// What a binding does, short: "Key: Space (hold)", "Controller: Right B", "Opens wheel 'right'".
static std::string ActionWords(const Action& a) {
    std::string mode = a.mode == ActMode::Hold ? " (hold)" : a.mode == ActMode::Toggle ? " (toggle)" : "";
    switch (a.type) {
    case ActType::Key:
        return a.keys.empty() ? "No key set" : "Key: " + a.keys + mode;
    case ActType::Sequence:
        return "Macro";
    case ActType::Wheel:
        return "Opens wheel '" + a.wheel + "'";
    case ActType::Keyboard:
        return "Opens the VR keyboard";
    case ActType::Layer:
        return "Layer '" + a.layer + "'" + mode;
    case ActType::XrInput:
        return "Controller: " + InputWords(a.target) + mode;
    case ActType::None:
        return "Disabled in the game";
    case ActType::Menu:
        return "Opens this menu";
    case ActType::Back:
        return "Wheel: back";
    case ActType::Close:
        return "Wheel: close";
    case ActType::Reload:
        return "Reloads the profile";
    case ActType::Page:
        return "Wrist panel page";
    case ActType::Adjust:
        return "Adjust mode";
    case ActType::Screen:
        return "Desktop screen";
    case ActType::Offsets:
        return "Controller offsets";
    case ActType::Mirror:
        return "Headset mirror";
    default:
        return "Set in the editor";
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
    return comp == C_BTN1 || comp == C_BTN2 || comp == C_TRIGGER || comp == C_STICK || comp == C_TRACKPAD ||
           (comp >= C_FRAME_FIRST && comp <= C_FRAME_LAST);
}

// What a part does, for its tag: the player's bindings first (blue), then the game's own controls; "Not used".
static MenuTag TagFor(Engine& e, int hand, int comp, ControllerKind kind) {
    MenuTag t;
    t.hand = hand;
    t.comp = comp;
    t.title = PartName(hand, comp, kind);
    for (const Binding& b : e.prof->bindings) {
        if (!b.enabled || b.chord.empty() || b.chord.back().hand != hand || b.chord.back().comp != comp) continue;
        std::string how = b.chord.size() > 1 ? "Combo" : kGestureWords[GestureIndex(b.gesture)];
        if (b.chord.back().feat == F_TOUCH && comp != C_THUMBREST) how += " (touch)";
        t.lines.push_back(how); // small and grey, then what it does
        t.lineKinds.push_back(3);
        t.lines.push_back(ActionWords(b.action));
        t.lineKinds.push_back(1);
    }
    for (const GameUse& u : e.gameUses) {
        if (u.hand != hand || u.comp != comp) continue;
        t.lines.push_back(u.name + (u.touch && comp != C_THUMBREST ? " (touch)" : ""));
        t.lineKinds.push_back(0);
    }
    if (t.lines.empty()) {
        t.lines.push_back("Default"); // the game's own control (its name isn't known)
        t.lineKinds.push_back(2);
    } else if (t.lines.size() > 5) {
        t.lines.resize(4);
        t.lineKinds.resize(4);
        t.lines.push_back("More on its page");
        t.lineKinds.push_back(2);
    }
    return t;
}

// The parts a controller has that can be bound, from its picture (and the trigger and grip, which all have).
static std::vector<int> PartsOf(ControllerKind kind, int hand) {
    std::vector<int> comps;
    auto add = [&](int c) {
        if (c >= 0 && std::find(comps.begin(), comps.end(), c) == comps.end()) comps.push_back(c);
    };
    if (const Silhouette* sil = SilhouetteFor(kind, hand))
        for (int i = 0; i < sil->partCount; i++)
            add(SilhouetteComp(sil->parts[i].comp));
    else
        for (const ArtPart& a : ArtFor(kind).parts)
            if (a.hands & (hand == 0 ? 1 : 2)) add(a.comp);
    add(C_TRIGGER);
    add(C_GRIP);
    return comps;
}

MenuPage Overview(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Controller configuration";
    p.scale = 2.4f;     // big and further away: easy to read and point at, still inside the view
    p.distance = 1.4f;
    p.width = 0.82f; // wider than the other pages: about 66 degrees across at its distance, inside the view
    std::vector<MenuTag> tags;
    for (int hand = 0; hand < 2; hand++) {
        ControllerKind kind = e.KindOf(hand);
        for (int comp : PartsOf(kind, hand))
            tags.push_back(TagFor(e, hand, comp, kind));
    }
    p.rows.push_back(MenuRow::Overview(std::move(tags), [&e, &m](int hand, int comp) {
        m.Push([&e, &m, hand, comp] { return PartPage(e, m, hand, comp); });
    }, 7));
    p.rows.push_back(MenuRow::Bar({ "Edit bindings", "Test controllers", "Right stick", "Left stick" },
                                  { [&e, &m] { m.Push([&e, &m] { return Home(e, m); }); },
                                    [&e, &m] { m.Push([&e, &m] { return TestControllers(e, m); }); },
                                    [&e, &m] { m.Push([&e, &m] { return StickOptions(e, m, 1); }); },
                                    [&e, &m] { m.Push([&e, &m] { return StickOptions(e, m, 0); }); } }));
    return p;
}

// Each way a part can fire a binding: the feature and gesture, and its name on the part's page.
struct PartWay {
    int feat;
    Gesture gesture;
    const char* name;
};
static std::vector<PartWay> WaysOf(int comp, ControllerKind kind) {
    if (comp == C_TRACKPAD)
        return { { F_PRESS, Gesture::Press, "Press" },   { F_TOUCH, Gesture::Press, "Touch" },       { F_UP, Gesture::Press, "Swipe up" },
                 { F_DOWN, Gesture::Press, "Swipe down" }, { F_LEFT, Gesture::Press, "Swipe left" }, { F_RIGHT, Gesture::Press, "Swipe right" } };
    if (comp == C_STICK)
        return { { F_PRESS, Gesture::Press, "Click" },   { F_UP, Gesture::Press, "Push up" },      { F_DOWN, Gesture::Press, "Push down" },
                 { F_LEFT, Gesture::Press, "Push left" }, { F_RIGHT, Gesture::Press, "Push right" }, { F_TOUCH, Gesture::Press, "Touch" },
                 // a double flick, then held: lasts while you keep pushing (sprint on a double flick forward)
                 { F_UP, Gesture::DoubleTapHold, "Double flick up" },     { F_DOWN, Gesture::DoubleTapHold, "Double flick down" },
                 { F_LEFT, Gesture::DoubleTapHold, "Double flick left" }, { F_RIGHT, Gesture::DoubleTapHold, "Double flick right" } };
    if (comp == C_THUMBREST) return { { F_TOUCH, Gesture::Press, "Touch" }, { F_TOUCH, Gesture::DoubleTap, "Double-tap" } };
    const char* press = comp == C_TRIGGER ? "Pull" : comp == C_GRIP ? "Squeeze" : "Press";
    bool frame = kind == ControllerKind::SteamFrame;
    std::vector<PartWay> w = { { F_PRESS, Gesture::Press, press },
                               { F_PRESS, Gesture::Tap, "Quick tap" },
                               { F_PRESS, Gesture::Hold, "Hold" },
                               { F_PRESS, Gesture::DoubleTap, "Double-tap" } };
    if (comp == C_BTN1 || comp == C_BTN2 || comp == C_TRIGGER || (comp >= C_FRAME_FIRST && comp <= C_FRAME_LAST))
        w.push_back({ F_TOUCH, Gesture::Press, "Touch" }); // the Steam Frame senses touch on all its buttons
    if (comp == C_GRIP && frame) w.push_back({ F_TOUCH, Gesture::Press, "Touch" });
    // the click at the end of a dual-stage pull: the Frame's grips and triggers, the Index trigger; the Index grip's
    // firm squeeze (force) stands in for a click
    bool index = kind == ControllerKind::Index;
    if ((comp == C_GRIP && (frame || index)) || (comp == C_TRIGGER && (frame || index)))
        w.push_back({ F_FULL, Gesture::Press, comp == C_GRIP ? "Full squeeze" : "Full pull" });
    return w;
}

MenuPage PartPage(Engine& e, InGameMenu& m, int hand, int comp) {
    MenuPage p;
    ControllerKind kind = e.KindOf(hand);
    p.title = std::string(hand ? "Right " : "Left ") + PartName(hand, comp, kind);
    p.rows.push_back(MenuRow::Live(LiveKind::Controller, hand, 3));
    std::string game;
    for (const GameUse& u : e.gameUses)
        if (u.hand == hand && u.comp == comp) game += (game.empty() ? "" : ", ") + u.name + (u.touch && comp != C_THUMBREST ? " (touch)" : "");
    p.rows.push_back(MenuRow::Note(game.empty() ? "In the game: default" : "In the game: " + game,
                                   game.empty() ? "The game's own name for its control isn't known" : "What the game itself does with this button"));
    p.rows.push_back(MenuRow::Heading("Bindings"));
    std::vector<bool> shown(e.prof->bindings.size(), false);
    for (const PartWay& w : WaysOf(comp, kind)) {
        int found = -1;
        for (int i = 0; i < (int)e.prof->bindings.size() && found < 0; i++) {
            const Binding& b = e.prof->bindings[i];
            const InputId& id = b.chord.back();
            if (b.chord.size() == 1 && id.hand == hand && id.comp == comp && id.feat == w.feat && b.gesture == w.gesture) found = i;
        }
        if (found >= 0) {
            shown[found] = true;
            const Binding& b = e.prof->bindings[found];
            p.rows.push_back(MenuRow::Link(w.name, ActionWords(b.action) + (b.enabled ? "" : "  (disabled)"),
                                           [&e, &m, found] { m.Push([&e, &m, found] { return BindingDetail(e, m, found); }); }));
            continue;
        }
        PartWay way = w;
        p.rows.push_back(MenuRow::Button(w.name, "Default",
                                         [&e, &m, hand, comp, way] {
                                             Binding b;
                                             InputId id;
                                             id.hand = hand;
                                             id.comp = comp;
                                             id.feat = way.feat;
                                             SetFiringInput(b, id);
                                             b.name = InputWords(id);
                                             b.gesture = way.gesture;
                                             b.block = way.feat == F_TOUCH ? BindingBlock::None : BindingBlock::Trigger;
                                             if (way.gesture == Gesture::DoubleTapHold) {
                                                 b.block = BindingBlock::None; // the game still gets the push (you keep walking)
                                                 b.doubleTapMs = 350;
                                             }
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
                                         }));
    }
    bool heading = false;
    for (int i = 0; i < (int)e.prof->bindings.size(); i++) {
        const Binding& b = e.prof->bindings[i];
        if (shown[i] || b.chord.empty()) continue;
        bool uses = false;
        for (const InputId& id : b.chord)
            uses = uses || (id.hand == hand && id.comp == comp);
        if (!uses) continue;
        if (!heading) p.rows.push_back(MenuRow::Heading("Other bindings with this button"));
        heading = true;
        p.rows.push_back(MenuRow::Link(b.name + (b.enabled ? "" : "  (disabled)"), BindingSummary(b),
                                       [&e, &m, i] { m.Push([&e, &m, i] { return BindingDetail(e, m, i); }); }));
    }
    return p;
}

MenuPage Home(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Edit bindings" + (e.prof->sourceName.empty() ? std::string() : " - " + e.prof->sourceName);
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
    p.rows.push_back(MenuRow::Toggle("Enabled", "", [get] { return get().enabled; }, [edit](bool on) { edit([on](Binding& x) { x.enabled = on; }); }));

    InputId firing = b.chord.back();
    p.rows.push_back(MenuRow::Button("Input: " + InputWords(firing) + (b.chord.size() > 1 ? " (in a combo)" : ""),
                                     "Select, then press the new button", [&m, edit] {
                                         m.CaptureInput([edit](const InputId& id) { edit([id](Binding& x) { SetFiringInput(x, id); }); });
                                     }));
    if (HasTouch(firing.comp) && (firing.feat == F_PRESS || firing.feat == F_TOUCH))
        p.rows.push_back(MenuRow::Choice("Activation", "Touch: a finger resting on it, without pressing", { "Press", "Touch" },
                                         [get] { return get().chord.back().feat == F_TOUCH ? 1 : 0; },
                                         [edit](int i) {
                                             edit([i](Binding& x) {
                                                 InputId id = x.chord.back();
                                                 id.feat = i == 1 ? F_TOUCH : F_PRESS;
                                                 SetFiringInput(x, id);
                                                 if (i == 1) x.block = BindingBlock::None; // hiding a touch would hide its presses
                                             });
                                         }));
    if (e.AnalogOf(firing) >= 0) {
        // Hair trigger (low) or a long pull (high); the profile's usual point when at the minimum.
        bool force = firing.feat == F_FULL;
        MenuRow at = MenuRow::Number(force ? "Squeeze strength" : "Press point",
                                     force ? "How hard to squeeze past a closed grip" : "Low = hair trigger, high = a long pull",
                                     [get] { return get().pressAt; },
                                     [edit](float v) { edit([v](Binding& x) { x.pressAt = v < 0.025f ? 0.f : v; }); }, 0, 0.95f, 0.05f);
        at.scale = 100;
        at.unit = "%";
        at.atMin = "Default";
        p.rows.push_back(at);
    }
    p.rows.push_back(MenuRow::Choice("Gesture", "", kGestureWords, [get] { return GestureIndex(get().gesture); },
                                     [edit](int i) { edit([i](Binding& x) { x.gesture = kGestures[i]; }); }));
    if (b.gesture == Gesture::Hold) {
        MenuRow hold = MenuRow::Number("Hold time", "", [get] { return (float)get().holdMs; },
                                       [edit](float v) { edit([v](Binding& x) { x.holdMs = (int)v; }); }, 100, 2000, 50);
        hold.unit = "ms";
        p.rows.push_back(hold);
    }
    bool isKey = b.action.type == ActType::Key, isButton = b.action.type == ActType::XrInput;
    p.rows.push_back(MenuRow::Button("Action: " + ActionWords(b.action),
                                     isKey || isButton ? "Select to choose a key, mouse button or controller button"
                                                       : "Set in the editor. Select to replace it with a key, mouse button or controller button",
                                     [&e, &m, index] { PickAction(e, m, index); }));
    if (isKey || isButton) {
        static const ActMode modes[] = { ActMode::Tap, ActMode::Hold, ActMode::Toggle };
        p.rows.push_back(MenuRow::Choice(isKey ? "Key behavior" : "Button behavior", "", { "Tap", "Hold", "Toggle" },
                                         [get] {
                                             ActMode md = get().action.mode;
                                             return md == ActMode::Hold ? 1 : md == ActMode::Toggle ? 2 : 0;
                                         },
                                         [edit](int i) { edit([i](Binding& x) { x.action.mode = modes[i]; }); }));
    }
    p.rows.push_back(MenuRow::Choice("Pass input to game", "Whether the game also receives this button", { "No", "Yes" },
                                     [get] { return get().block == BindingBlock::None ? 1 : 0; },
                                     [edit](int i) { edit([i](Binding& x) { x.block = i ? BindingBlock::None : BindingBlock::Trigger; }); }));
    MenuRow remove = MenuRow::Button("Delete binding", "", [&e, &m, index] {
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
    p.stickFree = true;
    p.rows.push_back(MenuRow::Live(LiveKind::BothControllers, 1, 4));
    p.rows.push_back(MenuRow::Note("Press, touch and push things: they light up here", "A ring = touched, filled = pressed. The white dot is the stick."));
    for (int hand : { 1, 0 })
        p.rows.push_back(MenuRow::Link(hand ? "Right stick" : "Left stick", "Its deadzones, with the stick live",
                                       [&e, &m, hand] { m.Push([&e, &m, hand] { return StickOptions(e, m, hand); }); }));
    return p;
}

// One stick setting at a time, with the stick big and live above it (the sticks don't scroll these pages).
static MenuPage StickPage(const char* title, int hand) {
    MenuPage p;
    p.title = title;
    p.stickFree = true;
    p.rows.push_back(MenuRow::Live(LiveKind::Stick, hand, 5));
    return p;
}

static MenuRow ShareRow(const char* label, const char* detail, float* field) {
    MenuRow r = MenuRow::Number(label, detail, [field] { return *field; }, [field](float v) { *field = v; }, 0, 0.9f, 0.01f);
    r.scale = 100;
    r.unit = "%";
    r.atMin = "off";
    return r;
}

static MenuPage StickDeadzones(Engine& e, int hand) {
    MenuPage p = StickPage(hand ? "Right stick: deadzones" : "Left stick: deadzones", hand);
    StickShape* s = &e.prof->sticks[hand];
    p.rows.push_back(ShareRow("Inner deadzone", "Small pushes count as none (stops drift)", &s->innerDeadzone));
    p.rows.push_back(ShareRow("Outer deadzone", "Near the edge already counts as full", &s->outerDeadzone));
    return p;
}

static MenuPage StickDirections(Engine& e, int hand) {
    MenuPage p = StickPage(hand ? "Right stick: directions" : "Left stick: directions", hand);
    StickShape* s = &e.prof->sticks[hand];
    MenuRow zone = MenuRow::Number("Direction zone", "Directions only count close to straight",
                                   [s] { return s->directionAngleDeg; }, [s](float v) { s->directionAngleDeg = v; }, 0, 45, 5);
    zone.unit = "Â°";
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

MenuPage StickOptions(Engine& e, InGameMenu& m, int hand) {
    MenuPage p = StickPage(hand ? "Right stick" : "Left stick", hand);
    p.rows.push_back(MenuRow::Link("Deadzones", "What the game sees: drift and the edge",
                                   [&e, &m, hand] { m.Push([&e, hand] { return StickDeadzones(e, hand); }); }));
    p.rows.push_back(MenuRow::Link("Directions", "For your Up / Down / Left / Right bindings",
                                   [&e, &m, hand] { m.Push([&e, hand] { return StickDirections(e, hand); }); }));
    p.rows.push_back(MenuRow::Link("Bindings", "Click, push, double flick...",
                                   [&e, &m, hand] { m.Push([&e, &m, hand] { return PartPage(e, m, hand, C_STICK); }); }));
    return p;
}

} // namespace MenuPages
