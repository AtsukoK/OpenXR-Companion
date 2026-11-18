#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <string>

struct WindowCapture;
bool WinCapSupported();
// Starts capturing one window with the given device. nullptr + err when it can't.
WindowCapture* WinCapOpen(HWND hwnd, ID3D11Device* dev, std::string& err);
// The newest frame since the last call (AddRef'd, the caller releases it), or nullptr.
// cw, ch = the window's picture size inside the texture. closed = the window is gone.
ID3D11Texture2D* WinCapNext(WindowCapture* wc, int& cw, int& ch, bool& closed);
void WinCapClose(WindowCapture* wc);
