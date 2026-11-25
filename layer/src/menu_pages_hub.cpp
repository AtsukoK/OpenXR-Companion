// The in-game menu's hub and the pages for wheels, wrist panels and general settings (see menu_pages.h).
// The pages follow the editor: a home page of big tiles, a list per wheel / wrist page, a page per button, and a
// "What it does" page of choices. Everything is pressed with the trigger; names are typed on the VR keyboard.
#include "menu_pages.h"
#include "engine.h"
#include "in_game_menu.h"
#include <algorithm>

namespace MenuPages {

namespace {
constexpr int kMaxItems = 16; // a wheel or a wrist page holds at most this many buttons (as the profile allows)

// The wheel or wrist page being edited; looked up again on every build (it can be renamed, moved or deleted).
Wheel* Find(Engine& e, const ItemListRef& ref) {
    if (ref.wrist) {
        auto& pages = e.prof->wrist[ref.hand].pages;
        auto it = pages.find(ref.name);
        return it == pages.end() ? nullptr : &it->second;
    }
    auto it = e.prof->wheels.find(ref.name);
    return it == e.prof->wheels.end() ? nullptr : &it->second;
}

std::string TitleOf(const Wheel& w) {
    return w.title.empty() ? w.name : w.title;
}

// A new internal name not used yet: "wheel_2", "page_3".
template <class Map>
std::string FreeName(const Map& m, const std::string& base) {
    for (int i = 1;; i++) {
        std::string n = base + "_" + std::to_string(i);
        if (!m.count(n)) return n;
    }
}

// What a wheel or wrist button does, short, for lists.
std::string ItemWords(Engine& e, const Action& a, const ItemListRef& ref) {
    switch (a.type) {
    case ActType::None:
        return "Does nothing yet";
    case ActType::Key:
        return a.keys.empty() ? "No key set" : "Key: " + a.keys;
    case ActType::XrInput:
        return "Controller: " + InputWords(a.target);
    case ActType::Gamepad:
        return "Gamepad: " + a.pad;
    case ActType::Wheel: {
        auto it = e.prof->wheels.find(a.wheel);
        return "Opens wheel '" + (it == e.prof->wheels.end() ? a.wheel : TitleOf(it->second)) + "'";
    }
    case ActType::Page: {
        const auto& pages = e.prof->wrist[ref.hand].pages;
        auto it = pages.find(a.page);
        return "Goes to page '" + (it == pages.end() ? a.page : TitleOf(it->second)) + "'";
    }
    case ActType::Back:
        return "Back";
    case ActType::Close:
        return "Closes it";
    case ActType::Keyboard:
        return "VR keyboard";
    case ActType::Screen:
        return "Desktop screen";
    case ActType::Menu:
        return "This settings menu";
    case ActType::Mirror:
        return "Headset mirror";
    case ActType::Offsets:
        return "Controller offsets";
    case ActType::Adjust:
        return "Adjust mode (place things)";
    case ActType::Reload:
        return "Reload the profile";
    case ActType::Sequence:
        return "Macro (edit in the editor)";
    case ActType::Layer:
        return "Layer '" + a.layer + "' (edit in the editor)";
    }
    return "Set in the editor";
}

// The buttons that open a wheel: bindings and buttons on other wheels / wrist pages.
std::vector<std::string> OpenersOf(Engine& e, const std::string& wheel) {
    std::vector<std::string> out;
    for (const Binding& b : e.prof->bindings)
        if (b.action.type == ActType::Wheel && b.action.wheel == wheel && !b.chord.empty())
            out.push_back(InputWords(b.chord.back()) + (b.enabled ? "" : " (off)"));
    for (const auto& [n, w] : e.prof->wheels)
        for (const WheelItem& it : w.items)
            if (it.action.type == ActType::Wheel && it.action.wheel == wheel && n != wheel)
                out.push_back("'" + it.label + "' on wheel '" + TitleOf(w) + "'");
    for (int h = 0; h < 2; h++)
        for (const auto& [n, pg] : e.prof->wrist[h].pages)
            for (const WheelItem& it : pg.items)
                if (it.action.type == ActType::Wheel && it.action.wheel == wheel)
                    out.push_back("'" + it.label + "' on the " + (h ? "right" : "left") + " wrist");
    return out;
}

// Every action that opens wheel `name` (or goes to wrist page `page` of hand) does nothing after it is deleted.
void ForgetWheel(Profile& p, const std::string& name) {
    auto clear = [&](Action& a) {
        if (a.type == ActType::Wheel && a.wheel == name) a = Action{};
    };
    for (Binding& b : p.bindings)
        clear(b.action);
    for (auto& [n, w] : p.wheels)
        for (WheelItem& it : w.items)
            clear(it.action);
    for (auto& W : p.wrist)
        for (auto& [n, pg] : W.pages)
            for (WheelItem& it : pg.items)
                clear(it.action);
}
void ForgetPage(WristStyle& W, const std::string& page) {
    for (auto& [n, pg] : W.pages)
        for (WheelItem& it : pg.items)
            if (it.action.type == ActType::Page && it.action.page == page) it.action = Action{};
}

void EditList(Engine& e, InGameMenu& m, const ItemListRef& ref, std::function<void(Wheel&)> f) {
    e.EditProfile([&](Profile&) {
        if (Wheel* w = Find(e, ref)) f(*w);
    });
    m.Changed();
}

// Short words for numbers shown in centimetres / milliseconds.
MenuRow Cm(const char* label, const char* detail, std::function<float()> get, std::function<void(float)> set, float min, float max,
           float step) {
    MenuRow r = MenuRow::Number(label, detail, std::move(get), std::move(set), min, max, step);
    r.scale = 100;
    r.unit = "cm";
    r.decimals = step < 0.01f ? 1 : 0;
    return r;
}
MenuRow Percent(const char* label, const char* detail, std::function<float()> get, std::function<void(float)> set, float min,
                float max, float step, const char* atMin = "") {
    MenuRow r = MenuRow::Number(label, detail, std::move(get), std::move(set), min, max, step);
    r.scale = 100;
    r.unit = "%";
    r.atMin = atMin;
    return r;
}
} // namespace

static void AddItem(Engine& e, InGameMenu& m, const ItemListRef& ref);
static MenuRow PreviewOf(Engine& e, InGameMenu& m, const ItemListRef& ref, std::vector<std::string> side,
                         std::vector<std::function<void()>> press, int span, int selected = -1);

// ------------------------------------------------------------------------------------------------ home
MenuPage Main(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "OpenXR Companion" + (e.prof->sourceName.empty() ? std::string() : "  -  " + e.prof->sourceName);
    p.scale = 1.6f; // big tiles, easy to hit
    p.shownRows = MenuView::kVisibleRows;
    p.distance = 1.0f;
    p.width = 0.62f;
    int nWheels = (int)e.prof->wheels.size(), nBindings = 0;
    for (const Binding& b : e.prof->bindings)
        nBindings += b.enabled ? 1 : 0;
    auto wristWord = [&](int h) {
        const WristStyle& W = e.prof->wrist[h];
        return !W.enabled ? std::string("off") : W.mirror ? std::string("mirrored") : std::to_string(W.pages.size()) + (W.pages.size() == 1 ? " page" : " pages");
    };
    std::vector<std::string> titles = { "Controllers", "Radial wheels", "Wrist panels", "Modes", "Sticks", "All bindings", "More settings" };
    std::string modeNow = e.wrist[0].mode;
    std::vector<std::string> details = {
        "What every button does. Point at a button to change it",
        std::to_string(nWheels) + (nWheels == 1 ? " wheel" : " wheels") + ": buttons, what opens them, look",
        "Left: " + wristWord(0) + ", right: " + wristWord(1),
        modeNow.empty() ? std::string("None on now. Turn one on: buttons do something else") : "Now: " + modeNow,
        "Deadzones, straight directions, test",
        std::to_string(nBindings) + " on: the full list, add one",
        "Laser, haptics, offsets, mirror...",
    };
    std::vector<std::function<void()>> open = {
        [&e, &m] { m.Push([&e, &m] { return Overview(e, m); }); },
        [&e, &m] { m.Push([&e, &m] { return Wheels(e, m); }); },
        [&e, &m] { m.Push([&e, &m] { return WristHub(e, m); }); },
        [&e, &m] { m.Push([&e, &m] { return Modes(e, m); }); },
        [&e, &m] { m.Push([&e, &m] { return Sticks(e, m); }); },
        [&e, &m] { m.Push([&e, &m] { return Home(e, m); }); },
        [&e, &m] { m.Push([&e, &m] { return MoreSettings(e, m); }); },
    };
    if (!modeNow.empty()) {
        MenuRow now = MenuRow::Note("Mode now:  " + modeNow, "Buttons work the way this mode sets them. Modes tile: switch or see what changes");
        now.isWarn = true;
        p.rows.push_back(now);
    }
    p.rows.push_back(MenuRow::Tiles(titles, details, open, 3, 7));
    p.rows.push_back(MenuRow::Bar({ "Save now  (changes also save by themselves a second later)" }, { [&m] { m.SaveNow(); } }));
    return p;
}

// ------------------------------------------------------------------------------------------------ modes
// Modes: switch each on or off right here (turning one on turns the others off, unless set to be together),
// and see the buttons each one changes.
MenuPage Modes(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Modes";
    auto names = e.ModeNames();
    std::string now = e.wrist[0].mode;
    p.rows.push_back(MenuRow::Note(now.empty() ? "No mode on now" : "Mode now:  " + now,
                                   "A mode changes what some buttons do while it's on (Combat: Y presses K)"));
    if (names.empty()) {
        p.rows.push_back(MenuRow::Note("No modes in this profile yet", "Make one in the desktop editor: Modes tab"));
        return p;
    }
    for (const std::string& n : names) {
        int changes = 0;
        for (auto& b : e.prof->bindings)
            for (auto& l : b.layers) changes += l == n ? 1 : 0;
        std::string up = n;
        for (char& c : up) c = (char)toupper((unsigned char)c);
        p.rows.push_back(MenuRow::Heading(up));
        p.rows.push_back(MenuRow::Toggle(up + " mode", std::to_string(changes) + (changes == 1 ? " button changes" : " buttons change"),
                                         [&e, n] { return e.ModeOn(n); },
                                         [&e, &m, n](bool on) {
                                             Action a;
                                             a.type = ActType::Layer;
                                             a.layer = n;
                                             a.mode = on ? ActMode::On : ActMode::Off;
                                             e.MenuRunAction(a);
                                             m.Refresh();
                                         }));
        for (int i = 0; i < (int)e.prof->bindings.size(); i++) {
            const Binding& b = e.prof->bindings[i];
            if (std::find(b.layers.begin(), b.layers.end(), n) == b.layers.end()) continue;
            p.rows.push_back(MenuRow::Link(b.chordText + ":  " + b.name, b.enabled ? "In this mode" : "In this mode (switched off)",
                                           [&e, &m, i] { m.Push([&e, &m, i] { return BindingDetail(e, m, i); }); }));
        }
    }
    return p;
}

// ------------------------------------------------------------------------------------------------ wheels
MenuPage Wheels(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Radial wheels";
    // the wheels as tiles: name, how many buttons, what opens it
    std::vector<std::string> titles, details;
    std::vector<std::function<void()>> open;
    std::vector<Wheel> pics;
    for (const auto& [name, w] : e.prof->wheels) {
        pics.push_back(w);
        auto by = OpenersOf(e, name);
        titles.push_back(TitleOf(w));
        details.push_back(std::to_string(w.items.size()) + " buttons. " + (by.empty() ? "Nothing opens it" : "Opened by " + by[0]));
        std::string n = name;
        open.push_back([&e, &m, n] { m.Push([&e, &m, n] { return WheelPage(e, m, n); }); });
    }
    titles.push_back("+ New wheel");
    details.push_back("An empty wheel; then add buttons and pick what opens it");
    open.push_back([&e, &m] {
        std::string name;
        e.EditProfile([&](Profile& pr) {
            name = FreeName(pr.wheels, "wheel");
            Wheel w;
            w.name = name;
            w.title = "New wheel";
            pr.wheels[name] = w;
        });
        m.Changed();
        m.Push([&e, &m, name] { return WheelPage(e, m, name); });
    });
    pics.push_back(Wheel{}); // "+ New wheel": no picture
    int lines = ((int)titles.size() + 1) / 2;
    p.width = 0.62f;
    MenuRow tiles = MenuRow::Tiles(titles, details, open, 2, std::min(7, std::max(3, lines * 2 + 1)));
    tiles.previews = pics;
    p.rows.push_back(tiles);
    p.rows.push_back(MenuRow::Link("Look and how you pick", "Where wheels show, size, stick or laser, which hand",
                                   [&e, &m] { m.Push([&e, &m] { return WheelLook(e, m); }); }));
    return p;
}

MenuPage WheelPage(Engine& e, InGameMenu& m, const std::string& name) {
    MenuPage p;
    ItemListRef ref{ false, 1, name };
    Wheel* w = Find(e, ref);
    if (!w) {
        p.title = "Wheel";
        p.rows.push_back(MenuRow::Note("This wheel is gone"));
        return p;
    }
    p.title = "Wheel: " + TitleOf(*w);
    p.width = 0.62f;
    p.rows.push_back(PreviewOf(e, m, ref, { "+ Add a button", "Rename", "Delete wheel" },
                               { [&e, &m, ref] { AddItem(e, m, ref); },
                                 [&e, &m, ref] {
                                     Wheel* w = Find(e, ref);
                                     if (!w) return;
                                     m.AskText("Wheel name", TitleOf(*w),
                                               [&e, &m, ref](const std::string& t) { EditList(e, m, ref, [&](Wheel& x) { x.title = t; }); });
                                 },
                                 [&e, &m, name] {
                                     e.EditProfile([&](Profile& pr) {
                                         pr.wheels.erase(name);
                                         ForgetWheel(pr, name);
                                     });
                                     m.Changed();
                                     m.Back();
                                 } },
                               5));
    p.rows.push_back(MenuRow::Heading("Opened by"));
    auto by = OpenersOf(e, name);
    for (const std::string& s : by)
        p.rows.push_back(MenuRow::Note(s));
    if (by.empty()) p.rows.push_back(MenuRow::Note("Nothing opens it yet", "Add a button below, or put it on another wheel"));
    p.rows.push_back(MenuRow::Button("+ Add a button that opens it", "Then press the controller button you want", [&e, &m, name] {
        m.CaptureInput([&e, &m, name](const InputId& id) {
            Binding b;
            b.chord.push_back(id);
            b.chordText = InputName(id);
            b.gesture = Gesture::Press;
            b.block = BindingBlock::Trigger; // the game doesn't also get the press that opens the wheel
            b.action.type = ActType::Wheel;
            b.action.wheel = name;
            auto it = e.prof->wheels.find(name);
            b.name = InputWords(id) + ": wheel '" + (it == e.prof->wheels.end() ? name : TitleOf(it->second)) + "'";
            int index = 0;
            e.EditBindings([&](std::vector<Binding>& list) {
                list.push_back(b);
                index = (int)list.size() - 1;
            });
            m.Changed();
            m.Push([&e, &m, index] { return BindingDetail(e, m, index); });
        });
    }));
    p.rows.push_back(MenuRow::Heading("Buttons in order"));
    AddItemRows(e, m, ref, p);
    return p;
}

// "+ Add a button": a new button, then its page and What it does.
static void AddItem(Engine& e, InGameMenu& m, const ItemListRef& ref) {
    Wheel* w = Find(e, ref);
    if (!w || (int)w->items.size() >= kMaxItems) {
        e.Toast("Full: 16 buttons. Put more on a sub-wheel or another page", 3.0);
        return;
    }
    int index = -1;
    EditList(e, m, ref, [&](Wheel& x) {
        WheelItem it;
        it.label = "New button";
        x.items.push_back(it);
        index = (int)x.items.size() - 1;
    });
    if (index < 0) return;
    m.Push([&e, &m, ref, index] { return ItemPage(e, m, ref, index); });
    m.Push([&e, &m, ref, index] { return ItemAction(e, m, ref, index); });
}

// The wheel or page drawn as in the game: point at a button to open it; big buttons beside it.
static MenuRow PreviewOf(Engine& e, InGameMenu& m, const ItemListRef& ref, std::vector<std::string> side,
                         std::vector<std::function<void()>> press, int span, int selected) {
    Wheel* w = Find(e, ref);
    int cols = ref.wrist ? e.prof->wrist[ref.hand].columns : 3;
    MenuRow r = MenuRow::Preview(w ? *w : Wheel{}, ref.wrist, cols,
                                 [&e, &m, ref](int i) { m.Push([&e, &m, ref, i] { return ItemPage(e, m, ref, i); }); }, std::move(side),
                                 std::move(press), span);
    r.previewSelected = selected;
    return r;
}

// The button rows of a wheel or wrist page, and "+ Add a button".
void AddItemRows(Engine& e, InGameMenu& m, const ItemListRef& ref, MenuPage& p) {
    Wheel* w = Find(e, ref);
    if (!w) return;
    for (int i = 0; i < (int)w->items.size(); i++) {
        const WheelItem& it = w->items[i];
        p.rows.push_back(MenuRow::Link(std::to_string(i + 1) + "   " + (it.label.empty() ? "(no label)" : it.label),
                                       ItemWords(e, it.action, ref), [&e, &m, ref, i] { m.Push([&e, &m, ref, i] { return ItemPage(e, m, ref, i); }); }));
    }
    if ((int)w->items.size() < kMaxItems)
        p.rows.push_back(MenuRow::Button("+ Add a button", "Then pick what it does", [&e, &m, ref] { AddItem(e, m, ref); }));
    else
        p.rows.push_back(MenuRow::Note("Full: 16 buttons", "Put more on a sub-wheel or another page"));
}

MenuPage ItemPage(Engine& e, InGameMenu& m, const ItemListRef& ref, int index) {
    MenuPage p;
    Wheel* w = Find(e, ref);
    if (!w || index < 0 || index >= (int)w->items.size()) {
        p.title = "Button";
        p.rows.push_back(MenuRow::Note("This button is gone"));
        return p;
    }
    const WheelItem& it = w->items[index];
    p.title = "Button: " + it.label;
    p.width = 0.62f;
    p.rows.push_back(PreviewOf(e, m, ref, { "Move earlier", "Move later", "Duplicate", "Delete button" },
                               { [&e, &m, ref, index] {
                                    if (index <= 0) return;
                                    EditList(e, m, ref, [&](Wheel& x) { std::swap(x.items[index], x.items[index - 1]); });
                                    m.Back();
                                    m.Push([&e, &m, ref, index] { return ItemPage(e, m, ref, index - 1); });
                                },
                                 [&e, &m, ref, index] {
                                     Wheel* w = Find(e, ref);
                                     if (!w || index + 1 >= (int)w->items.size()) return;
                                     EditList(e, m, ref, [&](Wheel& x) { std::swap(x.items[index], x.items[index + 1]); });
                                     m.Back();
                                     m.Push([&e, &m, ref, index] { return ItemPage(e, m, ref, index + 1); });
                                 },
                                 [&e, &m, ref, index] {
                                     Wheel* w = Find(e, ref);
                                     if (!w || (int)w->items.size() >= kMaxItems) return;
                                     EditList(e, m, ref, [&](Wheel& x) {
                                         WheelItem copy = x.items[index];
                                         copy.sourceIndex = -1;
                                         x.items.insert(x.items.begin() + index + 1, copy);
                                     });
                                     m.Back();
                                     m.Push([&e, &m, ref, index] { return ItemPage(e, m, ref, index + 1); });
                                 },
                                 [&e, &m, ref, index] {
                                     EditList(e, m, ref, [&](Wheel& x) {
                                         if (index < (int)x.items.size()) x.items.erase(x.items.begin() + index);
                                     });
                                     m.Back();
                                 } },
                               4, index));
    p.rows.push_back(MenuRow::Button("1  Label: " + it.label, "Type a new one on the keyboard", [&e, &m, ref, index] {
        Wheel* w = Find(e, ref);
        if (!w || index >= (int)w->items.size()) return;
        m.AskText("Label", w->items[index].label, [&e, &m, ref, index](const std::string& t) {
            EditList(e, m, ref, [&](Wheel& x) {
                if (index < (int)x.items.size()) x.items[index].label = t;
            });
        });
    }));
    p.rows.push_back(MenuRow::Link("2  What it does: " + ItemWords(e, it.action, ref), "A key, a wheel, a page, the keyboard...",
                                   [&e, &m, ref, index] { m.Push([&e, &m, ref, index] { return ItemAction(e, m, ref, index); }); }));
    if (it.action.type == ActType::Key || it.action.type == ActType::XrInput) {
        static const ActMode modes[] = { ActMode::Tap, ActMode::Toggle };
        p.rows.push_back(MenuRow::Choice("Key behavior", "Toggle: pressed until you pick it again", { "Tap", "Toggle" },
                                         [&e, ref, index] {
                                             Wheel* w = Find(e, ref);
                                             return w && index < (int)w->items.size() && w->items[index].action.mode == ActMode::Toggle ? 1 : 0;
                                         },
                                         [&e, &m, ref, index](int i) {
                                             EditList(e, m, ref, [&](Wheel& x) {
                                                 if (index < (int)x.items.size()) x.items[index].action.mode = modes[i];
                                             });
                                         }));
    }
    if (it.action.type == ActType::Wheel)
        p.rows.push_back(MenuRow::Link("Edit that wheel", "", [&e, &m, n = it.action.wheel] { m.Push([&e, &m, n] { return WheelPage(e, m, n); }); }));
    if (it.action.type == ActType::Page && ref.wrist)
        p.rows.push_back(MenuRow::Link("Edit that page", "", [&e, &m, h = ref.hand, n = it.action.page] {
            m.Push([&e, &m, h, n] { return WristPage(e, m, h, n); });
        }));
    return p;
}

// "What it does" for a wheel or wrist button: the choices the editor's card picker has, grouped.
MenuPage ItemAction(Engine& e, InGameMenu& m, const ItemListRef& ref, int index) {
    MenuPage p;
    p.title = "What it does";
    Wheel* w = Find(e, ref);
    if (!w || index < 0 || index >= (int)w->items.size()) {
        p.rows.push_back(MenuRow::Note("This button is gone"));
        return p;
    }
    // set the action; a button still called "New button" takes the action's name
    auto set = [&e, &m, ref, index](Action a, std::string label) {
        EditList(e, m, ref, [&](Wheel& x) {
            if (index >= (int)x.items.size()) return;
            x.items[index].action = a;
            if (x.items[index].label == "New button" && !label.empty()) x.items[index].label = label;
        });
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
    p.rows.push_back(MenuRow::Note("Now: " + ItemWords(e, w->items[index].action, ref)));
    p.rows.push_back(MenuRow::Heading("Keys"));
    p.rows.push_back(MenuRow::Button("Press a key or mouse button", "Or act as a controller button (on the keyboard)", [&e, &m, set] {
        m.Back();
        m.CaptureAction([set](const std::string& keys) {
                            Action a;
                            a.type = ActType::Key;
                            a.keys = keys;
                            set(a, keys);
                        },
                        [set](const InputId& target) {
                            Action a;
                            a.type = ActType::XrInput;
                            a.target = target;
                            set(a, InputWords(target));
                        });
    }));
    if (ref.wrist) {
        p.rows.push_back(MenuRow::Heading("Go to a page of this panel"));
        for (const auto& [n, pg] : e.prof->wrist[ref.hand].pages) {
            if (n == ref.name) continue;
            std::string name = n, title = TitleOf(pg);
            p.rows.push_back(MenuRow::Button("Page '" + title + "'", "", [set, &m, name, title] {
                Action a;
                a.type = ActType::Page;
                a.page = name;
                set(a, title);
                m.Back();
            }));
        }
        p.rows.push_back(MenuRow::Button("+ A new page", "Made empty, then opened", [&e, &m, set, ref] {
            std::string name;
            e.EditProfile([&](Profile& pr) {
                auto& pages = pr.wrist[ref.hand].pages;
                name = FreeName(pages, "page");
                Wheel pg;
                pg.name = name;
                pg.title = "New page";
                WheelItem back;
                back.label = "Back";
                back.action.type = ActType::Back;
                pg.items.push_back(back);
                pages[name] = pg;
            });
            Action a;
            a.type = ActType::Page;
            a.page = name;
            set(a, "More");
            m.Back();
            int h = ref.hand;
            m.Push([&e, &m, h, name] { return WristPage(e, m, h, name); });
        }));
    }
    p.rows.push_back(MenuRow::Heading(ref.wrist ? "Open a wheel" : "Open a sub-wheel"));
    for (const auto& [n, wh] : e.prof->wheels) {
        if (!ref.wrist && n == ref.name) continue;
        std::string name = n, title = TitleOf(wh);
        p.rows.push_back(MenuRow::Button("Wheel '" + title + "'", "", [set, &m, name, title] {
            Action a;
            a.type = ActType::Wheel;
            a.wheel = name;
            set(a, title);
            m.Back();
        }));
    }
    p.rows.push_back(MenuRow::Button("+ A new wheel", "Made empty, then opened", [&e, &m, set] {
        std::string name;
        e.EditProfile([&](Profile& pr) {
            name = FreeName(pr.wheels, "wheel");
            Wheel w;
            w.name = name;
            w.title = "New wheel";
            pr.wheels[name] = w;
        });
        Action a;
        a.type = ActType::Wheel;
        a.wheel = name;
        set(a, "More");
        m.Back();
        m.Push([&e, &m, name] { return WheelPage(e, m, name); });
    }));
    p.rows.push_back(MenuRow::Heading("Open or switch"));
    simple("VR keyboard", "", ActType::Keyboard, ActMode::Tap, "Keyboard");
    simple("Desktop screen (show / hide)", "", ActType::Screen, ActMode::Toggle, "Screen");
    simple("This settings menu", "", ActType::Menu, ActMode::Tap, "Settings");
    simple("Headset mirror (on / off)", "The window on the desktop showing your view", ActType::Mirror, ActMode::Toggle, "Mirror");
    simple("Controller offsets panel", "", ActType::Offsets, ActMode::Panel, "Offsets");
    simple("Adjust mode", "Grab wheels, panels and the keyboard to place them", ActType::Adjust, ActMode::Toggle, "Adjust");
    p.rows.push_back(MenuRow::Heading(ref.wrist ? "On the panel" : "On the wheel"));
    simple("Back", ref.wrist ? "To the page before" : "To the wheel before (on a sub-wheel)", ActType::Back, ActMode::Tap, "Back");
    simple("Close", "", ActType::Close, ActMode::Tap, "Close");
    simple("Nothing", "", ActType::None, ActMode::Tap, "");
    return p;
}

MenuPage WheelLook(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Wheels: look and picking";
    Profile* pr = e.prof.get();
    auto changed = [&m] { m.Changed(); };
    p.rows.push_back(MenuRow::Heading("Where it shows"));
    p.rows.push_back(MenuRow::Choice("Shows at", "Locked: stays where it opened", { "Hand", "Hand, locked", "Head", "Head, locked" },
                                     [pr] { return (int)pr->style.anchor; },
                                     [&e, changed](int i) {
                                         e.EditProfile([i](Profile& x) { x.style.anchor = (WheelAnchor)i; });
                                         changed();
                                     }));
    p.rows.push_back(Cm("Size (radius)", "", [pr] { return pr->style.radius; },
                        [&e, changed](float v) {
                            e.EditProfile([v](Profile& x) { x.style.radius = v; });
                            changed();
                        },
                        0.06f, 0.3f, 0.01f));
    p.rows.push_back(Percent("Text size", "", [pr] { return pr->style.fontScale; },
                             [&e, changed](float v) {
                                 e.EditProfile([v](Profile& x) { x.style.fontScale = v; });
                                 changed();
                             },
                             0.6f, 2.f, 0.1f));
    p.rows.push_back(MenuRow::Link("Place it by hand", "Adjust mode: grab the wheel with a grip and move it", [&e, &m] {
        m.Close();
        Action a;
        a.type = ActType::Adjust;
        a.mode = ActMode::Toggle;
        e.MenuRunAction(a);
    }));
    p.rows.push_back(MenuRow::Heading("Picking a button"));
    p.rows.push_back(MenuRow::Choice("Pick with", "", { "Stick", "Laser", "Both" }, [pr] { return (int)pr->sel.method; },
                                     [&e, changed](int i) {
                                         e.EditProfile([i](Profile& x) { x.sel.method = (SelectMethod)i; });
                                         changed();
                                     }));
    p.rows.push_back(MenuRow::Choice("Hand that picks", "", { "The one that opened it", "Left", "Right" }, [pr] { return (int)pr->sel.hand; },
                                     [&e, changed](int i) {
                                         e.EditProfile([i](Profile& x) { x.sel.hand = (SelectHand)i; });
                                         changed();
                                     }));
    p.rows.push_back(Percent("Stick deadzone", "How far to push before a button lights", [pr] { return pr->sel.deadzone; },
                             [&e, changed](float v) {
                                 e.EditProfile([v](Profile& x) { x.sel.deadzone = v; });
                                 changed();
                             },
                             0.1f, 0.9f, 0.05f));
    p.rows.push_back(MenuRow::Toggle("Close after picking", "", [pr] { return pr->sel.closeAfterSelect; },
                                     [&e, changed](bool on) {
                                         e.EditProfile([on](Profile& x) { x.sel.closeAfterSelect = on; });
                                         changed();
                                     }));
    p.rows.push_back(MenuRow::Choice("Game controls while open", "Which of the game's buttons wait while a wheel is open",
                                     { "All work", "Few held back", "That hand's", "All held back" }, [pr] { return (int)pr->blockWhileOpen; },
                                     [&e, changed](int i) {
                                         e.EditProfile([i](Profile& x) { x.blockWhileOpen = (BlockWhileOpen)i; });
                                         changed();
                                     }));
    return p;
}

// ------------------------------------------------------------------------------------------------ wrist panels
MenuPage WristHub(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Wrist panels";
    std::vector<std::string> titles, details;
    std::vector<std::function<void()>> open;
    for (int h : { 0, 1 }) {
        const WristStyle& W = e.prof->wrist[h];
        titles.push_back(h ? "Right forearm" : "Left forearm");
        std::string d = !W.enabled ? "Off" : W.mirror ? "On: shows the other arm's buttons" : "On: " + std::to_string(W.pages.size()) + (W.pages.size() == 1 ? " page" : " pages");
        details.push_back(d + ". Pressed with the " + (h ? "left" : "right") + " hand");
        open.push_back([&e, &m, h] { m.Push([&e, &m, h] { return WristPanel(e, m, h); }); });
    }
    MenuRow tiles = MenuRow::Tiles(titles, details, open, 2, 4);
    tiles.previewWrist = true;
    for (int h : { 0, 1 }) {
        const WristStyle& src = e.prof->WristPages(h);
        auto it = src.pages.find(src.start);
        Wheel pic = it != src.pages.end() && e.prof->wrist[h].enabled ? it->second : Wheel{};
        tiles.previews.push_back(pic);
        tiles.previewCols = e.prof->wrist[h].columns; // (both drawn with one count: the panels usually match)
    }
    p.width = 0.62f;
    p.rows.push_back(tiles);
    p.rows.push_back(MenuRow::Note("Turn your forearm toward your eyes to see a panel.", "Poke a button with the other hand's finger, or point and pull the trigger"));
    p.rows.push_back(MenuRow::Link("Place them by hand", "Adjust mode: grab a panel with a grip and move it", [&e, &m] {
        m.Close();
        Action a;
        a.type = ActType::Adjust;
        a.mode = ActMode::Toggle;
        e.MenuRunAction(a);
    }));
    return p;
}

MenuPage WristPanel(Engine& e, InGameMenu& m, int h) {
    MenuPage p;
    p.title = h ? "Right forearm panel" : "Left forearm panel";
    WristStyle* W = &e.prof->wrist[h];
    p.width = 0.62f;
    {
        int owner = W->mirror ? 1 - h : h;
        const WristStyle& src = e.prof->wrist[owner];
        if (W->enabled && src.pages.count(src.start)) {
            ItemListRef first{ true, owner, src.start };
            p.rows.push_back(PreviewOf(e, m, first, { "Open this page", "+ Add a page", "Place it by hand" },
                                       { [&e, &m, owner, n = src.start] { m.Push([&e, &m, owner, n] { return WristPage(e, m, owner, n); }); },
                                         [&e, &m, owner] {
                                             std::string name;
                                             e.EditProfile([&](Profile& pr) {
                                                 auto& pages = pr.wrist[owner].pages;
                                                 name = FreeName(pages, "page");
                                                 Wheel pg;
                                                 pg.name = name;
                                                 pg.title = "New page";
                                                 WheelItem back;
                                                 back.label = "Back";
                                                 back.action.type = ActType::Back;
                                                 pg.items.push_back(back);
                                                 pages[name] = pg;
                                             });
                                             m.Changed();
                                             m.Push([&e, &m, owner, name] { return WristPage(e, m, owner, name); });
                                         },
                                         [&e, &m] {
                                             m.Close();
                                             Action a;
                                             a.type = ActType::Adjust;
                                             a.mode = ActMode::Toggle;
                                             e.MenuRunAction(a);
                                         } },
                                       4));
        }
    }
    auto edit = [&e, &m, h](std::function<void(WristStyle&)> f) {
        e.EditProfile([&](Profile& x) { f(x.wrist[h]); });
        m.Changed();
    };
    p.rows.push_back(MenuRow::Toggle("Panel on this arm", "", [W] { return W->enabled; }, [edit, W](bool on) {
        edit([on](WristStyle& x) {
            x.enabled = on;
            if (on && !x.mirror && x.pages.empty()) { // a first page so it can show
                Wheel pg;
                pg.name = "main";
                pg.title = "Main";
                x.pages["main"] = pg;
                x.start = "main";
            }
        });
    }));
    const WristStyle& other = e.prof->wrist[1 - h];
    p.rows.push_back(MenuRow::Toggle("Show the other arm's buttons", other.mirror ? "Not while the other arm shows these" : "Same pages on both arms",
                                     [W] { return W->mirror; }, [edit, &e, h](bool on) {
                                         if (on && e.prof->wrist[1 - h].mirror) return; // they can't mirror each other
                                         edit([on](WristStyle& x) { x.mirror = on; });
                                     }));
    if (W->mirror) {
        p.rows.push_back(MenuRow::Link("Edit the other arm's pages", "", [&e, &m, h] { m.Push([&e, &m, h] { return WristPanel(e, m, 1 - h); }); }));
    } else {
        p.rows.push_back(MenuRow::Heading("Pages"));
        for (const auto& [n, pg] : W->pages) {
            std::string name = n;
            p.rows.push_back(MenuRow::Link(TitleOf(pg) + (n == W->start ? "   (shown first)" : ""), std::to_string(pg.items.size()) + " buttons",
                                           [&e, &m, h, name] { m.Push([&e, &m, h, name] { return WristPage(e, m, h, name); }); }));
        }
        p.rows.push_back(MenuRow::Button("+ Add a page", "A Back button is on it already", [&e, &m, h] {
            std::string name;
            e.EditProfile([&](Profile& pr) {
                auto& pages = pr.wrist[h].pages;
                name = FreeName(pages, "page");
                Wheel pg;
                pg.name = name;
                pg.title = "New page";
                WheelItem back;
                back.label = "Back";
                back.action.type = ActType::Back;
                pg.items.push_back(back);
                pages[name] = pg;
                if (pages.size() == 1) pr.wrist[h].start = name;
            });
            m.Changed();
            m.Push([&e, &m, h, name] { return WristPage(e, m, h, name); });
        }));
    }
    p.rows.push_back(MenuRow::Heading("Panel"));
    p.rows.push_back(MenuRow::Number("Columns", "", [W] { return (float)W->columns; }, [edit](float v) { edit([v](WristStyle& x) { x.columns = (int)v; }); }, 1, 4, 1));
    p.rows.push_back(Cm("Size", "Width and height", [W] { return W->size; }, [edit](float v) { edit([v](WristStyle& x) { x.size = v; }); }, 0.05f, 0.3f, 0.01f));
    p.rows.push_back(MenuRow::Choice("Side of the arm", "", { "Inside (palm)", "Outside (watch)" }, [W] { return (int)W->face; },
                                     [edit](int i) { edit([i](WristStyle& x) { x.face = (ArmSide)i; }); }));
    p.rows.push_back(MenuRow::Choice("Press by", "", { "Poking", "Pointing", "Both" }, [W] { return (int)W->press; },
                                     [edit](int i) { edit([i](WristStyle& x) { x.press = (PressMethod)i; }); }));
    p.rows.push_back(MenuRow::Heading("When it shows"));
    p.rows.push_back(MenuRow::Link("Try it and set it by hand", "Raise your arm and look: see when it shows, click a stick to set it",
                                   [&e] { e.StartWristViewSetup(); }));
    MenuRow facing = MenuRow::Number("Turned toward you within", "Bigger: shows with the arm less turned", [W] { return W->facingDeg; },
                                     [edit](float v) { edit([v](WristStyle& x) { x.facingDeg = v; }); }, 10, 80, 5);
    facing.unit = "\xC2\xB0";
    p.rows.push_back(facing);
    MenuRow gaze = MenuRow::Number("Looked at within", "Bigger: shows further from the middle of your view", [W] { return W->gazeDeg; },
                                   [edit](float v) { edit([v](WristStyle& x) { x.gazeDeg = v; }); }, 5, 60, 5);
    gaze.unit = "\xC2\xB0";
    p.rows.push_back(gaze);
    MenuRow show = MenuRow::Number("Shows after", "", [W] { return (float)W->showMs; }, [edit](float v) { edit([v](WristStyle& x) { x.showMs = (int)v; }); }, 0, 1500, 50);
    show.unit = "ms";
    p.rows.push_back(show);
    MenuRow hide = MenuRow::Number("Hides after", "", [W] { return (float)W->hideMs; }, [edit](float v) { edit([v](WristStyle& x) { x.hideMs = (int)v; }); }, 0, 1500, 50);
    hide.unit = "ms";
    p.rows.push_back(hide);
    return p;
}

MenuPage WristPage(Engine& e, InGameMenu& m, int h, const std::string& name) {
    MenuPage p;
    ItemListRef ref{ true, h, name };
    Wheel* pg = Find(e, ref);
    if (!pg) {
        p.title = "Page";
        p.rows.push_back(MenuRow::Note("This page is gone"));
        return p;
    }
    WristStyle& W = e.prof->wrist[h];
    p.title = std::string(h ? "Right" : "Left") + " wrist: " + TitleOf(*pg);
    p.width = 0.62f;
    p.rows.push_back(PreviewOf(e, m, ref, { "+ Add a button", "Rename" },
                               { [&e, &m, ref] { AddItem(e, m, ref); },
                                 [&e, &m, ref] {
                                     Wheel* w = Find(e, ref);
                                     if (!w) return;
                                     m.AskText("Page name", TitleOf(*w),
                                               [&e, &m, ref](const std::string& t) { EditList(e, m, ref, [&](Wheel& x) { x.title = t; }); });
                                 } },
                               5));
    p.rows.push_back(MenuRow::Heading("Buttons in order"));
    AddItemRows(e, m, ref, p);
    p.rows.push_back(MenuRow::Heading("Page"));
    if (W.start != name)
        p.rows.push_back(MenuRow::Button("Show this page first", "", [&e, &m, h, name] {
            e.EditProfile([&](Profile& pr) { pr.wrist[h].start = name; });
            m.Changed();
        }));
    else
        p.rows.push_back(MenuRow::Note("This page shows first"));
    if (W.pages.size() > 1) {
        MenuRow del = MenuRow::Button("Delete page", "Buttons that went to it will do nothing", [&e, &m, h, name] {
            e.EditProfile([&](Profile& pr) {
                WristStyle& x = pr.wrist[h];
                x.pages.erase(name);
                ForgetPage(x, name);
                if (x.start == name && !x.pages.empty()) x.start = x.pages.begin()->first;
            });
            m.Changed();
            m.Back();
        });
        del.isDanger = true;
        p.rows.push_back(del);
    }
    return p;
}

// ------------------------------------------------------------------------------------------------ sticks, more
MenuPage Sticks(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "Sticks";
    p.stickFree = true;
    p.rows.push_back(MenuRow::Live(LiveKind::BothControllers, 1, 3));
    auto word = [&](int h) {
        const StickShape& s = e.prof->sticks[h];
        std::string d;
        if (s.innerDeadzone > 0) d += "inner deadzone " + std::to_string((int)std::lround(s.innerDeadzone * 100)) + "%  ";
        if (s.directionAngleDeg > 0) d += "direction zone " + std::to_string((int)s.directionAngleDeg) + "\xC2\xB0";
        return d.empty() ? std::string("All off") : d;
    };
    p.rows.push_back(MenuRow::Tiles({ "Left stick", "Right stick" }, { word(0), word(1) },
                                    { [&e, &m] { m.Push([&e, &m] { return StickOptions(e, m, 0); }); },
                                      [&e, &m] { m.Push([&e, &m] { return StickOptions(e, m, 1); }); } },
                                    2, 3));
    p.rows.push_back(MenuRow::Link("Test controllers", "Everything live, sticks and buttons", [&e, &m] { m.Push([&e, &m] { return TestControllers(e, m); }); }));
    return p;
}

MenuPage MoreSettings(Engine& e, InGameMenu& m) {
    MenuPage p;
    p.title = "More settings";
    Profile* pr = e.prof.get();
    auto edit = [&e, &m](std::function<void(Profile&)> f) {
        e.EditProfile(f);
        m.Changed();
    };
    p.rows.push_back(MenuRow::Heading("Laser and fingertip"));
    p.rows.push_back(Percent("Steady the laser", "For shaky hands: higher is steadier, a little slower", [pr] { return pr->pointer.smoothing; },
                             [edit](float v) { edit([v](Profile& x) { x.pointer.smoothing = v; }); }, 0, 1, 0.1f, "off"));
    p.rows.push_back(MenuRow::Toggle("Fingertip near wrist panels", "", [pr] { return pr->pointer.finger; },
                                     [edit](bool on) { edit([on](Profile& x) { x.pointer.finger = on; }); }));
    p.rows.push_back(MenuRow::Toggle("Short laser while a panel shows", "", [pr] { return pr->pointer.guide; },
                                     [edit](bool on) { edit([on](Profile& x) { x.pointer.guide = on; }); }));
    p.rows.push_back(MenuRow::Heading("Feedback"));
    p.rows.push_back(MenuRow::Toggle("Vibration", "A small buzz on hover and press", [pr] { return pr->haptics; },
                                     [edit](bool on) { edit([on](Profile& x) { x.haptics = on; }); }));
    p.rows.push_back(Percent("Vibration strength", "", [pr] { return pr->hapticAmp; }, [edit](float v) { edit([v](Profile& x) { x.hapticAmp = v; }); },
                             0.05f, 1, 0.05f));
    p.rows.push_back(MenuRow::Toggle("Messages", "Short notes like this one in front of you", [pr] { return pr->notifications; },
                                     [edit](bool on) { edit([on](Profile& x) { x.notifications = on; }); }));
    p.rows.push_back(MenuRow::Heading("Panels"));
    p.rows.push_back(MenuRow::Link("Controller offsets", "How the game's hands and guns point (its own panel)", [&e, &m] {
        m.Close();
        Action a;
        a.type = ActType::Offsets;
        a.mode = ActMode::Panel;
        e.OffsetsAction(a);
    }));
    p.rows.push_back(MenuRow::Link("Adjust mode", "Grab wheels, wrist panels, the screen and the keyboard to place them", [&e, &m] {
        m.Close();
        Action a;
        a.type = ActType::Adjust;
        a.mode = ActMode::Toggle;
        e.MenuRunAction(a);
    }));
    p.rows.push_back(MenuRow::Button("Headset mirror on / off", "The window on the desktop showing your view", [&e] {
        Action a;
        a.type = ActType::Mirror;
        a.mode = ActMode::Toggle;
        e.MenuRunAction(a);
    }));
    return p;
}

} // namespace MenuPages
