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

    // Steps of Update, in order. Each returns false when Update should stop for this frame.
    void Hide(int h);
    bool UpdateVisibility(int h, const XrPosef& pose, const Vec3& normal); // show / hide on look
    void SyncView(int h, const XrPosef& pose);                            // page, flash, on-states into the view
    bool UpdateGrab(int h, XrPosef& pose);                                // adjust mode: true while held
    bool IsCellAllowed(int h, int cell) const;                            // adjust mode allows only navigation
    bool UpdatePoke(int h, const XrPosef& pose, bool settled, int& hover);
    bool UpdatePoint(int h, const XrPosef& pose, const Vec3& normal, bool settled, int& hover);
    // A point in the world -> panel coordinates (0..1 across, 0..1 down) and depth in front of it (metres).
    void ToPanel(int h, const XrPosef& pose, Vec3 worldPoint, float& u, float& v, float& depthM) const;

    struct Run {
        std::vector<const Wheel*> stack;
        double condSince = -1, lostSince = -1, pressFlashUntil = 0, shownAt = 0;
        float prevDepth = 1.f;
        bool pokeArmed = true;
        bool closedByItem = false;
    } run_[2]; // per arm
};
