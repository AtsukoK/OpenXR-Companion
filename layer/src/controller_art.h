#pragma once
// Simple drawings of controllers for the in-game menu, seen from above (the buttons' side), for the right hand;
// the left hand is the same mirrored. Our own shapes, not the makers' artwork. Design units: 200 x 260.
// The editor has fuller drawings (both views, more models) in editor/ControllerLayouts.cs; these are the same
// shapes for the controller types the layer can tell apart.
#include "config.h"
#include <vector>

// Which controller a hand holds, from the runtime's interaction profile.
// Touch = Quest 2 / Rift S style, TouchPlus = Quest 3 / 3S, TouchPro = Quest Pro. SteamFrame: only when the runtime
// gives the Frame's own profile (a game that supports it, or the Steam Frame setting on); else it shows as Index.
enum class ControllerKind { Unknown, Touch, TouchPlus, TouchPro, Index, Vive, Wmr, HpReverb, SteamFrame };
ControllerKind ControllerKindOf(const std::string& interactionProfile);
const char* ControllerKindName(ControllerKind k); // "Touch", "Index"... for the menu

enum class ArtShape { Ellipse, RoundRect, Ring, Circle, Pill };

struct ArtPiece { // part of the outline
    ArtShape shape;
    float x, y, w, h;
    float turnDeg = 0, ringWidth = 0;
};

struct ArtPart { // something that can be pressed or touched
    int comp;         // C_BTN1 ... ; -1 for a part we can't read (drawn plain)
    ArtShape shape;
    float x, y, w, h;
    int hands = 3;    // 1 left, 2 right, 3 both
};

struct ControllerArt {
    std::vector<ArtPiece> body;
    std::vector<ArtPart> parts;
};

const ControllerArt& ArtFor(ControllerKind k);
constexpr float kArtWidth = 200, kArtHeight = 260;
