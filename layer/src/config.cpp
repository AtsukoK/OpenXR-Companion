#include "config.h"
#include "gamepad.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

static std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}
static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(Trim(cur));
            cur.clear();
        } else
            cur += c;
    }
    out.push_back(Trim(cur));
    return out;
}

const char* const kCompNames[2][C_COUNT] = {
    { "x", "y", "trigger", "grip", "thumbstick", "thumbrest", "menu", "button3", "button4", "bumper", "dpad_up", "dpad_down",
      "dpad_left", "dpad_right", "trackpad" },
    { "a", "b", "trigger", "grip", "thumbstick", "thumbrest", "menu", "button3", "button4", "bumper", "dpad_up", "dpad_down",
      "dpad_left", "dpad_right", "trackpad" },
};

int ParseCompName(const std::string& n) {
    std::string s = Lower(n);
    if (s == "button3" || s == "btn3") return C_BTN3;
    if (s == "button4" || s == "btn4") return C_BTN4;
    if (s == "bumper") return C_BUMPER;
    if (s == "dpad_up") return C_DPAD_UP;
    if (s == "dpad_down") return C_DPAD_DOWN;
    if (s == "dpad_left") return C_DPAD_LEFT;
    if (s == "dpad_right") return C_DPAD_RIGHT;
    if (s == "view") return C_MENU;
    if (s == "a" || s == "x" || s == "primary" || s == "btn1") return C_BTN1;
    if (s == "b" || s == "y" || s == "secondary" || s == "btn2") return C_BTN2;
    if (s == "trigger") return C_TRIGGER;
    if (s == "grip" || s == "squeeze") return C_GRIP;
    if (s == "thumbstick" || s == "stick") return C_STICK;
    if (s == "trackpad" || s == "touchpad") return C_TRACKPAD;
    if (s == "thumbrest") return C_THUMBREST;
    if (s == "menu") return C_MENU;
    return -1;
}

bool ParseInput(const std::string& str, InputId& out) {
    auto parts = Split(Lower(str), '.');
    if (parts.size() < 2 || parts.size() > 3) return false;
    InputId r;
    if (parts[0] == "left" || parts[0] == "l")
        r.hand = 0;
    else if (parts[0] == "right" || parts[0] == "r")
        r.hand = 1;
    else
        return false;
    r.comp = ParseCompName(parts[1]);
    if (r.comp < 0) return false;
    r.feat = r.comp == C_THUMBREST ? F_TOUCH : F_PRESS;
    if (parts.size() == 3) {
        const auto& f = parts[2];
        if (f == "click" || f == "press")
            r.feat = F_PRESS;
        else if (f == "touch")
            r.feat = F_TOUCH;
        else if (f == "up")
            r.feat = F_UP;
        else if (f == "down")
            r.feat = F_DOWN;
        else if (f == "left")
            r.feat = F_LEFT;
        else if (f == "right")
            r.feat = F_RIGHT;
        else if (f == "full")
            r.feat = F_FULL;
        else
            return false;
        if (r.feat >= F_UP && r.feat <= F_RIGHT && r.comp != C_STICK && r.comp != C_TRACKPAD) return false; // sticks, trackpads
        if (r.feat == F_FULL && r.comp != C_TRIGGER && r.comp != C_GRIP) return false; // the click at the end of a pull
        if (r.comp == C_THUMBREST) r.feat = F_TOUCH;
    }
    out = r;
    return true;
}

std::string InputName(const InputId& i) {
    if (!i.valid()) return "?";
    static const char* feat[] = { "", ".touch", ".up", ".down", ".left", ".right", ".full" };
    std::string s = i.hand == 0 ? "left." : "right.";
    s += kCompNames[i.hand == 0 ? 0 : 1][i.comp];
    if (!(i.comp == C_THUMBREST && i.feat == F_TOUCH)) s += feat[i.feat];
    return s;
}

static uint32_t ParseColor(const json& j, uint32_t def) {
    if (!j.is_string()) return def;
    std::string s = j.get<std::string>();
    if (s.size() && s[0] == '#') s = s.substr(1);
    if (s.size() != 6 && s.size() != 8) return def;
    uint32_t v = (uint32_t)std::stoul(s, nullptr, 16);
    if (s.size() == 6) return 0xFF000000u | v;
    // #RRGGBBAA -> AARRGGBB
    return (v >> 8) | ((v & 0xFF) << 24);
}

template <class T>
static void Get(const json& j, const char* k, T& v) {
    if (j.contains(k) && !j[k].is_null()) v = j[k].get<T>();
}

static std::vector<std::string> StrList(const json& j, const char* k) {
    std::vector<std::string> out;
    if (!j.contains(k)) return out;
    if (j[k].is_string()) {
        for (auto& s : Split(j[k].get<std::string>(), ','))
            if (!s.empty()) out.push_back(s);
    } else if (j[k].is_array())
        for (auto& e : j[k])
            out.push_back(e.get<std::string>());
    return out;
}

static Action ParseAction(const json& j, const std::string& ctx) {
    Action a;
    if (!j.is_object()) return a;
    std::string t = Lower(j.value("type", "none"));
    if (t == "key" || t == "keys")
        a.type = ActType::Key;
    else if (t == "sequence" || t == "macro")
        a.type = ActType::Sequence;
    else if (t == "xr_input" || t == "button")
        a.type = ActType::XrInput;
    else if (t == "wheel" || t == "submenu")
        a.type = ActType::Wheel;
    else if (t == "back")
        a.type = ActType::Back;
    else if (t == "close")
        a.type = ActType::Close;
    else if (t == "layer")
        a.type = ActType::Layer;
    else if (t == "reload")
        a.type = ActType::Reload;
    else if (t == "keyboard")
        a.type = ActType::Keyboard;
    else if (t == "page")
        a.type = ActType::Page;
    else if (t == "adjust")
        a.type = ActType::Adjust;
    else if (t == "screen")
        a.type = ActType::Screen;
    else if (t == "controller_offsets" || t == "offsets")
        a.type = ActType::Offsets;
    else if (t == "headset_mirror" || t == "mirror")
        a.type = ActType::Mirror;
    else if (t == "settings_menu" || t == "settings")
        a.type = ActType::Menu;
    else if (t == "gamepad" || t == "pad")
        a.type = ActType::Gamepad;
    else if (t == "none" || t.empty())
        a.type = ActType::None;
    else
        throw std::runtime_error(ctx + ": unknown action type '" + t + "'");
    Get(j, "keys", a.keys);
    std::string modeText;
    Get(j, "mode", modeText);
    modeText = Lower(modeText);
    a.mode = modeText.empty() ? ActMode::Tap : ParseActMode(modeText);
    Get(j, "tap_ms", a.tapMs);
    Get(j, "steps", a.steps);
    Get(j, "wheel", a.wheel);
    Get(j, "page", a.page);
    std::string confirm;
    Get(j, "confirm", confirm);
    if (!confirm.empty() && !ParseEnum(kConfirmModeNames, Lower(confirm), a.confirm)) a.confirm = ConfirmMode::Button;
    Get(j, "layer", a.layer);
    std::string place;
    Get(j, "place", place);
    place = Lower(place);
    Get(j, "monitor", a.monitor);
    Get(j, "window", a.window);
    if (a.type == ActType::Screen && (a.mode == ActMode::Tap || a.mode == ActMode::Hold)) a.mode = ActMode::Toggle;
    if (a.type == ActType::Screen && !place.empty()) {
        ScreenPlace sp;
        if (!ParseScreenPlace(place, sp)) throw std::runtime_error(ctx + ": screen place must be wrist_left, wrist_right, front or head");
        a.place = sp;
    }
    if (a.type == ActType::Mirror) {
        if (a.mode == ActMode::Tap || a.mode == ActMode::Hold) a.mode = ActMode::Toggle;
        if (a.mode != ActMode::Toggle && a.mode != ActMode::On && a.mode != ActMode::Off)
            throw std::runtime_error(ctx + ": headset_mirror mode must be toggle, on or off");
    }
    if (a.type == ActType::Offsets) {
        if (a.mode == ActMode::Tap || a.mode == ActMode::Hold || a.mode == ActMode::Toggle) a.mode = ActMode::Panel;
        if (a.mode != ActMode::Panel && a.mode != ActMode::Enable && a.mode != ActMode::On && a.mode != ActMode::Off &&
            a.mode != ActMode::Grab && a.mode != ActMode::Reset)
            throw std::runtime_error(ctx + ": controller_offsets mode must be panel, enable, on, off, grab or reset");
    }
    if (a.type == ActType::Layer && a.mode == ActMode::Tap) a.mode = ActMode::Toggle;
    if (j.contains("target")) {
        std::string tg = j["target"].get<std::string>();
        if (!ParseInput(tg, a.target)) throw std::runtime_error(ctx + ": bad target input '" + tg + "'");
    }
    if (a.type == ActType::XrInput && !a.target.valid()) throw std::runtime_error(ctx + ": xr_input needs a target");
    if (a.type == ActType::Gamepad) {
        Get(j, "button", a.pad);
        a.pad = Lower(a.pad);
        if (!Gamepad::IsButton(a.pad)) throw std::runtime_error(ctx + ": unknown gamepad button '" + a.pad + "'");
    }
    return a;
}

static void ParsePose(const json& j, FreePose& fp) {
    if (!j.is_array() || j.size() != 7) return;
    for (auto& v : j)
        if (!v.is_number()) return;
    fp.set = true;
    fp.p.position = { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
    XrQuaternionf q{ j[3].get<float>(), j[4].get<float>(), j[5].get<float>(), j[6].get<float>() };
    float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l < 1e-4f) {
        fp.set = false;
        return;
    }
    fp.p.orientation = { q.x / l, q.y / l, q.z / l, q.w / l };
}
static json PoseJson(const FreePose& fp) {
    auto r = [](float v, float k) {
        return std::round(v * k) / k;
    };
    const auto& p = fp.p.position;
    const auto& q = fp.p.orientation;
    return json::array({ r(p.x, 1000), r(p.y, 1000), r(p.z, 1000), r(q.x, 10000), r(q.y, 10000), r(q.z, 10000), r(q.w, 10000) });
}
static void ParsePoses(const json& j, std::map<std::string, FreePose>& m) {
    if (!j.is_object()) return;
    for (auto& [k, v] : j.items()) {
        FreePose fp;
        ParsePose(v, fp);
        if (fp.set) m[Lower(k)] = fp;
    }
}

static void ParseStyle(const json& j, Style& s) {
    std::string anchor;
    Get(j, "anchor", anchor);
    if (!anchor.empty() && !ParseEnum(kWheelAnchorNames, Lower(anchor), s.anchor)) s.anchor = WheelAnchor::Head;
    Get(j, "radius_m", s.radius);
    Get(j, "head_distance_m", s.distance);
    Get(j, "head_down_m", s.headDown);
    Get(j, "hand_up_m", s.handUp);
    Get(j, "hand_toward_head_m", s.handToward);
    Get(j, "inner_ratio", s.innerRatio);
    Get(j, "gap_deg", s.gapDeg);
    Get(j, "font", s.font);
    Get(j, "font_scale", s.fontScale);
    Get(j, "texture_px", s.texSize);
    if (j.contains("poses")) ParsePoses(j["poses"], s.poses);
    s.texSize = std::clamp(s.texSize, 256, 2048);
    if (j.contains("colors")) {
        auto& c = j["colors"];
        s.bg = ParseColor(c.value("background", json()), s.bg);
        s.segment = ParseColor(c.value("segment", json()), s.segment);
        s.hover = ParseColor(c.value("hover", json()), s.hover);
        s.text = ParseColor(c.value("text", json()), s.text);
        s.accent = ParseColor(c.value("accent", json()), s.accent);
        s.border = ParseColor(c.value("border", json()), s.border);
        s.toggled = ParseColor(c.value("toggled_on", json()), s.toggled);
    }
}

static void ParseSel(const json& j, Selection& s) {
    std::string method;
    Get(j, "method", method);
    if (!method.empty() && !ParseEnum(kSelectMethodNames, Lower(method), s.method)) s.method = SelectMethod::Thumbstick;
    std::string hand;
    Get(j, "hand", hand);
    if (!hand.empty() && !ParseEnum(kSelectHandNames, Lower(hand), s.hand)) s.hand = SelectHand::Opener;
    Get(j, "deadzone", s.deadzone);
    if (j.contains("confirm_button")) s.confirmComp = ParseCompName(j["confirm_button"].get<std::string>());
    if (j.contains("cancel_button")) s.cancelComp = ParseCompName(j["cancel_button"].get<std::string>());
    Get(j, "submenu_dwell_ms", s.submenuDwellMs);
    Get(j, "close_after_select", s.closeAfterSelect);
    Get(j, "auto_close_ms", s.autoCloseMs);
}

// ---------- profile sections: each reads one part of the profile and clamps it to sane values ----------
static void ParsePointer(const json& k, PointerStyle& P) {
    std::string correction;
    Get(k, "correction", correction);
    if (!correction.empty() && !ParseEnum(kLaserCorrectionNames, Lower(correction), P.correction)) P.correction = LaserCorrection::Auto;
    Get(k, "pitch_down_deg", P.pitchDeg);
    Get(k, "down_m", P.downM);
    Get(k, "forward_m", P.forwardM);
    Get(k, "finger", P.finger);
    Get(k, "fingertip_forward_m", P.tipForwardM);
    Get(k, "fingertip_up_m", P.tipUpM);
    Get(k, "fingertip_inward_m", P.tipInwardM);
    Get(k, "guide_laser", P.guide);
    Get(k, "smoothing", P.smoothing);
    P.smoothing = std::clamp(P.smoothing, 0.f, 1.f);
    P.pitchDeg = std::clamp(P.pitchDeg, -45.f, 45.f);
    P.downM = std::clamp(P.downM, -0.1f, 0.1f);
    P.forwardM = std::clamp(P.forwardM, -0.1f, 0.1f);
    P.tipForwardM = std::clamp(P.tipForwardM, -0.1f, 0.15f);
    P.tipUpM = std::clamp(P.tipUpM, -0.1f, 0.1f);
    P.tipInwardM = std::clamp(P.tipInwardM, -0.1f, 0.1f);
}

static void ParseControllerOffsets(const json& k, OffsetStyle& O) {
    Get(k, "enabled", O.enabled);
    Get(k, "link_hands", O.linkHands);
    Get(k, "aim_same_as_grip", O.aimSame);
    Get(k, "smoothing", O.smoothing);
    O.smoothing = std::clamp(O.smoothing, 0.f, 1.f);
    const char* hn[2] = { "left", "right" };
    const char* kn[2] = { "grip", "aim" };
    for (int h = 0; h < 2; h++) {
        if (!k.contains(hn[h]) || !k[hn[h]].is_object()) continue;
        for (int c = 0; c < 2; c++) {
            if (!k[hn[h]].contains(kn[c])) continue;
            auto& e = k[hn[h]][kn[c]];
            HandOffset& o = O.o[h][c];
            Get(e, "pitch_deg", o.pitch);
            Get(e, "yaw_deg", o.yaw);
            Get(e, "roll_deg", o.roll);
            float cm[3] = { 0, 0, 0 };
            Get(e, "x_cm", cm[0]);
            Get(e, "y_cm", cm[1]);
            Get(e, "z_cm", cm[2]);
            o.pitch = std::clamp(o.pitch, -90.f, 90.f);
            o.yaw = std::clamp(o.yaw, -90.f, 90.f);
            o.roll = std::clamp(o.roll, -180.f, 180.f);
            o.x = std::clamp(cm[0], -30.f, 30.f) / 100.f;
            o.y = std::clamp(cm[1], -30.f, 30.f) / 100.f;
            o.z = std::clamp(cm[2], -30.f, 30.f) / 100.f;
        }
    }
}

static void ParseKeyboard(const json& k, KbStyle& K) {
    Get(k, "width_m", K.width);
    Get(k, "distance_m", K.distance);
    Get(k, "below_eyes_m", K.below);
    Get(k, "close_on_enter", K.closeOnEnter);
    K.width = std::clamp(K.width, 0.2f, 1.5f);
    if (k.contains("pose")) ParsePose(k["pose"], K.pose);
}

static void ParseDock(const json& d, DockStyle& D) {
    std::string m;
    Get(d, "mode", m);
    D.mode = m == "screen_to_keyboard"   ? DockMode::ScreenOnKeys
                   : m == "keyboard_to_screen" ? DockMode::KeysUnderScreen
                                               : DockMode::Apart;
    if (d.contains("pose")) ParsePose(d["pose"], D.pose);
    Get(d, "width_m", D.width);
    D.width = std::clamp(D.width, 0.f, 3.f);
}

static void ParseScreen(const json& k, ScreenStyle& S) {
    Get(k, "monitor", S.monitor);
    Get(k, "fps", S.fps);
    std::string source;
    Get(k, "source", source);
    S.source = Lower(source) == "window" ? ScreenSource::Window : ScreenSource::Monitor;
    Get(k, "window", S.window);
    Get(k, "window_title", S.windowTitle);
    if (k.contains("fps_steps") && k["fps_steps"].is_array()) {
        S.fpsSteps.clear();
        for (auto& v : k["fps_steps"])
            if (v.is_number()) S.fpsSteps.push_back(std::clamp(v.get<int>(), 0, 240));
        if (S.fpsSteps.empty()) S.fpsSteps = { 10, 15, 30, 60, 0 };
    }
    std::string place;
    Get(k, "place", place);
    if (!ParseScreenPlace(Lower(place), S.place)) S.place = ScreenPlace::WristLeft;
    Get(k, "wrist_width_m", S.wristWidth);
    Get(k, "toward_elbow_m", S.towardElbow);
    Get(k, "lift_m", S.lift);
    Get(k, "side_m", S.side);
    Get(k, "rotate_deg", S.rotateDeg);
    std::string face;
    Get(k, "face", face);
    if (!face.empty() && !ParseEnum(kArmSideNames, Lower(face), S.face)) S.face = ArmSide::Back;
    Get(k, "width_m", S.width);
    Get(k, "distance_m", S.distance);
    Get(k, "below_eyes_m", S.below);
    Get(k, "max_width_px", S.maxPx);
    Get(k, "opacity", S.opacity);
    Get(k, "cursor", S.cursor);
    Get(k, "start_on", S.startOn);
    Get(k, "interact", S.interact);
    Get(k, "refocus_game", S.refocus);
    Get(k, "bar_scale", S.barScale);
    S.barScale = std::clamp(S.barScale, 0.3f, 2.f);
    Get(k, "smoothing", S.smoothing);
    S.smoothing = std::clamp(S.smoothing, 0.f, 1.f);
    if (k.contains("poses")) ParsePoses(k["poses"], S.poses);
    S.monitor = std::clamp(S.monitor, 0, 16);
    S.fps = std::clamp(S.fps, 0, 240);
    S.wristWidth = std::clamp(S.wristWidth, 0.04f, 0.6f);
    S.width = std::clamp(S.width, 0.1f, 4.f);
    S.distance = std::clamp(S.distance, 0.2f, 5.f);
    S.opacity = std::clamp(S.opacity, 0.1f, 1.f);
    S.maxPx = std::clamp(S.maxPx, 0, 7680);
}

static void ParseBindings(const json& list, std::vector<Binding>& out) {
    int idx = 0;
    for (auto& b : list) {
        Binding B;
        Get(b, "name", B.name);
        std::string ctx = "binding " + std::to_string(idx++) + " '" + B.name + "'";
        Get(b, "enabled", B.enabled);
        std::string in = b.value("input", "");
        B.chordText = in;
        for (auto& part : Split(in, '+')) {
            InputId id;
            if (!ParseInput(part, id)) throw std::runtime_error(ctx + ": bad input '" + part + "'");
            B.chord.push_back(id);
        }
        if (B.chord.empty()) throw std::runtime_error(ctx + ": no input");
        std::string gesture;
        Get(b, "gesture", gesture);
        if (!gesture.empty() && !ParseEnum(kGestureNames, Lower(gesture), B.gesture))
            throw std::runtime_error(ctx + ": unknown gesture '" + gesture + "'");
        Get(b, "hold_ms", B.holdMs);
        Get(b, "press_at", B.pressAt);
        Get(b, "release_at", B.releaseAt);
        B.releaseAt = B.releaseAt <= 0 ? 0.f : std::clamp(B.releaseAt, 0.05f, 0.9f);
        B.pressAt = B.pressAt <= 0 ? 0.f : std::clamp(B.pressAt, 0.02f, 0.98f);
        Get(b, "tap_ms", B.tapMs);
        Get(b, "double_tap_ms", B.doubleTapMs);
        B.layers = StrList(b, "layers");
        B.notLayers = StrList(b, "not_layers");
        std::string block;
        Get(b, "block", block);
        if (!block.empty() && !ParseEnum(kBindingBlockNames, Lower(block), B.block)) B.block = BindingBlock::None;
        Get(b, "tap_passthrough", B.tapPassthrough);
        B.action = ParseAction(b.value("action", json::object()), ctx);
        if (b.contains("tap_action") && b["tap_action"].is_object()) {
            B.hasTapAction = true;
            B.tapAction = ParseAction(b["tap_action"], ctx);
        }
        B.sourceIndex = (int)out.size();
        out.push_back(std::move(B));
    }
}

// A wheel or a wrist panel page: its items (switched-off items skipped, at most 16).
// where = how errors name it, e.g. "wheel 'main'" or "left wrist page 'tools'".
static Wheel ParseItemList(const std::string& name, const json& w, const std::string& where) {
    Wheel W;
    W.name = name;
    W.title = w.value("title", name);
    int idx = 0;
    for (auto& it : w.value("items", json::array())) {
        if (!it.value("enabled", true)) { // switched off in the editor
            idx++;
            continue;
        }
        WheelItem I;
        I.sourceIndex = idx;
        Get(it, "label", I.label);
        Get(it, "description", I.description);
        Get(it, "icon", I.icon);
        if (it.contains("color")) {
            I.color = ParseColor(it["color"], 0);
            I.hasColor = I.color != 0;
        }
        I.action = ParseAction(it.value("action", json::object()), where + " item " + std::to_string(idx++));
        W.items.push_back(std::move(I));
    }
    if (W.items.size() > 16) throw std::runtime_error(where + " has more than 16 items");
    return W;
}

// wheels: for older profiles that pointed the panel at a wheel instead of having pages.
static void ParseWrist(const json& w, WristStyle& W, const std::string& tag, const std::map<std::string, Wheel>& wheels) {
    Get(w, "enabled", W.enabled);
    Get(w, "mirror", W.mirror);
    Get(w, "start_page", W.start);
    if (w.contains("pose")) ParsePose(w["pose"], W.pose);
    if (w.contains("pages"))
        for (auto& [name, pg] : w["pages"].items())
            W.pages[name] = ParseItemList(name, pg, tag + " page '" + name + "'");
    // Older profiles pointed the panel at a wheel: use that wheel as the only page.
    if (W.pages.empty() && w.contains("wheel")) {
        std::string wn = w["wheel"].get<std::string>();
        if (wheels.count(wn)) {
            W.pages[wn] = wheels.at(wn);
            W.start = wn;
        }
    }
    if (!W.pages.count(W.start) && !W.pages.empty() && !w.contains("start_page")) W.start = W.pages.begin()->first;
    if (W.enabled && !W.mirror && !W.pages.count(W.start)) {
        if (W.pages.empty()) throw std::runtime_error(tag + ": no pages");
        throw std::runtime_error(tag + ": first page '" + W.start + "' does not exist");
    }
    std::string face;
    Get(w, "face", face);
    bool faceOk = face.empty() || ParseEnum(kArmSideNames, Lower(face), W.face);
    std::string textUp;
    Get(w, "text_up", textUp);
    if (!textUp.empty() && !ParseEnum(kTextUpNames, Lower(textUp), W.textUp))
        throw std::runtime_error(tag + ": text_up must be fingers, between, along_arm or custom");
    Get(w, "rotate_deg", W.rotateDeg);
    Get(w, "size_m", W.size);
    Get(w, "toward_elbow_m", W.towardElbow);
    Get(w, "lift_m", W.lift);
    Get(w, "side_m", W.side);
    Get(w, "columns", W.columns);
    Get(w, "facing_deg", W.facingDeg);
    Get(w, "gaze_deg", W.gazeDeg);
    Get(w, "show_ms", W.showMs);
    Get(w, "hide_ms", W.hideMs);
    std::string press;
    Get(w, "press", press);
    bool pressOk = press.empty() || ParseEnum(kPressMethodNames, Lower(press), W.press);
    W.size = std::clamp(W.size, 0.05f, 0.4f);
    W.columns = std::clamp(W.columns, 1, 4);
    W.facingDeg = std::clamp(W.facingDeg, 5.f, 80.f);
    W.gazeDeg = std::clamp(W.gazeDeg, 5.f, 60.f);
    if (!faceOk) throw std::runtime_error(tag + ": face must be inner or back");
    if (!pressOk) throw std::runtime_error(tag + ": press must be poke, point or both");
}

// Every wheel and page an action names must exist; 'page' actions only on wrist panel buttons.
static void CheckReferences(const Profile& P) {
    auto checkA = [&](const Action& a, const std::string& ctx) {
        if (a.type == ActType::Wheel && !P.wheels.count(a.wheel))
            throw std::runtime_error(ctx + ": wheel '" + a.wheel + "' does not exist");
    };
    for (auto& b : P.bindings)
        checkA(b.action, "binding '" + b.name + "'");
    for (auto& b : P.bindings)
        if (b.action.type == ActType::Page) throw std::runtime_error("binding '" + b.name + "': 'page' only works on wrist panel buttons");
    for (auto& [n, w] : P.wheels)
        for (auto& i : w.items)
            if (i.action.type == ActType::Page)
                throw std::runtime_error("wheel '" + n + "' item '" + i.label + "': 'page' only works on wrist panel buttons");
    for (int h = 0; h < 2; h++) {
        const char* tag = h ? "right wrist" : "left wrist";
        for (auto& [n, pg] : P.wrist[h].pages)
            for (auto& i : pg.items) {
                checkA(i.action, std::string(tag) + " page '" + n + "' item '" + i.label + "'");
                if (i.action.type == ActType::Page && !P.wrist[h].pages.count(i.action.page))
                    throw std::runtime_error(std::string(tag) + " page '" + n + "' item '" + i.label + "': page '" + i.action.page +
                                             "' does not exist");
            }
    }
    for (auto& [n, w] : P.wheels)
        for (auto& i : w.items)
            checkA(i.action, "wheel '" + n + "' item '" + i.label + "'");
}

static std::shared_ptr<Profile> ParseProfile(const json& j) {
    auto p = std::make_shared<Profile>();
    Get(j, "enabled", p->enabled);
    std::string block;
    Get(j, "block_while_open", block);
    if (!block.empty() && !ParseEnum(kBlockWhileOpenNames, Lower(block), p->blockWhileOpen)) p->blockWhileOpen = BlockWhileOpen::None;
    if (j.contains("thresholds")) {
        auto& t = j["thresholds"];
        Get(t, "press_on", p->pressOn);
        Get(t, "press_off", p->pressOff);
        Get(t, "stick_on", p->stickOn);
        Get(t, "stick_off", p->stickOff);
    }
    if (j.contains("sticks") && j["sticks"].is_object()) { // { "left": {...}, "right": {...} }
        const char* hands[2] = { "left", "right" };
        for (int h = 0; h < 2; h++) {
            if (!j["sticks"].contains(hands[h])) continue;
            const auto& k = j["sticks"][hands[h]];
            StickShape& s = p->sticks[h];
            Get(k, "inner_deadzone", s.innerDeadzone);
            Get(k, "outer_deadzone", s.outerDeadzone);
            Get(k, "direction_angle", s.directionAngleDeg);
            Get(k, "ignore_outside", s.ignoreOutside);
            Get(k, "ignore_center", s.ignoreCenter);
            s.ignoreCenter = std::clamp(s.ignoreCenter, 0.f, 0.95f);
            s.innerDeadzone = std::clamp(s.innerDeadzone, 0.f, 0.9f);
            s.outerDeadzone = std::clamp(s.outerDeadzone, 0.f, 0.9f);
            s.directionAngleDeg = s.directionAngleDeg <= 0 ? 0.f : std::clamp(s.directionAngleDeg, 5.f, 45.f);
        }
    }
    Get(j, "haptics", p->haptics);
    Get(j, "haptic_strength", p->hapticAmp);
    Get(j, "notifications", p->notifications);
    Get(j, "focus_game_window", p->focusWindow);
    if (j.contains("style")) ParseStyle(j["style"], p->style);
    if (j.contains("selection")) ParseSel(j["selection"], p->sel);
    if (j.contains("pointer")) ParsePointer(j["pointer"], p->pointer);
    if (j.contains("controller_offsets")) ParseControllerOffsets(j["controller_offsets"], p->offsets);
    if (j.contains("keyboard")) ParseKeyboard(j["keyboard"], p->kb);
    if (j.contains("dock")) ParseDock(j["dock"], p->dock);
    if (j.contains("screen")) ParseScreen(j["screen"], p->screen);
    if (j.contains("bindings")) ParseBindings(j["bindings"], p->bindings);
    if (j.contains("wheels"))
        for (auto& [name, w] : j["wheels"].items())
            p->wheels[name] = ParseItemList(name, w, "wheel '" + name + "'");
    if (j.contains("modes") && j["modes"].is_object())
        for (auto& [name, m] : j["modes"].items()) {
            if (!m.is_object()) continue;
            Profile::ModeInfo mi;
            mi.color = ParseColor(m.value("color", json()), mi.color);
            mi.together = m.value("together", false);
            p->modes[Lower(name)] = mi;
        }

    // Wrist panels come after the wheels: older profiles use a wheel as the panel's page.
    if (j.contains("wrist_panel")) { // older profiles: one panel with a "hand"
        auto& w = j["wrist_panel"];
        int h = Lower(w.value("hand", std::string("left"))) == "right" ? 1 : 0;
        ParseWrist(w, p->wrist[h], h ? "right wrist" : "left wrist", p->wheels);
    }
    if (j.contains("wrist_left")) {
        p->wrist[0] = WristStyle{};
        ParseWrist(j["wrist_left"], p->wrist[0], "left wrist", p->wheels);
    }
    if (j.contains("wrist_right")) {
        p->wrist[1] = WristStyle{};
        ParseWrist(j["wrist_right"], p->wrist[1], "right wrist", p->wheels);
    }
    if (p->wrist[0].mirror && p->wrist[1].mirror)
        throw std::runtime_error("the left and right wrist panels mirror each other; untick mirror on one of them");
    for (int h = 0; h < 2; h++) {
        const WristStyle& o = p->wrist[1 - h];
        if (p->wrist[h].enabled && p->wrist[h].mirror && !o.pages.count(o.start))
            throw std::runtime_error(std::string(h ? "right" : "left") + " wrist mirrors the other panel, but that one has no pages");
    }
    CheckReferences(*p);
    return p;
}

static bool ReadFile(const std::wstring& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Write a whole file safely: a temp file next to it, then swapped in. A crash or a reader (the editor,
// another game) never sees a half-written profile.
static bool WriteFileAtomic(const std::wstring& path, const std::string& text) {
    std::wstring tmp = path + L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << text;
        f.flush();
        if (!f) return false;
    }
    if (MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(tmp.c_str());
    return false;
}

// default.json is the STANDARD profile: rewritten from the built-in one, so every release brings its new
// defaults. The user's own changes live in user_defaults.json, which wins over default.json.
// Once: an older default.json that differs from the built-in one holds the user's edits -> user_defaults.json.
void SetActiveProfileName(const std::string& n);
static bool Exists(const std::wstring& p) {
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}
void PrepareDefaults() {
    std::wstring dir = ProfilesDir();
    std::wstring def = dir + L"\\default.json", user = dir + L"\\user_defaults.json", marker = dir + L"\\.defaults_split";
    std::string text;
    if (!Exists(marker)) {
        if (Exists(def) && !Exists(user) && ReadFile(def, text) && text != DefaultProfileJson()) {
            if (MoveFileW(def.c_str(), user.c_str())) {
                Log("defaults: your default.json had changes, kept as user_defaults.json (and selected)");
                SetActiveProfileName("user_defaults.json");
                std::ofstream n(dir + L"\\.defaults_moved_notice", std::ios::binary);
                n << "show once in the editor"; // the editor tells the user, then deletes it
            }
        }
        std::ofstream m(marker, std::ios::binary);
        m << "default.json = standard, user_defaults.json = yours";
    }
    text.clear();
    if (!ReadFile(def, text) || text != DefaultProfileJson()) WriteFileAtomic(def, DefaultProfileJson());
}

// The profile the user selected in the editor: games always use it. Kept in <data>\\active_profile.txt (a file name).
std::wstring ActiveProfilePath() {
    return DataDir() + L"\\active_profile.txt";
}
std::string ActiveProfileName() {
    std::string t;
    if (!ReadFile(ActiveProfilePath(), t)) return "";
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' '))
        t.pop_back();
    if (t.find_first_of("\\/:") != std::string::npos || t.size() < 6 || _stricmp(t.c_str() + t.size() - 5, ".json") != 0)
        return ""; // a plain file name only
    return t;
}
void SetActiveProfileName(const std::string& n) {
    WriteFileAtomic(ActiveProfilePath(), n);
}

// Where a save by the layer goes: never into the standard default.json (copied to user_defaults.json first).
// Never into the standard: user_defaults.json, or a new "user_defaults N.json" when that already exists
// (the user's own is never replaced). The new profile becomes the selected one.
static std::wstring SaveTarget(const Profile& p) {
    std::wstring dir = ProfilesDir();
    if (_stricmp(p.sourceName.c_str(), "default.json") == 0) {
        std::string name = "user_defaults.json";
        for (int i = 2; Exists(dir + L"\\" + Utf8ToW(name)); i++)
            name = "user_defaults " + std::to_string(i) + ".json";
        CopyFileW((dir + L"\\default.json").c_str(), (dir + L"\\" + Utf8ToW(name)).c_str(), TRUE);
        p.sourceName = name;
        SetActiveProfileName(name);
        Log("defaults: a change made in the headset was saved to %s (default.json stays standard)", name.c_str());
    }
    return dir + L"\\" + Utf8ToW(p.sourceName);
}

std::shared_ptr<Profile> LoadProfile(std::wstring& pathOut, std::string& err) {
    std::wstring dir = ProfilesDir();
    std::wstring game = dir + L"\\" + Utf8ToW(ExeName()) + L".json";
    std::wstring def = dir + L"\\default.json", user = dir + L"\\user_defaults.json";
    PrepareDefaults();
    // 1. the profile selected in the editor, 2. a profile named after the game, 3. the user's defaults, 4. the standard
    std::string act = ActiveProfileName();
    std::wstring active = act.empty() ? L"" : dir + L"\\" + Utf8ToW(act);
    pathOut = !active.empty() && Exists(active) ? active : Exists(game) ? game : Exists(user) ? user : def;
    std::string text;
    if (!ReadFile(pathOut, text)) {
        err = "cannot read " + WToUtf8(pathOut);
        return nullptr;
    }
    try {
        json j = json::parse(text, nullptr, true, true); // allow comments
        auto p = ParseProfile(j);
        auto s = pathOut.find_last_of(L'\\');
        p->sourceName = WToUtf8(pathOut.substr(s + 1));
        return p;
    } catch (const std::exception& e) {
        err = e.what();
        return nullptr;
    }
}

// Adjust mode: put the placement numbers back into the profile file the layer loaded.
bool SavePlacement(const Profile& p, std::string& err) {
    std::wstring dir = ProfilesDir();
    std::wstring path = SaveTarget(p);
    std::string text;
    if (!ReadFile(path, text)) {
        err = "cannot read " + WToUtf8(path);
        return false;
    }
    try {
        json j = json::parse(text, nullptr, true, true);
        auto r = [](float v) {
            return std::round(v * 1000.f) / 1000.f;
        }; // millimetres are plenty
        json& s = j["style"];
        s["radius_m"] = r(p.style.radius);
        s["hand_up_m"] = r(p.style.handUp);
        s["hand_toward_head_m"] = r(p.style.handToward);
        s["head_distance_m"] = r(p.style.distance);
        s["head_down_m"] = r(p.style.headDown);
        {
            json& pt = j["pointer"]; // the fingertips, moved by hand in Adjust mode
            pt["fingertip_forward_m"] = r(p.pointer.tipForwardM);
            pt["fingertip_up_m"] = r(p.pointer.tipUpM);
            pt["fingertip_inward_m"] = r(p.pointer.tipInwardM);
        }
        {
            json& sc = j["screen"];
            const ScreenStyle& S = p.screen;
            sc["wrist_width_m"] = r(S.wristWidth);
            sc["toward_elbow_m"] = r(S.towardElbow);
            sc["lift_m"] = r(S.lift);
            sc["side_m"] = r(S.side);
            sc["rotate_deg"] = std::round(S.rotateDeg * 10.f) / 10.f;
            sc["width_m"] = r(S.width);
            sc["distance_m"] = r(S.distance);
            sc["below_eyes_m"] = r(S.below);
            json poses = json::object();
            for (auto& [k, fp] : S.poses)
                if (fp.set) poses[k] = PoseJson(fp);
            if (poses.empty())
                sc.erase("poses");
            else
                sc["poses"] = poses;
        }
        {
            json poses = json::object();
            for (auto& [k, fp] : p.style.poses)
                if (fp.set) poses[k] = PoseJson(fp);
            if (poses.empty())
                s.erase("poses");
            else
                s["poses"] = poses;
        }
        const char* keys[2] = { "wrist_left", "wrist_right" };
        for (int h = 0; h < 2; h++) {
            if (!j.contains(keys[h])) continue;
            json& w = j[keys[h]];
            const WristStyle& W = p.wrist[h];
            w["size_m"] = r(W.size);
            w["toward_elbow_m"] = r(W.towardElbow);
            w["lift_m"] = r(W.lift);
            w["side_m"] = r(W.side);
            w["facing_deg"] = std::round(W.facingDeg);
            w["gaze_deg"] = std::round(W.gazeDeg);
            w["text_up"] = EnumKey(kTextUpNames, W.textUp);
            if (W.textUp == TextUp::Custom) w["rotate_deg"] = std::round(W.rotateDeg * 10.f) / 10.f;
            if (W.pose.set)
                w["pose"] = PoseJson(W.pose);
            else
                w.erase("pose");
        }
        {
            json& k = j["keyboard"];
            k["width_m"] = r(p.kb.width);
            if (p.kb.pose.set)
                k["pose"] = PoseJson(p.kb.pose);
            else
                k.erase("pose");
        }
        {
            json& d = j["dock"];
            d["mode"] = p.dock.mode == DockMode::ScreenOnKeys      ? "screen_to_keyboard"
                        : p.dock.mode == DockMode::KeysUnderScreen ? "keyboard_to_screen"
                                                                   : "none";
            if (p.dock.pose.set)
                d["pose"] = PoseJson(p.dock.pose);
            else
                d.erase("pose");
            if (p.dock.width > 0)
                d["width_m"] = r(p.dock.width);
            else
                d.erase("width_m");
        }
        if (!WriteFileAtomic(path, j.dump(2))) {
            err = "cannot write " + WToUtf8(path);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

bool SaveScreenChoice(const Profile& p, std::string& err) {
    std::wstring dir = ProfilesDir();
    std::wstring path = SaveTarget(p);
    std::string text;
    if (!ReadFile(path, text)) {
        err = "cannot read " + WToUtf8(path);
        return false;
    }
    try {
        json j = json::parse(text, nullptr, true, true);
        json& sc = j["screen"];
        const ScreenStyle& S = p.screen;
        sc["source"] = S.source == ScreenSource::Window ? "window" : "monitor";
        sc["monitor"] = S.monitor;
        if (S.source == ScreenSource::Window) {
            sc["window"] = S.window;
            sc["window_title"] = S.windowTitle;
        } else {
            sc.erase("window_title");
        }
        if (!WriteFileAtomic(path, j.dump(2))) {
            err = "cannot write " + WToUtf8(path);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

bool SaveOffsets(const Profile& p, std::string& err) {
    std::wstring path = SaveTarget(p);
    std::string text;
    if (!ReadFile(path, text)) {
        err = "cannot read " + WToUtf8(path);
        return false;
    }
    try {
        json j = json::parse(text, nullptr, true, true);
        const OffsetStyle& O = p.offsets;
        auto r1 = [](float v) {
            return std::round(v * 10.f) / 10.f;
        };
        auto r2 = [](float v) {
            return std::round(v * 100.f) / 100.f;
        };
        json k = json::object();
        k["enabled"] = O.enabled;
        k["link_hands"] = O.linkHands;
        k["aim_same_as_grip"] = O.aimSame;
        k["smoothing"] = r2(O.smoothing);
        const char* hn[2] = { "left", "right" };
        const char* kn[2] = { "grip", "aim" };
        for (int h = 0; h < 2; h++)
            for (int c = 0; c < 2; c++) {
                const HandOffset& o = O.o[h][c];
                k[hn[h]][kn[c]] = { { "pitch_deg", r1(o.pitch) }, { "yaw_deg", r1(o.yaw) },    { "roll_deg", r1(o.roll) },
                                    { "x_cm", r1(o.x * 100.f) },  { "y_cm", r1(o.y * 100.f) }, { "z_cm", r1(o.z * 100.f) } };
            }
        j["controller_offsets"] = k;
        if (!WriteFileAtomic(path, j.dump(2))) {
            err = "cannot write " + WToUtf8(path);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

// An action as the file holds it. The original is kept (its other fields stay) when it is the same type, else replaced.
static json ActionJson(const Action& a, const json& original) {
    static const EnumName<ActType> types[] = {
        { ActType::None, "none" },         { ActType::Key, "key" },         { ActType::Sequence, "sequence" },
        { ActType::XrInput, "xr_input" },  { ActType::Wheel, "wheel" },     { ActType::Back, "back" },
        { ActType::Close, "close" },       { ActType::Layer, "layer" },     { ActType::Reload, "reload" },
        { ActType::Keyboard, "keyboard" }, { ActType::Page, "page" },       { ActType::Adjust, "adjust" },
        { ActType::Screen, "screen" },     { ActType::Offsets, "controller_offsets" },
        { ActType::Mirror, "headset_mirror" }, { ActType::Menu, "settings_menu" }, { ActType::Gamepad, "gamepad" } };
    json o = original.is_object() ? original : json::object();
    Action was;
    try {
        if (!o.empty()) was = ParseAction(o, "");
    } catch (...) {
        o = json::object();
    }
    if (o.empty() || was.type != a.type) o = json::object();
    o["type"] = EnumKey(types, a.type);
    switch (a.type) {
    case ActType::Key:
        o["keys"] = a.keys;
        o["mode"] = ActModeKey(a.mode);
        break;
    case ActType::XrInput:
        o["target"] = InputName(a.target);
        o["mode"] = ActModeKey(a.mode);
        break;
    case ActType::Gamepad:
        o["button"] = a.pad;
        o["mode"] = ActModeKey(a.mode);
        break;
    case ActType::Sequence:
        o["steps"] = a.steps;
        break;
    case ActType::Wheel:
        o["wheel"] = a.wheel;
        break;
    case ActType::Layer:
        o["layer"] = a.layer;
        o["mode"] = ActModeKey(a.mode);
        break;
    case ActType::Page:
        o["page"] = a.page;
        break;
    case ActType::Screen:
    case ActType::Mirror:
    case ActType::Offsets:
        o["mode"] = ActModeKey(a.mode);
        if (a.type == ActType::Screen) {
            if (a.place)
                o["place"] = ScreenPlaceKey(*a.place);
            else
                o.erase("place");
        }
        break;
    default:
        break;
    }
    return o;
}

// A wheel or wrist page's items as the file holds them: each from its original entry (other fields kept: color,
// icon...), then the items switched off in the editor (not loaded, so not shown in the headset) at the end.
static json ItemListJson(const Wheel& w, const json& originalList) {
    json out = originalList.is_object() ? originalList : json::object();
    json old = out.contains("items") && out["items"].is_array() ? out["items"] : json::array();
    json items = json::array();
    for (const WheelItem& it : w.items) {
        bool hasOriginal = it.sourceIndex >= 0 && it.sourceIndex < (int)old.size() && old[it.sourceIndex].is_object();
        json o = hasOriginal ? old[it.sourceIndex] : json::object();
        o["label"] = it.label;
        if (!it.description.empty())
            o["description"] = it.description;
        else
            o.erase("description");
        o["action"] = ActionJson(it.action, o.value("action", json::object()));
        items.push_back(o);
    }
    for (auto& o : old)
        if (o.is_object() && !o.value("enabled", true)) items.push_back(o);
    out["title"] = w.title;
    out["items"] = items;
    return out;
}

// One binding as the file holds it: the original entry (keeps fields the menu doesn't edit) with the menu's fields set.
static json MenuBindingJson(const Binding& b, const json& original) {
    json o = original.is_object() ? original : json::object();
    o["name"] = b.name;
    o["enabled"] = b.enabled;
    o["input"] = b.chordText;
    o["gesture"] = EnumKey(kGestureNames, b.gesture);
    if (b.gesture == Gesture::Hold) o["hold_ms"] = b.holdMs;
    if (b.pressAt > 0) o["press_at"] = std::round(b.pressAt * 100) / 100;
    else o.erase("press_at");
    if (b.releaseAt > 0) o["release_at"] = std::round(b.releaseAt * 100) / 100;
    else o.erase("release_at");
    o["block"] = EnumKey(kBindingBlockNames, b.block);
    if (b.tapPassthrough)
        o["tap_passthrough"] = true;
    else
        o.erase("tap_passthrough");
    if (b.gesture == Gesture::Tap) o["tap_ms"] = b.tapMs;
    if (b.gesture == Gesture::DoubleTap || b.gesture == Gesture::DoubleTapHold) o["double_tap_ms"] = b.doubleTapMs;
    auto names = [](const std::vector<std::string>& l) {
        std::string t;
        for (auto& n : l) t += (t.empty() ? "" : ",") + n;
        return t;
    };
    o["layers"] = names(b.layers);
    o["not_layers"] = names(b.notLayers);
    o["action"] = ActionJson(b.action, o.value("action", json::object()));
    if (b.hasTapAction)
        o["tap_action"] = ActionJson(b.tapAction, o.value("tap_action", json::object()));
    else
        o.erase("tap_action");
    return o;
}

bool SaveMenuEdits(Profile& p, std::string& err) {
    std::wstring path = SaveTarget(p);
    std::string text;
    if (!ReadFile(path, text)) {
        err = "cannot read " + WToUtf8(path);
        return false;
    }
    try {
        json j = json::parse(text, nullptr, true, true);
        auto r2 = [](float v) {
            return std::round(v * 100.f) / 100.f;
        };
        json sticks = json::object();
        const char* hands[2] = { "left", "right" };
        for (int h = 0; h < 2; h++) {
            const StickShape& s = p.sticks[h];
            if (!s.ChangesGame() && s.directionAngleDeg <= 0) continue; // all off: not written
            sticks[hands[h]] = { { "inner_deadzone", r2(s.innerDeadzone) },
                                 { "outer_deadzone", r2(s.outerDeadzone) },
                                 { "direction_angle", std::round(s.directionAngleDeg) } };
            if (s.ignoreOutside) sticks[hands[h]]["ignore_outside"] = true;
            if (s.ignoreCenter > 0) sticks[hands[h]]["ignore_center"] = r2(s.ignoreCenter);
        }
        if (sticks.empty())
            j.erase("sticks");
        else
            j["sticks"] = sticks;
        j["thresholds"]["stick_on"] = r2(p.stickOn);
        j["thresholds"]["stick_off"] = r2(p.stickOff);
        json old = j.contains("bindings") && j["bindings"].is_array() ? j["bindings"] : json::array();
        json list = json::array();
        for (const Binding& b : p.bindings) {
            bool hasOriginal = b.sourceIndex >= 0 && b.sourceIndex < (int)old.size();
            list.push_back(MenuBindingJson(b, hasOriginal ? old[b.sourceIndex] : json()));
        }
        j["bindings"] = list;
        // wheels: each from its original (other fields kept); deleted ones go
        {
            json oldWheels = j.contains("wheels") && j["wheels"].is_object() ? j["wheels"] : json::object();
            json wheels = json::object();
            for (const auto& [name, w] : p.wheels)
                wheels[name] = ItemListJson(w, oldWheels.contains(name) ? oldWheels[name] : json());
            j["wheels"] = wheels;
        }
        // wrist panels: pages and the settings the menu shows
        const char* wk[2] = { "wrist_left", "wrist_right" };
        for (int h = 0; h < 2; h++) {
            const WristStyle& W = p.wrist[h];
            json& w = j[wk[h]];
            if (!w.is_object()) w = json::object();
            json oldPages = w.contains("pages") && w["pages"].is_object() ? w["pages"] : json::object();
            json pages = json::object();
            for (const auto& [name, pg] : W.pages)
                pages[name] = ItemListJson(pg, oldPages.contains(name) ? oldPages[name] : json());
            w["pages"] = pages;
            w.erase("wheel"); // older profiles: the pages replace it
            w["enabled"] = W.enabled;
            w["mirror"] = W.mirror;
            w["start_page"] = W.start;
            w["columns"] = W.columns;
            w["size_m"] = std::round(W.size * 1000.f) / 1000.f;
            w["face"] = EnumKey(kArmSideNames, W.face);
            w["press"] = EnumKey(kPressMethodNames, W.press);
            w["facing_deg"] = std::round(W.facingDeg);
            w["gaze_deg"] = std::round(W.gazeDeg);
            w["show_ms"] = W.showMs;
            w["hide_ms"] = W.hideMs;
        }
        if (j.contains("wrist_panel")) j.erase("wrist_panel"); // older profiles: now wrist_left / wrist_right
        {
            json& st = j["style"];
            st["anchor"] = EnumKey(kWheelAnchorNames, p.style.anchor);
            st["radius_m"] = std::round(p.style.radius * 1000.f) / 1000.f;
            st["font_scale"] = r2(p.style.fontScale);
            json& se = j["selection"];
            se["method"] = EnumKey(kSelectMethodNames, p.sel.method);
            se["hand"] = EnumKey(kSelectHandNames, p.sel.hand);
            se["deadzone"] = r2(p.sel.deadzone);
            se["close_after_select"] = p.sel.closeAfterSelect;
            j["block_while_open"] = EnumKey(kBlockWhileOpenNames, p.blockWhileOpen);
            j["haptics"] = p.haptics;
            j["haptic_strength"] = r2(p.hapticAmp);
            j["notifications"] = p.notifications;
            json& pt = j["pointer"];
            pt["smoothing"] = r2(p.pointer.smoothing);
            pt["finger"] = p.pointer.finger;
            pt["guide_laser"] = p.pointer.guide;
        }
        if (!WriteFileAtomic(path, j.dump(2))) {
            err = "cannot write " + WToUtf8(path);
            return false;
        }
        for (int i = 0; i < (int)p.bindings.size(); i++)
            p.bindings[i].sourceIndex = i;
        // the items' places in the file now: in order, the switched-off ones after them
        auto renumber = [](Wheel& w) {
            for (int i = 0; i < (int)w.items.size(); i++)
                w.items[i].sourceIndex = i;
        };
        for (auto& [n, w] : p.wheels)
            renumber(w);
        for (auto& W : p.wrist)
            for (auto& [n, pg] : W.pages)
                renumber(pg);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

std::wstring SteamFrameSettingsPath() {
    return DataDir() + L"\\steam_frame.json";
}
bool LoadSteamFrameNative() {
    std::string text;
    if (!ReadFile(SteamFrameSettingsPath(), text)) return false;
    try {
        json j = json::parse(text);
        return j.is_object() && j.value("native", false);
    } catch (...) {
        return false;
    }
}

std::wstring HeadSmoothSettingsPath() {
    return DataDir() + L"\\head_smoothing.json";
}
bool LoadHeadSmoothSettings(HeadSmoothSettings& h) {
    h = HeadSmoothSettings{};
    std::string text;
    if (!ReadFile(HeadSmoothSettingsPath(), text)) return false;
    try {
        json j = json::parse(text, nullptr, true, true);
        Get(j, "on", h.on);
        Get(j, "strength", h.strength);
        Get(j, "keep_on", h.keepOn);
        std::string m;
        Get(j, "mode", m);
        h.mode = m == "headset" ? 1 : 0;
        h.strength = std::clamp(h.strength, 0.f, 100.f);
        return true;
    } catch (...) {
        h = HeadSmoothSettings{};
        return false;
    }
}
void SwitchOffHeadSmoothing() {
    std::string text;
    if (!ReadFile(HeadSmoothSettingsPath(), text)) return;
    json j;
    try {
        j = json::parse(text, nullptr, true, true);
    } catch (...) {
        return;
    }
    j["on"] = false;
    WriteFileAtomic(HeadSmoothSettingsPath(), j.dump(2));
}
std::wstring MirrorSettingsPath() {
    return DataDir() + L"\\headset_mirror.json";
}
bool LoadMirrorSettings(MirrorSettings& m) {
    std::string text;
    if (!ReadFile(MirrorSettingsPath(), text)) return false;
    try {
        json j = json::parse(text, nullptr, true, true);
        Get(j, "on", m.on);
        Get(j, "on_top", m.onTop);
        Get(j, "zoom", m.zoom);
        Get(j, "fps", m.fps);
        std::string e = "right";
        Get(j, "eye", e);
        m.eye = Lower(e) == "left" ? 0 : 1;
        Get(j, "smoothing", m.smoothing);
        m.smoothing = std::clamp(m.smoothing, 0.f, 1.f);
        m.zoom = std::clamp(m.zoom, 1.f, 3.f);
        m.fps = std::clamp(m.fps, 0, 240);
        std::string q = "light";
        Get(j, "quality", q);
        m.light = Lower(q) != "full";
        Get(j, "both_eyes", m.bothEyes);
        std::string ctr = "gaze";
        Get(j, "center", ctr);
        m.centerOnGaze = Lower(ctr) != "middle";
        Get(j, "steady_room", m.steadyRoom);
        Get(j, "side_margin", m.sideMargin);
        m.sideMargin = std::clamp(m.sideMargin, 0.f, 0.3f);
        return true;
    } catch (...) {
        return false;
    }
}
bool SaveMirrorSettings(const MirrorSettings& m) {
    json j = json::object();
    std::string text;
    if (ReadFile(MirrorSettingsPath(), text)) {
        try {
            j = json::parse(text, nullptr, true, true);
        } catch (...) {
            j = json::object();
        }
    }
    j["on"] = m.on;
    j["eye"] = m.eye ? "right" : "left";
    j["zoom"] = std::round((double)m.zoom * 100.0) / 100.0; // double: a float 1.4 would be saved as 1.39999997
    j["fps"] = m.fps;
    j["smoothing"] = std::round((double)m.smoothing * 100.0) / 100.0;
    j["on_top"] = m.onTop;
    j["quality"] = m.light ? "light" : "full";
    j["both_eyes"] = m.bothEyes;
    j["center"] = m.centerOnGaze ? "gaze" : "middle";
    j["steady_room"] = m.steadyRoom;
    j["side_margin"] = std::round((double)m.sideMargin * 100.0) / 100.0;
    return WriteFileAtomic(MirrorSettingsPath(), j.dump(2));
}

const char* DefaultProfileJson() {
    // Generic starting profile: every feature switched on so new users can see what exists.
    // split in two raw strings (MSVC: max ~16 KB per literal); the compiler joins them
    return R"JSON({
  "bindings": [
    {
      "action": {
        "confirm": "stick_return",
        "tap_ms": 60,
        "type": "wheel",
        "wheel": "Right Hand Default"
      },
      "block": "none",
      "double_tap_ms": 400,
      "enabled": true,
      "gesture": "double_tap",
      "hold_ms": 400,
      "input": "right.thumbrest",
      "layers": "",
      "name": "Right hand wheel",
      "not_layers": "",
      "tap_ms": 300
    },
    {
      "action": {
        "confirm": "stick_return",
        "tap_ms": 60,
        "type": "wheel",
        "wheel": "Left Hand Default"
      },
      "block": "none",
      "double_tap_ms": 400,
      "enabled": true,
      "gesture": "double_tap",
      "hold_ms": 400,
      "input": "left.thumbrest",
      "layers": "",
      "name": "Left hand wheel",
      "not_layers": "",
      "tap_ms": 300
    },
    {
      "action": {
        "tap_ms": 60,
        "type": "keyboard"
      },
      "block": "trigger",
      "double_tap_ms": 350,
      "enabled": false,
      "gesture": "hold",
      "hold_ms": 600,
      "input": "left.menu",
      "layers": "",
      "name": "VR keyboard (hold left menu)",
      "not_layers": "",
      "tap_ms": 300
    },
    {
      "action": {
        "layer": "shift",
        "mode": "hold",
        "tap_ms": 60,
        "type": "layer"
      },
      "block": "trigger",
      "double_tap_ms": 350,
      "enabled": false,
      "gesture": "while_held",
      "hold_ms": 400,
      "input": "left.grip+left.thumbstick",
      "layers": "",
      "name": "Shift layer (hold left grip + click left stick)",
      "not_layers": "",
      "tap_ms": 300
    }
  ],
  "block_while_open": "hand",
  "controller_offsets": {
    "aim_same_as_grip": true,
    "enabled": true,
    "left": {
      "aim": {
        "pitch_deg": 0,
        "roll_deg": 0,
        "x_cm": 0,
        "y_cm": 0,
        "yaw_deg": 0,
        "z_cm": 0
      },
      "grip": {
        "pitch_deg": 0,
        "roll_deg": 0,
        "x_cm": 0,
        "y_cm": 0,
        "yaw_deg": 0,
        "z_cm": 0
      }
    },
    "link_hands": true,
    "right": {
      "aim": {
        "pitch_deg": 0,
        "roll_deg": 0,
        "x_cm": 0,
        "y_cm": 0,
        "yaw_deg": 0,
        "z_cm": 0
      },
      "grip": {
        "pitch_deg": 0,
        "roll_deg": 0,
        "x_cm": 0,
        "y_cm": 0,
        "yaw_deg": 0,
        "z_cm": 0
      }
    },
    "smoothing": 0.5
  },
  "dock": {
    "mode": "screen_to_keyboard"
  },
  "enabled": true,
  "focus_game_window": false,
  "haptic_strength": 0.35,
  "haptics": true,
  "keyboard": {
    "below_eyes_m": 0.3,
    "close_on_enter": false,
    "distance_m": 0.45,
    "pose": [
      -0.088,
      -0.201,
      -0.582,
      -0.2127,
      0.0663,
      0.0089,
      0.9748
    ],
    "width_m": 0.56
  },
  "notifications": true,
  "pointer": {
    "correction": "auto",
    "down_m": 0.02,
    "finger": true,
    "fingertip_forward_m": 0.015,
    "fingertip_inward_m": 0,
    "fingertip_up_m": 0,
    "forward_m": 0,
    "guide_laser": true,
    "pitch_down_deg": 15,
    "smoothing": 0
  },
  "screen": {
    "bar_scale": 0.7,
    "below_eyes_m": 0.05,
    "cursor": true,
    "distance_m": 0.9,
    "face": "back",
    "fps": 30,
    "fps_steps": [
      10,
      15,
      30,
      60,
      0
    ],
    "interact": true,
    "lift_m": 0.03,
    "max_width_px": 0,
    "monitor": 0,
    "opacity": 1,
    "place": "wrist_left",
    "poses": {
      "wrist_left": [
        -0.194,
        0.089,
        -0.147,
        0.6993,
        -0.1477,
        0.0746,
        -0.6954
      ]
    },
    "refocus_game": true,
    "rotate_deg": 0,
    "side_m": 0,
    "smoothing": 0,
    "source": "monitor",
    "start_on": false,
    "toward_elbow_m": 0.14,
    "width_m": 0.8,
    "wrist_width_m": 0.252
  },
  "selection": {
    "auto_close_ms": 0,
    "cancel_button": "secondary",
    "close_after_select": true,
    "confirm_button": "trigger",
    "deadzone": 0.5,
    "hand": "opener",
    "method": "both",
    "submenu_dwell_ms": 450
  },
  "style": {
    "anchor": "head_locked",
    "colors": {
      "accent": "#7FC4FFFF",
      "background": "#101418B0",
      "border": "#FFFFFF60",
      "hover": "#3C8CFFF0",
      "segment": "#283040D0",
      "text": "#FFFFFFFF",
      "toggled_on": "#2E9E5BE0"
    },
    "font": "Segoe UI",
    "font_scale": 1,
    "gap_deg": 1.5,
    "hand_toward_head_m": 0.06,
    "hand_up_m": 0.1,
    "head_distance_m": 0.7,
    "head_down_m": 0.1,
    "inner_ratio": 0.34,
    "radius_m": 0.127,
    "texture_px": 768
  },
  "thresholds": {
    "press_off": 0.4,
    "press_on": 0.6,
    "stick_off": 0.5,
    "stick_on": 0.7
  },
  "wheels": {
    "Left Hand Default": {
      "items": [
        {
          "action": {
            "keys": "LCtrl",
            "mode": "toggle",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Walk toggle"
        },
        {
          "action": {
            "keys": "C",
            "mode": "toggle",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Crouch toggle"
        },
        {
          "action": {
            "tap_ms": 60,
            "type": "keyboard"
          },
          "enabled": true,
          "label": "Keyboard"
        },
        {
          "action": {
            "steps": "Ctrl+C; wait 100; Ctrl+V",
            "tap_ms": 60,
            "type": "sequence"
          },
          "enabled": true,
          "label": "Copy + paste"
        },
        {
          "action": {
            "keys": "Enter",
            "mode": "tap",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Chat"
        },
        {
          "action": {
            "tap_ms": 60,
            "type": "reload"
          },
          "enabled": true,
          "label": "Reload profile"
        }
      ],
      "title": "Left Hand Default"
    },
    "Right Hand Default": {
      "items": [
        {
          "action": {
            "keys": "M",
            "mode": "tap",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Map"
        },
        {
          "action": {
            "keys": "I",
            "mode": "tap",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Inventory"
        },
        {
          "action": {
            "keys": "Esc",
            "mode": "tap",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Pause"
        },
        {
          "action": {
            "keys": "F12",
            "mode": "tap",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Screenshot"
        },
        {
          "action": {
            "keys": "F5",
            "mode": "tap",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Quick save"
        },
        {
          "action": {
            "tap_ms": 60,
            "type": "wheel",
            "wheel": "Right hand more (sub-wheel)"
          },
          "enabled": true,
          "label": "More..."
        }
      ],
      "title": "Right Hand Default"
    },
    "Right hand more (sub-wheel)": {
      "items": [
        {
          "action": {
            "tap_ms": 60,
            "type": "back"
          },
          "enabled": true,
          "label": "Back"
        },
        {
          "action": {
            "keys": "F9",
            "mode": "tap",
            "tap_ms": 60,
            "type": "key"
          },
          "enabled": true,
          "label": "Quick load"
        },
        {
          "action": {
            "tap_ms": 60,
            "type": "keyboard"
          },
          "enabled": true,
          "label": "Keyboard"
        },
        {
          "action": {
            "tap_ms": 60,
            "type": "adjust"
          },
          "enabled": true,
          "label": "Adjust positions"
        },
        {
          "action": {
            "tap_ms": 60,
            "type": "reload"
          },
          "enabled": true,
          "label": "Reload profile"
        }
      ],
      "title": "More"
    }
  },
  "wrist_left": {
    "columns": 3,
    "enabled": true,
    "face": "inner",
    "facing_deg": 35,
    "gaze_deg": 20,
    "hide_ms": 350,
    "lift_m": 0.013,
    "mirror": false,
    "pages": {
      "Left hand main": {
)JSON"
    R"JSON(        "items": [
          {
            "action": {
              "keys": "F5",
              "mode": "tap",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": true,
            "label": "Quick save / F5"
          },
          {
            "action": {
              "keys": "Esc",
              "mode": "tap",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": true,
            "label": "Pause / ESC"
          },
          {
            "action": {
              "mode": "toggle",
              "place": "wrist_left",
              "tap_ms": 60,
              "type": "screen"
            },
            "enabled": true,
            "label": "Screen Mirror"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "keyboard"
            },
            "enabled": true,
            "label": "Open VR Keyboard"
          },
          {
            "action": {
              "page": "Left hand more (sub-page)",
              "tap_ms": 60,
              "type": "page"
            },
            "color": "#00838FE0",
            "enabled": true,
            "label": "-> Configuration Page"
          },
          {
            "action": {
              "keys": "LCtrl",
              "mode": "toggle",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "L-Ctrl toggle"
          },
          {
            "action": {
              "keys": "LShift",
              "mode": "toggle",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "L-Shift toggle"
          },
          {
            "action": {
              "keys": "RMB",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "Right-click"
          }
        ],
        "title": "Left Hand Default"
      },
      "Left hand more (sub-page)": {
        "items": [
          {
            "action": {
              "keys": "F9",
              "mode": "tap",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": true,
            "label": "Quick load/F9"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "reload"
            },
            "enabled": true,
            "label": "Reload profile Companion "
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "adjust"
            },
            "enabled": true,
            "label": "Calibrate Menu Positions"
          },
          {
            "action": {
              "mode": "panel",
              "tap_ms": 60,
              "type": "controller_offsets"
            },
            "enabled": true,
            "label": "Controller offsets Configuration"
          },
          {
            "action": {
              "mode": "toggle",
              "tap_ms": 60,
              "type": "headset_mirror"
            },
            "enabled": true,
            "label": "VR mirror to Desktop"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "settings_menu"
            },
            "enabled": true,
            "label": "OpenXR Companion Ingame Editor"
          },
          {
            "action": {
              "keys": "F12",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "Screenshot"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "close"
            },
            "enabled": false,
            "label": "Hide"
          }
        ],
        "title": "Configuration Page"
      }
    },
    "pose": [
      0.001,
      -0.016,
      -0.103,
      0.6134,
      0.0369,
      -0.0098,
      -0.7889
    ],
    "press": "both",
    "rotate_deg": -145.4,
    "show_ms": 250,
    "side_m": 0.087,
    "size_m": 0.078,
    "start_page": "Left hand main",
    "text_up": "custom",
    "toward_elbow_m": 0.074
  },
  "wrist_right": {
    "columns": 3,
    "enabled": true,
    "face": "inner",
    "facing_deg": 35,
    "gaze_deg": 20,
    "hide_ms": 350,
    "lift_m": 0.022,
    "mirror": false,
    "pages": {
      "Left hand main (right)": {
        "items": [
          {
            "action": {
              "keys": "F5",
              "mode": "tap",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": true,
            "label": "Quick save / F5"
          },
          {
            "action": {
              "keys": "Esc",
              "mode": "tap",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": true,
            "label": "Pause / ESC"
          },
          {
            "action": {
              "mode": "toggle",
              "place": "wrist_right",
              "tap_ms": 60,
              "type": "screen"
            },
            "enabled": true,
            "label": "Screen Mirror"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "keyboard"
            },
            "enabled": true,
            "label": "Open VR Keyboard"
          },
          {
            "action": {
              "page": "Left hand more (sub-page) (right)",
              "tap_ms": 60,
              "type": "page"
            },
            "color": "#00838FE0",
            "enabled": true,
            "label": "-> Configuration Page"
          },
          {
            "action": {
              "keys": "LCtrl",
              "mode": "toggle",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "L-Ctrl toggle"
          },
          {
            "action": {
              "keys": "LShift",
              "mode": "toggle",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "L-Shift toggle"
          },
          {
            "action": {
              "keys": "RMB",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "Right-click"
          }
        ],
        "title": "Right Hand Default"
      },
      "Left hand more (sub-page) (right)": {
        "items": [
          {
            "action": {
              "keys": "F9",
              "mode": "tap",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": true,
            "label": "Quick load/F9"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "reload"
            },
            "enabled": true,
            "label": "Reload profile Companion "
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "adjust"
            },
            "enabled": true,
            "label": "Calibrate Menu Positions"
          },
          {
            "action": {
              "mode": "panel",
              "tap_ms": 60,
              "type": "controller_offsets"
            },
            "enabled": true,
            "label": "Controller offsets Configuration"
          },
          {
            "action": {
              "mode": "toggle",
              "tap_ms": 60,
              "type": "headset_mirror"
            },
            "enabled": true,
            "label": "VR mirror to Desktop"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "settings_menu"
            },
            "enabled": true,
            "label": "OpenXR Companion Ingame Editor"
          },
          {
            "action": {
              "keys": "F12",
              "tap_ms": 60,
              "type": "key"
            },
            "enabled": false,
            "label": "Screenshot"
          },
          {
            "action": {
              "tap_ms": 60,
              "type": "close"
            },
            "enabled": false,
            "label": "Hide"
          }
        ],
        "title": "Configuration Page"
      }
    },
    "pose": [
      0.001,
      -0.007,
      -0.095,
      -0.6468,
      -0.0626,
      0.0559,
      0.7581
    ],
    "press": "both",
    "rotate_deg": 140.3,
    "show_ms": 250,
    "side_m": -0.085,
    "size_m": 0.073,
    "start_page": "Left hand main (right)",
    "text_up": "custom",
    "toward_elbow_m": 0.06
  }
})JSON";
}

// Parses profile text with the same parser as the layer (used by the tests and the editor).
std::shared_ptr<Profile> ParseProfileText(const std::string& text, std::string& err) {
    try {
        return ParseProfile(json::parse(text, nullptr, true, true));
    } catch (const std::exception& e) {
        err = e.what();
        return nullptr;
    }
}

// Used by the editor: validate profile text with the exact parser the layer uses.
#include "keys.h"
extern "C" __declspec(dllexport) int vro_validate(const char* text, char* err, int errLen) {
    std::string msg;
    try {
        json j = json::parse(text, nullptr, true, true);
        auto p = ParseProfile(j);
        std::string e;
        for (auto& b : p->bindings) {
            if (b.action.type == ActType::Key && !Keys::Validate(b.action.keys, e))
                throw std::runtime_error("binding '" + b.name + "': " + e);
            if (b.action.type == ActType::Sequence && !Keys::ValidateSequence(b.action.steps, e))
                throw std::runtime_error("binding '" + b.name + "': " + e);
        }
        for (auto& [n, w] : p->wheels)
            for (auto& i : w.items) {
                if (i.action.type == ActType::Key && !Keys::Validate(i.action.keys, e))
                    throw std::runtime_error("wheel '" + n + "' item '" + i.label + "': " + e);
                if (i.action.type == ActType::Sequence && !Keys::ValidateSequence(i.action.steps, e))
                    throw std::runtime_error("wheel '" + n + "' item '" + i.label + "': " + e);
            }
        for (int h = 0; h < 2; h++)
            for (auto& [n, w] : p->wrist[h].pages)
                for (auto& i : w.items) {
                    if (i.action.type == ActType::Key && !Keys::Validate(i.action.keys, e))
                        throw std::runtime_error("wrist page '" + n + "' item '" + i.label + "': " + e);
                    if (i.action.type == ActType::Sequence && !Keys::ValidateSequence(i.action.steps, e))
                        throw std::runtime_error("wrist page '" + n + "' item '" + i.label + "': " + e);
                }
        if (errLen > 0) err[0] = 0;
        return 1;
    } catch (const std::exception& ex) {
        msg = ex.what();
    }
    if (errLen > 0) {
        strncpy_s(err, errLen, msg.c_str(), _TRUNCATE);
    }
    return 0;
}

extern "C" __declspec(dllexport) const char* vro_default_profile() {
    return DefaultProfileJson();
}
extern "C" __declspec(dllexport) void vro_prepare_defaults() {
    PrepareDefaults();
}

// Editor helper: check one key combo (seq=0) or macro step list (seq=1). Returns 1 if valid.
extern "C" __declspec(dllexport) int vro_check_keys(const char* text, int seq, char* err, int errLen) {
    std::string e;
    bool ok = seq ? Keys::ValidateSequence(text ? text : "", e) : Keys::Validate(text ? text : "", e);
    if (errLen > 0) strncpy_s(err, errLen, ok ? "" : e.c_str(), _TRUNCATE);
    return ok ? 1 : 0;
}

const char* ScreenPlaceKey(ScreenPlace p) {
    switch (p) {
    case ScreenPlace::WristLeft:
        return "wrist_left";
    case ScreenPlace::WristRight:
        return "wrist_right";
    case ScreenPlace::Front:
        return "front";
    case ScreenPlace::Head:
        return "head";
    }
    return "wrist_left";
}

bool ParseScreenPlace(const std::string& key, ScreenPlace& out) {
    for (ScreenPlace p : kScreenPlaces) {
        if (key == ScreenPlaceKey(p)) {
            out = p;
            return true;
        }
    }
    return false;
}

namespace {
struct ModeName {
    ActMode mode;
    const char* key;
};
const ModeName kModeNames[] = {
    { ActMode::Tap, "tap" },
    { ActMode::Hold, "hold" },
    { ActMode::Toggle, "toggle" },
    { ActMode::On, "on" },
    { ActMode::Off, "off" },
    { ActMode::NextMonitor, "next_monitor" },
    { ActMode::PrevMonitor, "prev_monitor" },
    { ActMode::NextFps, "next_fps" },
    { ActMode::NextPlace, "next_place" },
    { ActMode::NextWindow, "next_window" },
    { ActMode::PrevWindow, "prev_window" },
    { ActMode::Choose, "choose" },
    { ActMode::Panel, "panel" },
    { ActMode::Enable, "enable" },
    { ActMode::Grab, "grab" },
    { ActMode::Reset, "reset" },
};
} // namespace

ActMode ParseActMode(const std::string& text) {
    for (const auto& n : kModeNames)
        if (text == n.key) return n.mode;
    return ActMode::Unknown;
}

const char* ActModeKey(ActMode m) {
    for (const auto& n : kModeNames)
        if (n.mode == m) return n.key;
    return "unknown";
}
