#pragma once
// Wrist panels: a grid of buttons on each forearm, shown when the arm is turned toward the eyes; pressed by poking or pointing.
#include "config.h"
#include "engine_types.h"

class Engine;

class WristPanels {
public:
    explicit WristPanels(Engine& engine) : e(engine) {}

    void Update(int h);
    void Activate(int h, int cell, int presser);
    int CellAt(int h, float u, float v) const;
    bool PoseNow(int h, XrPosef& pose, Vec3& normal) const;

private:
    friend class Engine;
    Engine& e;

    struct Run {
        std::vector<const Wheel*> stack;
        double condSince = -1, lostSince = -1, pressFlashUntil = 0, shownAt = 0;
        float prevDepth = 1.f;
        bool pokeArmed = true;
        bool closedByItem = false;
    } run_[2]; // per arm
};
