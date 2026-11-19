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
struct PoseSmoother {
    bool init = false;
    XrPosef s{ {0,0,0,1}, {0,0,0} }, last{ {0,0,0,1}, {0,0,0} };
    float speed = 0, turn = 0; // filtered m/s and rad/s
    void Reset() { init = false; }
    XrPosef Apply(const XrPosef& in, float dt, float strength);
};
