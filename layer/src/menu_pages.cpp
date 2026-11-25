// The in-game menu's pages: see menu_pages.h.
#include "binding_checks.h"
#include "gamepad.h"
#include "menu_pages.h"
#include "engine.h"
#include "controller_silhouettes.h"
#include "in_game_menu.h"
#include "keys.h"
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

static const std::vector<std::string> kGestureWords = { "Press it", "Hold it", "Double-tap it", "Double-tap and hold", "Quick tap only", "While held" };
static const Gesture kGestures[] = { Gesture::Press, Gesture::Hold, Gesture::DoubleTap, Gesture::DoubleTapHold, Gesture::Tap, Gesture::WhileHeld };

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
    case ActType::Gamepad:
        return "Gamepad: " + a.pad + mode;
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

// What a binding does, for its own page: 'nothing' says whether the game keeps the button.
static std::string WhatWords(const Binding& b) {
    if (b.action.type == ActType::None)
        return b.block == BindingBlock::None ? "Nothing yet" : "Nothing (the game doesn't get the button)";
    return ActionWords(b.action);
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

// When How changes: a plain press is taken from the game; hold lets a quick tap through; double-tap, quick tap and
// any touch leave the button to the game too. (The editor's binding sheet does the same.)
static void DefaultsForHow(Binding& b) {
    const InputId& f = b.chord.back();
    b.tapPassthrough = false;
    if (f.feat == F_TOUCH || b.gesture == Gesture::DoubleTap || b.gesture == Gesture::DoubleTapHold || b.gesture == Gesture::Tap)
        b.block = BindingBlock::None;
    else if (b.gesture == Gesture::Hold) {
        b.block = BindingBlock::Trigger;
        b.tapPassthrough = true;
    } else
        b.block = BindingBlock::Trigger;
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
    p.shownRows = MenuView::kVisibleRows; // big rows (scaled), as before
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
    p.rows.push_back(MenuRow::Bar({ "All bindings", "Test controllers", "Right stick", "Left stick", "Save now" },
                                  { [&e, &m] { m.Push([&e, &m] { return Home(e, m); }); },
                                    [&e, &m] { m.Push([&e, &m] { return TestControllers(e, m); }); },
                                    [&e, &m] { m.Push([&e, &m] { return StickOptions(e, m, 1); }); },
                                    [&e, &m] { m.Push([&e, &m] { return StickOptions(e, m, 0); }); },
                                    [&m] { m.SaveNow(); } }));
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

// ---- shared by the part page, the binding lists and the binding page

// A new binding on `id` that does nothing yet, opened on its page with "What happens" on top (like the editor's + Add).
static void AddBlank(Engine& e, InGameMenu& m, const InputId& id) {
    Binding b;
    SetFiringInput(b, id);
    b.name = InputWords(id);
    b.gesture = Gesture::Press;
    b.action.type = ActType::None;
    b.block = BindingBlock::None;
    int index = 0;
    e.EditBindings([&](std::vector<Binding>& list) {
        list.push_back(b);
        index = (int)list.size() - 1;
    });
    m.Changed();
    m.Push([&e, &m, index] { return BindingDetail(e, m, index); });
    m.Push([&e, &m, index] { return WhatHappens(e, m, index); });
}

// "Hold, with Left grip held, in layer combat, off": how a binding fires, short.
static std::string HowWords(const Binding& b) {
    std::string s = kGestureWords[GestureIndex(b.gesture)];
    for (size_t k = 0; k + 1 < b.chord.size(); k++) s += (k ? " + " : ", with ") + InputWords(b.chord[k]) + (k + 2 == b.chord.size() ? " held" : "");
    if (!b.layers.empty()) s += ", in layer " + b.layers[0];
    if (!b.enabled) s += "  (off)";
    return s;
}

// Every layer name the profile uses: in bindings' conditions and in actions that switch one.
static std::vector<std::string> LayerNames(const Profile& p) {
    std::vector<std::string> out;
    auto add = [&](const std::string& n) {
        if (!n.empty() && std::find(out.begin(), out.end(), n) == out.end()) out.push_back(n);
    };
    for (const Binding& b : p.bindings) {
        for (auto& l : b.layers) add(l);
        for (auto& l : b.notLayers) add(l);
        if (b.action.type == ActType::Layer) add(b.action.layer);
        if (b.hasTapAction && b.tapAction.type == ActType::Layer) add(b.tapAction.layer);
    }
    for (const auto& [n, w] : p.wheels)
        for (const WheelItem& it : w.items)
            if (it.action.type == ActType::Layer) add(it.action.layer);
    for (const WristStyle& W : p.wrist)
        for (const auto& [n, pg] : W.pages)
            for (const WheelItem& it : pg.items)
                if (it.action.type == ActType::Layer) add(it.action.layer);
    return out;
}

static MenuRow OnOffRow(Engine& e, InGameMenu& m, int i) {
    return MenuRow::Toggle("      on / off", "", [&e, i] { return i < (int)e.prof->bindings.size() && e.prof->bindings[i].enabled; },
                           [&e, &m, i](bool on) {
                               e.EditBindings([&](std::vector<Binding>& list) {
                                   if (i < (int)list.size()) list[i].enabled = on;
                               });
                               m.Changed();
                           });
}

// One part of one controller, like the editor's part panel: each way to use it (press, touch, push up...) with the
// bindings on it, + Add, Swap with... and Reset to default.
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
    std::vector<int> feats;
    for (const PartWay& w : WaysOf(comp, kind)) {
        if (std::find(feats.begin(), feats.end(), w.feat) != feats.end()) continue;
        feats.push_back(w.feat);
        InputId id;
        id.hand = hand;
        id.comp = comp;
        id.feat = w.feat;
        p.rows.push_back(MenuRow::Heading(w.name));
        std::vector<int> on;
        for (int i = 0; i < (int)e.prof->bindings.size(); i++)
            if (!e.prof->bindings[i].chord.empty() && e.prof->bindings[i].chord.back() == id) on.push_back(i);
        // the plain ones first, then the ones that only work in a layer, then the switched-off ones (as the editor)
        std::stable_sort(on.begin(), on.end(), [&](int a, int b) {
            const Binding &x = e.prof->bindings[a], &y = e.prof->bindings[b];
            return std::make_pair(!x.layers.empty(), !x.enabled) < std::make_pair(!y.layers.empty(), !y.enabled);
        });
        if (on.empty()) p.rows.push_back(MenuRow::Note("Default (the game's own)"));
        for (int i : on) {
            const Binding& b = e.prof->bindings[i];
            std::string what = ActionWords(b.action);
            if (b.gesture == Gesture::Hold && b.hasTapAction) what += "  ·  short: " + ActionWords(b.tapAction);
            if (b.gesture == Gesture::DoubleTap && b.hasTapAction) what += "  ·  single: " + ActionWords(b.tapAction);
            auto warns = CheckBinding(*e.prof, i, Gamepad::DriverInstalled());
            MenuRow link = MenuRow::Link((warns.empty() ? "" : "\u26A0 ") + kGestureWords[GestureIndex(b.gesture)] + ":  " + what,
                                         warns.empty() ? HowWords(b) : "Check this: " + warns[0].title,
                                         [&e, &m, i] { m.Push([&e, &m, i] { return BindingDetail(e, m, i); }); });
            link.isWarn = !warns.empty();
            p.rows.push_back(link);
            p.rows.push_back(OnOffRow(e, m, i));
        }
        std::vector<std::string> names = { "+ Add" };
        std::vector<std::function<void()>> press = { [&e, &m, id] { AddBlank(e, m, id); } };
        if (!(w.feat == F_TOUCH || comp == C_THUMBREST)) {
            names.push_back("Swap with...");
            press.push_back([&e, &m, id] {
                // this button and the one pressed next trade places in the game: two bindings, one on each
                m.CaptureInput([&e, &m, id](const InputId& other) {
                    if (other == id) return;
                    e.EditBindings([&](std::vector<Binding>& list) {
                        for (int k = 0; k < 2; k++) {
                            const InputId& from = k ? other : id;
                            const InputId& to = k ? id : other;
                            Binding b;
                            SetFiringInput(b, from);
                            SetButtonAction(b, to);
                            b.block = BindingBlock::Trigger;
                            list.push_back(b);
                        }
                    });
                    m.Changed();
                    e.Toast("Swapped: " + InputWords(id) + " <-> " + InputWords(other), 2.5);
                });
            });
        }
        if (!on.empty()) {
            names.push_back("Reset to default");
            press.push_back([&e, &m, id] {
                e.EditBindings([&](std::vector<Binding>& list) {
                    list.erase(std::remove_if(list.begin(), list.end(), [&](const Binding& b) { return !b.chord.empty() && b.chord.back() == id; }),
                               list.end());
                });
                m.Changed();
                e.Toast("Removed: the game gets " + InputWords(id) + " as usual", 2.5);
            });
        }
        p.rows.push_back(MenuRow::Bar(names, press));
    }
    // combos that use this part without firing on it
    bool heading = false;
    for (int i = 0; i < (int)e.prof->bindings.size(); i++) {
        const Binding& b = e.prof->bindings[i];
        if (b.chord.size() < 2 || (b.chord.back().hand == hand && b.chord.back().comp == comp)) continue;
        bool uses = false;
        for (size_t k = 0; k + 1 < b.chord.size(); k++) uses = uses || (b.chord[k].hand == hand && b.chord[k].comp == comp);
        if (!uses) continue;
        if (!heading) p.rows.push_back(MenuRow::Heading("Combos that hold this button"));
        heading = true;
        p.rows.push_back(MenuRow::Link(b.name + (b.enabled ? "" : "  (off)"), BindingSummary(b),
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
    auto all = [&e, &m, hand](bool on) {
        e.EditBindings([&](std::vector<Binding>& list) {
            for (Binding& b : list)
                if (!b.chord.empty() && b.chord.back().hand == hand) b.enabled = on;
        });
        m.Changed();
    };
    p.rows.push_back(MenuRow::Bar({ "+ New binding", "All on", "All off" },
                                  { [&e, &m] { m.CaptureInput([&e, &m](const InputId& id) { AddBlank(e, m, id); }); },
                                    [all] { all(true); }, [all] { all(false); } }));
    int shown = 0;
    for (int i = 0; i < (int)e.prof->bindings.size(); i++) {
        const Binding& b = e.prof->bindings[i];
        if (b.chord.empty() || b.chord.back().hand != hand) continue;
        shown++;
        auto warns = CheckBinding(*e.prof, i, Gamepad::DriverInstalled());
        MenuRow row = MenuRow::Link((warns.empty() ? "" : "\u26A0 ") + b.name + (b.enabled ? "" : "  (off)"),
                                    warns.empty() ? BindingSummary(b) : "Check this: " + warns[0].title,
                                    [&e, &m, i] { m.Push([&e, &m, i] { return BindingDetail(e, m, i); }); });
        row.isWarn = !warns.empty();
        p.rows.push_back(row);
        p.rows.push_back(OnOffRow(e, m, i));
    }
    if (shown == 0) p.rows.push_back(MenuRow::Note("No bindings on this controller yet", "+ New binding: press the button you want"));
    return p;
}

// The action a page edits: the binding's own, or (tap) what a short press of a Hold binding does.
static Action& Slot(Binding& b, bool tap) {
    if (tap) b.hasTapAction = true;
    return tap ? b.tapAction : b.action;
}

// "What happens" for binding `index` (tap: its short press): every kind of action the editor's picker has.
MenuPage WhatHappens(Engine& e, InGameMenu& m, int index, bool tap) {
    MenuPage p;
    p.title = tap ? "Short press does" : "What happens";
    if (index < 0 || index >= (int)e.prof->bindings.size()) {
        p.rows.push_back(MenuRow::Note("This binding is gone"));
        return p;
    }
    // set the action; the binding's name follows its own action (not the short press's)
    auto set = [&e, &m, index, tap](Action a, std::string name) {
        e.EditBindings([&](std::vector<Binding>& list) {
            if (index >= (int)list.size()) return;
            Binding& x = list[index];
            Slot(x, tap) = a;
            if (!tap && !name.empty()) x.name = InputWords(x.chord.back()) + ": " + name;
        });
        m.Changed();
    };
    auto simple = [&p, set, &m](const char* label, const char* detail, ActType t, ActMode mode, const char* name) {
        p.rows.push_back(MenuRow::Button(label, detail, [set, &m, t, mode, name] {
            Action a;
            a.type = t;
            a.mode = mode;
            set(a, name);
            m.Back();
        }));
    };
    const Binding& b = e.prof->bindings[index];
    p.rows.push_back(MenuRow::Note("Now: " + (tap ? (b.hasTapAction ? ActionWords(b.tapAction) : std::string("nothing")) : WhatWords(b))));
    p.rows.push_back(MenuRow::Heading("Keys"));
    p.rows.push_back(MenuRow::Button("Press a key or mouse button", "Or act as another controller button (on the keyboard)", [&m, set, tap] {
        m.Back();
        m.CaptureAction([set, tap](const std::string& keys) {
                            Action a;
                            a.type = ActType::Key;
                            a.keys = keys;
                            a.mode = tap ? ActMode::Tap : ActMode::Hold; // held with the button
                            set(a, keys);
                        },
                        [set, tap](const InputId& target) {
                            Action a;
                            a.type = ActType::XrInput;
                            a.target = target;
                            a.mode = tap ? ActMode::Tap : ActMode::Hold;
                            set(a, InputWords(target));
                        });
    }));
    p.rows.push_back(MenuRow::Button("Type a few keys in a row (macro)", "Steps split with ;   e.g.  Ctrl+C; wait 100; Ctrl+V", [&e, &m, set, index, tap] {
        const Binding& x = e.prof->bindings[index];
        const Action& now = tap ? x.tapAction : x.action;
        std::string start = now.type == ActType::Sequence ? now.steps : "";
        m.Back();
        m.AskText("Macro steps", start, [&e, set](const std::string& t) {
            std::string err;
            if (!Keys::ValidateSequence(t, err)) {
                e.Toast("Macro not set: " + err, 4.0);
                return;
            }
            Action a;
            a.type = ActType::Sequence;
            a.steps = t;
            set(a, "macro");
        });
    }));
    p.rows.push_back(MenuRow::Heading("Open a wheel"));
    for (const auto& [name, w] : e.prof->wheels) {
        std::string n = name, title = w.title.empty() ? name : w.title;
        p.rows.push_back(MenuRow::Button("Wheel '" + title + "'", "", [set, &m, n, title] {
            Action a;
            a.type = ActType::Wheel;
            a.wheel = n;
            set(a, "wheel '" + title + "'");
            m.Back();
        }));
    }
    p.rows.push_back(MenuRow::Heading("Open or switch"));
    simple("VR keyboard", "", ActType::Keyboard, ActMode::Tap, "VR keyboard");
    simple("Desktop screen (show / hide)", "Where and which monitor: Settings > Screen mirror", ActType::Screen, ActMode::Toggle, "desktop screen");
    simple("This settings menu", "", ActType::Menu, ActMode::Tap, "settings menu");
    simple("Headset mirror (on / off)", "The window on the desktop showing your view", ActType::Mirror, ActMode::Toggle, "headset mirror");
    simple("Controller offsets panel", "Fix hand / gun aim", ActType::Offsets, ActMode::Panel, "controller offsets");
    simple("Adjust mode", "Grab wheels, panels and the keyboard to place them", ActType::Adjust, ActMode::Toggle, "adjust mode");
    simple("Reload the profile", "", ActType::Reload, ActMode::Tap, "reload profile");
    p.rows.push_back(MenuRow::Heading("Switch a layer"));
    for (const std::string& l : LayerNames(*e.prof))
        p.rows.push_back(MenuRow::Button("Layer '" + l + "'", tap ? "Switched on / off by the short press" : "On while held (change it on the binding)",
                                         [set, &m, l, tap] {
                                             Action a;
                                             a.type = ActType::Layer;
                                             a.layer = l;
                                             a.mode = tap ? ActMode::Toggle : ActMode::Hold;
                                             set(a, "layer " + l);
                                             m.Back();
                                         }));
    p.rows.push_back(MenuRow::Button("+ A new layer", "Type its name", [&m, set, tap] {
        m.Back();
        m.AskText("Layer name", "", [set, tap](const std::string& t) {
            Action a;
            a.type = ActType::Layer;
            a.layer = t;
            a.mode = tap ? ActMode::Toggle : ActMode::Hold;
            set(a, "layer " + t);
        });
    }));
    p.rows.push_back(MenuRow::Heading("Other"));
    if (tap)
        p.rows.push_back(MenuRow::Button("Nothing (no short press action)", "A short press then does what 'the game still gets a tap' says", [&e, &m, index] {
            e.EditBindings([&](std::vector<Binding>& list) {
                if (index < (int)list.size()) {
                    list[index].hasTapAction = false;
                    list[index].tapAction = Action{};
                }
            });
            m.Changed();
            m.Back();
        }));
    else
        simple("Nothing", "Details decides whether the game still gets the button", ActType::None, ActMode::Tap, "");
    return p;
}

MenuPage WhatHappens(Engine& e, InGameMenu& m, int index) { return WhatHappens(e, m, index, false); }

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

// A layer condition as a Choice: "Always" (or "(none)"), each layer the profile uses, and "+ New..." (typed).
using BindingEdit = std::function<void(std::function<void(Binding&)>)>;
static MenuRow LayerRow(Engine& e, InGameMenu& m, int index, bool notIn, BindingEdit edit) {
    std::vector<std::string> names = LayerNames(*e.prof);
    std::vector<std::string> opts = { notIn ? "(none)" : "Always" };
    for (auto& n : names) opts.push_back(n);
    opts.push_back("+ New...");
    auto pick = [notIn](Binding& x) -> std::vector<std::string>& { return notIn ? x.notLayers : x.layers; };
    return MenuRow::Choice(notIn ? "Not while layer" : "Only while layer",
                           notIn ? "Off while this layer is on" : "Always, or only while that layer is on", opts,
                           [&e, index, names, notIn] {
                               if (index >= (int)e.prof->bindings.size()) return 0;
                               const Binding& x = e.prof->bindings[index];
                               const auto& l = notIn ? x.notLayers : x.layers;
                               if (l.empty()) return 0;
                               auto it = std::find(names.begin(), names.end(), l[0]);
                               return it == names.end() ? 0 : 1 + (int)(it - names.begin());
                           },
                           [&m, edit, names, pick](int i) {
                               if (i == 0)
                                   edit([pick](Binding& x) { pick(x).clear(); });
                               else if (i <= (int)names.size()) {
                                   std::string n = names[i - 1];
                                   edit([pick, n](Binding& x) { pick(x) = { n }; });
                               } else
                                   m.AskText("Layer name", "", [edit, pick](const std::string& t) { edit([pick, t](Binding& x) { pick(x) = { t }; }); });
                           });
}

MenuPage BindingDetail(Engine& e, InGameMenu& m, int index) {
    MenuPage p;
    if (index < 0 || index >= (int)e.prof->bindings.size()) {
        p.title = "Binding";
        p.rows.push_back(MenuRow::Note("This binding is gone"));
        return p;
    }
    // Every row edits the binding through the engine (EditBindings), which ends whatever it was doing first.
    BindingEdit edit = [&e, &m, index](std::function<void(Binding&)> change) {
        e.EditBindings([&](std::vector<Binding>& list) {
            if (index < (int)list.size()) change(list[index]);
        });
        m.Changed();
    };
    auto get = [&e, index]() -> const Binding& { return e.prof->bindings[index]; };
    const Binding& b = get();
    p.title = b.name;
    p.rows.push_back(MenuRow::Bar({ "Save now", "Duplicate", "Delete binding" },
                                  { [&m] { m.SaveNow(); },
                                    [&e, &m, index] {
                                        int at = index + 1;
                                        e.EditBindings([&](std::vector<Binding>& list) {
                                            if (index >= (int)list.size()) return;
                                            Binding c = list[index];
                                            c.name += " copy";
                                            c.enabled = false; // two of the same would both fire
                                            c.sourceIndex = -1;
                                            list.insert(list.begin() + at, c);
                                        });
                                        m.Changed();
                                        e.Toast("Made a copy (switched off, so the two don't both fire)", 3.0);
                                        m.Back();
                                        m.Push([&e, &m, at] { return BindingDetail(e, m, at); });
                                    },
                                    [&e, &m, index] {
                                        e.EditBindings([index](std::vector<Binding>& list) {
                                            if (index < (int)list.size()) list.erase(list.begin() + index);
                                        });
                                        m.Changed();
                                        m.Back();
                                    } }));
    // May need your attention: the same checks as the desktop editor
    for (auto& w : CheckBinding(*e.prof, index, Gamepad::DriverInstalled())) {
        MenuRow r = MenuRow::Note("\u26A0 " + w.title, w.text);
        r.isWarn = true;
        p.rows.push_back(r);
    }
    p.rows.push_back(MenuRow::Choice("1  How", "Also sets if the game gets it", kGestureWords, [get] { return GestureIndex(get().gesture); },
                                     [edit](int i) { edit([i](Binding& x) { x.gesture = kGestures[i]; DefaultsForHow(x); }); }));
    InputId firing = b.chord.back();
    p.rows.push_back(MenuRow::Button("2  Button: " + InputWords(firing), "Select, then press the new button", [&m, edit] {
        m.CaptureInput([edit](const InputId& id) { edit([id](Binding& x) { SetFiringInput(x, id); }); });
    }));
    if (HasTouch(firing.comp) && (firing.feat == F_PRESS || firing.feat == F_TOUCH))
        p.rows.push_back(MenuRow::Choice("Press or touch", "Touch: a finger resting on it, without pressing", { "Press", "Touch" },
                                         [get] { return get().chord.back().feat == F_TOUCH ? 1 : 0; },
                                         [edit](int i) {
                                             edit([i](Binding& x) {
                                                 InputId id = x.chord.back();
                                                 id.feat = i == 1 ? F_TOUCH : F_PRESS;
                                                 SetFiringInput(x, id);
                                                 if (i == 1) x.block = BindingBlock::None; // hiding a touch would hide its presses
                                             });
                                         }));
    // a combo: other buttons held first
    std::string held;
    for (size_t k = 0; k + 1 < b.chord.size(); k++) held += (k ? " + " : "") + InputWords(b.chord[k]);
    p.rows.push_back(MenuRow::Button(held.empty() ? "Combo: also hold..." : "Combo: with " + held + " held",
                                     held.empty() ? "Select, then press the button to hold first (e.g. the other grip)" : "Select to add another",
                                     [&m, edit] {
                                         m.CaptureInput([edit](const InputId& id) {
                                             edit([id](Binding& x) {
                                                 for (const InputId& c : x.chord)
                                                     if (c == id) return;
                                                 x.chord.insert(x.chord.end() - 1, id);
                                                 SetFiringInput(x, x.chord.back()); // writes the text again
                                             });
                                         });
                                     }));
    if (!held.empty())
        p.rows.push_back(MenuRow::Button("Remove the combo", "Just the button again", [edit] {
            edit([](Binding& x) {
                InputId last = x.chord.back();
                x.chord = { last };
                SetFiringInput(x, last);
                if (x.block == BindingBlock::All) x.block = BindingBlock::Trigger;
            });
        }));
    bool isKey = b.action.type == ActType::Key, isButton = b.action.type == ActType::XrInput;
    p.rows.push_back(MenuRow::Link("3  What happens: " + WhatWords(b), "A key, a macro, a wheel, a layer, the VR keyboard...",
                                   [&e, &m, index] { m.Push([&e, &m, index] { return WhatHappens(e, m, index, false); }); }));
    if (isKey || isButton) {
        static const ActMode modes[] = { ActMode::Tap, ActMode::Hold, ActMode::Toggle };
        p.rows.push_back(MenuRow::Choice(isKey ? "Key behavior" : "Button behavior", "Press / Hold: held while you hold the button",
                                         { "Press", "Hold", "Toggle" },
                                         [get] {
                                             ActMode md = get().action.mode;
                                             return md == ActMode::Hold ? 1 : md == ActMode::Toggle ? 2 : 0;
                                         },
                                         [edit](int i) { edit([i](Binding& x) { x.action.mode = modes[i]; }); }));
    }
    if (b.action.type == ActType::Layer) {
        static const ActMode modes[] = { ActMode::Hold, ActMode::Toggle, ActMode::On, ActMode::Off };
        p.rows.push_back(MenuRow::Choice("Layer", "", { "On while held", "Toggle", "Switch on", "Switch off" },
                                         [get] {
                                             ActMode md = get().action.mode;
                                             return md == ActMode::Toggle ? 1 : md == ActMode::On ? 2 : md == ActMode::Off ? 3 : 0;
                                         },
                                         [edit](int i) { edit([i](Binding& x) { x.action.mode = modes[i]; }); }));
    }
    if (b.action.type == ActType::Wheel) {
        std::string n = b.action.wheel;
        p.rows.push_back(MenuRow::Link("Edit that wheel", "", [&e, &m, n] { m.Push([&e, &m, n] { return WheelPage(e, m, n); }); }));
    }
    if (b.gesture == Gesture::DoubleTap) {
        p.rows.push_back(MenuRow::Heading("Single tap (no second tap in time)"));
        p.rows.push_back(MenuRow::Link("Single tap does: " + (b.hasTapAction ? ActionWords(b.tapAction) : std::string("nothing (off)")),
                                       "Off by default. When set, it comes a moment late: it waits to see if a second tap comes",
                                       [&e, &m, index] { m.Push([&e, &m, index] { return WhatHappens(e, m, index, true); }); }));
    }
    if (b.gesture == Gesture::Hold) {
        p.rows.push_back(MenuRow::Heading("Short press (let go before the hold time)"));
        p.rows.push_back(MenuRow::Link("Short press does: " + (b.hasTapAction ? ActionWords(b.tapAction) : std::string("nothing of its own")),
                                       "Something else on a short press: a key, a wheel... The hold still does the action above",
                                       [&e, &m, index] { m.Push([&e, &m, index] { return WhatHappens(e, m, index, true); }); }));
        if (!b.hasTapAction)
            p.rows.push_back(MenuRow::Toggle("Else the game still gets a normal tap", "", [get] { return get().tapPassthrough; },
                                             [edit](bool on) { edit([on](Binding& x) { x.tapPassthrough = on; }); }));
        MenuRow hold = MenuRow::Number("Hold time", "How long counts as a hold", [get] { return (float)get().holdMs; },
                                       [edit](float v) { edit([v](Binding& x) { x.holdMs = (int)v; }); }, 100, 2000, 50);
        hold.unit = "ms";
        p.rows.push_back(hold);
    }
    p.rows.push_back(MenuRow::Heading("Details"));
    p.rows.push_back(MenuRow::Button("Name: " + b.name, "Type a new one on the keyboard", [&e, &m, index, edit] {
        if (index >= (int)e.prof->bindings.size()) return;
        m.AskText("Name", e.prof->bindings[index].name, [edit](const std::string& t) { edit([t](Binding& x) { x.name = t; }); });
    }));
    p.rows.push_back(MenuRow::Toggle("Enabled", "", [get] { return get().enabled; }, [edit](bool on) { edit([on](Binding& x) { x.enabled = on; }); }));
    {
        std::vector<std::string> opts = { "No", "Yes" };
        if (b.chord.size() > 1) opts.push_back("No, nor the combo's others");
        p.rows.push_back(MenuRow::Choice("The game also gets the button", "Set for you from How", opts,
                                         [get] { return get().block == BindingBlock::None ? 1 : get().block == BindingBlock::All ? 2 : 0; },
                                         [edit](int i) {
                                             edit([i](Binding& x) { x.block = i == 1 ? BindingBlock::None : i == 2 ? BindingBlock::All : BindingBlock::Trigger; });
                                         }));
    }
    if (b.gesture == Gesture::Tap) {
        MenuRow t = MenuRow::Number("Tap shorter than", "", [get] { return (float)get().tapMs; },
                                    [edit](float v) { edit([v](Binding& x) { x.tapMs = (int)v; }); }, 50, 2000, 50);
        t.unit = "ms";
        p.rows.push_back(t);
    }
    if (b.gesture == Gesture::DoubleTap || b.gesture == Gesture::DoubleTapHold) {
        MenuRow t = MenuRow::Number("Second tap within", "How quick the second tap (or flick) must be", [get] { return (float)get().doubleTapMs; },
                                    [edit](float v) { edit([v](Binding& x) { x.doubleTapMs = (int)v; }); }, 100, 2000, 50);
        t.unit = "ms";
        p.rows.push_back(t);
    }
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
    if (b.gesture == Gesture::DoubleTapHold && (firing.comp == C_STICK || firing.comp == C_TRACKPAD) && firing.feat != F_PRESS && firing.feat != F_TOUCH) {
        // a double flick: once going, how far back toward the middle the stick may come before it stops
        MenuRow rel = MenuRow::Number("Keeps going until", "Let go below this; steer freely meanwhile. Off: stops when it leaves the direction",
                                      [get] { return get().releaseAt; },
                                      [edit](float v) { edit([v](Binding& x) { x.releaseAt = v < 0.025f ? 0.f : v; }); }, 0, 0.9f, 0.05f);
        rel.scale = 100;
        rel.unit = "%";
        rel.atMin = "off";
        p.rows.push_back(rel);
    }
    p.rows.push_back(LayerRow(e, m, index, false, edit));
    p.rows.push_back(LayerRow(e, m, index, true, edit));
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
    MenuRow zone = MenuRow::Number("Direction zone", "Near-straight pushes count as straight, for the game too",
                                   [s] { return s->directionAngleDeg; }, [s](float v) { s->directionAngleDeg = v; }, 0, 45, 5);
    zone.unit = "°"; // the degree sign, as bytes (the source file's encoding can't garble it)
    zone.atMin = "off";
    p.rows.push_back(zone);
    p.rows.push_back(MenuRow::Toggle("Ignore outside the zones", "Diagonals give the game nothing: only clean up / down / left / right",
                                     [s] { return s->ignoreOutside; }, [s](bool on) { s->ignoreOutside = on; }));
    MenuRow centre = MenuRow::Number("Game: ignore small pushes below", "With Ignore outside: smaller pushes give the GAME nothing",
                                     [s] { return s->ignoreCenter; }, [s](float v) { s->ignoreCenter = v; }, 0, 0.95f, 0.05f);
    centre.scale = 100;
    centre.unit = "%";
    centre.atMin = "off";
    p.rows.push_back(centre);
    MenuRow pressAt = MenuRow::Number("Your bindings: direction counts at", "Only your own stick-direction bindings, not the game (both sticks)", [&e] { return e.prof->stickOn; },
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
    p.rows.push_back(MenuRow::Link("Directions", "Direction zone (straighten the stick) and how far a push counts",
                                   [&e, &m, hand] { m.Push([&e, hand] { return StickDirections(e, hand); }); }));
    p.rows.push_back(MenuRow::Link("Bindings", "Click, push, double flick...",
                                   [&e, &m, hand] { m.Push([&e, &m, hand] { return PartPage(e, m, hand, C_STICK); }); }));
    return p;
}

} // namespace MenuPages
