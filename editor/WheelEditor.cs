using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

public class WheelEditor : EditorBase
{
    public readonly WheelPreview Preview = new();
    public WheelEditor(WheelModel w, Action<string, string> renamed, WristModel wristPanel = null, int columns = 3)
    {
        bool wrist = wristPanel != null;
        var names = wrist ? wristPanel.Pages : MainForm.Current.Profile.Wheels;
        var name = new TextBox { Width = 220, Text = w.Name };
        var title = new TextBox { Width = 220, Text = w.Title };
        var f = new FormPanel();
        f.AddRow("Name", name, wrist ? "Used by 'open another page' buttons. Renaming updates them." : "Used by 'Open a wheel' actions. Renaming updates them.");
        f.AddRow("Title", title, wrist ? "Shown at the top of the page." : "Shown in the middle of the wheel.");
        if (!wrist) // what happens when this wheel opens: which buttons open it, and where it appears
        {
            var p = MainForm.Current.Profile;
            var how = new List<string>();
            foreach (var b in p.Bindings.Where(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == w.Name))
            {
                string verb = b.Gesture switch { "double_tap" => "Double-tap", "hold" or "while_held" => "Hold", "tap" => "Tap", _ => "Press" };
                how.Add(verb + " " + InputText.Friendly(b.Input) + (b.Enabled ? "" : "  (that binding is off)"));
            }
            foreach (var o in p.Wheels.Values)
                foreach (var i in o.Items.Where(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name))
                    how.Add($"The '{i.Label}' item on wheel '{o.Title ?? o.Name}'" + (i.Enabled ? "" : "  (that item is off)"));
            for (int h = 0; h < 2; h++)
                foreach (var o in p.WristFor(h).Pages.Values)
                    foreach (var i in o.Items.Where(i => i.Action?.Type == ActionType.Wheel && i.Action.Wheel == w.Name))
                        how.Add($"The '{i.Label}' button on the {(h == 0 ? "left" : "right")} wrist panel" + (i.Enabled ? "" : "  (that button is off)"));
            var opens = new Label
            {
                AutoSize = true,
                MaximumSize = new Size(460, 0),
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                Text = how.Count == 0 ? "Nothing opens it yet: add a binding or a wheel item with 'Open a wheel'." : string.Join("\n", how)
            };
            f.AddRow("Opens with", opens);
            string anchor = SettingsEditor.Anchors.FirstOrDefault(a => a.Code == p.Style?.Anchor)?.Text ?? p.Style?.Anchor ?? "";
            var where = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
            where.Controls.Add(new Label { AutoSize = true, Text = anchor, Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), Margin = new Padding(0, 6, 10, 0) });
            var change = new Button { Text = "Change...", AutoSize = true };
            change.Click += (s, e) => MainForm.Current.GoTo(SettingsPage.ByKey("wheel"));
            where.Controls.Add(change);
            f.AddRow("Appears", where, "Where every wheel appears is set in Settings > Wheel look and picking (the same for all wheels).");
        }
        Preview.Wheel = w;
        Preview.IsWristPage = wrist;
        Preview.GridColumns = columns;
        Preview.Margin = new Padding(170, 8, 0, 0);
        var previewHost = new Panel { Height = 340, Dock = DockStyle.Top };
        previewHost.Controls.Add(Preview);
        Preview.Location = new Point(170, 8);
        Stack(Title(wrist ? "Wrist page" : "Wheel"),
              FormPanel.Header(wrist ? "A page of buttons on the wrist panel (max 16). To add one, click '+ Add button' under this page in the list on the left. Click a button in the picture below to edit it."
                                     : "A radial menu (max 16 items). To add one, click '+ Add item' under this wheel in the list on the left. Click a segment in the picture below to edit it."),
              f, previewHost);
        name.Leave += (s, e) =>
        {
            string n = name.Text.Trim();
            if (n.Length == 0 || n == w.Name)
            { name.Text = w.Name; return; }
            if (names.ContainsKey(n))
            { MessageBox.Show((wrist ? "A page" : "A wheel") + " called " + n + " already exists."); name.Text = w.Name; return; }
            string old = w.Name;
            w.Name = n;
            renamed(old, n);
            Fire();
        };
        title.TextChanged += (s, e) => { w.Title = title.Text; Preview.Invalidate(); Fire(); };
    }
}

public class ItemEditor : EditorBase
{
    public readonly WheelPreview Preview = new();
    public ItemEditor(WheelModel w, WheelItemModel it, WristModel wristPanel = null, int columns = 3)
    {
        bool wrist = wristPanel != null;
        var label = new TextBox { Width = 260, Text = it.Label };
        var desc = new TextBox { Width = 360, Text = it.Description ?? "" };
        var icon = new TextBox { Width = 260, Text = it.Icon ?? "" };
        var browse = new Button { Text = "Browse...", AutoSize = true };
        var iconRow = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
        iconRow.Controls.AddRange(new Control[] { icon, browse });
        var colorBtn = new Button { Width = 120, Height = 26, Tag = "swatch" };
        var colorClear = new Button { Text = "Default", AutoSize = true };
        var colorRow = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
        colorRow.Controls.AddRange(new Control[] { colorBtn, colorClear });
        void ShowColor()
        {
            bool has = WheelPreview.TryColor(it.Color, out var c);
            colorBtn.BackColor = has ? Color.FromArgb(255, c) : SystemColors.Control;
            colorBtn.Text = has ? "" : "(default)";
        }
        ShowColor();
        var action = new ActionEditor(true, wristPanel);
        action.Bind(it.Action ??= new ActionModel());

        var on = new CheckBox { Text = wrist ? "On (untick to hide it from the page)" : "On (untick to hide it from the wheel)", AutoSize = true, Checked = it.Enabled };
        on.CheckedChanged += (s, e) => { it.Enabled = on.Checked; Preview.Invalidate(); Fire(); };
        var f = new FormPanel();
        f.AddRow("", on);
        f.AddRow("Label", label);
        if (!wrist)
            f.AddRow("Description", desc, "Optional, shown in the middle while this item is pointed at.");
        f.AddRow("Icon", iconRow, "Optional PNG.");
        f.AddRow("Colour", colorRow);
        Preview.Wheel = w;
        Preview.Highlight = it;
        Preview.IsWristPage = wrist;
        Preview.GridColumns = columns;
        Preview.Size = new Size(220, 220);
        var previewHost = new Panel { Height = 230, Dock = DockStyle.Top };
        previewHost.Controls.Add(Preview);
        Preview.Location = new Point(170, 4);

        Stack(Title(wrist ? "Wrist button" : "Wheel item"), FormPanel.Header(wrist ? $"A button on the '{w.Name}' wrist page." : $"An entry on the '{w.Name}' wheel."), f, previewHost, FormPanel.Header(wrist ? "When pressed:" : "When picked:"), action);

        label.TextChanged += (s, e) => { it.Label = label.Text; Preview.Invalidate(); Fire(); };
        desc.TextChanged += (s, e) => { it.Description = desc.Text.Length > 0 ? desc.Text : null; Fire(); };
        icon.TextChanged += (s, e) => { it.Icon = icon.Text.Length > 0 ? icon.Text : null; Fire(); };
        browse.Click += (s, e) => { using var d = new OpenFileDialog { Filter = "PNG images|*.png" }; if (d.ShowDialog() == DialogResult.OK) icon.Text = d.FileName; };
        colorBtn.Click += (s, e) =>
        {
            using var d = new ColorDialog { FullOpen = true };
            if (WheelPreview.TryColor(it.Color, out var c))
                d.Color = c;
            if (d.ShowDialog() == DialogResult.OK)
            { it.Color = $"#{d.Color.R:X2}{d.Color.G:X2}{d.Color.B:X2}E0"; ShowColor(); Preview.Invalidate(); Fire(); }
        };
        colorClear.Click += (s, e) => { it.Color = null; ShowColor(); Preview.Invalidate(); Fire(); };
        action.Changed += Fire;
    }
}
