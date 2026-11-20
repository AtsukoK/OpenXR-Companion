#pragma once
// Small enums shared by the engine and its feature classes.

// Buttons of the bar under the mirrored screen, left to right.
enum class ScreenBarButton { Choose, FrameRate, Move, Keyboard, Adjust, Hide };
// What the screen picker lists: what to show (monitors, windows), or where to put the screen.
enum class PickerMode { Source, Place };
// Buttons of the bar under the VR keyboard, left to right.
enum class KbBarButton { DockScreen, DockKeys, AdjustDock, AdjustKeyboard, Close };
// What a wheel or wrist button asks of the headset mirror window (the layer does it).
enum class MirrorRequest { None, Toggle, On, Off };

enum class GrabTarget { None = -1, LeftWrist = 0, RightWrist = 1, Wheel, Screen, Keyboard, Dock };
inline GrabTarget WristGrab(int hand) {
    return hand == 0 ? GrabTarget::LeftWrist : GrabTarget::RightWrist;
}
