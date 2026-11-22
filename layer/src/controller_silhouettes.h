#pragma once
// Real controller outlines (from above), traced from the WebXR input profile models (MIT licence, (c) 2019 Amazon)
// by tools/ControllerSilhouettes into controller_silhouettes.inc. Design units: 200 x 260, the same as
// controller_art.h. Left and right are traced separately. Loops are filled together with alternate fill
// (inner loops are holes: rings, straps).
#include "controller_art.h"

struct SilLoop {
    const float* xy; // x, y pairs
    int points;
};
struct SilPart {
    const char* comp; // "primary", "secondary", "thumbstick", "thumbrest", "menu", "system", "touchpad"
    const SilLoop* loops;
    int loopCount;
};
struct Silhouette {
    const char* id; // the editor's layout id: "quest3", "index"...
    int hand;       // 0 left, 1 right
    const SilLoop* body;
    int bodyCount;
    const SilPart* parts;
    int partCount;
};

// The outline for a controller kind and hand; null when there is no model (then controller_art.h's drawing is used).
const Silhouette* SilhouetteFor(ControllerKind kind, int hand);
// The input part a traced part is (C_BTN1...), or -1 for parts we can't read (touchpad, system button).
int SilhouetteComp(const char* comp);
