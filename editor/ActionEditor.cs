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

    public ActionEditor(bool inWheel, WristModel wrist = null)
    {
        this.inWheel = inWheel;
        this.inWrist = wrist != null;
        wristOwner = wrist;
        var types = Choices.ActionTypes.Where(c => c.Code != "page").ToList();
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
        AddRow("What happens", type);
        lKeys = AddRow("Key", keys, "Click Record key, then press the key or combo on your keyboard. Or pick a mouse button.");
        lKeyMode = AddRow("How", keyMode);
        lSteps = AddRow("Macro steps", steps, "One step per line or split with ;  e.g.\nCtrl+C\nwait 100\nCtrl+V\ndown W / up W to hold a key");
        lWheel = AddRow("Wheel", wheel);
        lPage = AddRow("Page", page, "Its buttons replace these; a Back button is added automatically.");
        lConfirm = AddRow("Pick items by", confirm);
        lLayer = AddRow("Layer name", layer, "Type a new name or pick one you already use.");
        lLayerMode = AddRow("Layer", layerMode);
        lTarget = AddRow("Game button", target, "The game will think this controller button is pressed.");
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
            case "key":
                a.Keys ??= "F1";
                if (a.Mode is not ("tap" or "hold" or "toggle"))
                    a.Mode = null;
                break;
            case "wheel":
                a.Wheel ??= MainForm.Current.Profile.Wheels.Keys.FirstOrDefault();
                break;
            case "page":
                a.Page ??= wristOwner?.Pages.Keys.FirstOrDefault();
                break;
            case "layer":
                a.Layer ??= "shift";
                if (a.Mode is not ("hold" or "toggle" or "on" or "off"))
                    a.Mode = inWheel ? "toggle" : "hold";
                break;
            case "xr_input":
                a.Target ??= "left.menu";
                break;
            case "screen":
                if (Choices.ScreenModes.All(c => c.Code != a.Mode))
                    a.Mode = "toggle";
                break;
            case "headset_mirror":
                if (Choices.MirrorModes.All(c => c.Code != a.Mode))
                    a.Mode = "toggle";
                break;
            case "controller_offsets":
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
        keyMode.Code = string.IsNullOrEmpty(a.Mode) || a.Type != "key" ? "tap" : a.Mode;
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
        layerMode.Code = a.Type == "layer" && !string.IsNullOrEmpty(a.Mode) ? a.Mode : (inWheel ? "toggle" : "hold");
        target.Value = a.Target ?? "left.menu";
        screenMode.Code = a.Type == "screen" && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "toggle";
        screenPlace.Code = a.Place ?? "";
        mirMode.Code = a.Type == "headset_mirror" && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "toggle";
        offMode.Code = a.Type == "controller_offsets" && !string.IsNullOrEmpty(a.Mode) ? a.Mode : "panel";
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
        Show(lKeys, t == "key");
        Show(lKeyMode, t is "key" or "xr_input");
        Show(lSteps, t == "sequence");
        Show(lWheel, t == "wheel");
        Show(lConfirm, t == "wheel" && (!inWheel || inWrist));
        Show(lPage, t == "page");
        Show(lLayer, t == "layer");
        Show(lLayerMode, t == "layer");
        Show(lTarget, t == "xr_input");
        bool turnsOn = t == "screen" && (a.Mode is null or "toggle" or "on");
        Show(lOffMode, t == "controller_offsets");
        Show(lMirMode, t == "headset_mirror");
        Show(lScreenMode, t == "screen");
        Show(lScreenPlace, turnsOn);
        Show(lScreenMon, turnsOn);
        Show(lScreenWin, turnsOn);
        if (t == "xr_input")
        { keyMode.Code = string.IsNullOrEmpty(a.Mode) ? "tap" : a.Mode; }
    }
}
