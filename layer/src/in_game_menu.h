#pragma once
// The in-game settings menu, like SteamVR's dashboard but for this app: a panel in front of you with pages of
// settings (menu.h), pointed at with the laser and pressed with the trigger. The stick scrolls (up / down) and
// moves a number you point at (left / right). Pages come from menu_pages.cpp.
//
// Edits go into the engine's profile at once and are saved to the profile file a moment after the last change.
#include "config.h"
#include "menu.h"

class Engine;

class InGameMenu {
public:
    explicit InGameMenu(Engine& engine) : e(engine) {}

    void Toggle();
    void Open();
    void Close();
    bool IsOpen() const;
    void Update();

    // For pages: go to another page, go back, show the page again after a change, save soon.
    void Push(MenuPageBuilder page);
    void Back();
    void Refresh();
    void Changed(); // the profile changed: refresh, and save a moment later

    // Ask for a controller input: the next button press or stick push (not the trigger press that asked).
    void CaptureInput(std::function<void(const InputId&)> done);
    // Ask for a key: the VR keyboard opens, and the next key pressed on it is returned instead of typed.
    void CaptureKey(std::function<void(const std::string&)> done);

    int PointingHand() const { return run_.uiHand; } // the hand using the menu, -1 when none (its trigger and stick are ours)
    bool IsCapturingInput() const { return static_cast<bool>(run_.inputDone); }

private:
    void Place();
    void BuildView();
    void Activate(const MenuHit& hit, bool repeat);
    void Step(int row, int direction);
    void UpdatePointing();
    void UpdateStick(float dt);
    void UpdateInputCapture();
    void SaveWhenDue();

    Engine& e;
    std::vector<MenuPageBuilder> stack_;
    MenuPage page_;
    struct Run {
        int uiHand = -1;
        MenuHit repeatHit;
        double repeatAt = 0, flashUntil = 0, saveAt = -1;
        float stickCarry = 0; // part-steps of a number moved by the stick
        double scrollAt = 0;  // the stick scrolls a row at a time, this often
        std::function<void(const InputId&)> inputDone;
        bool inputArmed = false; // the trigger that asked has been let go
    } run_;
};
