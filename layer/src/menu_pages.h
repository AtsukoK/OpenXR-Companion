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
// The first page: both controllers with what every button does (the player's bindings, else the game's own
// controls); a part opens its page, the buttons under it open the rest.
MenuPage Overview(Engine& e, InGameMenu& m);
// One part of one controller: what the game does with it, and each way to use it (assigned or not).
MenuPage PartPage(Engine& e, InGameMenu& m, int hand, int comp);
MenuPage Home(Engine& e, InGameMenu& m); // "Edit bindings": both controllers' binding lists, sticks, offsets
MenuPage ControllerBindings(Engine& e, InGameMenu& m, int hand); // one controller's bindings, and "new binding"
MenuPage BindingDetail(Engine& e, InGameMenu& m, int index); // one binding: on, button, how, key, hide from game
// What binding `index` does, picked on the VR keyboard: a key, a mouse button, or a controller button the game sees.
void PickAction(Engine& e, InGameMenu& m, int index);
MenuPage WhatHappens(Engine& e, InGameMenu& m, int index);
MenuPage StickOptions(Engine& e, InGameMenu& m, int hand);
MenuPage TestControllers(Engine& e, InGameMenu& m); // both controllers live    // one stick's deadzones and direction zone

// "Right A", "Left thumbstick: up", "Right trigger (touch)": an input in words.
std::string InputWords(const InputId& id);
// A part's name on its own, capitalised: "A", "Trigger", "D-pad up" ("View" for the Steam Frame's left menu button).
std::string PartName(int hand, int comp, ControllerKind kind);
} // namespace MenuPages
