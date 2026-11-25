#pragma once
// Analog stick shaping, per hand and per profile. Everything is off at 0 (the stick works as before).
//
// - Inner deadzone: small pushes near the middle count as no push (stops drift). What is left is stretched, so
//   the stick still reaches full at the edge.
// - Outer deadzone: the last part toward the edge already counts as fully pushed.
//   Both change what the GAME sees from the stick (its own stick and movement actions).
// - Direction angle (direction zone): a push within this many degrees of up, down, left or right counts as exactly
//   that direction, for the GAME too (the stick is straightened: up and a little right is plain up, so a jump on up
//   doesn't also turn) and for our bindings on stick directions. Pushes outside every zone (diagonals) are left
//   as they are for the game, and count as no direction for our bindings. 0 = off. 45 = every push snaps to the
//   nearest of the four directions.
// - Ignore outside (with a direction zone): pushes outside every zone count as no push for the game too, so the
//   stick only ever gives a clean up, down, left or right (a jump on up never turns, however you lean).
// - Ignore center (with ignore outside): pushes shorter than this share give the game nothing either, so only
//   firm, straight pushes reach it. 0 = off (the default). Its own setting, apart from "Direction counts at".
#include <cmath>

struct StickShape {
    float innerDeadzone = 0;     // 0 .. 0.9, share of the full push
    float outerDeadzone = 0;     // 0 .. 0.9
    float directionAngleDeg = 0; // 0 = off, else 5 .. 45
    bool ignoreOutside = false;  // with a direction zone: diagonals give the game no push at all
    float ignoreCenter = 0;      // ignore outside: pushes shorter than this share give no push either (0 = off)
    bool ChangesGame() const { return innerDeadzone > 0 || outerDeadzone > 0 || directionAngleDeg > 0; }
};

// The stick as the game should see it: radial, so the direction never changes, only how far it is pushed.
// Within the direction zone of an axis: straight along it, same length (the game sees a clean up / down / left / right).
inline void SnapStick(const StickShape& s, float& x, float& y) {
    if (s.directionAngleDeg <= 0) return;
    float length = std::sqrt(x * x + y * y);
    if (length <= 1e-6f) return;
    if (s.ignoreOutside && length < s.ignoreCenter) { x = y = 0; return; } // a small push: nothing for the game
    bool vertical = std::fabs(y) >= std::fabs(x);
    float cosToAxis = (vertical ? std::fabs(y) : std::fabs(x)) / length;
    if (cosToAxis < std::cos(s.directionAngleDeg * 3.14159265f / 180.f)) { // a diagonal
        if (s.ignoreOutside) x = y = 0; // ignored: no push at all
        return;                         // else left alone
    }
    if (vertical) { y = y < 0 ? -length : length; x = 0; }
    else { x = x < 0 ? -length : length; y = 0; }
}

inline void ShapeStick(const StickShape& s, float& x, float& y) {
    if (!s.ChangesGame()) return;
    SnapStick(s, x, y);
    float length = std::sqrt(x * x + y * y);
    if (length <= 1e-6f) return;
    float inner = s.innerDeadzone, outer = 1.f - s.outerDeadzone;
    if (outer <= inner + 0.05f) outer = inner + 0.05f; // never a zero-width range
    float shaped = length <= inner ? 0.f : length >= outer ? 1.f : (length - inner) / (outer - inner);
    x *= shaped / length;
    y *= shaped / length;
}

// One axis alone (a game action that reads only the stick's x or y): the same shaping along that axis.
inline float ShapeStickAxis(const StickShape& s, float v) {
    float x = v, y = 0;
    ShapeStick(s, x, y);
    return x;
}

// Whether a push (x, y) points close enough to a direction (dx, dy = one of the four axes) to count for it.
inline bool InDirectionZone(const StickShape& s, float x, float y, float dx, float dy) {
    if (s.directionAngleDeg <= 0) return true;
    float length = std::sqrt(x * x + y * y);
    if (length <= 1e-6f) return false;
    float cosToAxis = (x * dx + y * dy) / length;
    return cosToAxis >= std::cos(s.directionAngleDeg * 3.14159265f / 180.f);
}
