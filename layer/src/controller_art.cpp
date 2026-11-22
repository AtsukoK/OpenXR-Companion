// Controller drawings for the in-game menu: see controller_art.h.
#include "controller_art.h"

ControllerKind ControllerKindOf(const std::string& p) {
    if (p.find("touch_plus") != std::string::npos || p.find("touch_controller_plus") != std::string::npos) return ControllerKind::TouchPlus;
    if (p.find("touch_pro") != std::string::npos || p.find("touch_controller_pro") != std::string::npos) return ControllerKind::TouchPro;
    if (p.find("touch") != std::string::npos) return ControllerKind::Touch;
    if (p.find("index") != std::string::npos) return ControllerKind::Index;
    if (p.find("valve/frame_controller") != std::string::npos) return ControllerKind::SteamFrame;
    if (p.find("vive_controller") != std::string::npos) return ControllerKind::Vive;
    if (p.find("hp/mixed_reality") != std::string::npos) return ControllerKind::HpReverb;
    if (p.find("microsoft/motion") != std::string::npos) return ControllerKind::Wmr;
    return ControllerKind::Unknown;
}

const char* ControllerKindName(ControllerKind k) {
    switch (k) {
    case ControllerKind::Touch:
        return "Touch controller";
    case ControllerKind::TouchPlus:
        return "Touch Plus controller";
    case ControllerKind::TouchPro:
        return "Touch Pro controller";
    case ControllerKind::Index:
        return "Index controller";
    case ControllerKind::Vive:
        return "Vive wand";
    case ControllerKind::Wmr:
        return "WMR controller";
    case ControllerKind::HpReverb:
        return "Reverb G2 controller";
    case ControllerKind::SteamFrame:
        return "Steam Frame controller";
    default:
        return "Controller";
    }
}

static const ControllerArt kTouch{
    { { ArtShape::Ellipse, 30, 40, 146, 110, -10 }, { ArtShape::RoundRect, 74, 126, 56, 128, 6 } },
    { { C_STICK, ArtShape::Circle, 48, 66, 38, 38 },
      { C_BTN2, ArtShape::Circle, 126, 58, 22, 22 },
      { C_BTN1, ArtShape::Circle, 112, 88, 22, 22 },
      { C_THUMBREST, ArtShape::Pill, 148, 84, 14, 32 },
      { C_MENU, ArtShape::Circle, 72, 114, 14, 14, 1 },
      { -1, ArtShape::Circle, 72, 114, 14, 14, 2 } },
};
static const ControllerArt kIndex{
    { { ArtShape::RoundRect, 60, 12, 80, 156 }, { ArtShape::RoundRect, 70, 140, 60, 116, 3 } },
    { { C_STICK, ArtShape::Circle, 80, 24, 36, 36 },
      { C_BTN2, ArtShape::Circle, 70, 72, 17, 17 },
      { C_BTN1, ArtShape::Circle, 76, 96, 17, 17 },
      { -1, ArtShape::Pill, 98, 68, 28, 66 },
      { -1, ArtShape::Circle, 93, 144, 13, 13 } },
};
static const ControllerArt kVive{
    { { ArtShape::Ring, 38, 4, 124, 104, 0, 22 }, { ArtShape::RoundRect, 66, 92, 68, 164 } },
    { { C_MENU, ArtShape::Circle, 91, 104, 18, 18 }, { C_STICK, ArtShape::Circle, 70, 128, 60, 60 }, { -1, ArtShape::Circle, 93, 200, 14, 14 } },
};
static const ControllerArt kWmr{
    { { ArtShape::Ring, 12, 6, 176, 112, 0, 20 }, { ArtShape::RoundRect, 68, 96, 64, 160 } },
    { { C_STICK, ArtShape::Circle, 70, 108, 32, 32 },
      { -1, ArtShape::Circle, 104, 110, 28, 28 },
      { C_MENU, ArtShape::Circle, 92, 156, 16, 16 },
      { -1, ArtShape::Circle, 92, 184, 16, 16 } },
};
static const ControllerArt kReverb{
    { { ArtShape::Ring, 16, 6, 168, 110, -8, 16 }, { ArtShape::Ellipse, 38, 82, 128, 84, -10 }, { ArtShape::RoundRect, 76, 140, 52, 114, 6 } },
    { { C_STICK, ArtShape::Circle, 52, 98, 34, 34 },
      { C_BTN2, ArtShape::Circle, 124, 92, 20, 20 },
      { C_BTN1, ArtShape::Circle, 110, 118, 20, 20 },
      { C_MENU, ArtShape::Circle, 76, 140, 13, 13 },
      { -1, ArtShape::Circle, 96, 146, 13, 13 } },
};

// The right Frame controller: a round head with the stick on the inside, A / B / X / Y on the outside, the menu
// slot above and the Steam button below; a bumper on the top edge. The left one (mirrored) has a D-pad there.
static const ControllerArt kSteamFrame{
    { { ArtShape::Ellipse, 28, 14, 146, 132 }, { ArtShape::RoundRect, 66, 118, 52, 136, 4 } },
    { { C_STICK, ArtShape::Circle, 44, 62, 42, 42 },
      { C_BUMPER, ArtShape::Pill, 82, 8, 40, 9 },
      { C_MENU, ArtShape::Pill, 74, 42, 20, 8 },
      { -1, ArtShape::Circle, 108, 104, 15, 15 },
      { C_BTN4, ArtShape::Circle, 120, 38, 19, 19, 2 },
      { C_BTN3, ArtShape::Circle, 102, 58, 19, 19, 2 },
      { C_BTN2, ArtShape::Circle, 138, 58, 19, 19, 2 },
      { C_BTN1, ArtShape::Circle, 120, 78, 19, 19, 2 },
      { C_DPAD_UP, ArtShape::RoundRect, 121, 38, 16, 20, 1 },
      { C_DPAD_DOWN, ArtShape::RoundRect, 121, 76, 16, 20, 1 },
      { C_DPAD_LEFT, ArtShape::RoundRect, 136, 59, 20, 16, 1 },
      { C_DPAD_RIGHT, ArtShape::RoundRect, 102, 59, 20, 16, 1 } },
};

const ControllerArt& ArtFor(ControllerKind k) {
    switch (k) {
    case ControllerKind::Index:
        return kIndex;
    case ControllerKind::Vive:
        return kVive;
    case ControllerKind::Wmr:
        return kWmr;
    case ControllerKind::HpReverb:
        return kReverb;
    case ControllerKind::SteamFrame:
        return kSteamFrame;
    default:
        return kTouch; // the most common, and a fair stand-in for unknown ones
    }
}
