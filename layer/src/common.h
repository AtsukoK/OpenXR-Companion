#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <string>
#include <vector>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <cstdint>

void Log(const char* fmt, ...);
void LogInit();
std::wstring Utf8ToW(const std::string& s);
std::string WToUtf8(const std::wstring& s);
std::wstring DataDir();      // %LOCALAPPDATA%\OpenXRCompanion
std::wstring ProfilesDir();  // ...\profiles
std::string ExeName();       // lower-case exe name without extension
double NowSec();

struct Vec3 { float x = 0, y = 0, z = 0; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float Len(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline Vec3 Norm(Vec3 a) { float l = Len(a); return l > 1e-6f ? a * (1.f / l) : Vec3{ 0, 0, -1 }; }
inline Vec3 V(XrVector3f v) { return { v.x, v.y, v.z }; }
inline XrVector3f X(Vec3 v) { return { v.x, v.y, v.z }; }
Vec3 Rotate(const XrQuaternionf& q, Vec3 v);
Vec3 RotateInv(const XrQuaternionf& q, Vec3 v);
XrQuaternionf QuatFromBasis(Vec3 x, Vec3 y, Vec3 z);
XrQuaternionf BillboardTo(Vec3 center, Vec3 eye);
XrQuaternionf QMul(const XrQuaternionf& a, const XrQuaternionf& b);
inline XrQuaternionf QInv(const XrQuaternionf& q) { return { -q.x, -q.y, -q.z, q.w }; }
XrPosef PMul(const XrPosef& a, const XrPosef& b);   // b given in a's frame -> world
XrPosef PInv(const XrPosef& a);
XrPosef YawFrame(const XrPosef& head);              // head position, turned only left/right like the head

// Steadies a pose that shakes a little (hand tremor) without lagging behind real movement:
// a one-euro filter. Nearly still = smoothed hard; moving on purpose = follows quickly.
// strength 0 = off, 1 = strongest.
// A heavy camera: two smoothing stages in a row, the same for every movement (no speeding up on fast turns),
// so it eases into and out of every turn like a weighted camera rig. Time constant = 0.12 s per strength step.
struct HeavyCam {
    bool init = false; XrQuaternionf a{ 0,0,0,1 }, b{ 0,0,0,1 };
    void Reset() { init = false; }
    static XrQuaternionf Toward(XrQuaternionf from, XrQuaternionf to, float k) {
        if (from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w < 0) to = { -to.x, -to.y, -to.z, -to.w };
        XrQuaternionf q{ from.x + (to.x - from.x) * k, from.y + (to.y - from.y) * k, from.z + (to.z - from.z) * k, from.w + (to.w - from.w) * k };
        float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        return l > 1e-6f ? XrQuaternionf{ q.x / l, q.y / l, q.z / l, q.w / l } : to;
    }
    XrQuaternionf Apply(const XrQuaternionf& in, float dt, float strength) {
        if (!init || dt <= 0.f || dt > 0.5f) { a = b = in; init = true; return in; }
        float tau = 0.12f * strength * 0.5f;              // each stage gets half
        float k = 1.f - std::exp(-dt / std::max(tau, 1e-3f));
        a = Toward(a, in, k); b = Toward(b, a, k);
        return b;
    }
};

struct PoseSmoother {
    bool init = false;
    XrPosef s{ {0,0,0,1}, {0,0,0} }, last{ {0,0,0,1}, {0,0,0} };
    float speed = 0, turn = 0; // filtered m/s and rad/s
    void Reset() { init = false; }
    XrPosef Apply(const XrPosef& in, float dt, float strength);
};

// Game-wide steadying of one game hand space (grip or aim), as the game reads it at time t.
// Only fully tracked poses are steadied; anything else passes through untouched and restarts the filter,
// so a lost or inferred controller never drags the steadied hand toward a stale or zero position.
// The game may ask for several times per frame (e.g. now and the predicted display time): each time gets
// the steadied answer it had (the correction recorded for the nearest time, applied to the raw pose),
// so the hand never flips between a raw and a steadied pose.
struct SpaceSteady {
    PoseSmoother f;
    XrTime last = 0;
    struct Rec { XrTime t = 0; XrQuaternionf dq{ 0,0,0,1 }; XrVector3f dp{ 0,0,0 }; };
    Rec hist[16]; int next = 0;
    void Reset() { f.Reset(); last = 0; for (auto& h : hist) h = Rec{}; }
    // returns the pose to hand the game; flags = the runtime's location flags
    XrPosef Step(XrTime t, const XrPosef& raw, uint64_t flags, float strength);
};
