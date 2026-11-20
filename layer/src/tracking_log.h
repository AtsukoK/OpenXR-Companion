#pragma once
// Tracking diagnostics for the game's hand poses (grip and aim, located in a world space).
// Logs tracking state changes and big jumps as they happen, and every 10 s a summary of the reads
// and of how far our offsets and steadying moved each hand, so a log from a glitch shows whether
// the layer moved it. Not thread safe: the caller holds the lock that guards the game's spaces.
#include "common.h"

class TrackingLog {
public:
    // A pose the runtime returned for a hand's grip (kind 0) or aim (kind 1), before we touch it.
    void Read(int hand, int kind, XrTime t, uint64_t flags, const XrPosef& raw);
    // The same pose after our offsets and steadying.
    void Moved(int hand, int kind, const XrPosef& raw, const XrPosef& out);
    // Writes the 10 s summary when it is due.
    void Summary(double now);

private:
    static constexpr int kMaxLinesPerPeriod = 8; // a flickering hand must not flood the log
    static constexpr double kPeriodSec = 10.0;
    static constexpr float kJumpMetres = 0.2f;    // a jump this big ...
    static constexpr double kJumpWithinSec = 0.1; // ... this quickly is logged

    struct PoseStats {
        uint64_t lastFlags = ~0ull;
        bool havePrev = false;
        XrPosef prev{};
        XrTime prevT = 0;
        int reads = 0, older = 0, untracked = 0, jumps = 0, changes = 0;
        int lines = 0; // written this period
        float movedSum = 0, movedMax = 0, turnedSum = 0, turnedMax = 0;
        int movedN = 0;
        void ResetPeriod();
    };
    PoseStats stats_[2][2]; // [hand][kind]
    double periodStart_ = 0;
};
