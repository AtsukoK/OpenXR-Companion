#pragma once
#include "common.h"
#include <functional>

namespace Keys {
// Combo syntax: "Ctrl+Shift+S", "F5", "LMB", "WheelUp".
bool Validate(const std::string& combo, std::string& err);
bool ValidateSequence(const std::string& steps, std::string& err);
void Tap(const std::string& combo, int holdMs);
void Down(const std::string& combo);
void Up(const std::string& combo);
// Up after waiting waitMs (keeps a very short press down long enough for the game to see it).
void UpAfter(const std::string& combo, int waitMs);
// "Ctrl+C; wait 100; down W; wait 500; up W; F5"
void Sequence(const std::string& steps);
void ReleaseAll();
void SetFocusWindow(bool on);
// Desktop mouse for the mirrored screen. x, y are desktop pixels (not scaled by Windows' text size).
void MouseTo(int x, int y);
void MouseButton(int button, bool down); // 0 left, 1 right, 2 middle
void MouseWheel(int delta);              // 120 = one notch up
void FocusGameNow();                     // give the game's window the focus back
void BringToFront(uintptr_t hwnd);       // before clicking a mirrored window that may be covered
// Tests: record instead of moving the real mouse.
void SetDryRun(bool on);
void DryState(int& x, int& y, bool& left, bool& right, int& wheel, int& focus);
void Shutdown();
} // namespace Keys
