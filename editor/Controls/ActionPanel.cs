using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// "What happens": what an action does and its settings. Port of the WinForms ActionEditor, for a controller binding
// ("binding"), a wheel item ("wheel") or a wrist button ("wrist"). Only the rows that matter for the chosen type show.
public class ActionPanel : Form
{
    public event Action Changed;
    readonly string kind;
    readonly WristModel wristOwner;
    readonly ProfileModel profile = Shell.Store.Profile;
    ActionModel a;
    bool loading;

    // what it does now, in a box of its own, and a separate button to pick something else
    readonly TextBlock whatText = new() { FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center };
    readonly Button what = new() { Content = "Change…", Classes = { "pill" } };
    readonly KeyField keys = new();
    readonly ChoiceCombo keyMode = new(Choices.KeyModes);

    // Key behavior, by where the action is. On a controller button the key is held while the button is (layer: a tap
    // on a binding acts like the real key); on a wheel item or panel button nothing stays held, so it is one quick press.
    // 'hold' is the same as tap now: offered only when an action already has it (so a profile shows what it says).
    static readonly Choice ToggleChoice = new("toggle", "Toggle  (press once = held down, press again = let go)");
    Choice[] KeyModeList(bool withHold) => kind is "wheel" or "wrist"
        ? (withHold ? new[] { new Choice("tap", "Tap  (one quick key press)"), new Choice("hold", "Hold  (old setting, the same as Tap here)"), ToggleChoice }
                    : new[] { new Choice("tap", "Tap  (one quick key press)"), ToggleChoice })
        : (withHold ? new[] { new Choice("tap", "Press  (like the real key: held while you hold the button)"), new Choice("hold", "Hold  (old setting, the same as Press)"), ToggleChoice }
                    : new[] { new Choice("tap", "Press  (like the real key: held while you hold the button)"), ToggleChoice });
    readonly TextBox steps = new() { Width = 420, Height = 80, AcceptsReturn = true, TextWrapping = TextWrapping.Wrap };
    readonly ComboBox wheel = new() { Width = 400 };
    readonly ComboBox page = new() { Width = 400 };
    readonly ChoiceCombo confirm = new(Choices.Confirms);
    readonly AutoCompleteBox layer = new() { Width = 400, FilterMode = AutoCompleteFilterMode.StartsWith };
    readonly ChoiceCombo layerMode = new(Choices.LayerModes);
    readonly ChordPicker target = new();
    readonly ChoiceCombo screenMode = new(Choices.ScreenModes, 420);
    readonly ChoiceCombo screenPlace = new(Choices.ScreenPlaces);
    readonly ChoiceCombo screenMon = new(Monitors.Choices(true));
    readonly TextBox screenWin = new() { Width = 400, Watermark = "empty = a monitor; or part of a window title / program name" };
    readonly ChoiceCombo offMode = new(Choices.OffsetModes, 460);
    readonly ChoiceCombo mirMode = new(Choices.MirrorModes, 300);
    readonly Control lKeys, lKeyMode, lSteps, lWheel, lPage, lConfirm, lLayer, lLayerMode, lTarget, lScreenMode, lScreenPlace, lScreenMon, lScreenWin, lOffMode, lMirMode;

    public ActionPanel(string kind, WristModel wrist = null)
    {
        this.kind = kind;
        wristOwner = wrist;
        LabelWidth = 130; // it sits in a narrow card
        AddRow("What it does", new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 8,
            Children =
            {
                new Border { Child = whatText, MinWidth = 220, Padding = new Thickness(10, 6), CornerRadius = new CornerRadius(4),
                             Background = Brush.Parse("#2A2F37"), BorderBrush = Brush.Parse("#4A5260"), BorderThickness = new Thickness(1) },
                what,
            },
        });
        ToolTip.SetTip(what, "Pick what it does: press a key, open a wheel, show the desktop...");
        lKeys = AddRow("Key", keys, "Click Record key, then press the key or combo on your keyboard. Or pick a mouse button.");
        lKeyMode = AddRow("Key behavior", keyMode);
        var addStep = new Button { Content = "⌨ Add a key…" };
        addStep.Click += async (s, e) =>
        {
            var k = await KeyboardPicker.Show(this, null);
            if (k == null)
                return;
            string t = (steps.Text ?? "").TrimEnd('\r', '\n');
            steps.Text = t.Length == 0 ? k : t + "\n" + k;
        };
        lSteps = AddRow("Macro steps", new StackPanel { Spacing = 4, Children = { steps, addStep } }, "One step per line or split with ;  e.g.\nCtrl+C\nwait 100\nCtrl+V\ndown W / up W to hold a key");
        lWheel = AddRow("Wheel", wheel);
        lPage = AddRow("Page", page, "Its buttons replace these; a Back button is added automatically.");
        lConfirm = AddRow("Pick items by", confirm);
        lLayer = AddRow("Layer name", layer, "Type a new name or pick one you already use.");
        lLayerMode = AddRow("Layer", layerMode);
        lTarget = AddRow("Game button", target, "The game will think this controller button is pressed.");
        lScreenMode = AddRow("Screen", screenMode, "Settings > Screen mirror sets the monitor, frame rates, size and position.");
        lScreenPlace = AddRow("Show it", screenPlace);
        lScreenMon = AddRow("Monitor", screenMon, "Monitors are numbered from left to right.");
        lScreenWin = AddRow("Or a window", screenWin, "Leave empty for a monitor. Type part of a window's title or a program name (e.g. chrome, Discord). The first matching window is shown.");
        lOffMode = AddRow("Offsets", offMode, "Settings > Controller offsets has the numbers, sliders and reset buttons too.");
        lMirMode = AddRow("Headset mirror", mirMode, "The window shows on the desktop; Settings > Headset mirror sets eye, zoom and frame rate.");

        what.Click += async (s, e) =>
        {
            var opts = EditorApi.ActionOptions(kind);
            string thing = kind == "wrist" ? "this button" : kind == "wheel" ? "this item" : "this binding";
            int i = await OptionPicker.Show(this, $"What should {thing} do?", "Pick one; you can change it any time.", opts.Select(o => o.option).ToList());
            if (i >= 0)
                Pick(opts[i].code);
        };
        keys.Changed += () => { if (!loading && !Same(a.Keys, keys.Value)) { a.Keys = keys.Value; Fire(); } };
        keyMode.SelectionChanged += (s, e) => { if (!loading && keyMode.Code != null && !Same(a.Mode ?? "tap", keyMode.Code)) { a.Mode = keyMode.Code; Fire(); } };
        steps.TextChanged += (s, e) =>
        {
            string text = steps.Text ?? "";
            var err = EditorApi.CheckKeys(string.Join("; ", text.Split('\n').Select(x => x.Trim('\r'))), true);
            steps.Background = err == null ? Brush.Parse("#2A2F37") : Brush.Parse("#5A2A2A");
            ToolTip.SetTip(steps, err == null ? null : "Unknown bind: " + err);
            string joined = text.Replace("\r\n", "; ").Replace("\n", "; ");
            if (!loading && !Same(a.Steps, joined)) { a.Steps = joined; Fire(); }
        };
        wheel.SelectionChanged += (s, e) => { if (!loading && wheel.SelectedItem is string w && a.Wheel != w) { a.Wheel = w; Fire(); } };
        page.SelectionChanged += (s, e) => { if (!loading && page.SelectedItem is string pg && a.Page != pg) { a.Page = pg; Fire(); } };
        confirm.SelectionChanged += (s, e) => { if (!loading && confirm.Code != null && !Same(a.Confirm ?? "auto", confirm.Code)) { a.Confirm = confirm.Code; Fire(); } };
        layer.TextChanged += (s, e) => { if (!loading && !Same(a.Layer, layer.Text)) { a.Layer = (layer.Text ?? "").Trim(); Fire(); } };
        layerMode.SelectionChanged += (s, e) => { if (!loading && layerMode.Code != null && a.Mode != layerMode.Code) { a.Mode = layerMode.Code; Fire(); } };
        target.Changed += () => { string t = target.Value.Split('+').Last(); if (!loading && a.Target != t) { a.Target = t; Fire(); } };
        screenMode.SelectionChanged += (s, e) => { if (!loading && screenMode.Code != null && a.Mode != screenMode.Code) { a.Mode = screenMode.Code; Sync(); Fire(); } };
        screenPlace.SelectionChanged += (s, e) => { string pl = string.IsNullOrEmpty(screenPlace.Code) ? null : screenPlace.Code; if (!loading && screenPlace.Code != null && a.Place != pl) { a.Place = pl; Fire(); } };
        screenMon.SelectionChanged += (s, e) => { int? m = int.TryParse(screenMon.Code, out var mm) ? mm : null; if (!loading && screenMon.Code != null && a.Monitor != m) { a.Monitor = m; Fire(); } };
        screenWin.TextChanged += (s, e) => { if (!loading && !Same(a.Window, screenWin.Text)) { a.Window = string.IsNullOrWhiteSpace(screenWin.Text) ? null : screenWin.Text.Trim(); Fire(); } };
        offMode.SelectionChanged += (s, e) => { if (!loading && offMode.Code != null && a.Mode != offMode.Code) { a.Mode = offMode.Code; Fire(); } };
        mirMode.SelectionChanged += (s, e) => { if (!loading && mirMode.Code != null && a.Mode != mirMode.Code) { a.Mode = mirMode.Code; Fire(); } };
    }

    // a field showing what the model already has (empty and null count as the same) is no change
    static bool Same(string model, string shown) => (model ?? "").Trim() == (shown ?? "").Trim();

    void Fire()
    {
        if (!loading)
            Changed?.Invoke();
    }

    bool InWheel => kind != "binding";

    // A card was picked: the action becomes that type, with its defaults (WinForms ActionEditor.Pick + Defaults).
    void Pick(string code)
    {
        a.Type = code;
        switch (code)
        {
            case ActionType.Key:
                a.Keys ??= "F1";
                if (a.Mode is not ("tap" or "hold" or "toggle"))
                    a.Mode = null;
                break;
            case ActionType.Wheel:
                a.Wheel ??= profile.Wheels.Keys.FirstOrDefault();
                break;
            case ActionType.Page:
                a.Page ??= wristOwner?.Pages.Keys.FirstOrDefault();
                break;
            case ActionType.Layer:
                a.Layer ??= "shift";
                if (a.Mode is not ("hold" or "toggle" or "on" or "off"))
                    a.Mode = InWheel ? "toggle" : "hold";
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
        Bind(a);
        Changed?.Invoke();
    }

    public void Bind(ActionModel model)
    {
        loading = true;
        a = model;
        keys.Value = a.Keys;
        string km = string.IsNullOrEmpty(a.Mode) || a.Type is not (ActionType.Key or ActionType.XrInput) ? "tap" : a.Mode;
        keyMode.SetChoices(KeyModeList(km == "hold"));
        keyMode.Code = km;
        steps.Text = (a.Steps ?? "").Replace("; ", "\n").Replace(";", "\n");
        wheel.ItemsSource = profile.Wheels.Keys.ToList();
        wheel.SelectedItem = a.Wheel;
        page.ItemsSource = wristOwner?.Pages.Keys.ToList() ?? new List<string>();
        page.SelectedItem = a.Page;
        confirm.Code = string.IsNullOrEmpty(a.Confirm) ? "auto" : a.Confirm;
        layer.ItemsSource = EditorApi.LayerNames(profile).ToList();
        layer.Text = a.Layer ?? "";
        layerMode.Code = a.Type == ActionType.Layer && !string.IsNullOrEmpty(a.Mode) ? a.Mode : (InWheel ? "toggle" : "hold");
        target.Value = a.Target ?? "left.menu";
        screenMode.Code = a.Type == ActionType.Screen && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "toggle";
        screenPlace.Code = a.Place ?? "";
        screenMon.Code = a.Monitor?.ToString() ?? "";
        screenWin.Text = a.Window ?? "";
        mirMode.Code = a.Type == ActionType.HeadsetMirror && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "toggle";
        offMode.Code = a.Type == ActionType.ControllerOffsets && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "panel";
        loading = false;
        Sync();
    }

    void Sync()
    {
        string t = a.Type;
        var title = EditorApi.ActionOptions(kind).FirstOrDefault(x => x.code == t).option?.Title
                    ?? Choices.ActionTypes.FirstOrDefault(c => c.Code == t)?.Text ?? t;
        whatText.Text = title;
        Show(lKeys, t == ActionType.Key);
        Show(lKeyMode, t is ActionType.Key or ActionType.XrInput);
        Show(lSteps, t == ActionType.Sequence);
        Show(lWheel, t == ActionType.Wheel);
        Show(lConfirm, t == ActionType.Wheel && (!InWheel || kind == "wrist"));
        Show(lPage, t == ActionType.Page);
        Show(lLayer, t == ActionType.Layer);
        Show(lLayerMode, t == ActionType.Layer);
        Show(lTarget, t == ActionType.XrInput);
        bool turnsOn = t == ActionType.Screen && (a.Mode is null or "toggle" or "on");
        Show(lScreenMode, t == ActionType.Screen);
        Show(lScreenPlace, turnsOn);
        Show(lScreenMon, turnsOn);
        Show(lScreenWin, turnsOn);
        Show(lOffMode, t == ActionType.ControllerOffsets);
        Show(lMirMode, t == ActionType.HeadsetMirror);
    }
}
