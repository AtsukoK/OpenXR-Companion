// Real controller outlines for the in-game menu: see controller_silhouettes.h.
#include "controller_silhouettes.h"
#include <cstring>

#include "controller_silhouettes.inc"

static const char* SilhouetteId(ControllerKind kind) {
    switch (kind) {
    case ControllerKind::Touch:
        return "quest2";
    case ControllerKind::TouchPlus:
        return "quest3";
    case ControllerKind::TouchPro:
        return "questpro";
    case ControllerKind::Index:
        return "index";
    case ControllerKind::Wmr:
        return "wmr";
    case ControllerKind::HpReverb:
        return "reverbg2";
    case ControllerKind::Vive:
        return "vive"; // from a CC BY model
    case ControllerKind::SteamFrame:
        return "frame"; // outlined from Valve's reference drawing
    case ControllerKind::Unknown:
        return "quest3"; // the most common now
    default:
        return nullptr; // any other kind: the simple drawing (controller_art.cpp)
    }
}

const Silhouette* SilhouetteFor(ControllerKind kind, int hand) {
    const char* id = SilhouetteId(kind);
    if (!id) return nullptr;
    for (const Silhouette& s : kSilhouettes)
        if (s.hand == hand && strcmp(s.id, id) == 0) return &s;
    return nullptr;
}

int SilhouetteComp(const char* comp) {
    static const struct {
        const char* name;
        int comp;
    } kComps[] = { { "primary", C_BTN1 }, { "secondary", C_BTN2 },  { "thumbstick", C_STICK }, { "thumbrest", C_THUMBREST },
                   { "menu", C_MENU },    { "trigger", C_TRIGGER }, { "grip", C_GRIP },
                   { "button3", C_BTN3 }, { "button4", C_BTN4 },    { "bumper", C_BUMPER },
                   { "dpad_up", C_DPAD_UP }, { "dpad_down", C_DPAD_DOWN }, { "dpad_left", C_DPAD_LEFT }, { "dpad_right", C_DPAD_RIGHT } };
    for (const auto& c : kComps)
        if (strcmp(c.name, comp) == 0) return c.comp;
    return -1;
}
