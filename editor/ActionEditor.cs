using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// ------------------------------------------------------------ action editor ("then do")
public class ActionEditor : FormPanel
{
    public event Action Changed;
    ActionModel a;
    bool loading;
    readonly bool inWheel;
    readonly ChoiceBox type;
    readonly ComboBox page = new() { Width = 220, DropDownStyle = ComboBoxStyle.DropDownList };
    readonly Label lPage;
    readonly bool inWrist;
    readonly WristModel wristOwner;
    readonly KeyBox keys = new();
    readonly ChoiceBox keyMode = new(Choices.KeyModes, 300);
    static readonly ToolTip stepsTip = new();
    readonly TextBox steps = new() { Width = 420, Multiline = true, Height = 70, ScrollBars = ScrollBars.Vertical };
    readonly ComboBox wheel = new() { Width = 220, DropDownStyle = ComboBoxStyle.DropDownList };
    readonly ChoiceBox confirm = new(Choices.Confirms, 360);
    readonly ComboBox layer = new() { Width = 220 };
    readonly ChoiceBox layerMode = new(Choices.LayerModes, 220);
    readonly ChordEditor target = new();
    readonly ChoiceBox screenMode = new(Choices.ScreenModes, 420);
    readonly ChoiceBox screenPlace = new(Choices.ScreenPlaces, 360);
    readonly ChoiceBox screenMon = new(Monitors.Choices(true), 360);
    readonly ComboBox screenWin = new() { Width = 260 };
    readonly ChoiceBox offMode = new(Choices.OffsetModes, 460);
    Label lOffMode;
    readonly ChoiceBox mirMode = new(Choices.MirrorModes, 300);
    Label lMirMode;
    readonly Label lKeys, lKeyMode, lSteps, lWheel, lConfirm, lLayer, lLayerMode, lTarget, lScreenMode, lScreenPlace, lScreenMon, lScreenWin;

    // What a wrist button can do, in groups, worded short. Same picker as the controllers' "+ Add".
    static readonly (string code, PickOption option)[] WristActions =
    {
        (ActionType.Key, new("Keys", "Press a key", "A keyboard key, key combo or mouse button.", "quick save, map, push-to-talk.")),
        (ActionType.Sequence, new("Keys", "Type a few keys in a row", "Several keys one after another, with waits if needed (a macro).", "copy-paste, a chat command.")),
        (ActionType.XrInput, new("Keys", "Press a controller button", "The game thinks one of its controller buttons is pressed.", "a game action that has no keyboard key.")),
        (ActionType.Page, new("This panel", "Go to another page", "Shows another page of this wrist panel.", "a 'Next page' button.")),
        (ActionType.Back, new("This panel", "Go back a page", "Back to the page before. On the first page it hides the panel.", null)),
        (ActionType.Close, new("This panel", "Hide the panel", "Hides it until you look away from your arm.", null)),
        (ActionType.Keyboard, new("Open", "VR keyboard", "Opens the keyboard you type on in the headset.", "chat, names, searching.")),
        (ActionType.Screen, new("Open", "Desktop screen", "Shows a monitor or window in the headset, or switches which one.", "Discord, a guide, a video.")),
        (ActionType.Wheel, new("Open", "Radial wheel", "Opens one of your radial wheels.", null)),
        (ActionType.SettingsMenu, new("Open", "Settings menu", "Change bindings and sticks inside the headset.", null)),
        (ActionType.Adjust, new("Set up", "Move panels and wheels", "Grab wrist panels and wheels and put them where you like.", null)),
        (ActionType.ControllerOffsets, new("Set up", "Fix hand / gun aim", "Opens the controller offsets, for games whose hands point wrong.", null)),
        (ActionType.HeadsetMirror, new("Set up", "Headset view on the desktop", "A window on the monitor showing what the headset shows.", "streaming or showing someone.")),
        (ActionType.Reload, new("Set up", "Reload profile", "Loads the profile again after you saved changes.", null)),
        (ActionType.Layer, new("Advanced", "Switch a layer", "Changes what other buttons do while the layer is on.", null)),
        (ActionType.None, new("Advanced", "Nothing", "The button is shown but does nothing.", null)),
    };

    // The same for a radial wheel's items: "This wheel" instead of "This panel".
    static readonly (string code, PickOption option)[] WheelActions = WristActions
        .Where(x => x.code is not (ActionType.Page or ActionType.Back or ActionType.Close or ActionType.Wheel))
        .SelectMany(x => x.code == ActionType.Keyboard
            ? new[] {
                (ActionType.Wheel, new PickOption("This wheel", "Open a sub-wheel", "Opens another wheel in its place (or any of your wheels).", "a 'More' item.")),
                (ActionType.Back, new PickOption("This wheel", "Go back a level", "Back to the wheel before; on the first wheel it closes.", null)),
                (ActionType.Close, new PickOption("This wheel", "Close the wheel", "Closes it without doing anything else.", null)),
                x }
            : new[] { x })
        .Select(x => (code: x.Item1, option: x.Item2))
        .Select(x => x.code == ActionType.None ? (x.code, x.option with { Does = "The item is shown but does nothing." }) : x)
        .ToArray();

    public static void WristPickerShot(string path) => OptionPicker.Shot(path, "What should this button do?", "Pick one; you can change it any time.", WristActions.Select(x => x.option).ToList());

    // The wrist action: a button with the current choice; clicking it opens the cards.
    Control WristPicker()
    {
        var acts = inWrist ? WristActions : WheelActions;
        string what = inWrist ? "this button" : "this item";
        var b = new Button { AutoSize = true, MinimumSize = new Size(300, 36), FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, TextAlign = ContentAlignment.MiddleLeft,
                             Font = new Font("Segoe UI", 10f, FontStyle.Bold), BackColor = Theme.Button, ForeColor = Theme.Text, Tag = "swatch", Padding = new Padding(6, 0, 6, 0) };
        b.FlatAppearance.BorderColor = Theme.CardBorder;
        void Show() => b.Text = (acts.FirstOrDefault(x => x.code == type.Code).option?.Title ?? type.Text) + "   ▾  change";
        type.SelectedIndexChanged += (s, e) => Show();
        b.Click += (s, e) =>
        {
            int i = OptionPicker.Show(FindForm(), $"What should {what} do?", "Pick one; you can change it any time.", acts.Select(x => x.option).ToList());
            if (i >= 0)
                type.Code = acts[i].code; // the type box's own handler sets the action up
        };
        Show();
        return b;
    }

    public ActionEditor(bool inWheel, WristModel wrist = null)
    {
        this.inWheel = inWheel;
        this.inWrist = wrist != null;
        wristOwner = wrist;
        var types = Choices.ActionTypes.Where(c => c.Code != ActionType.Page).ToList();
        if (wrist != null)
        {
            types = types.Select(c => c.Code switch
            {
                "back" => new Choice("back", "Wrist panel: go back a page (hides it on the first page)"),
                "close" => new Choice("close", "Wrist panel: hide it until I look away"),
                "wheel" => new Choice("wheel", "Open a radial wheel"),
                _ => c,
            }).ToList();
            types.Insert(3, new Choice("page", "Wrist panel: open another page"));
        }
        type = new ChoiceBox(types.ToArray(), 360);
        if (wrist != null || inWheel)
            AddRow("What it does", WristPicker()); // big grouped cards instead of the long dropdown
        else
            AddRow("Action", type);
        lKeys = AddRow("Key", keys, "Click Record key, then press the key or combo on your keyboard. Or pick a mouse button.");
        lKeyMode = AddRow("Key behavior", keyMode);
        lSteps = AddRow("Macro steps", steps, "One step per line or split with ;  e.g.\nCtrl+C\nwait 100\nCtrl+V\ndown W / up W to hold a key");
        lWheel = AddRow("Wheel", wheel);
        lPage = AddRow("Page", page, "Its buttons replace these; a Back button is added automatically.");
        lConfirm = AddRow("Pick items by", confirm);
        lLayer = AddRow("Layer name", layer, "Type a new name or pick one you already use.");
        lLayerMode = AddRow("Layer", layerMode);
        lTarget = AddRow("Controller button", target, "The game will think this controller button is pressed.");
        lTarget.Text = "Game button";
        lScreenMode = AddRow("Screen", screenMode, "Settings > Screen mirror sets the monitor, frame rates, size and position.");
        lScreenPlace = AddRow("Show it", screenPlace);
        lScreenMon = AddRow("Monitor", screenMon, "Monitors are numbered from left to right.");
        lScreenWin = AddRow("Or a window", screenWin, "Leave empty for a monitor. Type part of a window's title or a program name (e.g. chrome, Discord), or pick one that's open now. The first matching window is shown.");
        lOffMode = AddRow("Offsets", offMode, "Settings > Controller offsets has the numbers, sliders and reset buttons too.");
        lMirMode = AddRow("Headset mirror", mirMode, "The window shows on the desktop; the editor's toolbar has the same switch, and Settings > Headset mirror sets eye, zoom and frame rate.");
        foreach (var (_, ctl, _) in Rows)
            if (ctl is ComboBox)
                ctl.Width = 400; // one width for every dropdown here, so they line up
        mirMode.SelectedIndexChanged += (s, e) => { if (!loading) { a.Mode = mirMode.Code; Fire(); } };
        offMode.SelectedIndexChanged += (s, e) => { if (!loading) { a.Mode = offMode.Code; Fire(); } };
        screenWin.TextChanged += (s, e) => { if (!loading) { a.Window = string.IsNullOrWhiteSpace(screenWin.Text) ? null : screenWin.Text.Trim(); Fire(); } };
        screenMode.SelectedIndexChanged += (s, e) => { if (!loading) { a.Mode = screenMode.Code; Sync(); Fire(); } };
        screenPlace.SelectedIndexChanged += (s, e) => { if (!loading) { a.Place = string.IsNullOrEmpty(screenPlace.Code) ? null : screenPlace.Code; Fire(); } };
        screenMon.SelectedIndexChanged += (s, e) => { if (!loading) { a.Monitor = int.TryParse(screenMon.Code, out var m) ? m : null; Fire(); } };

        type.SelectedIndexChanged += (s, e) => { if (loading) return; a.Type = type.Code; Defaults(); Sync(); Fire(); };
        keys.Changed += () => { if (!loading) { a.Keys = keys.Value; Fire(); } };
        keyMode.SelectedIndexChanged += (s, e) => { if (!loading) { a.Mode = keyMode.Code; Fire(); } };
        steps.TextChanged += (s, e) =>
        {
            var err = Native.CheckKeys(string.Join("; ", steps.Text.Split((char)10).Select(x => x.Trim((char)13))), true);
            steps.BackColor = err == null ? Theme.InputBack : Theme.ErrorBack;
            stepsTip.SetToolTip(steps, err == null ? null : "Unknown bind: " + err);
        };
        steps.TextChanged += (s, e) => { if (!loading) { a.Steps = steps.Text.Replace("\r\n", "; ").Replace("\n", "; "); Fire(); } };
        wheel.SelectedIndexChanged += (s, e) => { if (!loading) { a.Wheel = wheel.SelectedItem as string; Fire(); } };
        page.SelectedIndexChanged += (s, e) => { if (!loading) { a.Page = page.SelectedItem as string; Fire(); } };
        confirm.SelectedIndexChanged += (s, e) => { if (!loading) { a.Confirm = confirm.Code; Fire(); } };
        layer.TextChanged += (s, e) => { if (!loading) { a.Layer = layer.Text.Trim(); Fire(); } };
        layerMode.SelectedIndexChanged += (s, e) => { if (!loading) { a.Mode = layerMode.Code; Fire(); } };
        target.Changed += () => { if (!loading) { a.Target = target.Value.Split('+').Last(); Fire(); } };
    }
    void Fire() { if (!loading) Changed?.Invoke(); }

    void Defaults()
    {
        switch (a.Type)
        {
            case ActionType.Key:
                a.Keys ??= "F1";
                if (a.Mode is not ("tap" or "hold" or "toggle"))
                    a.Mode = null;
                break;
            case ActionType.Wheel:
                a.Wheel ??= MainForm.Current.Profile.Wheels.Keys.FirstOrDefault();
                break;
            case ActionType.Page:
                a.Page ??= wristOwner?.Pages.Keys.FirstOrDefault();
                break;
            case ActionType.Layer:
                a.Layer ??= "shift";
                if (a.Mode is not ("hold" or "toggle" or "on" or "off"))
                    a.Mode = inWheel ? "toggle" : "hold";
                break;
            case ActionType.XrInput:
                a.Target ??= "left.menu";
                break;
            case ActionType.Screen:
                if (Choices.ScreenModes.All(c => c.Code != a.Mode))
                    a.Mode = "toggle";
                break;
            case ActionType.HeadsetMirror:
                if (Choices.MirrorModes.All(c => c.Code != a.Mode))
                    a.Mode = "toggle";
                break;
            case ActionType.ControllerOffsets:
                if (Choices.OffsetModes.All(c => c.Code != a.Mode))
                    a.Mode = "panel";
                break;
        }
    }

    public void Bind(ActionModel model)
    {
        loading = true;
        a = model;
        type.Code = a.Type ?? "none";
        keys.Value = a.Keys;
        keyMode.Code = string.IsNullOrEmpty(a.Mode) || a.Type != ActionType.Key ? "tap" : a.Mode;
        steps.Text = (a.Steps ?? "").Replace("; ", "\r\n").Replace(";", "\r\n");
        wheel.Items.Clear();
        foreach (var w in MainForm.Current.Profile.Wheels.Keys)
            wheel.Items.Add(w);
        wheel.SelectedItem = a.Wheel;
        page.Items.Clear();
        if (wristOwner != null)
            foreach (var pg in wristOwner.Pages.Keys)
                page.Items.Add(pg);
        page.SelectedItem = a.Page;
        confirm.Code = string.IsNullOrEmpty(a.Confirm) ? "auto" : a.Confirm;
        layer.Items.Clear();
        foreach (var l in new LayerConv().GetStandardValues(null))
            layer.Items.Add(l);
        layer.Text = a.Layer ?? "";
        layerMode.Code = a.Type == ActionType.Layer && !string.IsNullOrEmpty(a.Mode) ? a.Mode : (inWheel ? "toggle" : "hold");
        target.Value = a.Target ?? "left.menu";
        screenMode.Code = a.Type == ActionType.Screen && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "toggle";
        screenPlace.Code = a.Place ?? "";
        mirMode.Code = a.Type == ActionType.HeadsetMirror && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "toggle";
        offMode.Code = a.Type == ActionType.ControllerOffsets && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "panel";
        screenMon.Code = a.Monitor?.ToString() ?? "";
        screenWin.Items.Clear();
        OpenWindows.FillOnOpen(screenWin);
        screenWin.Text = a.Window ?? "";
        loading = false;
        Sync();
    }

    void Sync()
    {
        string t = a.Type;
        Show(lKeys, t == ActionType.Key);
        Show(lKeyMode, t is ActionType.Key or ActionType.XrInput);
        Show(lSteps, t == ActionType.Sequence);
        Show(lWheel, t == ActionType.Wheel);
        Show(lConfirm, t == ActionType.Wheel && (!inWheel || inWrist));
        Show(lPage, t == ActionType.Page);
        Show(lLayer, t == ActionType.Layer);
        Show(lLayerMode, t == ActionType.Layer);
        Show(lTarget, t == ActionType.XrInput);
        bool turnsOn = t == ActionType.Screen && (a.Mode is null or "toggle" or "on");
        Show(lOffMode, t == ActionType.ControllerOffsets);
        Show(lMirMode, t == ActionType.HeadsetMirror);
        Show(lScreenMode, t == ActionType.Screen);
        Show(lScreenPlace, turnsOn);
        Show(lScreenMon, turnsOn);
        Show(lScreenWin, turnsOn);
        if (t == ActionType.XrInput)
        { keyMode.Code = string.IsNullOrEmpty(a.Mode) ? "tap" : a.Mode; }
    }
}
