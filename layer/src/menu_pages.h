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
MenuPage WhatHappens(Engine& e, InGameMenu& m, int index, bool tap); // tap: what a Hold binding's short press does
MenuPage StickOptions(Engine& e, InGameMenu& m, int hand);
MenuPage TestControllers(Engine& e, InGameMenu& m); // both controllers live    // one stick's deadzones and direction zone

// ---- the hub and the pages for wheels, wrist panels and general settings (menu_pages_hub.cpp)
// A wheel (wrist = false, name) or a wrist panel page (wrist = true, hand, name): a list of buttons.
struct ItemListRef {
    bool wrist = false;
    int hand = 1;
    std::string name;
};
MenuPage Main(Engine& e, InGameMenu& m); // the first page: big tiles to every part
MenuPage Wheels(Engine& e, InGameMenu& m);
MenuPage WheelPage(Engine& e, InGameMenu& m, const std::string& name);
MenuPage WheelLook(Engine& e, InGameMenu& m);
MenuPage ItemPage(Engine& e, InGameMenu& m, const ItemListRef& ref, int index);
MenuPage ItemAction(Engine& e, InGameMenu& m, const ItemListRef& ref, int index);
void AddItemRows(Engine& e, InGameMenu& m, const ItemListRef& ref, MenuPage& p);
MenuPage WristHub(Engine& e, InGameMenu& m);
MenuPage WristPanel(Engine& e, InGameMenu& m, int hand);
MenuPage WristPage(Engine& e, InGameMenu& m, int hand, const std::string& name);
MenuPage Sticks(Engine& e, InGameMenu& m);
MenuPage MoreSettings(Engine& e, InGameMenu& m);

// "Right A", "Left thumbstick: up", "Right trigger (touch)": an input in words.
std::string InputWords(const InputId& id);
// A part's name on its own, capitalised: "A", "Trigger", "D-pad up" ("View" for the Steam Frame's left menu button).
std::string PartName(int hand, int comp, ControllerKind kind);
} // namespace MenuPages
