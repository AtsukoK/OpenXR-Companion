#pragma once
#include <cstdint>
#include <string>

// A virtual Xbox 360 pad through the ViGEmBus driver (optional: without it, gamepad actions do nothing).
// The pad is plugged in the first time a gamepad action is used and stays until the game closes.
namespace Gamepad {

// What the pad shows this frame. Sticks -1..1, triggers 0..1.
struct State {
    uint16_t buttons = 0; // XUSB_GAMEPAD_* bits
    float lt = 0, rt = 0;
    float lx = 0, ly = 0, rx = 0, ry = 0;
    bool operator==(const State& o) const {
        return buttons == o.buttons && lt == o.lt && rt == o.rt && lx == o.lx && ly == o.ly && rx == o.rx && ry == o.ry;
    }
};

// Adds one pad button ("a", "lb", "lt", "dpad_up", "ls_left", ...) to the state. False for an unknown name.
bool Apply(const std::string& button, State& s);
bool IsButton(const std::string& button);
// The ViGEmBus driver is installed (checked once).
bool DriverInstalled();

// Sends the state when it changed. wanted = some gamepad action is in the profile or on: plugs the pad in on first
// use. Returns an error text once when the driver is missing (for a toast), else empty.
std::string Submit(const State& s, bool wanted);

} // namespace Gamepad
