namespace OpenXRCompanionEditor;

// Guided tours. A coach card explains each step and an orange frame marks what to look at or click. "Your turn" steps
// wait until the user has done the thing. Three kinds, all picked from the Tutorial menu (toolbar):
//   the full tour (first run), feature how-tos (sprint on a double flick, ...), and one short tour per page, also
//   started from the "Show me this page" button on that page's tip bar.
// Everything made during a tour can be thrown away at the end (the profile is snapshotted first).
public partial class MainForm
{
    sealed class TourStep
    {
        public string Title, Text;
        public Func<Rectangle?> Target;   // form coordinates; null = no frame
        public Func<bool> Done;           // null = plain "Next" step
        public Action Enter;
        public Action Auto;               // tests and screenshots: what the user would do on a "your turn" step
    }

    sealed record TourInfo(string Id, string Group, string Title, string Does, string Example);

    // The menu: "Start here", "How do I...", "About each page". Section ids match SectionTour().
    static readonly TourInfo[] Tours =
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
        new("sec_wrist", "About each page", "Wrist panels", "Pages, buttons, the list, and the panel's advanced settings.", null),
        new("sec_binding", "About each page", "A binding's page", "The button, how you press it, and what it does.", null),
    };

    // Which page tour belongs to the page shown (for the tip bar's "Show me this page").
    internal static string SectionTour(object tag, bool wrist) => tag switch
    {
        "overview" => "sec_overview",
        ProfileModel or SettingsPage => "sec_settings",
        ControllersPage or "bindings_left" or "bindings_right" => "sec_controllers",
        BindingsPage => "sec_bindings",
        "wheels" => "sec_wheels",
        "wrist0" or "wrist1" => "sec_wrist",
        WheelModel or WheelItemModel when wrist => "sec_wrist",
        WheelModel or WheelItemModel => "sec_wheel",
        BindingModel => "sec_binding",
        _ => null,
    };

    List<TourStep> tour;
    string tourId;
    int tourAt;
    Panel coach; Label coachTitle, coachText, coachCount; Button coachNext, coachBack;
    readonly Panel[] frame = new Panel[4];
    System.Windows.Forms.Timer tourTimer;
    string tourSnapshot, tourFile; bool tourWasDirty;
    WheelModel tWheel; int tCount;

    TreeNode OverviewNode() => tree.Nodes.Cast<TreeNode>().FirstOrDefault(n => "overview".Equals(n.Tag));

    static string TourFlag => Path.Combine(DataDir, "tutorial_seen");

    void OfferTutorialOnFirstRun()
    {
        if (File.Exists(TourFlag) || Environment.GetCommandLineArgs().Length > 1)
            return;
        void Seen()
        { try { Directory.CreateDirectory(DataDir); File.WriteAllText(TourFlag, DateTime.Now.ToString("s")); } catch { } }
        using var dlg = new Form { Text = "First time here?", FormBorderStyle = FormBorderStyle.FixedDialog, StartPosition = FormStartPosition.CenterParent, MinimizeBox = false, MaximizeBox = false, ShowInTaskbar = false, ClientSize = new Size(540, 250), Font = new Font("Segoe UI", 10.5f) };
        dlg.Controls.Add(new Label { Text = "Welcome to OpenXR Companion!\n\nTake a short guided tour? You'll make a radial wheel, put something on it and pick the controller button that opens it in VR, then see every page. Anything you make can be thrown away at the end.\n\nLater: the 'Tutorial' button at the top has this tour, short how-tos, and a tour of each page.", Location = new Point(18, 14), Size = new Size(504, 170) });
        var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 54, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(10) };
        Button B(string t, DialogResult r, bool main = false)
        {
            var b = new Button
            {
                Text = t, AutoSize = true, Height = 34, DialogResult = r, FlatStyle = FlatStyle.Flat, Margin = new Padding(8, 0, 0, 0),
                Font = new Font("Segoe UI", 10f, FontStyle.Bold),
                BackColor = main ? Color.FromArgb(240, 140, 30) : Color.White,
                ForeColor = main ? Color.White : Color.FromArgb(40, 50, 70),
            };
            bar.Controls.Add(b);
            return b;
        }
        dlg.AcceptButton = B("Start the tour", DialogResult.Yes, true);
        B("Not now", DialogResult.Cancel);
        B("Don't ask again", DialogResult.No);
        dlg.Controls.Add(bar);
        ThemeWalker.Apply(dlg);
        dlg.HandleCreated += (s, e) => ThemeWalker.TitleBar(dlg, Theme.IsDark);
        var res = dlg.ShowDialog(this);
        if (res == DialogResult.Yes)
        { Seen(); StartTutorial("main"); }
        else if (res == DialogResult.No)
            Seen();   // "Not now" asks again next launch
    }

    // The toolbar's Tutorial button: the menu of tours, with the one for the page you're on first under "This page".
    void TutorialMenu()
    {
        string here = SectionTour(lastTag, tree.SelectedNode != null && WristOf(tree.SelectedNode) >= 0);
        var list = new List<TourInfo>();
        if (here != null && Tours.FirstOrDefault(t => t.Id == here) is TourInfo h)
            list.Add(h with { Group = "This page", Title = "This page: " + h.Title });
        list.AddRange(Tours);
        int pick = OptionPicker.Show(this, "Tutorials", "Pick a tour. You can stop any time; what you make can be thrown away at the end.",
                                     list.Select(t => new PickOption(t.Group, t.Title, t.Does, t.Example)).ToList());
        if (pick >= 0)
            StartTutorial(list[pick].Id);
    }

    internal void StartTutorial(string id = "main")
    {
        if (Profile == null)
            return;
        if (coach != null)
            EndTutorial(false);
        tourId = id;
        tourSnapshot = Serialize();
        tourFile = currentFile;
        tourWasDirty = dirty;
        tWheel = null;
        tour = BuildTour(id);
        if (tour == null || tour.Count == 0)
            return;
        BuildCoach();
        tourAt = -1;
        tourTimer = new System.Windows.Forms.Timer { Interval = 200 };
        tourTimer.Tick += (s, e) => TourTick();
        tourTimer.Start();
        TourGo(0);
    }

    // ---------- finding things on screen ----------
    // The first control on screen whose text (or tile title) contains this.
    Control FindShown(string text, Control root = null, bool clickable = false)
    {
        foreach (Control c in (root ?? this).Controls)
        {
            if (!c.Visible || c == coach || frame.Contains(c))
                continue;
            string t = c is HubTile ht ? ht.Title : c.Text;
            if (!string.IsNullOrEmpty(t) && t.Contains(text, StringComparison.OrdinalIgnoreCase) && c is not (Form or UserControl) && c.Width > 0
                && (!clickable || c is Button or HubTile))
                return c;
            var inner = FindShown(text, c, clickable);
            if (inner != null)
                return inner;
        }
        return null;
    }
    TC FindShown<TC>(Control root = null) where TC : Control
    {
        foreach (Control c in (root ?? this).Controls)
        {
            if (!c.Visible) continue;
            if (c is TC t) return t;
            var inner = FindShown<TC>(c);
            if (inner != null) return inner;
        }
        return null;
    }
    Rectangle? Rect(Control c)
    {
        if (c == null || c.Parent == null) return null;
        var r = RectangleToClient(c.Parent.RectangleToScreen(c.Bounds));
        var page = RectangleToClient(editorHost.RectangleToScreen(editorHost.ClientRectangle));
        if (editorHost.Contains(c))
        {
            if (!page.IntersectsWith(r)) return null; // scrolled out of view
            r.Intersect(page);
        }
        return r;
    }
    Rectangle? T(string text) => Rect(FindShown(text, null, true) ?? FindShown(text)); // a button with that text first
    Rectangle? Page() => Rect(editorHost);
    Rectangle? Pic() => Rect(FindShown<ControllerPicture>());
    bool Shows(string text) => FindShown(text) != null;
    // tests: a click on it (PerformClick skips buttons that can't take focus, e.g. while the window isn't active)
    void Press(string text)
    {
        var c = FindShown(text, null, true);
        if (c is HubTile t) t.PerformClick();
        else if (c != null) typeof(Control).GetMethod("OnClick", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)?.Invoke(c, new object[] { EventArgs.Empty });
    }

    // ---------- the tours ----------
    List<TourStep> BuildTour(string id)
    {
        TourStep S(string title, string text, Func<Rectangle?> target = null, Action enter = null, Func<bool> done = null, Action auto = null) =>
            new() { Title = title, Text = text, Target = target, Enter = enter, Done = done, Auto = auto };
        WheelModel FirstWheel() => Profile.Wheels.Values.FirstOrDefault(w => Profile.Bindings.Any(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == w.Name)) ?? Profile.Wheels.Values.FirstOrDefault();
        int Openers() => Profile.Bindings.Count(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == tWheel?.Name);
        void Refresh(object page) { GoTo(BindingsPage); GoTo(page); } // after a test changed the profile behind the page's back
        int openersAtStart = 0;

        // the steps that teach the main thing: a controller button that opens a wheel
        List<TourStep> OpenWheelSteps(Func<WheelModel> wheel) => new()
        {
            S("Your turn: open the 'Opened by' tab",
              "A wheel only shows up in VR when a controller button opens it.\n\nClick the 'Opened by' tab on the right.",
              () => T("Opened by"), () => { tWheel ??= wheel(); if (tWheel != null && !ReferenceEquals(lastTag, tWheel)) GoTo(tWheel); },
              () => Shows("+ Add a button that opens it"), () => Press("Opened by")),
            S("Your turn: pick the button",
              "Click '+ Add a button that opens it'. Choose the button (for example Right Thumbrest or Right B) and how you press it:\n\n" +
              "• Double-tap or Hold leaves a normal press for the game.\n• Press takes the button over completely.",
              () => T("+ Add a button that opens it"), () => openersAtStart = Openers(),
              () => Openers() > openersAtStart,
              () => { if (tWheel != null) { Profile.Bindings.Add(WheelPageEditor.Opener(tWheel, "right.secondary", GestureKind.Hold)); MarkDirty(); Refresh(tWheel); Press("Opened by"); } }),
            S("That's it: it opens in VR",
              "The new line under 'Opened by' is the button that opens this wheel. Its circle switches it on or off; ✕ deletes it; click it for timing and more.\n\n" +
              "In VR: do that with the controller, point at an item and pull the trigger (or let go, for a hold). Save first (Ctrl+S): a running game picks it up within a second.",
              () => T("Opened by")),
        };

        return id switch
        {
            "main" => new List<TourStep>
            {
                S("Welcome!", "This tour takes about five minutes. You'll make a radial wheel, put something on it, pick the controller button that opens it, and see every page.\n\nThe orange frame shows where to look. Steps marked 'Your turn' wait until you've done them.",
                  enter: () => { SetFolded(true); GoTo("overview"); }),
                S("The Overview", "What is on right now: the wheels on each controller, the wrist panels, the VR keyboard. The switches work right here; the blue buttons jump to where you edit each thing.", Page, () => GoTo("overview")),
                S("The icon bar", "Every page is one click away: Overview, Settings, Controllers, All bindings, Wheels, the left and right wrist panels (L, R), Tutorials (the cap) and Search.\n\n» at the top unfolds the full list, if you like it.", () => Rect(rail)),
                S("Your turn: make a wheel", "A wheel is a radial menu you open in VR. This page shows your wheels.\n\nClick the 'New wheel' card.",
                  () => T("New wheel"), () => { GoTo("wheels"); tCount = Profile.Wheels.Count; },
                  () => { if (Profile.Wheels.Count <= tCount) return false; tWheel = Profile.Wheels.Values.Last(); return true; },
                  () => Press("New wheel")),
                S("Your wheel", "This is your wheel's page: its picture on the left, the list of its items on the right. The name and title are at the top (the title is shown in the middle of the wheel in VR).",
                  Page, () => { if (tWheel != null && !ReferenceEquals(lastTag, tWheel)) GoTo(tWheel); }),
                S("Your turn: add an item", "Click '+ Add item'.", () => T("+ Add item"), () => tCount = tWheel?.Items.Count ?? 0,
                  () => tWheel != null && tWheel.Items.Count > tCount, () => Press("+ Add item")),
                S("What the item does", "Each item has a label and 'What it does'. Click the button next to 'What it does' for the choices: press a key, type a few keys, open the VR keyboard, show your desktop, open a sub-wheel...\n\nTo bind a key: 'Record key', then press it on your keyboard.",
                  () => T("What it does")),
            }.Concat(OpenWheelSteps(() => tWheel)).Concat(new List<TourStep>
            {
                S("Sub-wheels", "Too many items? '+ Add sub-wheel' makes a second wheel with a Back item, and puts a 'More' item on this one that opens it. The tabs at the top go between them.", () => T("+ Add sub-wheel")),
                S("Controllers", "Both controllers, with what each button does. Click a button (or its tag) to see and change it; the + on a tag adds something: press a key, swap with another button, a second job on hold, a double flick...",
                  Pic, () => GoTo(ControllersPage)),
                S("All bindings", "Everything you made, in one list: controller bindings, wheel items and wrist buttons. Switch any of them off and on, copy or delete them, or search.", Page, () => GoTo(BindingsPage)),
                S("Wrist panels", "Buttons on your forearm. They show when you turn your wrist toward your face and look at it; press them by poking with your other hand.\n\nThe list on the right switches buttons on and off; 'Advanced settings' places the panel on your arm.", Page, () => GoTo("wrist0")),
                S("Settings", "How wheels look and how you pick items, pointing, the VR keyboard, the screen mirror, controller offsets...", Page, () => GoTo(Profile)),
                S("Help on every page", "Each page's yellow tip bar has a 'Show me this page' button for a short tour of it. The Tutorials icon (the cap, on the left) and the 'Tutorial' button at the top have this tour again, and how-tos like sprinting with a double flick.", () => ToolItemRect("Tutorial")),
                S("Save", "Changes reach the game when you save (Ctrl+S). A running game picks them up within a second.\n\nThat's it! Click Finish.", () => ToolItemRect("Save")),
            }).ToList(),

            "open_wheel" => new List<TourStep>
            {
                S("Open a wheel from a controller button", "A wheel is opened by a controller button: for example double-tap the right thumbrest, or hold B. Let's set one up for one of your wheels.",
                  Page, () => { tWheel = FirstWheel(); if (tWheel != null) GoTo(tWheel); }),
            }.Concat(OpenWheelSteps(FirstWheel)).Concat(new List<TourStep>
            {
                S("Another way", "On the Controllers page you can also click a button, then '+ Add...' and 'Something else', and set 'What it does' to Radial wheel. 'Apply a controller preset...' sets the usual buttons for your controllers in one go.",
                  () => T("Apply a controller preset"), () => GoTo(ControllersPage)),
            }).ToList(),

            "sprint" => new List<TourStep>
            {
                S("Sprint with a double flick", "Flick the left stick forward twice quickly and keep pushing: your sprint key is held while you push. The game still gets the stick as usual, so you walk as normal.",
                  Pic, () => GoTo(ControllersPage)),
                S("Your turn: the left stick", "On the LEFT controller, click the + on the 'Thumbstick' tag. Pick 'Up', then 'Double flick: press a key', then 'Record key' and press your game's sprint key (often Left Shift).",
                  Pic, () => tCount = Profile.Bindings.Count(b => b.Gesture == GestureKind.DoubleTapHold),
                  () => Profile.Bindings.Count(b => b.Gesture == GestureKind.DoubleTapHold) > tCount,
                  () => { Profile.Bindings.Add(QuickBindings.DoubleFlickKey("left.thumbstick.up", "Left thumbstick: push up", "LShift")); MarkDirty(); Refresh(ControllersPage); }),
                S("Fine-tuning", "Click the Thumbstick tag again: the side panel shows your double flick. Click it for 'Keeps going until (%)': how far you can let the stick come back before sprinting stops (30 = until you mostly let go).\n\nThe same works on Down, Left and Right (a dodge, say).",
                  Pic),
            },

            "hold" => new List<TourStep>
            {
                S("A second job on hold", "A quick press of the button still does what the game normally does; holding it a moment presses a key you choose instead.", Pic, () => GoTo(ControllersPage)),
                S("Your turn", "Click the + on a button's tag (for example B on the right controller). Pick 'Hold: press a key', then 'Record key' and press the key (for example M for the map).",
                  Pic, () => tCount = Profile.Bindings.Count(b => b.Gesture == GestureKind.Hold),
                  () => Profile.Bindings.Count(b => b.Gesture == GestureKind.Hold) > tCount,
                  () => { Profile.Bindings.Add(QuickBindings.OnlyWhenHeld(QuickBindings.Key("right.secondary", "Right B", "M"))); MarkDirty(); Refresh(ControllersPage); }),
                S("How long is a hold?", "Click the tag again and then the new line: 'Hold time (ms)' sets how long counts as a hold. 'If I let go early, the game still gets a normal tap' keeps the quick press for the game.", Pic),
            },

            "swap" => new List<TourStep>
            {
                S("Swap two buttons", "Two buttons trade places in the game: each does what the other one did.", Pic, () => GoTo(ControllersPage)),
                S("Your turn", "Click the + on a button's tag, pick 'Swap with another button', then choose the other button.",
                  Pic, () => tCount = Profile.Bindings.Count(b => b.Action?.Type == ActionType.XrInput),
                  () => Profile.Bindings.Count(b => b.Action?.Type == ActionType.XrInput) >= tCount + 2,
                  () => { Profile.Bindings.Add(QuickBindings.GameButton("right.primary", "Right A", "right.secondary", "Right B")); Profile.Bindings.Add(QuickBindings.GameButton("right.secondary", "Right B", "right.primary", "Right A")); MarkDirty(); Refresh(ControllersPage); }),
                S("Undo it", "A swap is two bindings, one on each button. To undo it, 'Reset to default' both buttons in the side panel, or delete both lines in All bindings.", Pic),
            },

            "reuse" => new List<TourStep>
            {
                S("Reuse what you made", "Every wheel and wrist page has an 'All buttons' tab: everything in your profile, with a search box. '+ Add here' puts a copy on the page you're on.",
                  Page, () => { tWheel = FirstWheel(); if (tWheel != null) GoTo(tWheel); }),
                S("Your turn: open 'All buttons'", "Click the 'All buttons' tab on the right.", () => T("All buttons"), null,
                  () => Shows("+ Add here"), () => Press("All buttons")),
                S("Your turn: add one here", "Find one (try the search box) and click '+ Add here'. It's a copy: changing it later doesn't change the original.",
                  () => T("+ Add here"), () => tCount = tWheel?.Items.Count ?? 0,
                  () => tWheel != null && tWheel.Items.Count > tCount, () => Press("+ Add here")),
            },

            "screen" => new List<TourStep>
            {
                S("Your desktop on your wrist", "A wrist button can show a monitor or a window (Discord, a guide, a video) on your arm.", Page, () => GoTo("wrist0")),
                S("Your turn: add a button", "Click '+ Add button'.", () => T("+ Add button"),
                  () => tCount = Profile.WristFor(0).Pages.Values.Sum(p => p.Items.Count),
                  () => Profile.WristFor(0).Pages.Values.Sum(p => p.Items.Count) > tCount, () => Press("+ Add button")),
                S("Your turn: make it show the screen", "Click the button next to 'What it does' and pick 'Desktop screen'. Then 'Show it: on my left wrist', and the monitor (or type part of a window's name, like discord).",
                  () => T("What it does"), () => tCount = Profile.WristFor(0).Pages.Values.Sum(p => p.Items.Count(i => i.Action?.Type == ActionType.Screen)),
                  () => Profile.WristFor(0).Pages.Values.Sum(p => p.Items.Count(i => i.Action?.Type == ActionType.Screen)) > tCount,
                  () => { var it = Profile.WristFor(0).Pages.Values.SelectMany(p => p.Items).LastOrDefault(i => i.Label == "New button"); if (it != null) { it.Label = "Screen"; it.Action = new ActionModel { Type = ActionType.Screen, Mode = "toggle", Place = "wrist_left" }; } MarkDirty(); Refresh("wrist0"); }),
                S("More screen settings", "Settings > Screen mirror sets the frame rate, size and which monitor comes first. In VR the screen has its own button bar to switch monitors and windows.", Page),
            },

            "offsets" => new List<TourStep>
            {
                S("Hands and guns that point wrong", "Some games hold the guns or hands a little off. Controller offsets turn and move them until they match your real controllers.", () => T("Controller offsets"), () => GoTo(ControllersPage)),
                S("Your turn", "Click 'Controller offsets' (here on the Controllers page).", () => T("Controller offsets"), null,
                  () => lastTag is SettingsPage sp && sp.Key == "offsets", () => GoTo(SettingsPage.ByKey("offsets"))),
                S("Easiest in the headset", "The sliders work here, but it's easiest in VR: open the offsets panel from a wheel or wrist button ('Fix hand / gun aim'), grab and turn until it looks right. Reset puts it back.", Page),
            },

            // ---- one short tour per page
            "sec_overview" => new List<TourStep>
            {
                S("Overview", "Each card is one feature: is it on, and how you open it in VR.", Page, () => GoTo("overview")),
                S("Switches and jumps", "'Turn on' / 'Turn off' switch it right here. The blue buttons jump to where you edit it; Back (top left) returns here.", Page),
                S("Your profile", "The big Profile button at the top switches to another profile (each game can have its own). Rename and delete are in the toolbar.", () => T("Profile:")),
            },
            "sec_settings" => new List<TourStep>
            {
                S("Settings", "One tile per feature. Click a tile for its settings.", Page, () => GoTo(Profile)),
                S("Saved or straight away", "Cards tagged 'Saved with profile' reach the game when you Save (Ctrl+S). 'Applies straight away' ones work at once. Hover or click ⓘ for help on a setting.", Page),
            },
            "sec_controllers" => new List<TourStep>
            {
                S("Controllers", "Your controllers from above and from the side, with a tag on each button: blue = your bindings, grey 'Default' = the game's own control.", Pic, () => GoTo(ControllersPage)),
                S("The + on a tag", "Adds something to that button: press a key, swap with another button, a second job on hold, a double flick or double-tap, or anything else.", Pic),
                S("The side panel", "Click a button or its tag: the side panel lists each way of using it (press, touch, up...). There the circle switches a binding on or off, ⧉ copies, ✕ deletes, and 'Reset to default' gives the button back to the game.", Pic),
                S("Controller model", "Pick your controllers here. 'Apply a controller preset...' sets the usual buttons that open your wheels; 'Controller offsets' fixes how hands and guns point.", () => T("Apply a controller preset")),
            },
            "sec_bindings" => new List<TourStep>
            {
                S("All bindings", "Everything in this profile: what your controller buttons do, every wheel item and every wrist button.", Page, () => GoTo(BindingsPage)),
                S("Switch, copy, delete", "The circle switches one on or off (kept here when off), ⧉ copies it, ✕ deletes it. Click a line for all its settings.", Page),
                S("Find it", "Search by name, key or button. 'Show' limits the list; 'Sort' puts switched-on ones first or sorts by name.", () => T("Search:")),
            },
            "sec_wheels" => new List<TourStep>
            {
                S("Wheels", "One card per wheel, with what opens it. Click a card to open the wheel's page.", Page, () => GoTo("wheels")),
                S("A new wheel", "The 'New wheel' card makes one. Then add items, and pick the button that opens it under 'Opened by'.", () => T("New wheel")),
            },
            "sec_wheel" => new List<TourStep>
            {
                S("A wheel", "Click an item in the picture or in the list to change it; drag one item onto another to swap them. '+ Add item' adds one.",
                  Page, () => { if (lastTag is not WheelModel) { tWheel = FirstWheel(); if (tWheel != null) GoTo(tWheel); } }),
                S("Sub-wheels", "The tabs at the top are this wheel and its sub-wheels. '+ Add sub-wheel' makes one with a Back item, and a 'More' item here that opens it.", () => T("+ Add sub-wheel")),
                S("The tabs on the right", "'Items on this wheel': ≡ drags to reorder, the circle switches an item on or off. 'Opened by' shows and adds the buttons that open this wheel. 'All buttons' copies anything you made onto it.", () => T("Items on this wheel")),
                S("Keys", "With an item chosen: Del deletes it, Ctrl+D copies it, Alt+← / → move it.", Page),
            },
            "sec_wrist" => new List<TourStep>
            {
                S("Wrist panel", "Buttons on your forearm. Turn your wrist toward your face and look at it to see them; poke with your other hand, or point and pull the trigger.", Page, () => { if (lastTag as string is not ("wrist0" or "wrist1")) GoTo("wrist0"); }),
                S("Pages", "The tabs are its pages, in the order you go through them. '+ Add page' makes one, with a 'Next page' button to it.", () => T("+ Add page")),
                S("Buttons", "Click a button in the picture or the list to change it; drag one onto another to swap them. The circle in the list switches a button on or off.", () => T("Buttons on this page")),
                S("Advanced settings", "Where the panel sits on your arm, how big it is, when it shows, and 'Reset wrist panel location' if you lose it.", () => T("Advanced settings")),
            },
            "sec_binding" => new List<TourStep>
            {
                S("A binding", "The line at the top says it in one sentence: what you do with which button, and what happens.", Page,
                  () => { if (lastTag is not BindingModel && Profile.Bindings.FirstOrDefault() is BindingModel b) GoTo(b); }),
                S("When you use the button", "The button (add another to make a combo), how you press it (press, hold, double-tap, double flick...), its timing, and whether the game still gets the button.", () => T("When you use the button")),
                S("What it does", "Click the button next to 'What it does' for the choices; the settings for the one you pick show below it.", () => T("What it does")),
            },
            _ => null,
        };
    }

    Rectangle? ToolItemRect(string startsWith)
    {
        var ts = Controls.OfType<ToolStrip>().FirstOrDefault();
        var it = ts?.Items.Cast<ToolStripItem>().FirstOrDefault(i => i.Text?.StartsWith(startsWith) == true);
        return it == null ? null : RectangleToClient(ts.RectangleToScreen(it.Bounds));
    }

    // ---------- coach card ----------
    void BuildCoach()
    {
        coach = new Panel { Size = new Size(460, 280), BackColor = Color.FromArgb(28, 36, 52), Padding = new Padding(16) };
        coachTitle = new Label { Dock = DockStyle.Top, Height = 32, Font = new Font("Segoe UI", 13f, FontStyle.Bold), ForeColor = Color.White };
        coachText = new Label { Dock = DockStyle.Fill, Font = new Font("Segoe UI", 10.5f), ForeColor = Color.FromArgb(220, 228, 240) };
        var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 40, FlowDirection = FlowDirection.RightToLeft, BackColor = Color.Transparent };
        Button B(string t, Color bg, Action a)
        {
            var b = new Button { Text = t, AutoSize = true, Height = 32, FlatStyle = FlatStyle.Flat, BackColor = bg, ForeColor = Color.White, Font = new Font("Segoe UI", 10f, FontStyle.Bold), Cursor = Cursors.Hand, Margin = new Padding(6, 0, 0, 0) };
            b.FlatAppearance.BorderSize = 0;
            b.Click += (s, e) => a();
            return b;
        }
        coachNext = B("Next  ▶", Color.FromArgb(240, 140, 30), () => { if (tourAt >= tour.Count - 1) EndTutorial(true); else TourGo(tourAt + 1); });
        coachBack = B("◀  Back", Color.FromArgb(70, 82, 104), () => TourGo(Math.Max(0, tourAt - 1)));
        var skip = B("Stop", Color.FromArgb(70, 82, 104), () => EndTutorial(false));
        coachCount = new Label { AutoSize = true, ForeColor = Color.FromArgb(150, 160, 180), Margin = new Padding(0, 8, 10, 0) };
        bar.Controls.AddRange(new Control[] { coachNext, coachBack, skip, coachCount });
        coach.Controls.Add(coachText);
        coach.Controls.Add(bar);
        coach.Controls.Add(coachTitle);
        Controls.Add(coach);
        for (int i = 0; i < 4; i++)
        { frame[i] = new Panel { BackColor = Color.FromArgb(255, 140, 20), Visible = false }; Controls.Add(frame[i]); frame[i].BringToFront(); }
        coach.BringToFront();
        Resize += TourLayout;
    }

    void TourGo(int i)
    {
        tourAt = i;
        var st = tour[i];
        try { st.Enter?.Invoke(); } catch { }
        coachTitle.Text = st.Title;
        coachText.Text = st.Text;
        coachCount.Text = $"{i + 1} / {tour.Count}";
        coachBack.Enabled = i > 0;
        TourTick();
    }

    void TourTick()
    {
        if (coach == null || tourAt < 0)
            return;
        var st = tour[tourAt];
        bool waiting = st.Done != null && !SafeDone(st);
        // a "your turn" step that is done moves on by itself (the last one just offers Finish)
        if (st.Done != null && !waiting && coachNext.Tag as string != "done" && tourAt + 1 < tour.Count)
        { coachNext.Tag = "done"; TourGo(tourAt + 1); coachNext.Tag = null; return; }
        coachNext.Enabled = !waiting;
        coachNext.Text = waiting ? "Waiting for you..." : tourAt >= tour.Count - 1 ? "Finish  ✓" : "Next  ▶";
        TourLayout(null, null);
    }

    static bool SafeDone(TourStep st) { try { return st.Done(); } catch { return false; } }

    void TourLayout(object s, EventArgs e)
    {
        if (coach == null)
            return;
        Rectangle? t = null;
        try { t = tour[tourAt].Target?.Invoke(); } catch { }
        // the coach card in the bottom corner away from the target
        bool targetRight = t is Rectangle tr && tr.X + tr.Width / 2 > ClientSize.Width / 2 && tr.Width < ClientSize.Width * 0.6;
        int x = targetRight ? 90 : ClientSize.Width - coach.Width - 24;
        coach.Location = new Point(x, ClientSize.Height - coach.Height - 34);
        const int w = 4;
        if (t is Rectangle r)
        {
            r.Inflate(3, 3);
            frame[0].Bounds = new Rectangle(r.X - w, r.Y - w, r.Width + 2 * w, w);
            frame[1].Bounds = new Rectangle(r.X - w, r.Bottom, r.Width + 2 * w, w);
            frame[2].Bounds = new Rectangle(r.X - w, r.Y, w, r.Height);
            frame[3].Bounds = new Rectangle(r.Right, r.Y, w, r.Height);
            foreach (var f in frame)
            { f.Visible = true; f.BringToFront(); }
        }
        else
            foreach (var f in frame)
                f.Visible = false;
        coach.BringToFront();
    }

    void EndTutorial(bool finished)
    {
        if (coach == null)
            return;
        tourTimer?.Stop();
        tourTimer?.Dispose();
        tourTimer = null;
        Resize -= TourLayout;
        Controls.Remove(coach);
        coach.Dispose();
        coach = null;
        foreach (var f in frame)
        { Controls.Remove(f); f?.Dispose(); }
        bool changed = Serialize() != tourSnapshot;
        if (changed && MessageBox.Show(this, (finished ? "Nice work! " : "") + "Keep what you made during the tour?\n\nChoose No to put your profile back the way it was before the tour.",
                "Tutorial", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.No)
        {
            var tmp = Path.Combine(Path.GetTempPath(), "vro_tour_restore.json");
            File.WriteAllText(tmp, tourSnapshot);
            LoadFile(tmp);
            currentFile = tourFile;
            Text = "OpenXR Companion - Profile Editor - " + Path.GetFileName(currentFile);
            if (tourWasDirty)
                MarkDirty();
            else
                dirty = false;
            SetStatus("Tour changes thrown away.", false);
            GoTo("overview");
        }
    }

    // Test: run every tour on the built-in defaults, doing each "your turn" step, and screenshot every step.
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    public static void TourShots(string dir)
    {
        Directory.CreateDirectory(dir);
        var f = new MainForm { StartPosition = FormStartPosition.Manual, Location = new Point(0, 0), Size = new Size(1920, 1040) };
        f.Show();
        var builtin = Path.Combine(Path.GetTempPath(), "vro_builtin_default.json");
        File.WriteAllText(builtin, Native.DefaultProfile() ?? "{}");
        void Pump()
        { for (int i = 0; i < 8; i++) Application.DoEvents(); }
        var log = new List<string>();
        int stuck = 0;
        foreach (var info in Tours)
        {
            f.LoadFile(builtin);
            Pump();
            f.StartTutorial(info.Id);
            log.Add($"== {info.Id}");
            for (int guard = 0; guard < 40 && f.coach != null; guard++)
            {
                Pump();
                f.TourTick();
                Pump();
                int at = f.tourAt;
                var st = f.tour[at];
                Rectangle? t = null;
                try { t = st.Target?.Invoke(); } catch { }
                bool waiting = st.Done != null && !SafeDone(st);
                log.Add($"{at + 1} {st.Title}  target={(t?.ToString() ?? "NONE")}  waiting={waiting}");
                using (var bmp = new Bitmap(f.Width, f.Height))
                { using (var g = Graphics.FromImage(bmp)) { var dc = g.GetHdc(); PrintWindow(f.Handle, dc, 2); g.ReleaseHdc(dc); } bmp.Save(Path.Combine(dir, $"{info.Id}_{at + 1:00}.png")); }
                if (waiting)
                {
                    st.Auto?.Invoke();
                    Pump();
                    f.TourTick();
                    Pump();
                    if (f.tourAt == at)
                    {
                        if (at >= f.tour.Count - 1 && SafeDone(st)) { log.Add("finished"); break; } // the last step, done
                        log.Add("  STUCK"); stuck++; break;
                    }
                    continue;
                }
                if (at >= f.tour.Count - 1)
                { log.Add("finished"); break; }
                f.TourGo(at + 1);
            }
            if (f.coach != null)
            { f.tourSnapshot = f.Serialize(); f.EndTutorial(false); }
            f.dirty = false;
        }
        log.Add(stuck == 0 ? "ALL TOURS FINISHED" : stuck + " STUCK");
        File.WriteAllLines(Path.Combine(dir, "tour.txt"), log);
        f.dirty = false;
        f.Close();
    }
}
