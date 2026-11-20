#pragma once
// What the game's own button / trigger / stick actions read.
//
// The layer learns which controller part each of the game's actions is bound to (from its suggested
// bindings). When the game asks for an action's state, the real value comes from the runtime and is then
// changed here: zeroed while the engine holds that part for a menu (Blocked), or replaced by a value a
// binding pushes into the game (Override). changedSinceLastSync is kept honest for changed values.
//
// Not thread safe: the layer calls it with its main lock held.
#include "engine.h"
#include <functional>
#include <unordered_map>

// Which part of a controller input an action reads.
enum class InputFeature { Value = 0, Touch = 1, Vector = 2, X = 3, Y = 4 };

// "/user/hand/right/input/trigger/value" -> hand 1, C_TRIGGER, Value. False for paths we don't handle.
bool ParseGameInputPath(const std::string& path, int& hand, int& comp, InputFeature& feature);

class GameInputFilter {
public:
    void SetHandPaths(XrPath left, XrPath right) { handPath_[0] = left; handPath_[1] = right; }
    void AddBinding(XrAction action, int hand, int comp, InputFeature feature);
    void SetSubactionPaths(XrAction action, const XrPath* paths, uint32_t count);
    void Clear();
    // The game synced its actions: a new frame for changedSinceLastSync.
    void NewSync() { sync_++; }

    // Asks the runtime for the action's state on one hand only (used when one hand is held and the other isn't).
    template <class State> using Requery = std::function<bool(XrPath hand, State& out)>;

    // Each changes the state the runtime returned; `now` = the time to report for a change.
    void Filter(const Engine& eng, XrAction a, XrPath sub, XrTime now, XrActionStateBoolean& st, const Requery<XrActionStateBoolean>& requery);
    void Filter(const Engine& eng, XrAction a, XrPath sub, XrTime now, XrActionStateFloat& st, const Requery<XrActionStateFloat>& requery);
    void Filter(const Engine& eng, XrAction a, XrPath sub, XrTime now, XrActionStateVector2f& st, const Requery<XrActionStateVector2f>& requery);

private:
    struct Binding { int hand, comp; InputFeature feature; };
    // What the engine wants for one query of an action.
    struct Verdict {
        bool any = false;         // the action is bound to a part on the asked hand(s)
        bool blockedAll = false, blockedSome = false;
        bool overridden = false;
        Override value;           // when overridden
        InputFeature feature = InputFeature::Value;
        int freeHand = -1;        // a hand whose part is not blocked
    };
    // Previous and current value per (action, subaction path), for changedSinceLastSync.
    struct Track { uint64_t sync = ~0ull; float prev[2]{}, cur[2]{}; };

    Verdict Evaluate(const Engine& eng, XrAction a, XrPath sub) const;
    int HandsMask(XrPath sub) const;
    bool CanRequery(XrAction a, int hand) const;
    bool Changed(XrAction a, XrPath sub, float v0, float v1);

    XrPath handPath_[2]{};
    std::unordered_map<uint64_t, std::vector<Binding>> bindings_;   // action -> controller parts
    std::unordered_map<uint64_t, std::vector<XrPath>> subactions_;  // action -> its subaction paths
    std::unordered_map<uint64_t, Track> track_;
    uint64_t sync_ = 0;
};
