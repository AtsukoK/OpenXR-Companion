#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Mirrors one desktop monitor with DXGI Desktop Duplication on a background thread.
// Uses its own D3D11 device on the monitor's graphics card, shrinks the image on the GPU
// (mipmaps) and hands over premultiplied BGRA pixels, so it works the same for D3D11,
// D3D12 and 32-bit games. Frames come at most `fps` times a second (0 = whenever the
// desktop changes); nothing is captured while the desktop doesn't change.
struct MonitorInfo { std::wstring device; int x = 0, y = 0, w = 0, h = 0; bool primary = false; };

class ScreenCapture {
public:
    ~ScreenCapture() { Stop(); }
    // Starts or retargets the capture. monitor: 0 = main, 1..N = left to right.
    void Configure(int monitor, int fps, int maxWidth, bool cursor, float opacity);
    void Stop();
    bool Running() const;
    // Copies the newest frame into px when there is one the caller hasn't taken yet.
    bool TakeFrame(std::vector<uint32_t>& px, int& w, int& h);
    // What the capture thread found: monitor count, the main monitor's number (1-based),
    // the monitor shown (1-based) and a status line for messages and the log.
    void Status(int& count, int& primary, int& shown, std::string& msg, uint64_t& msgVersion);

    static std::vector<MonitorInfo> Monitors(); // left to right, then top to bottom
    struct Impl;
private:
    Impl* d = nullptr;
};
