#pragma once
// Controller offsets and steadying for the game's own hands.
//
// The layer learns which of the game's action spaces are hand poses (from its suggested bindings and
// xrCreateActionSpace) and registers them here. Every pose the game then locates for such a space is
// turned / moved by the profile's offsets and, if asked, steadied. Poses located relative to another
// hand space get the inverse offset of that base, so hand-to-hand poses stay consistent.
//
// Thread safe: the game may locate from its render thread while the layer updates the offsets.
#include "common.h"
#include "tracking_log.h"
#include <map>
#include <unordered_map>

enum class PoseKind { Grip = 0, Aim = 1 };

// "/user/hand/left/input/grip/pose" -> hand 0, Grip. False for anything that isn't a hand pose.
bool ParseHandPosePath(const std::string& path, int& hand, PoseKind& kind);

class GameHandOffsets {
public:
    // What the game's hand poses are moved by right now (a copy taken from the engine each frame).
    struct Settings {
        bool on = false;
        bool has[2][2]{};     // [hand][kind]: an offset to apply
        XrPosef pose[2][2]{}; // [hand][kind]: the offset, in the space's own frame
        float steadying = 0;  // 0 = off
    };
    void SetSettings(const Settings& s);

    void AddSpace(XrSpace space, int hand, PoseKind kind);
    void RemoveSpace(XrSpace space);
    void Clear();

    // Called for every pose the game locates. Changes the pose (and linear velocity) in place.
    void Apply(XrSpace space, XrSpace base, XrTime t, XrSpaceLocationFlags flags, XrPosef& pose, const XrSpaceVelocityFlags* velocityFlags,
               XrVector3f* linear, const XrVector3f* angular);

private:
    struct HandSpace {
        int hand;
        PoseKind kind;
    };
    using SpacePair = std::pair<uint64_t, uint64_t>; // (space, base)

    std::mutex mx_;
    Settings set_;
    std::unordered_map<uint64_t, HandSpace> spaces_;
    std::map<SpacePair, SpaceSteady> steadiers_; // one filter per (space, base) the game asks for
    TrackingLog log_;
};
