#pragma once
// The in-game menu's pages. Each builder makes a page from the current profile; its rows change the profile
// through the engine and tell the menu (Changed) so it is shown again and saved.
//
// To add a part of the app to the menu: write a builder here and add a Link to it on Home.
#include "config.h"
#include "menu.h"

class Engine;
class InGameMenu;

namespace MenuPages {
MenuPage Home(Engine& e, InGameMenu& m);
MenuPage ControllerBindings(Engine& e, InGameMenu& m, int hand); // one controller's bindings, and "new binding"
MenuPage BindingDetail(Engine& e, InGameMenu& m, int index); // one binding: on, button, how, key, hide from game
// What binding `index` does, picked on the VR keyboard: a key, a mouse button, or a controller button the game sees.
void PickAction(Engine& e, InGameMenu& m, int index);
MenuPage StickOptions(Engine& e, InGameMenu& m, int hand);
MenuPage TestControllers(Engine& e, InGameMenu& m); // both controllers live    // one stick's deadzones and direction zone

// "Right A", "Left thumbstick: up", "Right trigger (touch)": an input in words.
std::string InputWords(const InputId& id);
} // namespace MenuPages
