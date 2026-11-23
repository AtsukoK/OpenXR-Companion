#include "game_input.h"

// Where one of the game's Index / Touch inputs is on the Frame ("" when it has no place there).
// The left Frame has a D-pad where the others have X / Y (or A / B): the lower button is D-pad down, the upper one up.
std::string SteamFramePathFor(const std::string& from, const std::string& path) {
    const std::string L = "/user/hand/left/", R = "/user/hand/right/";
    bool left = path.rfind(L, 0) == 0, right = path.rfind(R, 0) == 0;
    if (!left && !right) return "";
    std::string hand = left ? L : R, sub = path.substr(hand.size());
    auto starts = [&](const char* p) {
        return sub.rfind(p, 0) == 0;
    };
    auto feature = [&](const std::string& part) {
        return sub.substr(part.size());
    };
    bool isIndex = from.find("index") != std::string::npos;
    if (sub == "input/grip/pose" || sub == "input/aim/pose" || sub == "output/haptic") return path;
    if (starts("input/trigger/") || starts("input/thumbstick")) return path;
    if (starts("input/squeeze/")) return hand + (sub == "input/squeeze/force" ? "input/squeeze/value" : sub);
    std::string lower = isIndex || right ? "input/a" : "input/x", upper = isIndex || right ? "input/b" : "input/y";
    if (starts(lower.c_str()) || starts(upper.c_str())) {
        bool isLower = starts(lower.c_str());
        std::string f = feature(isLower ? lower : upper);
        if (right) return hand + (isLower ? "input/a" : "input/b") + f;
        return hand + (isLower ? "input/dpad_down" : "input/dpad_up") + f;
    }
    // The menu: the Touch's left menu, or the Index's system button (games bind it as their menu).
    if (starts("input/menu/") || (isIndex && starts("input/system/")))
        return hand + (left ? "input/view" : "input/menu") + feature(starts("input/menu/") ? "input/menu" : "input/system");
    return ""; // thumbrest, trackpad: not on the Frame
}

bool ParseGameInputPath(const std::string& path, int& hand, int& comp, InputFeature& feature) {
    const std::string prefix[2] = { "/user/hand/left/input/", "/user/hand/right/input/" };
    for (int h = 0; h < 2; h++) {
        if (path.rfind(prefix[h], 0) != 0) continue;
        std::string rest = path.substr(prefix[h].size());
        auto slash = rest.find('/');
        std::string part = rest.substr(0, slash);
        std::string feat = slash == std::string::npos ? "" : rest.substr(slash + 1);
        int c = part == "select" ? C_TRIGGER : ParseCompName(part);
        if (h == 1 && (part == "x" || part == "y")) c = part == "x" ? C_BTN3 : C_BTN4; // only the Steam Frame's right hand
        if (c < 0 || part == "system") return false;
        hand = h;
        comp = c;
        if (feat == "touch")
            feature = InputFeature::Touch;
        else if (feat.empty() && (c == C_STICK || c == C_TRACKPAD))
            feature = InputFeature::Vector;
        else if (feat == "x")
            feature = InputFeature::X;
        else if (feat == "y")
            feature = InputFeature::Y;
        else if (feat == "click" || feat == "value" || feat == "force" || feat.empty())
            feature = InputFeature::Value;
        else
            return false;
        return true;
    }
    return false;
}

void GameInputFilter::AddBinding(XrAction action, int hand, int comp, InputFeature feature) {
    auto& list = bindings_[(uint64_t)action];
    for (auto& b : list)
        if (b.hand == hand && b.comp == comp && b.feature == feature) return;
    list.push_back({ hand, comp, feature });
}

void GameInputFilter::SetSubactionPaths(XrAction action, const XrPath* paths, uint32_t count) {
    subactions_[(uint64_t)action].assign(paths, paths + count);
}

void GameInputFilter::Clear() {
    logged_.clear();
    bindings_.clear();
    subactions_.clear();
}

// Which hands a query covers: bit 0 left, bit 1 right.
int GameInputFilter::HandsMask(XrPath sub) const {
    if (sub == XR_NULL_PATH) return 3;
    if (sub == handPath_[0]) return 1;
    if (sub == handPath_[1]) return 2;
    return 0;
}

GameInputFilter::Verdict GameInputFilter::Evaluate(const Engine& eng, XrAction a, XrPath sub) const {
    Verdict v;
    if (!eng.Active()) return v;
    auto it = bindings_.find((uint64_t)a);
    if (it == bindings_.end()) return v;
    int mask = HandsMask(sub);
    int bound = 0, blocked = 0;
    for (auto& b : it->second) {
        if (!(mask & (1 << b.hand))) continue;
        v.any = true;
        bound++;
        const Override& o = eng.Ov(b.hand, b.comp);
        if (o.on && !v.overridden) {
            v.overridden = true;
            v.value = o;
            v.feature = b.feature;
        }
        if (eng.Blocked(b.hand, b.comp))
            blocked++;
        else
            v.freeHand = b.hand;
    }
    v.blockedAll = bound > 0 && blocked == bound;
    v.blockedSome = blocked > 0 && blocked < bound;
    return v;
}

bool GameInputFilter::CanRequery(XrAction a, int hand) const {
    auto it = subactions_.find((uint64_t)a);
    if (it == subactions_.end()) return false;
    for (auto p : it->second)
        if (p == handPath_[hand]) return true;
    return false;
}

// Keeps changedSinceLastSync honest when we alter values.
bool GameInputFilter::Changed(XrAction a, XrPath sub, float v0, float v1) {
    uint64_t key = (uint64_t)a * 1000003ull ^ (uint64_t)sub;
    Track& t = track_[key];
    if (t.sync != sync_) {
        if (t.sync != ~0ull) {
            t.prev[0] = t.cur[0];
            t.prev[1] = t.cur[1];
        } else {
            t.prev[0] = v0;
            t.prev[1] = v1;
        }
        t.sync = sync_;
    }
    t.cur[0] = v0;
    t.cur[1] = v1;
    return t.prev[0] != v0 || t.prev[1] != v1;
}

// The three state types differ only in their value; these say how to read and write it.
namespace {
struct BoolValue {
    using State = XrActionStateBoolean;
    static constexpr XrStructureType kType = XR_TYPE_ACTION_STATE_BOOLEAN;
    using T = XrBool32;
    static T Zero() { return XR_FALSE; }
    static T FromOverride(const Override& o, InputFeature f) { return f == InputFeature::Touch ? o.touch : o.press; }
    static void Floats(T v, float& a, float& b) {
        a = v ? 1.f : 0.f;
        b = 0;
    }
};
struct FloatValue {
    using State = XrActionStateFloat;
    static constexpr XrStructureType kType = XR_TYPE_ACTION_STATE_FLOAT;
    using T = float;
    static T Zero() { return 0.f; }
    static T FromOverride(const Override& o, InputFeature f) {
        switch (f) {
        case InputFeature::X:
            return o.x;
        case InputFeature::Y:
            return o.y;
        case InputFeature::Touch:
            return o.touch ? 1.f : 0.f;
        default:
            return o.value;
        }
    }
    static void Floats(T v, float& a, float& b) {
        a = v;
        b = 0;
    }
};
struct VectorValue {
    using State = XrActionStateVector2f;
    static constexpr XrStructureType kType = XR_TYPE_ACTION_STATE_VECTOR2F;
    using T = XrVector2f;
    static T Zero() { return { 0, 0 }; }
    static T FromOverride(const Override& o, InputFeature) { return { o.x, o.y }; }
    static void Floats(T v, float& a, float& b) {
        a = v.x;
        b = v.y;
    }
};

// Shared by the three Filter overloads.
template <class V, class Verdict, class CanRequeryFn, class ChangedFn>
void FilterState(const Verdict& e, XrPath sub, XrTime now, typename V::State& st, XrPath freeHandPath,
                 const std::function<bool(XrPath, typename V::State&)>& requery, CanRequeryFn canRequery, ChangedFn changed) {
    typename V::T v = st.currentState;
    if (e.overridden) { // a binding pushes a value into the game
        v = V::FromOverride(e.value, e.feature);
        st.isActive = XR_TRUE;
    } else if (e.blockedAll) { // held for a menu
        v = V::Zero();
    } else if (e.blockedSome && sub == XR_NULL_PATH) { // one hand held: answer with the other hand's value
        v = V::Zero();
        if (e.freeHand >= 0 && canRequery()) {
            typename V::State other{ V::kType };
            if (requery(freeHandPath, other)) v = other.currentState;
        }
    }
    st.currentState = v;
    float a, b;
    V::Floats(v, a, b);
    bool ch = changed(a, b);
    st.changedSinceLastSync = ch;
    if (ch) st.lastChangeTime = now;
}
} // namespace

void GameInputFilter::Filter(const Engine& eng, XrAction a, XrPath sub, XrTime now, XrActionStateBoolean& st,
                             const Requery<XrActionStateBoolean>& requery) {
    Verdict e = Evaluate(eng, a, sub);
    if (!e.any) return;
    // An on/off action on one stick axis (snap turn on x, jump on y): off while the push is straightened onto the
    // other axis by the direction zone (up and a little right doesn't also turn).
    if (!e.overridden && !e.blockedAll) {
        InputFeature axis = InputFeature::Value;
        if (int h = StickHand(eng, a, sub, &axis); h >= 0 && (axis == InputFeature::X || axis == InputFeature::Y)) {
            const StickShape& sh = eng.StickShapeOf(h);
            LogStickAction(a, sub, "on/off", h, axis, sh.directionAngleDeg > 0);
            if (sh.directionAngleDeg > 0) {
                float x = eng.stickX[h], y = eng.stickY[h];
                ShapeStick(sh, x, y);
                if ((axis == InputFeature::X ? x : y) == 0.f) st.currentState = XR_FALSE;
            }
        }
    }
    FilterState<BoolValue>(
        e, sub, now, st, e.freeHand >= 0 ? handPath_[e.freeHand] : XR_NULL_PATH, requery, [&] { return CanRequery(a, e.freeHand); },
        [&](float v0, float v1) { return Changed(a, sub, v0, v1); });
}

int GameInputFilter::StickHand(const Engine& eng, XrAction a, XrPath sub, InputFeature* axis) const {
    auto it = bindings_.find((uint64_t)a);
    if (it == bindings_.end()) return -1;
    int mask = HandsMask(sub), found = -1;
    for (const auto& b : it->second) {
        bool readsStick = b.comp == C_STICK && b.feature != InputFeature::Touch && b.feature != InputFeature::Value;
        if (!readsStick || !(mask & (1 << b.hand)) || b.hand == found) continue;
        if (found < 0) {
            found = b.hand;
            if (axis) *axis = b.feature;
            continue;
        }
        // both sticks feed it: the runtime reports the one pushed further, so its shaping applies
        auto push = [&](int h) { return eng.stickX[h] * eng.stickX[h] + eng.stickY[h] * eng.stickY[h]; };
        if (push(b.hand) > push(found)) {
            found = b.hand;
            if (axis) *axis = b.feature;
        }
        return found;
    }
    return found;
}

void GameInputFilter::LogStickAction(XrAction a, XrPath sub, const char* type, int hand, InputFeature axis, bool shaped) {
    uint64_t key = (uint64_t)a * 31ull ^ (uint64_t)sub;
    if (logged_[key]) return;
    logged_[key] = true;
    static const char* axes[] = { "value", "touch", "x+y", "x", "y" };
    int bothHands = 0;
    for (const auto& b : bindings_[(uint64_t)a])
        if (b.comp == C_STICK) bothHands |= 1 << b.hand;
    Log("game stick action %llx (%s, %s stick%s, reads %s, asked for %s): direction zone %s", (unsigned long long)a, type,
        hand ? "right" : "left", bothHands == 3 ? ", bound to both sticks" : "", axes[(int)axis],
        sub == XR_NULL_PATH ? "any hand" : sub == handPath_[0] ? "left" : sub == handPath_[1] ? "right" : "?", shaped ? "applied" : "off");
}

void GameInputFilter::Filter(const Engine& eng, XrAction a, XrPath sub, XrTime now, XrActionStateFloat& st,
                             const Requery<XrActionStateFloat>& requery) {
    Verdict e = Evaluate(eng, a, sub);
    if (!e.any) return;
    // A stick axis read on its own: deadzones along that axis (before the changed check below).
    if (!e.overridden && !e.blockedAll)
    {
        InputFeature axis = InputFeature::Value;
        if (int h = StickHand(eng, a, sub, &axis); h >= 0) {
            const StickShape& sh = eng.StickShapeOf(h);
            LogStickAction(a, sub, "number", h, axis, sh.directionAngleDeg > 0);
            if (sh.directionAngleDeg > 0 && (axis == InputFeature::X || axis == InputFeature::Y)) {
                // straightened with the whole stick: up and a little right gives this x axis 0, not a small turn
                float x = eng.stickX[h], y = eng.stickY[h];
                ShapeStick(sh, x, y);
                st.currentState = axis == InputFeature::X ? x : y;
            } else
                st.currentState = ShapeStickAxis(sh, st.currentState);
        }
    }
    FilterState<FloatValue>(
        e, sub, now, st, e.freeHand >= 0 ? handPath_[e.freeHand] : XR_NULL_PATH, requery, [&] { return CanRequery(a, e.freeHand); },
        [&](float v0, float v1) { return Changed(a, sub, v0, v1); });
}

void GameInputFilter::Filter(const Engine& eng, XrAction a, XrPath sub, XrTime now, XrActionStateVector2f& st,
                             const Requery<XrActionStateVector2f>& requery) {
    Verdict e = Evaluate(eng, a, sub);
    if (!e.any) return;
    // The hand's stick deadzones (off unless set in the profile); a value a binding pushes in is left as it is.
    if (!e.overridden && !e.blockedAll)
        if (int h = StickHand(eng, a, sub); h >= 0) {
            LogStickAction(a, sub, "x+y", h, InputFeature::Vector, eng.StickShapeOf(h).directionAngleDeg > 0);
            ShapeStick(eng.StickShapeOf(h), st.currentState.x, st.currentState.y);
        }
    FilterState<VectorValue>(
        e, sub, now, st, e.freeHand >= 0 ? handPath_[e.freeHand] : XR_NULL_PATH, requery, [&] { return CanRequery(a, e.freeHand); },
        [&](float v0, float v1) { return Changed(a, sub, v0, v1); });
}
