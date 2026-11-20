#include "config.h"
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

int ParseCompName(const std::string& n) {
    std::string s = Lower(n);
    if (s == "a" || s == "x" || s == "primary" || s == "btn1") return C_BTN1;
    if (s == "b" || s == "y" || s == "secondary" || s == "btn2") return C_BTN2;
    if (s == "trigger") return C_TRIGGER;
    if (s == "grip" || s == "squeeze") return C_GRIP;
    if (s == "thumbstick" || s == "stick" || s == "trackpad") return C_STICK;
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
        else
            return false;
        if (r.feat >= F_UP && r.comp != C_STICK) return false;
        if (r.comp == C_THUMBREST) r.feat = F_TOUCH;
    }
    out = r;
    return true;
}

std::string InputName(const InputId& i) {
    if (!i.valid()) return "?";
    static const char* compR[] = { "a", "b", "trigger", "grip", "thumbstick", "thumbrest", "menu" };
    static const char* compL[] = { "x", "y", "trigger", "grip", "thumbstick", "thumbrest", "menu" };
    static const char* feat[] = { "", ".touch", ".up", ".down", ".left", ".right" };
    std::string s = i.hand == 0 ? "left." : "right.";
    s += i.hand == 0 ? compL[i.comp] : compR[i.comp];
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
    else if (t == "none" || t.empty())
        a.type = ActType::None;
    else
        throw std::runtime_error(ctx + ": unknown action type '" + t + "'");
    Get(j, "keys", a.keys);
    Get(j, "mode", a.mode);
    a.mode = Lower(a.mode);
    Get(j, "tap_ms", a.tapMs);
    Get(j, "steps", a.steps);
    Get(j, "wheel", a.wheel);
    Get(j, "page", a.page);
    Get(j, "confirm", a.confirm);
    a.confirm = Lower(a.confirm);
    Get(j, "layer", a.layer);
    std::string place;
    Get(j, "place", place);
    place = Lower(place);
    Get(j, "monitor", a.monitor);
    Get(j, "window", a.window);
    if (a.mode.empty()) a.mode = "tap";
    if (a.type == ActType::Screen && (a.mode == "tap" || a.mode == "hold")) a.mode = "toggle";
    if (a.type == ActType::Screen && !place.empty()) {
        ScreenPlace sp;
        if (!ParseScreenPlace(place, sp)) throw std::runtime_error(ctx + ": screen place must be wrist_left, wrist_right, front or head");
        a.place = sp;
    }
    if (a.type == ActType::Mirror) {
        if (a.mode == "tap" || a.mode == "hold") a.mode = "toggle";
        if (a.mode != "toggle" && a.mode != "on" && a.mode != "off")
            throw std::runtime_error(ctx + ": headset_mirror mode must be toggle, on or off");
    }
    if (a.type == ActType::Offsets) {
        if (a.mode == "tap" || a.mode == "hold" || a.mode == "toggle") a.mode = "panel";
        if (a.mode != "panel" && a.mode != "enable" && a.mode != "on" && a.mode != "off" && a.mode != "grab" && a.mode != "reset")
            throw std::runtime_error(ctx + ": controller_offsets mode must be panel, enable, on, off, grab or reset");
    }
    if (a.confirm.empty()) a.confirm = "auto";
    if (a.type == ActType::Layer && a.mode == "tap") a.mode = "toggle";
    if (j.contains("target")) {
        std::string tg = j["target"].get<std::string>();
        if (!ParseInput(tg, a.target)) throw std::runtime_error(ctx + ": bad target input '" + tg + "'");
    }
    if (a.type == ActType::XrInput && !a.target.valid()) throw std::runtime_error(ctx + ": xr_input needs a target");
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
    Get(j, "anchor", s.anchor);
    s.anchor = Lower(s.anchor);
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
    Get(j, "method", s.method);
    s.method = Lower(s.method);
    Get(j, "hand", s.hand);
    s.hand = Lower(s.hand);
    Get(j, "deadzone", s.deadzone);
    if (j.contains("confirm_button")) s.confirmComp = ParseCompName(j["confirm_button"].get<std::string>());
    if (j.contains("cancel_button")) s.cancelComp = ParseCompName(j["cancel_button"].get<std::string>());
    Get(j, "submenu_dwell_ms", s.submenuDwellMs);
    Get(j, "close_after_select", s.closeAfterSelect);
    Get(j, "auto_close_ms", s.autoCloseMs);
}

static std::shared_ptr<Profile> ParseProfile(const json& j) {
    auto p = std::make_shared<Profile>();
    Get(j, "enabled", p->enabled);
    Get(j, "block_while_open", p->blockWhileOpen);
    p->blockWhileOpen = Lower(p->blockWhileOpen);
    if (j.contains("thresholds")) {
        auto& t = j["thresholds"];
        Get(t, "press_on", p->pressOn);
        Get(t, "press_off", p->pressOff);
        Get(t, "stick_on", p->stickOn);
        Get(t, "stick_off", p->stickOff);
    }
    Get(j, "haptics", p->haptics);
    Get(j, "haptic_strength", p->hapticAmp);
    Get(j, "notifications", p->notifications);
    Get(j, "focus_game_window", p->focusWindow);
    if (j.contains("style")) ParseStyle(j["style"], p->style);
    if (j.contains("selection")) ParseSel(j["selection"], p->sel);
    if (j.contains("pointer")) {
        auto& k = j["pointer"];
        auto& P = p->pointer;
        Get(k, "correction", P.correction);
        P.correction = Lower(P.correction);
        if (P.correction != "auto" && P.correction != "always" && P.correction != "off") P.correction = "auto";
        Get(k, "pitch_down_deg", P.pitchDeg);
        Get(k, "down_m", P.downM);
        Get(k, "forward_m", P.forwardM);
        Get(k, "finger", P.finger);
        Get(k, "guide_laser", P.guide);
        Get(k, "smoothing", P.smoothing);
        P.smoothing = std::clamp(P.smoothing, 0.f, 1.f);
        P.pitchDeg = std::clamp(P.pitchDeg, -45.f, 45.f);
        P.downM = std::clamp(P.downM, -0.1f, 0.1f);
        P.forwardM = std::clamp(P.forwardM, -0.1f, 0.1f);
    }
    if (j.contains("controller_offsets")) {
        auto& k = j["controller_offsets"];
        auto& O = p->offsets;
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
    if (j.contains("keyboard")) {
        auto& k = j["keyboard"];
        Get(k, "width_m", p->kb.width);
        Get(k, "distance_m", p->kb.distance);
        Get(k, "below_eyes_m", p->kb.below);
        Get(k, "close_on_enter", p->kb.closeOnEnter);
        p->kb.width = std::clamp(p->kb.width, 0.2f, 1.5f);
        if (k.contains("pose")) ParsePose(k["pose"], p->kb.pose);
    }

    if (j.contains("dock")) {
        auto& d = j["dock"];
        std::string m;
        Get(d, "mode", m);
        p->dock.mode = m == "screen_to_keyboard"   ? DockMode::ScreenOnKeys
                       : m == "keyboard_to_screen" ? DockMode::KeysUnderScreen
                                                   : DockMode::Apart;
        if (d.contains("pose")) ParsePose(d["pose"], p->dock.pose);
        Get(d, "width_m", p->dock.width);
        p->dock.width = std::clamp(p->dock.width, 0.f, 3.f);
    }

    if (j.contains("screen")) {
        auto& k = j["screen"];
        auto& S = p->screen;
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
        Get(k, "face", S.face);
        S.face = Lower(S.face);
        if (S.face != "back" && S.face != "inner") S.face = "back";
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

    if (j.contains("bindings")) {
        int idx = 0;
        for (auto& b : j["bindings"]) {
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
            Get(b, "gesture", B.gesture);
            B.gesture = Lower(B.gesture);
            static const char* okG[] = { "press", "tap", "hold", "double_tap", "while_held" };
            if (std::find_if(std::begin(okG), std::end(okG), [&](const char* g) { return B.gesture == g; }) == std::end(okG))
                throw std::runtime_error(ctx + ": unknown gesture '" + B.gesture + "'");
            Get(b, "hold_ms", B.holdMs);
            Get(b, "tap_ms", B.tapMs);
            Get(b, "double_tap_ms", B.doubleTapMs);
            B.layers = StrList(b, "layers");
            B.notLayers = StrList(b, "not_layers");
            Get(b, "block", B.block);
            B.block = Lower(B.block);
            Get(b, "tap_passthrough", B.tapPassthrough);
            B.action = ParseAction(b.value("action", json::object()), ctx);
            p->bindings.push_back(std::move(B));
        }
    }
    if (j.contains("wheels")) {
        for (auto& [name, w] : j["wheels"].items()) {
            Wheel W;
            W.name = name;
            W.title = w.value("title", name);
            int idx = 0;
            for (auto& it : w.value("items", json::array())) {
                if (!it.value("enabled", true)) {
                    idx++;
                    continue;
                } // switched off in the editor
                WheelItem I;
                Get(it, "label", I.label);
                Get(it, "description", I.description);
                Get(it, "icon", I.icon);
                if (it.contains("color")) {
                    I.color = ParseColor(it["color"], 0);
                    I.hasColor = I.color != 0;
                }
                I.action = ParseAction(it.value("action", json::object()), "wheel '" + name + "' item " + std::to_string(idx++));
                W.items.push_back(std::move(I));
            }
            if (W.items.size() > 16) throw std::runtime_error("wheel '" + name + "' has more than 16 items");
            p->wheels[name] = std::move(W);
        }
    }
    auto parseWrist = [&](const json& w, WristStyle& W, const std::string& tag) {
        Get(w, "enabled", W.enabled);
        Get(w, "mirror", W.mirror);
        Get(w, "start_page", W.start);
        if (w.contains("pose")) ParsePose(w["pose"], W.pose);
        if (w.contains("pages"))
            for (auto& [name, pg] : w["pages"].items()) {
                Wheel P;
                P.name = name;
                P.title = pg.value("title", name);
                int idx = 0;
                for (auto& it : pg.value("items", json::array())) {
                    if (!it.value("enabled", true)) {
                        idx++;
                        continue;
                    }
                    WheelItem I;
                    Get(it, "label", I.label);
                    Get(it, "description", I.description);
                    Get(it, "icon", I.icon);
                    if (it.contains("color")) {
                        I.color = ParseColor(it["color"], 0);
                        I.hasColor = I.color != 0;
                    }
                    I.action = ParseAction(it.value("action", json::object()), tag + " page '" + name + "' item " + std::to_string(idx++));
                    P.items.push_back(std::move(I));
                }
                if (P.items.size() > 16) throw std::runtime_error(tag + " page '" + name + "' has more than 16 items");
                W.pages[name] = std::move(P);
            }
        // Older profiles pointed the panel at a wheel: use that wheel as the only page.
        if (W.pages.empty() && w.contains("wheel")) {
            std::string wn = w["wheel"].get<std::string>();
            if (p->wheels.count(wn)) {
                W.pages[wn] = p->wheels[wn];
                W.start = wn;
            }
        }
        if (!W.pages.count(W.start) && !W.pages.empty() && !w.contains("start_page")) W.start = W.pages.begin()->first;
        if (W.enabled && !W.mirror && !W.pages.count(W.start)) {
            if (W.pages.empty()) throw std::runtime_error(tag + ": no pages");
            throw std::runtime_error(tag + ": first page '" + W.start + "' does not exist");
        }
        Get(w, "face", W.face);
        W.face = Lower(W.face);
        Get(w, "text_up", W.textUp);
        W.textUp = Lower(W.textUp);
        if (W.textUp != "fingers" && W.textUp != "between" && W.textUp != "along_arm" && W.textUp != "custom")
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
        Get(w, "press", W.press);
        W.press = Lower(W.press);
        W.size = std::clamp(W.size, 0.05f, 0.4f);
        W.columns = std::clamp(W.columns, 1, 4);
        W.facingDeg = std::clamp(W.facingDeg, 5.f, 80.f);
        W.gazeDeg = std::clamp(W.gazeDeg, 5.f, 60.f);
        if (W.face != "inner" && W.face != "back") throw std::runtime_error(tag + ": face must be inner or back");
        if (W.press != "poke" && W.press != "point" && W.press != "both")
            throw std::runtime_error(tag + ": press must be poke, point or both");
    };
    if (j.contains("wrist_panel")) { // older profiles: one panel with a "hand"
        auto& w = j["wrist_panel"];
        int h = Lower(w.value("hand", std::string("left"))) == "right" ? 1 : 0;
        parseWrist(w, p->wrist[h], h ? "right wrist" : "left wrist");
    }
    if (j.contains("wrist_left")) {
        p->wrist[0] = WristStyle{};
        parseWrist(j["wrist_left"], p->wrist[0], "left wrist");
    }
    if (j.contains("wrist_right")) {
        p->wrist[1] = WristStyle{};
        parseWrist(j["wrist_right"], p->wrist[1], "right wrist");
    }
    if (p->wrist[0].mirror && p->wrist[1].mirror)
        throw std::runtime_error("the left and right wrist panels mirror each other; untick mirror on one of them");
    for (int h = 0; h < 2; h++) {
        const WristStyle& o = p->wrist[1 - h];
        if (p->wrist[h].enabled && p->wrist[h].mirror && !o.pages.count(o.start))
            throw std::runtime_error(std::string(h ? "right" : "left") + " wrist mirrors the other panel, but that one has no pages");
    }

    // Check references.
    auto checkA = [&](const Action& a, const std::string& ctx) {
        if (a.type == ActType::Wheel && !p->wheels.count(a.wheel))
            throw std::runtime_error(ctx + ": wheel '" + a.wheel + "' does not exist");
    };
    for (auto& b : p->bindings)
        checkA(b.action, "binding '" + b.name + "'");
    for (auto& b : p->bindings)
        if (b.action.type == ActType::Page) throw std::runtime_error("binding '" + b.name + "': 'page' only works on wrist panel buttons");
    for (auto& [n, w] : p->wheels)
        for (auto& i : w.items)
            if (i.action.type == ActType::Page)
                throw std::runtime_error("wheel '" + n + "' item '" + i.label + "': 'page' only works on wrist panel buttons");
    for (int h = 0; h < 2; h++) {
        const char* tag = h ? "right wrist" : "left wrist";
        for (auto& [n, pg] : p->wrist[h].pages)
            for (auto& i : pg.items) {
                checkA(i.action, std::string(tag) + " page '" + n + "' item '" + i.label + "'");
                if (i.action.type == ActType::Page && !p->wrist[h].pages.count(i.action.page))
                    throw std::runtime_error(std::string(tag) + " page '" + n + "' item '" + i.label + "': page '" + i.action.page +
                                             "' does not exist");
            }
    }
    for (auto& [n, w] : p->wheels)
        for (auto& i : w.items)
            checkA(i.action, "wheel '" + n + "' item '" + i.label + "'");
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
            w["text_up"] = W.textUp;
            if (W.textUp == "custom") w["rotate_deg"] = std::round(W.rotateDeg * 10.f) / 10.f;
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
    j["zoom"] = std::round(m.zoom * 100.f) / 100.f;
    j["fps"] = m.fps;
    j["smoothing"] = std::round(m.smoothing * 100.f) / 100.f;
    j["on_top"] = m.onTop;
    return WriteFileAtomic(MirrorSettingsPath(), j.dump(2));
}

const char* DefaultProfileJson() {
    // Generic starting profile: every feature switched on so new users can see what exists.
    return R"JSON({
  "enabled": true,
  "block_while_open": "hand",
  "haptics": true,
  "haptic_strength": 0.35,
  "notifications": true,
  "style": { "anchor": "head_locked", "radius_m": 0.127, "head_distance_m": 0.7, "head_down_m": 0.1 },
  "selection": { "method": "both", "hand": "opener", "deadzone": 0.5, "confirm_button": "trigger", "cancel_button": "secondary" },
  "keyboard": { "width_m": 0.56, "distance_m": 0.45, "below_eyes_m": 0.3 },
  "bindings": [
    { "name": "Right hand wheel", "input": "right.thumbrest", "gesture": "double_tap", "double_tap_ms": 400, "block": "none",
      "action": { "type": "wheel", "wheel": "right", "confirm": "stick_return" } },
    { "name": "Left hand wheel", "input": "left.thumbrest", "gesture": "double_tap", "double_tap_ms": 400, "block": "none",
      "action": { "type": "wheel", "wheel": "left", "confirm": "stick_return" } },
    { "name": "VR keyboard (hold left menu)", "enabled": false, "input": "left.menu", "gesture": "hold", "hold_ms": 600, "block": "trigger",
      "action": { "type": "keyboard" } },
    { "name": "Shift layer (hold left grip + click left stick)", "enabled": false, "input": "left.grip+left.thumbstick", "gesture": "while_held", "block": "trigger",
      "action": { "type": "layer", "layer": "shift", "mode": "hold" } },
    { "name": "Quick save (shift + A)", "enabled": false, "input": "right.a", "layers": "shift", "gesture": "press",
      "action": { "type": "key", "keys": "F5" } },
    { "name": "Quick load (shift + hold B)", "enabled": false, "input": "right.b", "layers": "shift", "gesture": "hold", "hold_ms": 800,
      "action": { "type": "key", "keys": "F9" } }
  ],
  "wheels": {
    "right": { "title": "Right hand", "items": [
      { "label": "Map", "action": { "type": "key", "keys": "M" } },
      { "label": "Inventory", "action": { "type": "key", "keys": "I" } },
      { "label": "Pause", "action": { "type": "key", "keys": "Esc" } },
      { "label": "Screenshot", "action": { "type": "key", "keys": "F12" } },
      { "label": "Quick save", "action": { "type": "key", "keys": "F5" } },
      { "label": "More...", "action": { "type": "wheel", "wheel": "Right hand more (sub-wheel)" } }
    ] },
    "Right hand more (sub-wheel)": { "title": "More", "items": [
      { "label": "Back", "action": { "type": "back" } },
      { "label": "Quick load", "action": { "type": "key", "keys": "F9" } },
      { "label": "Keyboard", "action": { "type": "keyboard" } },
      { "label": "Adjust positions", "action": { "type": "adjust" } },
      { "label": "Reload profile", "action": { "type": "reload" } }
    ] },
    "left": { "title": "Left hand", "items": [
      { "label": "Walk toggle", "action": { "type": "key", "keys": "LCtrl", "mode": "toggle" } },
      { "label": "Crouch toggle", "action": { "type": "key", "keys": "C", "mode": "toggle" } },
      { "label": "Keyboard", "action": { "type": "keyboard" } },
      { "label": "Copy + paste", "action": { "type": "sequence", "steps": "Ctrl+C; wait 100; Ctrl+V" } },
      { "label": "Chat", "action": { "type": "key", "keys": "Enter" } },
      { "label": "Reload profile", "action": { "type": "reload" } }
    ] }
  },
  "screen": { "monitor": 0, "fps": 30, "fps_steps": [10, 15, 30, 60, 0], "place": "wrist_left", "wrist_width_m": 0.16, "width_m": 0.8, "distance_m": 0.9 },
  "wrist_left": {
    "enabled": true, "mirror": false, "face": "inner", "text_up": "custom", "rotate_deg": -145.4, "press": "both", "columns": 3,
    "size_m": 0.063, "toward_elbow_m": 0.074, "lift_m": 0.013, "side_m": 0.087,
    "start_page": "Left hand main",
    "pages": {
      "Left hand main": { "title": "Left hand", "items": [
        { "label": "Quick save", "action": { "type": "key", "keys": "F5" } },
        { "label": "Pause", "action": { "type": "key", "keys": "Esc" } },
        { "label": "Monitor", "action": { "type": "screen", "mode": "toggle", "place": "wrist_left" } },
        { "label": "Keyboard", "action": { "type": "keyboard" } },
        { "label": "More", "action": { "type": "page", "page": "Left hand more (sub-page)" } },
        { "label": "L-Ctrl toggle", "enabled": false, "action": { "type": "key", "keys": "LCtrl", "mode": "toggle" } },
        { "label": "L-Shift toggle", "enabled": false, "action": { "type": "key", "keys": "LShift", "mode": "toggle" } },
        { "label": "Right-click", "enabled": false, "action": { "type": "key", "keys": "RMB" } }
      ] },
      "Left hand more (sub-page)": { "title": "More", "items": [
        { "label": "Quick load", "action": { "type": "key", "keys": "F9" } },
        { "label": "Reload profile", "action": { "type": "reload" } },
        { "label": "Calibrate Positions", "action": { "type": "adjust" } },
        { "label": "Controller offsets", "action": { "type": "controller_offsets", "mode": "panel" } },
        { "label": "Headset mirror", "action": { "type": "headset_mirror", "mode": "toggle" } },
        { "label": "Screen options", "action": { "type": "page", "page": "Screen (sub-page)" } },
        { "label": "Screenshot", "enabled": false, "action": { "type": "key", "keys": "F12" } },
        { "label": "Hide", "enabled": false, "action": { "type": "close" } }
      ] },
      "Screen (sub-page)": { "title": "Screen", "items": [
        { "label": "On my wrist", "action": { "type": "screen", "mode": "toggle", "place": "wrist_left" } },
        { "label": "In front of me", "action": { "type": "screen", "mode": "toggle", "place": "front" } },
        { "label": "Next monitor", "action": { "type": "screen", "mode": "next_monitor" } },
        { "label": "Choose window", "action": { "type": "screen", "mode": "choose" } },
        { "label": "Frame rate", "action": { "type": "screen", "mode": "next_fps" } },
        { "label": "Move it", "action": { "type": "screen", "mode": "next_place" } },
        { "label": "Screen off", "action": { "type": "screen", "mode": "off" } }
      ] }
    }
  },
  "wrist_right": {
    "enabled": true, "mirror": false, "face": "inner", "text_up": "custom", "rotate_deg": 140.3, "press": "both", "columns": 3,
    "size_m": 0.073, "toward_elbow_m": 0.06, "lift_m": 0.022, "side_m": -0.085,
    "start_page": "Right hand main",
    "pages": {
      "Right hand main": { "title": "Right hand", "items": [
        { "label": "Crouch toggle", "action": { "type": "key", "keys": "C", "mode": "toggle" } },
        { "label": "Chat", "action": { "type": "key", "keys": "Enter" } },
        { "label": "Quick save", "action": { "type": "key", "keys": "F5" } },
        { "label": "Screenshot", "action": { "type": "key", "keys": "F12" } },
        { "label": "Open right wheel", "action": { "type": "wheel", "wheel": "right" } },
        { "label": "Hide", "action": { "type": "close" } }
      ] }
    }
  }
}
)JSON";
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
