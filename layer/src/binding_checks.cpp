#include "binding_checks.h"
#include <algorithm>
#include <cctype>

namespace {
std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
bool SameInput(const InputId& a, const InputId& b) { return a.hand == b.hand && a.comp == b.comp && a.feat == b.feat; }
bool Keeps(const Binding& b) { return b.block != BindingBlock::None; }
bool Leaks(const Binding& b) { return b.gesture == Gesture::Hold && b.tapPassthrough && !b.hasTapAction; }
int StartGroup(Gesture g) { return g == Gesture::Press || g == Gesture::WhileHeld ? 0 : 1 + (int)g; }
std::vector<std::string> Sorted(std::vector<std::string> v) {
    for (auto& s : v) s = Lower(s);
    std::sort(v.begin(), v.end());
    return v;
}
bool SameChord(const Binding& a, const Binding& b) {
    if (a.chord.size() != b.chord.size()) return false;
    for (auto& x : a.chord)
        if (std::none_of(b.chord.begin(), b.chord.end(), [&](const InputId& y) { return SameInput(x, y); })) return false;
    return true;
}
bool TurnsOn(const Action& a, const std::string& layer) {
    return a.type == ActType::Layer && Lower(a.layer) == Lower(layer) && a.mode != ActMode::Off;
}
bool LayerSwitchedOn(const Profile& p, const std::string& layer) {
    for (auto& b : p.bindings)
        if (b.enabled && (TurnsOn(b.action, layer) || (b.hasTapAction && TurnsOn(b.tapAction, layer)))) return true;
    for (auto& [n, w] : p.wheels)
        for (auto& i : w.items)
            if (TurnsOn(i.action, layer)) return true;
    for (int h = 0; h < 2; h++)
        for (auto& [n, w] : p.wrist[h].pages)
            for (auto& i : w.items)
                if (TurnsOn(i.action, layer)) return true;
    return false;
}
} // namespace

std::vector<BindingWarning> CheckBinding(const Profile& p, int index, bool vigemInstalled) {
    std::vector<BindingWarning> out;
    if (index < 0 || index >= (int)p.bindings.size()) return out;
    const Binding& b = p.bindings[index];
    if (!b.enabled || b.chord.empty()) return out;
    const InputId& trig = b.chord.back();
    for (int i = 0; i < (int)p.bindings.size(); i++) {
        const Binding& o = p.bindings[i];
        if (i == index || !o.enabled || o.chord.empty()) continue;
        bool sameButton = o.chord.size() == 1 && SameInput(o.chord.back(), trig);
        // 1. a Hold lets short presses through while the other keeps the button from the game
        if (sameButton && Keeps(b) && Leaks(o))
            out.push_back({ "Single presses still reach the game", "'" + o.name + "' (Hold) sends short presses to the game" });
        if (sameButton && Leaks(b) && Keeps(o))
            out.push_back({ "Short presses get past another binding", "'" + o.name + "' keeps this button from the game" });
        // 2. the same press starts both
        if (SameChord(o, b) && StartGroup(o.gesture) == StartGroup(b.gesture) && Sorted(o.layers) == Sorted(b.layers) &&
            Sorted(o.notLayers) == Sorted(b.notLayers))
            out.push_back({ "Another binding fires on the same press", "'" + o.name + "' runs too" });
    }
    // 3. a double-tap the game also gets (buttons and clicks, not touches or stick directions)
    bool direction = trig.feat == F_UP || trig.feat == F_DOWN || trig.feat == F_LEFT || trig.feat == F_RIGHT;
    if (b.gesture == Gesture::DoubleTap && !Keeps(b) && trig.feat != F_TOUCH && trig.comp != C_THUMBREST && !direction)
        out.push_back({ "The game gets both taps", "The game still gets this button: a double tap presses it twice there" });
    // 4. keeping a stick click from the game also holds back that stick's movement
    if (Keeps(b) && std::any_of(b.chord.begin(), b.chord.end(), [](const InputId& x) { return x.comp == C_STICK && x.feat == F_PRESS; }))
        out.push_back({ "Movement pauses while you click", "The stick is kept from the game while you press it" });
    // 5. a layer nothing switches on
    for (auto& l : b.layers)
        if (!LayerSwitchedOn(p, l)) out.push_back({ "This layer is never switched on", "Nothing switches layer '" + l + "' on" });
    // 7. Xbox 360 buttons without the driver
    if (!vigemInstalled && (b.action.type == ActType::Gamepad || (b.hasTapAction && b.tapAction.type == ActType::Gamepad)))
        out.push_back({ "Xbox 360 buttons need ViGEmBus", "The optional driver isn't installed: nothing happens" });
    return out;
}
