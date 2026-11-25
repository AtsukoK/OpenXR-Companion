using Avalonia.Controls;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// The guided tours (port of the WinForms Tutorial.cs, same words). Each step: a title and text on the coach card, a
// control to frame (or none), what to show first (Enter), and for "Your turn" steps when it is done.
public sealed class TourStep
{
    public string Title, Text;
    public Func<Control> Target;
    public Action Enter;
    public Func<bool> Done;
}

public sealed record TourInfo(string Id, string Group, string Title, string Does, string Example);

// What a tour needs from the window: controls to frame.
public interface ITourHost
{
    Control Find(string text);   // a button (or else any text) on screen containing this
    Control PageArea();          // the page shown
    Control Picture();           // the first controller picture
    Control RailArea();          // the icon bar
    bool Shows(string text);
}

public static class Tours
{
    public static readonly TourInfo[] All =
    {
        new("main", "Start here", "The full tour (5 minutes)", "Make a wheel, put something on it, open it from a controller button, and see every page.", "your first time here."),
        new("open_wheel", "Start here", "Open a wheel from a controller button", "Pick the button and how you press it (double-tap, hold...) that opens a wheel in VR.", "the one thing every setup needs."),
        new("sprint", "How do I...", "Sprint with a double flick", "Flick the left stick forward twice and keep pushing: the sprint key is held while you push.", "games without a sprint button on the controller."),
        new("hold", "How do I...", "Give a button a second job on hold", "A quick press stays the game's; holding it presses a key of your choice.", "hold B for the map, while B still does its normal thing."),
        new("swap", "How do I...", "Swap two buttons", "Two controller buttons trade places in the game.", "a jump or crouch that feels better on the other button."),
        new("reuse", "How do I...", "Put a button you made somewhere else too", "Copy any wheel item, wrist button or binding onto another wheel or page, without making it again.", "the same quick save on your wheel and your wrist."),
        new("screen", "How do I...", "Show my desktop on my wrist", "A wrist button that shows a monitor or window (Discord, a guide) on your arm.", "reading chat without taking the headset off."),
        new("offsets", "How do I...", "Fix how hands and guns point", "Controller offsets: turn and move the game's hands until they match your real ones.", "games whose guns aim a little off."),
        new("sec_overview", "About each page", "Overview", "What's on, how you open it, quick switches.", null),
        new("sec_settings", "About each page", "Settings", "The feature tiles and what each one sets.", null),
        new("sec_controllers", "About each page", "Controllers", "The controller pictures, the + on each button, and the side panel.", null),
        new("sec_bindings", "About each page", "All bindings", "Everything in one list: switch, copy, delete, search.", null),
        new("sec_wheels", "About each page", "Wheels", "The wheel cards and making a new wheel.", null),
        new("sec_wheel", "About each page", "A wheel's page", "Items, sub-wheels, what opens it, and reusing buttons.", null),
        new("sec_wrist", "About each page", "Wrist panels", "Pages, buttons, the list, and the panel's settings.", null),
        new("sec_binding", "About each page", "A binding's page", "The button, how you press it, and what it does.", null),
    };

    // The page tour for the page shown (the tip bar's "Show me this page").
    public static string SectionTour(string key) => key?.Split(':')[0] switch
    {
        "overview" => "sec_overview",
        "settings" => "sec_settings",
        "controllers" => "sec_controllers",
        "bindings" => "sec_bindings",
        "wheels" => "sec_wheels",
        "wheel" => "sec_wheel",
        "wrist0" or "wrist1" => "sec_wrist",
        "binding" => "sec_binding",
        _ => null,
    };

    public static List<TourStep> Build(string id, ITourHost h)
    {
        var P = Shell.Store.Profile;
        TourStep S(string title, string text, Func<Control> target = null, Action enter = null, Func<bool> done = null) =>
            new() { Title = title, Text = text, Target = target, Enter = enter, Done = done };
        Control T(string text) => h.Find(text);
        WheelModel FirstWheel() => P.Wheels.Values.FirstOrDefault(w => P.Bindings.Any(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == w.Name)) ?? P.Wheels.Values.FirstOrDefault();
        void GoWheel(WheelModel w) { if (w != null && Shell.Current != "wheel:" + w.Name) Shell.Go("wheel:" + w.Name); }
        WheelModel tWheel = null;
        int tCount = 0, openersAtStart = 0;
        int Openers() => P.Bindings.Count(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == tWheel?.Name);

        List<TourStep> OpenWheelSteps(Func<WheelModel> wheel) => new()
        {
            S("Your turn: open the 'Opened by' tab",
              "A wheel only shows up in VR when a controller button opens it.\n\nClick the 'Opened by' tab on the right.",
              () => T("Opened by"), () => { tWheel ??= wheel(); GoWheel(tWheel); },
              () => h.Shows("+ Add a button that opens it")),
            S("Your turn: pick the button",
              "Click '+ Add a button that opens it'. Choose the button (for example Right Thumbrest or Right B) and how you press it:\n\n" +
              "• Double-tap or Hold leaves a normal press for the game.\n• Press takes the button over completely.",
              () => T("+ Add a button that opens it"), () => openersAtStart = Openers(),
              () => Openers() > openersAtStart),
            S("That's it: it opens in VR",
              "The new line under 'Opened by' is the button that opens this wheel. Its On / Off switches it; ✕ deletes it; click it for timing and more.\n\n" +
              "In VR: do that with the controller, point at an item and pull the trigger (or let go, for a hold). Save first (Ctrl+S): a running game picks it up within a second.",
              () => T("Opened by")),
        };

        return id switch
        {
            "main" => new List<TourStep>
            {
                S("Welcome!", "This tour takes about five minutes. You'll make a radial wheel, put something on it, pick the controller button that opens it, and see every page.\n\nThe orange frame shows where to look. Steps marked 'Your turn' wait until you've done them.",
                  enter: () => Shell.Go("overview")),
                S("The Overview", "What is on right now: the wheels on each controller, the wrist panels, the VR keyboard. The switches work right here; the blue buttons jump to where you edit each thing.", h.PageArea, () => Shell.Go("overview")),
                S("The icon bar", "Every page is one click away: Overview, Settings, Controllers, All bindings, Wheels, the left and right wrist panels (L, R) and Tutorials (the cap).", h.RailArea),
                S("Your turn: make a wheel", "A wheel is a radial menu you open in VR. This page shows your wheels.\n\nClick the 'New wheel' card.",
                  () => T("New wheel"), () => { Shell.Go("wheels"); tCount = P.Wheels.Count; },
                  () => { if (P.Wheels.Count <= tCount) return false; tWheel = P.Wheels.Values.Last(); return true; }),
                S("Your wheel", "This is your wheel's page: its picture on the left, the list of its items on the right. The name and title are at the top (the title is shown in the middle of the wheel in VR).",
                  h.PageArea, () => GoWheel(tWheel)),
                S("Your turn: add an item", "Click '+ Add item'.", () => T("+ Add item"), () => tCount = tWheel?.Items.Count ?? 0,
                  () => tWheel != null && tWheel.Items.Count > tCount),
                S("What the item does", "Each item has a label and 'What it does'. Click 'Change…' next to 'What it does' for the choices: press a key, type a few keys, open the VR keyboard, show your desktop, open a sub-wheel...\n\nTo bind a key: 'Record key', then press it on your keyboard.",
                  () => T("What it does")),
            }.Concat(OpenWheelSteps(() => tWheel)).Concat(new List<TourStep>
            {
                S("Sub-wheels", "Too many items? '+ Add sub-wheel' makes a second wheel with a Back item, and puts a 'More' item on this one that opens it. The tabs at the top go between them.", () => T("+ Add sub-wheel")),
                S("Controllers", "Both controllers, with what each button does. Click a button (or its tag) to see and change it. The + on a tag makes a new binding for it and opens it: pick How you press it, the Button, and What happens.",
                  h.Picture, () => Shell.Go("controllers")),
                S("All bindings", "Everything you made, in one list: controller bindings, wheel items and wrist buttons. Switch any of them off and on, copy or delete them, or search.", h.PageArea, () => Shell.Go("bindings")),
                S("Wrist panels", "Buttons on your forearm. They show when you turn your wrist toward your face and look at it; press them by poking with your other hand.\n\nThe list on the right switches buttons on and off; 'Panel settings' places the panel on your arm.", h.PageArea, () => Shell.Go("wrist0")),
                S("Settings", "How wheels look and how you pick items, pointing, the VR keyboard, the screen mirror, controller offsets...", h.PageArea, () => Shell.Go("settings")),
                S("Help on every page", "Each page's yellow tip bar has a 'Show me this page' button for a short tour of it. The Tutorials icon (the cap, on the left) and the 'Tutorial' button at the top have this tour again, and how-tos like sprinting with a double flick.", () => T("Tutorial")),
                S("Save", "Changes reach the game when you save (Ctrl+S). A running game picks them up within a second.\n\nThat's it! Click Finish.", () => T("Save")),
            }).ToList(),

            "open_wheel" => new List<TourStep>
            {
                S("Open a wheel from a controller button", "A wheel is opened by a controller button: for example double-tap the right thumbrest, or hold B. Let's set one up for one of your wheels.",
                  h.PageArea, () => { tWheel = FirstWheel(); GoWheel(tWheel); }),
            }.Concat(OpenWheelSteps(FirstWheel)).Concat(new List<TourStep>
            {
                S("Another way", "On the Controllers page you can also click a button, then '+ Add...', and set 'What it does' to Radial wheel. 'Apply a controller preset...' sets the usual buttons for your controllers in one go.",
                  () => T("Apply a controller preset"), () => Shell.Go("controllers")),
            }).ToList(),

            "sprint" => new List<TourStep>
            {
                S("Sprint with a double flick", "Flick the left stick forward twice quickly and keep pushing: your sprint key is held while you push. The game still gets the stick as usual, so you walk as normal.",
                  h.Picture, () => Shell.Go("controllers")),
                S("Your turn: the left stick", "On the LEFT controller, click the 'Thumbstick' tag and '+ Add…' on 'Push up'. In the binding that opens: How = 'Double-tap and hold (stick: double flick)', What happens = 'Press a key', then 'Record key' and press your game's sprint key (often Left Shift).",
                  h.Picture, () => tCount = P.Bindings.Count(b => b.Gesture == GestureKind.DoubleTapHold),
                  () => P.Bindings.Count(b => b.Gesture == GestureKind.DoubleTapHold) > tCount),
                S("Fine-tuning", "Under Details: 'Keeps going until (%)' is how far you can let the stick come back before sprinting stops (30 = until you mostly let go). The game still gets the stick, so you walk as normal.\n\nThe same works on Down, Left and Right (a dodge, say).",
                  h.PageArea),
            },

            "hold" => new List<TourStep>
            {
                S("A second job on hold", "A quick press of the button still does what the game normally does; holding it a moment presses a key you choose instead.", h.Picture, () => Shell.Go("controllers")),
                S("Your turn", "Click the + on a button's tag (for example B on the right controller). In the binding that opens: How = 'Hold it', What happens = 'Press a key', then 'Record key' and press the key (for example M for the map).",
                  h.Picture, () => tCount = P.Bindings.Count(b => b.Gesture == GestureKind.Hold),
                  () => P.Bindings.Count(b => b.Gesture == GestureKind.Hold) > tCount),
                S("How long is a hold?", "Under Details: 'Hold time (ms)' sets how long counts as a hold, and 'If I let go early, the game still gets a normal tap' (set for you) keeps the quick press for the game.", h.PageArea),
            },

            "swap" => new List<TourStep>
            {
                S("Swap two buttons", "Two buttons trade places in the game: each does what the other one did.", h.Picture, () => Shell.Go("controllers")),
                S("Your turn", "Click a button's tag; below the pictures click '⇄ Swap with...' next to '+ Add...', then choose the other button.",
                  h.Picture, () => tCount = P.Bindings.Count(b => b.Action?.Type == ActionType.XrInput),
                  () => P.Bindings.Count(b => b.Action?.Type == ActionType.XrInput) >= tCount + 2),
                S("Undo it", "A swap is two bindings, one on each button. To undo it, 'Reset to default' both buttons, or delete both lines in All bindings.", h.Picture),
            },

            "reuse" => new List<TourStep>
            {
                S("Reuse what you made", "Every wheel and wrist page has an 'All buttons' tab: everything in your profile, with a search box. '+ Add here' puts a copy on the page you're on.",
                  h.PageArea, () => { tWheel = FirstWheel(); GoWheel(tWheel); }),
                S("Your turn: open 'All buttons'", "Click the 'All buttons' tab on the right.", () => T("All buttons"), null,
                  () => h.Shows("+ Add here")),
                S("Your turn: add one here", "Find one (try the search box) and click '+ Add here'. It's a copy: changing it later doesn't change the original.",
                  () => T("+ Add here"), () => tCount = tWheel?.Items.Count ?? 0,
                  () => tWheel != null && tWheel.Items.Count > tCount),
            },

            "screen" => new List<TourStep>
            {
                S("Your desktop on your wrist", "A wrist button can show a monitor or a window (Discord, a guide, a video) on your arm.", h.PageArea, () => Shell.Go("wrist0")),
                S("Your turn: add a button", "Click '+ Add button'.", () => T("+ Add button"),
                  () => tCount = P.WristFor(0).Pages.Values.Sum(p => p.Items.Count),
                  () => P.WristFor(0).Pages.Values.Sum(p => p.Items.Count) > tCount),
                S("Your turn: make it show the screen", "Click 'Change…' next to 'What it does' and pick 'Desktop screen'. Then 'Show it: on my left wrist', and the monitor (or type part of a window's name, like discord).",
                  () => T("What it does"), () => tCount = P.WristFor(0).Pages.Values.Sum(p => p.Items.Count(i => i.Action?.Type == ActionType.Screen)),
                  () => P.WristFor(0).Pages.Values.Sum(p => p.Items.Count(i => i.Action?.Type == ActionType.Screen)) > tCount),
                S("More screen settings", "Settings > Screen mirror sets the frame rate, size and which monitor comes first. In VR the screen has its own button bar to switch monitors and windows.", h.PageArea),
            },

            "offsets" => new List<TourStep>
            {
                S("Hands and guns that point wrong", "Some games hold the guns or hands a little off. Controller offsets turn and move them until they match your real controllers.", () => T("Controller offsets"), () => Shell.Go("controllers")),
                S("Your turn", "Click 'Controller offsets' (here on the Controllers page).", () => T("Controller offsets"), null,
                  () => Shell.Current == "settings:offsets"),
                S("Easiest in the headset", "The sliders work here, but it's easiest in VR: open the offsets panel from a wheel or wrist button ('Fix hand / gun aim'), grab and turn until it looks right. Reset puts it back.", h.PageArea),
            },

            "sec_overview" => new List<TourStep>
            {
                S("Overview", "Each card is one feature: is it on, and how you open it in VR.", h.PageArea, () => Shell.Go("overview")),
                S("Switches and jumps", "The switches turn things on or off right here. The blue buttons jump to where you edit it; Back (top left) returns here.", h.PageArea),
                S("Your profile", "The Profile box at the top switches to another profile (each game can have its own). New, rename and delete are next to it.", () => T("Profile:")),
            },
            "sec_settings" => new List<TourStep>
            {
                S("Settings", "One tile per feature. Click a tile for its settings.", h.PageArea, () => Shell.Go("settings")),
                S("Saved or straight away", "Settings saved with the profile reach the game when you Save (Ctrl+S). 'Applies straight away' ones work at once. Hover the ⓘ for help on a setting.", h.PageArea),
            },
            "sec_controllers" => new List<TourStep>
            {
                S("Controllers", "Your controllers from above and from the side, with a tag on each button: blue = your bindings, grey 'Default' = the game's own control.", h.Picture, () => Shell.Go("controllers")),
                S("The + on a tag", "Makes a new binding for that button and opens it: 1 How you press it, 2 the Button, 3 What happens, and the Details (all set for you).", h.Picture),
                S("A button's panel", "Click a button or its tag: below the pictures, each way of using it (press, touch, up...). There On / Off switches a binding, ⧉ copies, ✕ deletes, and 'Reset to default' gives the button back to the game.", h.Picture),
                S("Controller model", "Pick your controllers here. 'Apply a controller preset...' sets the usual buttons that open your wheels; 'Controller offsets' fixes how hands and guns point.", () => T("Apply a controller preset")),
            },
            "sec_bindings" => new List<TourStep>
            {
                S("All bindings", "Everything in this profile: what your controller buttons do, every wheel item and every wrist button.", h.PageArea, () => Shell.Go("bindings")),
                S("Switch, copy, delete", "On / Off switches one (kept here when off), ⧉ copies it, ✕ deletes it. Click a line for all its settings.", h.PageArea),
                S("Find it", "Search by name, key or button. 'Show' limits the list; 'Sort' puts switched-on ones first or sorts by name.", () => T("Search:")),
            },
            "sec_wheels" => new List<TourStep>
            {
                S("Wheels", "One card per wheel, with what opens it. Click a card to open the wheel's page.", h.PageArea, () => Shell.Go("wheels")),
                S("A new wheel", "The 'New wheel' card makes one. Then add items, and pick the button that opens it under 'Opened by'.", () => T("New wheel")),
            },
            "sec_wheel" => new List<TourStep>
            {
                S("A wheel", "Click an item in the picture or in the list to change it; drag one item onto another in the picture to swap them. '+ Add item' adds one.",
                  h.PageArea, () => { if (Shell.Current?.StartsWith("wheel:") != true) GoWheel(FirstWheel()); }),
                S("Sub-wheels", "The tabs at the top are this wheel and its sub-wheels. '+ Add sub-wheel' makes one with a Back item, and a 'More' item here that opens it.", () => T("+ Add sub-wheel")),
                S("The tabs on the right", "'Items on this wheel': ↑ ↓ reorder, On / Off switches an item. 'Opened by' shows and adds the buttons that open this wheel. 'All buttons' copies anything you made onto it.", () => T("Items on this wheel")),
                S("Keys", "With an item chosen: Del deletes it, Ctrl+D copies it, Alt+← / → move it.", h.PageArea),
            },
            "sec_wrist" => new List<TourStep>
            {
                S("Wrist panel", "Buttons on your forearm. Turn your wrist toward your face and look at it to see them; poke with your other hand, or point and pull the trigger.", h.PageArea,
                  () => { if (Shell.Current is not ("wrist0" or "wrist1")) Shell.Go("wrist0"); }),
                S("Pages", "The tabs are its pages, in the order you go through them. '+ Add page' makes one, with a 'Next page' button to it.", () => T("+ Add page")),
                S("Buttons", "Click a button in the picture or the list to change it; drag one onto another in the picture to swap them. On / Off in the list switches a button.", () => T("Buttons on this page")),
                S("Panel settings", "Where the panel sits on your arm, how big it is, and when it shows.", () => T("Panel settings")),
            },
            "sec_binding" => new List<TourStep>
            {
                S("A binding", "The line at the top says it in one sentence. Below: 1 How you press it, 2 the Button, 3 What happens. That's all a binding needs.", h.PageArea,
                  () => { if (Shell.Current?.StartsWith("binding:") != true && P.Bindings.Count > 0) Shell.Go("binding:0"); }),
                S("Details", "Set for you from How: whether the game still gets the button, timings, layers, the name and the on/off switch. Change any; the Details link folds them away.", () => T("Details")),
                S("What happens", "Click 'Change…' next to 'What it does' for the choices; the settings for the one you pick show below it.", () => T("What it does")),
            },
            _ => null,
        };
    }
}
