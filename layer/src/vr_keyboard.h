#pragma once
// The VR keyboard: typing by pointing, the Move/Adjust keyboard placing, and the bar that docks it with the mirrored screen.
#include "config.h"
#include "engine_types.h"

class Engine;

class VrKeyboard {
public:
    explicit VrKeyboard(Engine& engine) : e(engine) {}

    void ApplyDock(); // after the screen: put the screen above the keyboard, or the keyboard under the screen
    void Close();
    XrPosef DockRel() const; // the screen in the keyboard's frame
    float DockScreenWidth() const;
    void BarActivate(KbBarButton button);
    int KeyAt(int hand) const;
    void Press(int key, int hand);
    void ToggleMove();
    void Open(int bi);
    void UpdatePlacing();
    void UpdateBar();
    void Update();

private:
    friend class Engine;
    Engine& e;

    struct Run {
        double openTime = 0; // presses right after opening are ignored
        double pressFlashUntil = 0, barFlashUntil = 0;
        int repeatKey = -1, repeatHand = -1; // a held key that repeats
        double nextRepeat = 0;
        int opener = -1;                              // the binding that opened it (-1: a menu button)
        XrPosef frame{ { 0, 0, 0, 1 }, { 0, 0, 0 } }; // the head (turned like the head) when it opened
    } run_;
};
