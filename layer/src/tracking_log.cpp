#include "tracking_log.h"

namespace {
const char* const kHandName[2] = { "left", "right" };
const char* const kKindName[2] = { "grip", "aim" };

std::string FlagText(uint64_t f) {
    std::string s;
    if (!(f & XR_SPACE_LOCATION_POSITION_VALID_BIT))
        s += "position LOST";
    else
        s += (f & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) ? "position tracked" : "position guessed";
    s += ", ";
    if (!(f & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
        s += "angle LOST";
    else
        s += (f & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) ? "angle tracked" : "angle guessed";
    return s;
}
} // namespace

void TrackingLog::PoseStats::ResetPeriod() {
    reads = older = untracked = jumps = changes = lines = movedN = 0;
    movedSum = movedMax = turnedSum = turnedMax = 0;
}

void TrackingLog::Read(int hand, int kind, XrTime t, uint64_t flags, const XrPosef& raw) {
    PoseStats& d = stats_[hand][kind];
    const char* hn = kHandName[hand];
    const char* kn = kKindName[kind];
    d.reads++;

    const uint64_t fullyTracked = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                  XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if ((flags & fullyTracked) != fullyTracked) d.untracked++;

    if (flags != d.lastFlags) {
        if (d.lastFlags == ~0ull) {
            Log("tracking: %s %s first read: %s", hn, kn, FlagText(flags).c_str());
        } else {
            d.changes++;
            if (d.lines++ < kMaxLinesPerPeriod) Log("tracking: %s %s now %s", hn, kn, FlagText(flags).c_str());
        }
        d.lastFlags = flags;
    }

    if (t < d.prevT) d.older++;
    bool positionValid = (flags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
    if (!positionValid || t <= d.prevT) return;

    if (d.havePrev) {
        float jump = Len(V(raw.position) - V(d.prev.position));
        double gap = (t - d.prevT) * 1e-9;
        if (jump > kJumpMetres && gap < kJumpWithinSec) {
            d.jumps++;
            if (d.lines++ < kMaxLinesPerPeriod) {
                Log("tracking: %s %s JUMPED %.0f cm in %.0f ms (from %.2f %.2f %.2f to %.2f %.2f %.2f), %s", hn, kn, jump * 100.f,
                    gap * 1000.0, d.prev.position.x, d.prev.position.y, d.prev.position.z, raw.position.x, raw.position.y, raw.position.z,
                    FlagText(flags).c_str());
            }
        }
    }
    d.prev = raw;
    d.prevT = t;
    d.havePrev = true;
}

void TrackingLog::Moved(int hand, int kind, const XrPosef& raw, const XrPosef& out) {
    PoseStats& d = stats_[hand][kind];
    float moved = Len(V(out.position) - V(raw.position));
    XrQuaternionf dq = QMul(out.orientation, QInv(raw.orientation));
    float turnedDeg = 2.f * std::acos(std::min(1.f, std::fabs(dq.w))) * 57.2958f;
    d.movedSum += moved;
    d.movedMax = std::max(d.movedMax, moved);
    d.turnedSum += turnedDeg;
    d.turnedMax = std::max(d.turnedMax, turnedDeg);
    d.movedN++;
}

void TrackingLog::Summary(double now) {
    if (periodStart_ == 0) {
        periodStart_ = now;
        return;
    }
    if (now - periodStart_ < kPeriodSec) return;
    periodStart_ = now;
    for (int h = 0; h < 2; h++) {
        for (int k = 0; k < 2; k++) {
            PoseStats& d = stats_[h][k];
            if (!d.reads) continue;
            float n = (float)std::max(1, d.movedN);
            Log("tracking 10 s: %s %s reads %d (older time %d, not fully tracked %d, tracking changes %d, jumps %d%s); "
                "layer moved it %.1f cm avg / %.1f max, turned %.1f deg avg / %.1f max",
                kHandName[h], kKindName[k], d.reads, d.older, d.untracked, d.changes, d.jumps,
                d.lines > kMaxLinesPerPeriod ? ", some lines not written" : "", d.movedSum * 100.f / n, d.movedMax * 100.f, d.turnedSum / n,
                d.turnedMax);
            d.ResetPeriod();
        }
    }
}
