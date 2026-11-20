#pragma once
// Experimental head smoothing: the game is handed a steadied head (direction only; the position stays
// real). In "headset" mode each frame is then submitted with the real head pose, so the compositor
// shows the steadied picture as it was rendered.
//
// Games ask for their head in one of two ways, and both are handled:
//  - eye views located in their world (most Unity / native games): OnLocateViews steadies the eyes;
//  - the head (a VIEW reference space) located on its own, eyes relative to it (Unreal): OnLocateSpace.
//
// Thread safe: every call takes the object's own lock (the game may call from several threads).
#include "common.h"
#include "config.h"
#include <set>

class HeadSmoother {
public:
    // New settings from the file; returns whether it was on before.
    bool SetSettings(const HeadSmoothSettings& s);
    HeadSmoothSettings Settings() const;

    // The game's VIEW reference spaces (its head).
    void AddViewSpace(XrSpace s);
    void RemoveViewSpace(XrSpace s);

    // Hooks: called with what the runtime returned, they change the poses in place.
    void OnLocateViews(const XrViewLocateInfo& info, const XrViewState& state, uint32_t count, XrView* views);
    void OnLocateSpace(XrSpace space, XrSpace base, XrTime t, XrSpaceLocationFlags flags, XrPosef& pose);
    // "headset" mode: returns frame info with the real head pose put back (or `fi` unchanged).
    // The returned pointer stays valid until the next call.
    const XrFrameEndInfo* OnEndFrame(const XrFrameEndInfo* fi);

    std::string StateText() const; // for the log at game start

private:
    static constexpr int kHistory = 64;               // steadied directions kept, one per time
    static constexpr int kRecords = 8;                // head poses kept for "headset" mode
    static constexpr XrTime kRecordMatchNs = 3000000; // a record within 3 ms of a frame's time matches it
    static constexpr double kStatsPeriodSec = 10.0;

    struct TimedTurn {
        XrTime t = 0;
        XrQuaternionf steadied{ 0, 0, 0, 1 };
    };
    // The head, real and steadied, at one time in one space.
    struct HeadRecord {
        XrTime t = 0;
        XrSpace space = XR_NULL_HANDLE;
        XrPosef real{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
        XrPosef steadied{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    };
    struct Stats {
        double since = 0;
        float lagSum = 0, lagMax = 0;
        int lagN = 0;
        int framesFixed = 0, framesMissed = 0;
        int viewsWorld = 0, viewsHead = 0, headReads = 0, frames = 0, framesNoRecord = 0;
        void Reset(double now);
    };

    bool Step(XrTime t, const XrQuaternionf& real, XrQuaternionf& turn); // false: off
    XrQuaternionf SteadiedAt(XrTime t, const XrQuaternionf& fallback) const;
    void Record(XrTime t, XrSpace space, const XrPosef& real, const XrPosef& steadied);
    void LogStatsIfDue();
    void Stop();

    mutable std::mutex mx_;
    HeadSmoothSettings set_;
    HeavyCam cam_;
    XrTime lastT_ = 0;
    TimedTurn history_[kHistory];
    int historyNext_ = 0;
    HeadRecord records_[kRecords];
    int recordNext_ = 0;
    std::set<uint64_t> viewSpaces_;
    Stats stats_;
    bool inUseLogged_ = false;
    // "how this game asks for its head", logged once each
    bool saidViews_[2] = {};
    bool saidHeadRead_ = false, saidFrame_ = false;

    // OnEndFrame's output, kept so the returned pointer stays valid
    XrFrameEndInfo outInfo_{};
    std::vector<XrCompositionLayerProjection> outLayers_;
    std::vector<XrCompositionLayerProjectionView> outViews_;
    std::vector<const XrCompositionLayerBaseHeader*> outPtrs_;
};
