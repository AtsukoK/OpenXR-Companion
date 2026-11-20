#pragma once
// The mirrored screen, engine side: where it sits, its on/off/monitor/window actions, pointing at it to use the mouse, and its bar and picker.
#include "config.h"
#include "engine_types.h"

class Engine;

class ScreenMirror {
public:
    explicit ScreenMirror(Engine& engine) : e(engine) {}

    void BarActivate(ScreenBarButton button);
    void BuildPicker();
    void OpenPicker();
    bool PanelRay(int h, const XrPosef& pose, float w, float hgt, float& u, float& v) const;
    void PickerActivate(int cell);
    void ScreenAction(const Action& a); // screen on/off, monitor, frame rate, place
    void Interact();
    void UpdatePose();
    bool Ray(int h, float& u, float& v, float& t) const;
    void UpdateUi();
    void Update();

private:
    friend class Engine;
    Engine& e;

    struct Run {
        bool init = false;       // the profile's start-on setting has been applied
        bool placeFront = false; // put it in front of the head on the next update
        XrPosef frontPose{ { 0, 0, 0, 1 }, { 0, 0, 0 } }, frontFrame{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
        bool mouseL = false, mouseR = false; // mouse buttons we hold down on the desktop
        bool clicked = false;                // clicked into the desktop: give the game focus back later
        int lastMx = -1, lastMy = -1;
        float wheelAcc = 0; // stick scrolling, in notches
        double leftAt = -1; // when the pointer left the screen after a click
        int pickPage = 0;
        std::vector<int> pickMap; // picker cell -> choice index
        uint64_t seenChoices = 0;
        PickerMode pickMode = PickerMode::Source;
        double barUntil = -1, uiFlashUntil = 0;
        int uiHand = -1; // the hand on the bar or picker this frame
    } run_;
};
