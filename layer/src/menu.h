#pragma once
// The in-game settings menu: what a page holds (the model), what the overlay draws (the view) and where each
// clickable part sits (the layout, shared by drawing and pointing so they always agree).
//
// A page is a list of rows. Each row knows how to show and change its own value through small functions, so a
// page can be built straight from the profile and its edits go straight back into it. To add a part of the app to
// the menu: write a page builder (menu_pages.cpp) and link to it from the home page.
#include "common.h"
#include "controller_art.h"
#include <functional>
#include <string>
#include <vector>

enum class MenuRowKind {
    Heading, // a small title between groups of rows
    Note,    // a line of text, nothing to press
    Link,    // opens another page (or a panel)
    Button,  // does something once
    Toggle,  // on / off
    Number,  // a value with - and + (the stick moves it too)
    Choice,  // one of a few options, < and > step through them
    Live,    // a live picture (a controller with what is pressed now, a stick with its zones), several rows tall
};

// What a Live row shows.
enum class LiveKind {
    Controller,      // one controller from above: pressed / touched parts light up, the stick dot moves, trigger and grip bars
    BothControllers, // left and right side by side
    Stick,           // one stick to scale: deadzones, direction zones, where the stick is and what the game sees
};

struct MenuRow {
    MenuRowKind kind = MenuRowKind::Note;
    std::string label;
    std::string detail; // a second, smaller line: help or what it does now
    bool isDanger = false; // Button: something that can't be undone (shown in red)

    std::function<void()> press;      // Link, Button
    std::function<bool()> isOn;       // Toggle
    std::function<void(bool)> setOn;  // Toggle
    std::function<float()> value;     // Number
    std::function<void(float)> set;   // Number
    float min = 0, max = 1, step = 1; // Number
    int decimals = 0;                 // Number
    std::string unit;                 // Number: "%", "°"...
    std::string atMin;                // Number: shown instead of the minimum ("off")
    float scale = 1;                  // Number: shown = value * scale (a 0..1 share shown as %)
    std::vector<std::string> options; // Choice
    std::function<int()> chosen;      // Choice
    std::function<void(int)> choose;  // Choice
    LiveKind live = LiveKind::Controller; // Live
    int liveHand = 1;                     // Live: 0 left, 1 right
    int span = 1;                         // rows of height it takes (Live rows are taller)

    static MenuRow Heading(std::string label);
    static MenuRow Note(std::string label, std::string detail = "");
    static MenuRow Link(std::string label, std::string detail, std::function<void()> open);
    static MenuRow Button(std::string label, std::string detail, std::function<void()> press);
    static MenuRow Toggle(std::string label, std::string detail, std::function<bool()> isOn, std::function<void(bool)> setOn);
    static MenuRow Number(std::string label, std::string detail, std::function<float()> value, std::function<void(float)> set, float min,
                          float max, float step);
    static MenuRow Choice(std::string label, std::string detail, std::vector<std::string> options, std::function<int()> chosen,
                          std::function<void(int)> choose);
    static MenuRow Live(LiveKind what, int hand, int span);

    // The value as shown on the right ("12 %", "off", "Hold", "ON").
    std::string ValueText() const;
};

struct MenuPage {
    std::string title;
    std::vector<MenuRow> rows;
};
using MenuPageBuilder = std::function<MenuPage()>;

// ---------------------------------------------------------------- what the overlay draws
struct MenuViewRow {
    MenuRowKind kind = MenuRowKind::Note;
    std::string label, detail, value;
    bool isOn = false;            // Toggle
    bool canLess = false, canMore = false; // Number / Choice: - and + can still change it
    bool isDanger = false;
    float fraction = -1;
    LiveKind live = LiveKind::Controller;
    int liveHand = 1, span = 1;                   // Number: where the value sits between its min and max (0..1), for the slider
};

// The parts of the panel that can be pointed at.
enum class MenuPart { None, Row, Less, More, Back, Close, ScrollUp, ScrollDown };

struct MenuHit {
    MenuPart part = MenuPart::None;
    int row = -1; // index into MenuView::rows (Row / Less / More)
    bool operator==(const MenuHit& o) const { return part == o.part && row == o.row; }
    bool operator!=(const MenuHit& o) const { return !(*this == o); }
};

// One hand's controller right now, for Live rows (copied from the engine a few times a second while shown).
struct LiveHand {
    ControllerKind kind = ControllerKind::Unknown;
    bool press[C_COUNT]{}, touch[C_COUNT]{};
    float trigger = 0, grip = 0, stickX = 0, stickY = 0;
    StickShape shape; // this hand's stick options
    bool operator==(const LiveHand& o) const;
};

struct MenuView {
    bool open = false;
    XrPosef pose{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    float w = 0.5f, h = 0.56f; // metres
    std::string title;
    std::string status; // a line at the bottom: what to do now ("Press the button you want...")
    bool canBack = false;
    std::vector<MenuViewRow> rows;
    int first = 0;             // the first row shown (scrolled)
    static constexpr int kVisibleRows = 8;
    MenuHit hover, pressed;
    LiveHand live[2];
    float stickOn = 0.7f; // where a stick direction counts as pushed
    uint64_t version = 0;
};

// The largest `first` that still fills the panel (rows can be taller than one slot).
int MenuMaxFirst(const MenuView& view);

// One clickable box, in panel coordinates (u right, v down, both 0..1).
struct MenuBox {
    float x, y, w, h;
    MenuHit hit;
    bool Contains(float u, float v) const { return u >= x && u < x + w && v >= y && v < y + h; }
};

// Where everything sits on the panel: the header (back, title, close), the visible rows and their - / +, the
// scroll arrows and the status line. Rows are drawn in the order given.
void MenuLayout(const MenuView& view, std::vector<MenuBox>& out);
// The fixed areas, for drawing: header and status bands (v from / to).
constexpr float kMenuHeaderH = 0.12f, kMenuFooterH = 0.1f;
