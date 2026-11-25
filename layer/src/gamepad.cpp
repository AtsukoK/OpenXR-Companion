#include "gamepad.h"
#define NOMINMAX
#include <windows.h>
#include <ViGEm/Client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "common.h"

namespace Gamepad {

namespace {
PVIGEM_CLIENT client = nullptr;
PVIGEM_TARGET pad = nullptr;
bool failed = false, told = false;
State sent;
bool everSent = false;

struct Name {
    const char* name;
    uint16_t bit;
};
const Name kButtons[] = {
    { "a", XUSB_GAMEPAD_A }, { "b", XUSB_GAMEPAD_B }, { "x", XUSB_GAMEPAD_X }, { "y", XUSB_GAMEPAD_Y },
    { "lb", XUSB_GAMEPAD_LEFT_SHOULDER }, { "rb", XUSB_GAMEPAD_RIGHT_SHOULDER },
    { "back", XUSB_GAMEPAD_BACK }, { "start", XUSB_GAMEPAD_START }, { "guide", XUSB_GAMEPAD_GUIDE },
    { "ls", XUSB_GAMEPAD_LEFT_THUMB }, { "rs", XUSB_GAMEPAD_RIGHT_THUMB },
    { "dpad_up", XUSB_GAMEPAD_DPAD_UP }, { "dpad_down", XUSB_GAMEPAD_DPAD_DOWN },
    { "dpad_left", XUSB_GAMEPAD_DPAD_LEFT }, { "dpad_right", XUSB_GAMEPAD_DPAD_RIGHT },
};

SHORT Axis(float v) { return (SHORT)std::lround(std::clamp(v, -1.f, 1.f) * 32767.f); }
BYTE Trig(float v) { return (BYTE)std::lround(std::clamp(v, 0.f, 1.f) * 255.f); }

bool Connect(std::string& err) {
    if (pad) return true;
    if (failed) return false;
    client = vigem_alloc();
    if (!client) { failed = true; err = "Gamepad: out of memory"; return false; }
    VIGEM_ERROR e = vigem_connect(client);
    if (!VIGEM_SUCCESS(e)) {
        vigem_free(client);
        client = nullptr;
        failed = true;
        err = "Xbox 360 button emulation is optional and needs the free ViGEmBus driver (github.com/nefarius/ViGEmBus). Everything else works.";
        Log("gamepad: vigem_connect failed 0x%08X", (unsigned)e);
        return false;
    }
    PVIGEM_TARGET t = vigem_target_x360_alloc();
    e = vigem_target_add(client, t);
    if (!VIGEM_SUCCESS(e)) {
        vigem_target_free(t);
        vigem_disconnect(client);
        vigem_free(client);
        client = nullptr;
        failed = true;
        err = "Gamepad: couldn't plug in the virtual pad";
        Log("gamepad: vigem_target_add failed 0x%08X", (unsigned)e);
        return false;
    }
    pad = t;
    Log("gamepad: virtual Xbox 360 pad plugged in");
    return true;
}
} // namespace

bool DriverInstalled() {
    static int known = -1;
    if (known < 0) {
        HKEY k;
        known = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\ViGEmBus", 0, KEY_READ, &k) == ERROR_SUCCESS;
        if (known) RegCloseKey(k);
    }
    return known == 1;
}

bool IsButton(const std::string& b) {
    State s;
    return Apply(b, s);
}

bool Apply(const std::string& b, State& s) {
    for (auto& n : kButtons)
        if (b == n.name) { s.buttons |= n.bit; return true; }
    if (b == "lt") { s.lt = 1; return true; }
    if (b == "rt") { s.rt = 1; return true; }
    if (b.size() > 3 && (b.rfind("ls_", 0) == 0 || b.rfind("rs_", 0) == 0)) {
        float& x = b[0] == 'l' ? s.lx : s.rx;
        float& y = b[0] == 'l' ? s.ly : s.ry;
        std::string d = b.substr(3);
        if (d == "up") y = 1;
        else if (d == "down") y = -1;
        else if (d == "left") x = -1;
        else if (d == "right") x = 1;
        else return false;
        return true;
    }
    return false;
}

std::string Submit(const State& s, bool wanted) {
    std::string err;
    if (!pad) {
        // nothing to show and no pad yet: don't plug one in for nothing
        if (!wanted || s == State{}) return "";
        if (!Connect(err)) {
            if (told) return "";
            told = true;
            return err;
        }
    }
    if (everSent && s == sent) return "";
    XUSB_REPORT r{};
    r.wButtons = s.buttons;
    r.bLeftTrigger = Trig(s.lt);
    r.bRightTrigger = Trig(s.rt);
    r.sThumbLX = Axis(s.lx);
    r.sThumbLY = Axis(s.ly);
    r.sThumbRX = Axis(s.rx);
    r.sThumbRY = Axis(s.ry);
    if (VIGEM_SUCCESS(vigem_target_x360_update(client, pad, r))) {
        sent = s;
        everSent = true;
    }
    return "";
}

} // namespace Gamepad
