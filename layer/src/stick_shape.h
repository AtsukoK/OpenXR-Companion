#pragma once
// Analog stick shaping, per hand and per profile. Everything is off at 0 (the stick works as before).
//
// - Inner deadzone: small pushes near the middle count as no push (stops drift). What is left is stretched, so
//   the stick still reaches full at the edge.
// - Outer deadzone: the last part toward the edge already counts as fully pushed.
//   Both change what the GAME sees from the stick (its own stick and movement actions).
// - Direction angle: for our bindings on stick directions (up / down / left / right), a direction only counts
//   while the stick points within this many degrees of it. 0 = off: any push past the threshold counts, so a
//   diagonal is both up and right. 45 = only the nearest direction. Smaller leaves a gap between directions,
//   so pushing forward can't also fire left or right.
#include <cmath>

struct StickShape {
    float innerDeadzone = 0;     // 0 .. 0.9, share of the full push
    float outerDeadzone = 0;     // 0 .. 0.9
    float directionAngleDeg = 0; // 0 = off, else 5 .. 45
    bool ChangesGame() const { return innerDeadzone > 0 || outerDeadzone > 0; }
};

// The stick as the game should see it: radial, so the direction never changes, only how far it is pushed.
inline void ShapeStick(const StickShape& s, float& x, float& y) {
    if (!s.ChangesGame()) return;
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
