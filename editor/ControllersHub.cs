namespace OpenXRCompanionEditor;

// Both controllers side by side, a tag per button with what it does; + on a tag adds something to that button.
public sealed class ControllersHub : DrawerPage
{
    readonly ProfileModel p;
    ControllerLayout layout;
    Label titleLabel;
    readonly ControllerPicture[] pics = new ControllerPicture[2];
    readonly Action<int, string> openPart;
    readonly Action<BindingModel, bool> added;

    // openPart: show that hand's controller page on this part. added: a binding was put in the profile (true = open it).
    readonly Action edited, listChanged;
    int drawerHand;
    string drawerComp;

    // edited: a binding changed in the drawer. listChanged: one was added or deleted (the list is rebuilt).
    public ControllersHub(ProfileModel profile, ControllerLayout layout, Action<int, string> openPart, Action<BindingModel, bool> added,
                          Action edited = null, Action listChanged = null)
    {
        this.edited = edited ?? (() => { });
        this.listChanged = listChanged ?? (() => { });
        p = profile;
        this.layout = layout;
        this.openPart = openPart;
        this.added = added;
        Dock = DockStyle.Fill;
        AutoScroll = true;
        BackColor = Theme.Page;
        Padding = new Padding(28, 20, 20, 16);
        var grid = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 2, RowCount = 2, AutoSize = true };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        foreach (int hand in new[] { 0, 1 })
        {
            int h = hand;
            var cap = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 0, 0, 4) };
            cap.Controls.Add(new Label { Text = h == 0 ? "LEFT CONTROLLER" : "RIGHT CONTROLLER", AutoSize = true, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), ForeColor = Theme.Heading, Margin = new Padding(0, 6, 14, 0) });
            grid.Controls.Add(cap, h, 0);
            caps[h] = cap;
            var pic = new ControllerPicture { Dock = DockStyle.Fill, BackColor = Theme.Card, AddButtons = true, Stacked = true, Margin = new Padding(0, 0, h == 0 ? 10 : 0, 0) };
            pic.PartClicked += part => OpenPart(h, part.Comp); // in the drawer: the controllers stay in view
            pic.AddClicked += part => Add(h, part);
            pic.SizeChanged += (s, e) => FitPictures();
            pics[h] = pic;
            grid.Controls.Add(pic, h, 1);

        }
        Controls.Add(grid);
        this.grid = grid;
        BuildDrawer();
        DrawerClosed += () => { foreach (var pic in pics) pic.Selected = null; ShowOnly(-1); };
        Resize += (s, e) => FitPictures();
        Controls.Add(new Label
        {
            Text = "Click a button (or its tag) to change what it does. The + on a tag adds something to that button. Blue = your bindings, grey 'Default' = the game's own control.",
            AutoSize = true, MaximumSize = new Size(1000, 0), Dock = DockStyle.Top, ForeColor = Theme.SubText, Font = new Font("Segoe UI", 10.5f), Padding = new Padding(0, 0, 0, 12),
        });
        // which controllers are drawn (and the Steam Frame setting): the same choice the per-controller pages had
        var chooserRow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = false, Padding = new Padding(0, 0, 0, 8) };
        chooserRow.Controls.Add(new Label { Text = "Controller:", AutoSize = true, Margin = new Padding(0, 7, 8, 0), ForeColor = Theme.Text });
        var chooser = new ChoiceBox(ControllerLayouts.All.Select(l => new Choice(l.Id, l.Name)), 330) { Code = layout.Id };
        var models = new Label { AutoSize = true, Margin = new Padding(10, 7, 0, 0), ForeColor = Theme.SubText, Text = "For: " + layout.Models };
        var frameNative = SteamFrameSetting.NewCheckBox();
        frameNative.Visible = layout.Id == "frame";
        chooser.SelectedIndexChanged += (s, e) =>
        {
            this.layout = ControllerLayouts.ById(chooser.Code); // the field: the constructor parameter "layout" hides it here
            ControllerPictureChoice.Save(this.layout.Id);
            titleLabel.Text = "Controllers  \u00B7  " + this.layout.Name;
            models.Text = "For: " + this.layout.Models;
            frameNative.Visible = this.layout.Id == "frame";
            if (DrawerOpen)
                CloseDrawer(); // its page was made for the old controllers
            Refresh2();
            FitPictures();
        };
        chooserRow.Controls.Add(chooser);
        chooserRow.Controls.Add(models);
        chooserRow.Controls.Add(frameNative);
        // the "Your controllers" question: a preset for which buttons open the wheels
        var preset = new Button
        {
            Text = "Apply a controller preset…", AutoSize = true, Height = 32, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand,
            Font = new Font("Segoe UI", 10f, FontStyle.Bold), BackColor = Color.FromArgb(40, 110, 200), ForeColor = Color.White,
            Margin = new Padding(18, 0, 0, 0), Padding = new Padding(8, 0, 8, 0),
        };
        preset.FlatAppearance.BorderSize = 0;
        FormPanel.Tips.SetToolTip(preset, "Pick the controllers you use (Quest, Index, Vive, WMR...): their usual buttons are set to open your right and left hand wheels. Other bindings are not changed.");
        preset.Click += (s, e) => MainForm.Current?.ChooseControllersFromScreen();
        chooserRow.Controls.Add(preset);
        chooserRow.Controls.Add(new Label { Text = "sets which buttons open your wheels", AutoSize = true, ForeColor = Theme.SubText, Margin = new Padding(8, 8, 0, 0) });
        // shortcut: how the game's hands and guns point belongs with the controllers too
        var offsets = new Button
        {
            Text = "Controller offsets  ▶", AutoSize = true, Height = 32, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand,
            Font = new Font("Segoe UI", 10f, FontStyle.Bold), BackColor = Theme.Button, ForeColor = Theme.Text,
            Margin = new Padding(18, 0, 0, 0), Padding = new Padding(8, 0, 8, 0), Tag = "swatch",
        };
        offsets.FlatAppearance.BorderColor = Theme.CardBorder;
        FormPanel.Tips.SetToolTip(offsets, "Fix how the game's hands and guns point (Settings > Controller offsets).");
        offsets.Click += (s, e) => MainForm.Current?.GoTo(SettingsPage.ByKey("offsets"));
        chooserRow.Controls.Add(offsets);
        Controls.Add(chooserRow);
        titleLabel = new Label { Text = "Controllers  \u00B7  " + layout.Name, AutoSize = true, Dock = DockStyle.Top, Font = new Font("Segoe UI", 22f, FontStyle.Bold), ForeColor = Theme.Text, Padding = new Padding(0, 0, 0, 4) };
        Controls.Add(titleLabel);
        Refresh2();
    }

    TableLayoutPanel grid;
    bool fitting;
    readonly Control[] caps = new Control[2], configures = new Control[2];

    // While the drawer is open only that hand's controller shows (big enough to read); -1 = both.
    void ShowOnly(int hand)
    {
        if (grid == null)
            return;
        grid.SuspendLayout();
        for (int h = 0; h < 2; h++)
        {
            bool show = hand < 0 || h == hand;
            grid.ColumnStyles[h] = show ? new ColumnStyle(SizeType.Percent, hand < 0 ? 50 : 100) : new ColumnStyle(SizeType.Absolute, 0);
            pics[h].Visible = caps[h].Visible = show;
        }
        grid.ResumeLayout();
        FitPictures();
    }

    // One button of one controller: each way of using it, its bindings and + Add.
    public void OpenPart(int hand, string comp)
    {
        ShowOnly(hand);
        drawerHand = hand;
        drawerComp = comp;
        foreach (int h in new[] { 0, 1 })
            pics[h].Selected = h == hand ? comp : null;
        var part = layout.PartsFor(hand).FirstOrDefault(x => x.Comp == comp);
        var page = new ControllerButtonsPage(p, hand, b => OpenBinding(hand, comp, b),
                                             (b, finish) => { listChanged(); Refresh2(); if (finish) OpenBinding(hand, comp, b); else OpenPart(hand, comp); },
                                             () => { edited(); Refresh2(); }, () => layout.Id, _ => { }, comp);
        ShowInDrawer((hand == 0 ? "Left " : "Right ") + (part?.LabelFor(hand) ?? comp), page);
    }

    // One binding, edited in the drawer; Back returns to its button.
    internal void OpenBinding(int hand, string comp, BindingModel b)
    {
        ShowOnly(hand);
        foreach (int h in new[] { 0, 1 })
            pics[h].Selected = h == hand ? comp : null;
        var ed = new BindingEditor(b);
        ed.Changed += () => { edited(); Refresh2(); };
        ShowInDrawer(b.Name, ed,
                     HeadButton("◀  Back", () => OpenPart(hand, comp)),
                     HeadButton("Delete", () => { p.Bindings.Remove(b); listChanged(); Refresh2(); OpenPart(hand, comp); }, Color.FromArgb(170, 50, 50)));
    }

    // As tall as the picture wants at its width, but no taller than the window has room for (with the Configure buttons under it).
    void FitPictures()
    {
        if (fitting || grid == null)
            return;
        fitting = true;
        try
        {
            int room = ClientSize.Height - grid.Top - 34 /*captions*/ - 12 - Padding.Bottom;
            foreach (var pic in pics)
            {
                if (pic == null || pic.Width <= 0)
                    continue;
                int want = Math.Max(260, Math.Min(pic.HeightFor(pic.Width), room));
                if (pic.Height != want)
                    pic.Height = want;
            }
        }
        finally { fitting = false; }
    }

    void Refresh2()
    {
        foreach (int h in new[] { 0, 1 })
            pics[h].ShowController(layout, h, null, part => QuickBindings.UseOf(p.Bindings, h, part), part => ControllerButtonsPage.TagLinesOf(p.Bindings, h, part));
    }

    // Which way of using it (when it has more than one), then what it should do.
    void Add(int hand, ControllerPart part)
    {
        string way = part.Ways.FirstOrDefault() ?? "";
        if (part.Ways.Count() > 1)
        {
            var ways = part.Ways.ToList();
            var opts = ways.Select(w => new PickOption(null, ControllerLayouts.WayName(part, w), WayDoes(part, w), null)).ToList();
            int pick = OptionPicker.Show(FindForm(), (hand == 0 ? "Left " : "Right ") + part.LabelFor(hand), "Which way of using it should do something new?", opts);
            if (pick < 0)
                return;
            way = ways[pick];
        }
        var made = ControllerButtonsPage.AddFor(FindForm(), p, layout, hand, part, way);
        if (made == null)
            return;
        listChanged();
        Refresh2();
        if (made.Value.finishInEditor) // "Something else": finish it in the drawer
            OpenBinding(hand, part.Comp, made.Value.binding);
        else
            OpenPart(hand, part.Comp);
    }

    static string WayDoes(ControllerPart part, string way) => way switch
    {
        "" => part.Comp switch
        {
            "trigger" => "Pulling the trigger.",
            "grip" => "Squeezing the grip.",
            "thumbstick" => "Clicking the stick down.",
            "thumbrest" => "Resting your thumb on it.",
            _ => "Pressing it.",
        },
        "touch" => "A finger resting on it, without pressing.",
        "full" => part.Comp == "grip" ? "Squeezing all the way (the click at the end, or a firm squeeze)." : "Pulling all the way to the click at the end.",
        "up" => part.Comp == "trackpad" ? "Touching the top edge." : "Pushing it forward. Also: double flick forward (sprint).",
        "down" => part.Comp == "trackpad" ? "Touching the bottom edge." : "Pulling it back. Also: double flick back.",
        "left" => part.Comp == "trackpad" ? "Touching the left edge." : "Pushing it left. Also: double flick left.",
        "right" => part.Comp == "trackpad" ? "Touching the right edge." : "Pushing it right. Also: double flick right.",
        _ => "",
    };
}


// A page with a drawer on the right: a panel that slides in with the settings of what was clicked, while the page
// (a controller picture, a wheel) stays in view.
public class DrawerPage : Panel
{
    protected readonly Panel drawer = new() { Dock = DockStyle.Right, Visible = false };
    protected readonly Panel drawerBody = new() { Dock = DockStyle.Fill };
    readonly Label drawerTitle = new() { AutoSize = true, Font = new Font("Segoe UI", 14f, FontStyle.Bold) };
    readonly FlowLayoutPanel drawerHead = new() { Dock = DockStyle.Top, Height = 62, WrapContents = false, Padding = new Padding(14, 10, 10, 6) };

    public bool DrawerOpen => drawer.Visible;

    // ---------------- the drawer: one button's settings (or one binding) on the right, the pictures stay in view
    protected void BuildDrawer()
    {
        drawer.BackColor = Theme.IsDark ? Color.FromArgb(30, 34, 42) : Color.FromArgb(244, 246, 250);
        drawerBody.BackColor = drawer.BackColor;
        drawerHead.BackColor = Theme.IsDark ? Color.FromArgb(38, 44, 56) : Color.FromArgb(228, 234, 244);
        drawer.Controls.Add(drawerBody);
        drawer.Controls.Add(drawerHead);
        drawer.Controls.Add(new Panel { Dock = DockStyle.Left, Width = 3, BackColor = Color.FromArgb(60, 140, 255) });
        Controls.Add(drawer); // added last: docked first, so it takes the whole height
        Resize += (s, e) => { if (drawer.Visible) drawer.Width = DrawerWidth; };
    }

    protected int DrawerWidth => Math.Clamp(ClientSize.Width * 42 / 100, 640, 900);

    protected Button HeadButton(string text, Action click, Color? back = null)
    {
        var b = new Button { Text = text, AutoSize = true, Height = 34, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                             BackColor = back ?? Theme.Button, ForeColor = back != null ? Color.White : Theme.Text, Margin = new Padding(0, 0, 8, 0) };
        b.FlatAppearance.BorderColor = Theme.CardBorder;
        b.Click += (s, e) => click();
        return b;
    }

    protected void ShowInDrawer(string title, Control content, params Control[] headButtons)
    {
        drawerHead.SuspendLayout();
        foreach (Control c in drawerHead.Controls.Cast<Control>().ToList())
            if (c != drawerTitle) c.Dispose();
        drawerHead.Controls.Clear();
        // a big, bright Close: the way back to both controllers
        var close = HeadButton("✕  Close", CloseDrawer, Color.FromArgb(225, 120, 30));
        close.Font = new Font("Segoe UI", 12f, FontStyle.Bold);
        close.Height = 40;
        close.Padding = new Padding(10, 0, 10, 0);
        FormPanel.Tips.SetToolTip(close, "Close this panel and show both controllers again");
        drawerHead.Controls.Add(close);
        foreach (var b in headButtons)
            drawerHead.Controls.Add(b);
        drawerTitle.Text = title;
        drawerTitle.ForeColor = Theme.Text;
        drawerTitle.Margin = new Padding(10, 4, 0, 0);
        drawerHead.Controls.Add(drawerTitle);
        drawerHead.ResumeLayout();
        content.Dock = DockStyle.Fill;
        if (Theme.IsDark)
            ThemeWalker.Apply(content);
        foreach (Control c in drawerBody.Controls.Cast<Control>().ToList())
            c.Dispose();
        drawerBody.Controls.Clear();
        drawerBody.Controls.Add(content);
        drawer.Width = DrawerWidth;
        drawer.Visible = true;
    }

    public event Action DrawerClosed;
    protected void CloseDrawer()
    {
        drawer.Visible = false;
        DrawerClosed?.Invoke();
    }

}
