#pragma once
// Controller offsets panel in the headset: +/- buttons, stick sliders, grab-to-set and resets; and the offsets the layer applies to the game.
#include "config.h"
#include "engine_types.h"

class Engine;

class OffsetsPanel {
public:
    explicit OffsetsPanel(Engine& engine) : e(engine) {}

    void Build();
    HandOffset& Edit(int hand, int kind);
    void PressCell(int cell, bool repeat);
    bool OffsetPose(int hand, int kind, XrPosef& out) const;
    void OffsetsAction(const Action& a); // panel, enable, on, off, grab, reset
    void Changed();
    void Open();
    void Update();

private:
    // Steps of Update, in order.
    void UpdateGrabToSet();             // Grab to set: arm, start on a grip, follow the turn, keep on release
    void SaveWhenDue();                 // a short while after the last change, unless a grab is going on
    void UpdatePanelInput(float dt);    // pointing, presses and the stick slider
    friend class Engine;
    Engine& e;

    struct Run {
        int sel = 2;             // 0 left, 1 right, 2 both hands (linked)
        int kind = 2;            // 0 grip, 1 aim, 2 both
        int step = 1;            // index into the step sizes
        bool grabArmed = false;  // waiting for a grip to start grab-to-set
        double grabArmedAt = -1; // it gives up after a few seconds without a grip
        XrQuaternionf grabStart{ 0, 0, 0, 1 };
        HandOffset grabFrom[2]; // [kind] at the start of the grab
        int uiHand = -1, repeatCell = -1;
        double repeatAt = 0, flashUntil = 0, saveAt = -1;
    } run_;
};
