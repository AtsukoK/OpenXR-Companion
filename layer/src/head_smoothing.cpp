#include "head_smoothing.h"

namespace {
float AngleDeg(const XrQuaternionf& q) {
    return 2.f * std::acos(std::min(1.f, std::fabs(q.w))) * 57.2958f;
}

XrQuaternionf Blend(XrQuaternionf a, XrQuaternionf b, float k) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0) b = { -b.x, -b.y, -b.z, -b.w };
    XrQuaternionf q{ a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k, a.w + (b.w - a.w) * k };
    float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return l > 1e-6f ? XrQuaternionf{ q.x / l, q.y / l, q.z / l, q.w / l } : a;
}

bool IsProjection(const XrCompositionLayerBaseHeader* h) {
    return h && h->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION;
}

const char* ModeName(int mode) {
    return mode == 1 ? "headset" : "recording";
}
} // namespace

void HeadSmoother::Stats::Reset(double now) {
    *this = Stats{};
    since = now;
}

bool HeadSmoother::SetSettings(const HeadSmoothSettings& s) {
    std::lock_guard l(mx_);
    bool was = set_.on;
    set_ = s;
    return was;
}

HeadSmoothSettings HeadSmoother::Settings() const {
    std::lock_guard l(mx_);
    return set_;
}

void HeadSmoother::AddViewSpace(XrSpace s) {
    std::lock_guard l(mx_);
    viewSpaces_.insert((uint64_t)s);
}

void HeadSmoother::RemoveViewSpace(XrSpace s) {
    std::lock_guard l(mx_);
    viewSpaces_.erase((uint64_t)s);
}

std::string HeadSmoother::StateText() const {
    std::lock_guard l(mx_);
    if (!set_.on) return "off (switch it on in the editor while the game runs, or tick Keep it on)";
    return set_.mode == 1 ? "on, in the headset" : "on, for recording";
}

void HeadSmoother::Stop() {
    if (inUseLogged_) {
        Log("head smoothing: stopped");
        inUseLogged_ = false;
    }
    cam_.Reset();
    lastT_ = 0;
    for (auto& h : history_)
        h = TimedTurn{};
}

// The steadied direction for a time. The game may ask for several times per frame (Unreal's game and
// render threads): an older time gets the camera it had then, not the newest one.
XrQuaternionf HeadSmoother::SteadiedAt(XrTime t, const XrQuaternionf& fallback) const {
    const TimedTurn* before = nullptr;
    const TimedTurn* after = nullptr;
    for (const auto& h : history_) {
        if (!h.t) continue;
        if (h.t <= t && (!before || h.t > before->t)) before = &h;
        if (h.t >= t && (!after || h.t < after->t)) after = &h;
    }
    if (before && after && after->t > before->t) {
        float k = (float)(t - before->t) / (float)(after->t - before->t);
        return Blend(before->steadied, after->steadied, k);
    }
    if (before) return before->steadied;
    if (after) return after->steadied;
    return fallback;
}

// One step of the heavy camera for the head at time t; `turn` = from the real head to the steadied one.
bool HeadSmoother::Step(XrTime t, const XrQuaternionf& real, XrQuaternionf& turn) {
    if (!set_.on || set_.strength <= 0.001f) {
        Stop();
        return false;
    }
    if (t > lastT_) { // a newer time: one step (the same or an older time reuses what it had)
        float dt = lastT_ ? std::min(0.1f, (float)((t - lastT_) * 1e-9)) : 0.f;
        XrQuaternionf steadied = cam_.Apply(real, dt, set_.strength);
        lastT_ = t;
        history_[historyNext_] = { t, steadied };
        historyNext_ = (historyNext_ + 1) % kHistory;
        if (!inUseLogged_) {
            inUseLogged_ = true;
            stats_.Reset(NowSec());
            Log("head smoothing: in use, mode %s, strength %.2f (about %.2f s of camera lag)", ModeName(set_.mode), set_.strength,
                0.12f * set_.strength);
        }
        float lag = AngleDeg(QMul(steadied, QInv(real)));
        stats_.lagSum += lag;
        stats_.lagMax = std::max(stats_.lagMax, lag);
        stats_.lagN++;
        LogStatsIfDue();
    }
    turn = QMul(SteadiedAt(t, real), QInv(real));
    return true;
}

void HeadSmoother::LogStatsIfDue() {
    double now = NowSec();
    if (now - stats_.since <= kStatsPeriodSec || stats_.lagN == 0) return;
    std::string frames;
    if (set_.mode == 1) {
        frames =
            " ; frames shown as steadied: " + std::to_string(stats_.framesFixed) + ", not matched: " + std::to_string(stats_.framesMissed);
    }
    Log("head smoothing: last 10 s the camera trailed your head by %.1f deg on average, %.1f deg at most%s", stats_.lagSum / stats_.lagN,
        stats_.lagMax, frames.c_str());
    Log("head smoothing: calls in 10 s: eye views in world %d, eye views relative to head %d, head reads %d, frames %d (no head record for "
        "%d)",
        stats_.viewsWorld, stats_.viewsHead, stats_.headReads, stats_.frames, stats_.framesNoRecord);
    stats_.Reset(now);
}

void HeadSmoother::Record(XrTime t, XrSpace space, const XrPosef& real, const XrPosef& steadied) {
    if (set_.mode != 1) return;
    HeadRecord* r = &records_[(recordNext_ + kRecords - 1) % kRecords];
    if (r->t != t || r->space != space) { // a new time or space: take the next slot
        r = &records_[recordNext_];
        recordNext_ = (recordNext_ + 1) % kRecords;
    }
    *r = { t, space, real, steadied };
}

void HeadSmoother::OnLocateViews(const XrViewLocateInfo& info, const XrViewState& state, uint32_t count, XrView* views) {
    std::lock_guard l(mx_);
    bool relativeToHead = viewSpaces_.count((uint64_t)info.space) > 0;
    if (set_.on) {
        (relativeToHead ? stats_.viewsHead : stats_.viewsWorld)++;
        if (!saidViews_[relativeToHead]) {
            saidViews_[relativeToHead] = true;
            Log("head smoothing: the game asks for its eye views %s",
                relativeToHead ? "relative to its head (Unreal style)" : "in its world");
        }
    }
    if (relativeToHead) return; // the head itself is steadied in OnLocateSpace
    if (!(state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) || count == 0) return;

    Vec3 centre{};
    for (uint32_t i = 0; i < count; i++)
        centre = centre + V(views[i].pose.position);
    centre = centre * (1.f / count);

    XrQuaternionf turn;
    if (!Step(info.displayTime, views[0].pose.orientation, turn)) return;
    XrPosef realHead{ views[0].pose.orientation, X(centre) };
    XrPosef steadiedHead{ QMul(turn, views[0].pose.orientation), X(centre) };
    for (uint32_t i = 0; i < count; i++) { // turn the eyes around the point between them
        XrPosef& p = views[i].pose;
        Vec3 fromCentre = Rotate(turn, V(p.position) - centre);
        p.orientation = QMul(turn, p.orientation);
        p.position = X(centre + fromCentre);
    }
    Record(info.displayTime, info.space, realHead, steadiedHead);
}

void HeadSmoother::OnLocateSpace(XrSpace space, XrSpace base, XrTime t, XrSpaceLocationFlags flags, XrPosef& pose) {
    if (!(flags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) return;
    std::lock_guard l(mx_);
    if (!viewSpaces_.count((uint64_t)space) || viewSpaces_.count((uint64_t)base)) return;
    if (set_.on) {
        stats_.headReads++;
        if (!saidHeadRead_) {
            saidHeadRead_ = true;
            Log("head smoothing: the game reads its head on its own (Unreal style): steadying it there");
        }
    }
    XrQuaternionf turn;
    if (!Step(t, pose.orientation, turn)) return;
    XrPosef real = pose;
    pose.orientation = QMul(turn, pose.orientation);
    Record(t, base, real, pose);
}

const XrFrameEndInfo* HeadSmoother::OnEndFrame(const XrFrameEndInfo* fi) {
    std::lock_guard l(mx_);
    if (!fi || !set_.on) return fi;

    stats_.frames++;
    if (!saidFrame_) {
        for (uint32_t i = 0; i < fi->layerCount; i++) {
            if (!IsProjection(fi->layers[i])) continue;
            saidFrame_ = true;
            auto* pl = (const XrCompositionLayerProjection*)fi->layers[i];
            Log("head smoothing: the game hands in its frames in space %llx (%s), %u views", (unsigned long long)(uint64_t)pl->space,
                viewSpaces_.count((uint64_t)pl->space) ? "its head space" : "a world space", pl->viewCount);
            break;
        }
    }
    if (set_.mode != 1) return fi;

    size_t viewCount = 0, layerCount = 0;
    for (uint32_t i = 0; i < fi->layerCount; i++) {
        if (!IsProjection(fi->layers[i])) continue;
        layerCount++;
        viewCount += ((const XrCompositionLayerProjection*)fi->layers[i])->viewCount;
    }
    if (!layerCount) return fi;

    outLayers_.assign(layerCount, {});
    outViews_.assign(viewCount, {});
    outPtrs_.assign(fi->layers, fi->layers + fi->layerCount);
    bool changed = false;
    size_t li = 0, vi = 0;
    for (uint32_t i = 0; i < fi->layerCount; i++) {
        if (!IsProjection(fi->layers[i])) continue;
        auto* src = (const XrCompositionLayerProjection*)fi->layers[i];
        XrCompositionLayerProjection& layer = outLayers_[li++];
        layer = *src;
        layer.views = outViews_.data() + vi;

        // the head pose recorded closest to this frame's time, in the frame's space
        const HeadRecord* rec = nullptr;
        XrTime best = kRecordMatchNs;
        for (const auto& h : records_) {
            if (!h.t || h.space != src->space) continue;
            XrTime d = h.t > fi->displayTime ? h.t - fi->displayTime : fi->displayTime - h.t;
            if (d <= best) {
                best = d;
                rec = &h;
            }
        }
        if (!rec) stats_.framesNoRecord++;
        XrPosef steadiedToReal = rec ? PMul(rec->real, PInv(rec->steadied)) : XrPosef{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
        for (uint32_t k = 0; k < src->viewCount; k++) {
            XrCompositionLayerProjectionView& v = outViews_[vi++];
            v = src->views[k];
            if (rec) {
                v.pose = PMul(steadiedToReal, v.pose);
                changed = true;
            }
        }
        outPtrs_[i] = (const XrCompositionLayerBaseHeader*)&layer;
    }
    if (!changed) {
        stats_.framesMissed++;
        return fi;
    }
    stats_.framesFixed++;
    outInfo_ = *fi;
    outInfo_.layers = outPtrs_.data();
    return &outInfo_;
}
