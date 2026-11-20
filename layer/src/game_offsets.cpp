#include "game_offsets.h"

bool ParseHandPosePath(const std::string& path, int& hand, PoseKind& kind) {
    const std::string prefix[2] = { "/user/hand/left/input/", "/user/hand/right/input/" };
    for (int h = 0; h < 2; h++) {
        if (path.rfind(prefix[h], 0) != 0) continue;
        std::string rest = path.substr(prefix[h].size());
        if (rest == "grip/pose" || rest == "grip_surface/pose" || rest == "palm_ext/pose") {
            hand = h;
            kind = PoseKind::Grip;
            return true;
        }
        if (rest == "aim/pose") {
            hand = h;
            kind = PoseKind::Aim;
            return true;
        }
        return false;
    }
    return false;
}

void GameHandOffsets::SetSettings(const Settings& s) {
    std::lock_guard l(mx_);
    set_ = s;
}

void GameHandOffsets::AddSpace(XrSpace space, int hand, PoseKind kind) {
    std::lock_guard l(mx_);
    spaces_[(uint64_t)space] = { hand, kind };
}

void GameHandOffsets::RemoveSpace(XrSpace space) {
    std::lock_guard l(mx_);
    uint64_t s = (uint64_t)space;
    spaces_.erase(s);
    for (auto it = steadiers_.begin(); it != steadiers_.end();) {
        if (it->first.first == s || it->first.second == s) it = steadiers_.erase(it);
        else ++it;
    }
}

void GameHandOffsets::Clear() {
    std::lock_guard l(mx_);
    spaces_.clear();
    steadiers_.clear();
    set_ = Settings{};
}

// With an offset O on the located space and Ob on the base: pose' = inv(Ob) * pose * O.
void GameHandOffsets::Apply(XrSpace space, XrSpace base, XrTime t, XrSpaceLocationFlags flags, XrPosef& pose,
                            const XrSpaceVelocityFlags* velocityFlags, XrVector3f* linear, const XrVector3f* angular) {
    std::lock_guard l(mx_);
    auto located = spaces_.find((uint64_t)space);
    auto baseSpace = spaces_.find((uint64_t)base);
    bool isHand = located != spaces_.end();
    bool baseIsHand = baseSpace != spaces_.end();
    bool handInWorld = isHand && !baseIsHand;

    if (handInWorld) log_.Read(located->second.hand, (int)located->second.kind, t, flags, pose);
    log_.Summary(NowSec());

    if (!(flags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) return;
    if (!set_.on || spaces_.empty()) return;

    bool offsetSpace = isHand && set_.has[located->second.hand][(int)located->second.kind];
    bool offsetBase = baseIsHand && set_.has[baseSpace->second.hand][(int)baseSpace->second.kind];
    bool steady = handInWorld && set_.steadying > 0.001f;
    if (!offsetSpace && !offsetBase && !steady) return;

    XrPosef p = pose;
    if (offsetSpace) {
        const XrPosef& o = set_.pose[located->second.hand][(int)located->second.kind];
        // the offset point sits on a lever: its speed gains (angular velocity x lever arm)
        bool haveVelocity = velocityFlags && linear && angular &&
                            (*velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) &&
                            (*velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT);
        if (!offsetBase && haveVelocity) {
            Vec3 arm = Rotate(pose.orientation, V(o.position));
            *linear = X(V(*linear) + Cross(V(*angular), arm));
        }
        p = PMul(p, o);
    }
    if (offsetBase) p = PMul(PInv(set_.pose[baseSpace->second.hand][(int)baseSpace->second.kind]), p);
    if (steady) {
        SpaceSteady& st = steadiers_[{ (uint64_t)space, (uint64_t)base }];
        p = st.Step(t, p, flags, set_.steadying); // untracked poses pass through and restart the filter
    }
    if (handInWorld) log_.Moved(located->second.hand, (int)located->second.kind, pose, p);
    pose = p;
}
