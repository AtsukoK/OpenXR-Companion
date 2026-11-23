namespace OpenXRCompanionEditor;

// Both controllers side by side, a tag per button with what it does; + on a tag adds something to that button.
public sealed class ControllersHub : DrawerPage
{
    readonly ProfileModel p;
    readonly ControllerLayout layout;
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
        var grid = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 2, RowCount = 3, AutoSize = true };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        foreach (int hand in new[] { 0, 1 })
        {
            int h = hand;
            var cap = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 0, 0, 4) };
            cap.Controls.Add(new Label { Text = h == 0 ? "LEFT CONTROLLER" : "RIGHT CONTROLLER", AutoSize = true, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), ForeColor = Theme.Heading, Margin = new Padding(0, 6, 14, 0) });
            grid.Controls.Add(cap, h, 0);
            var pic = new ControllerPicture { Dock = DockStyle.Fill, BackColor = Theme.Card, AddButtons = true, Stacked = true, Margin = new Padding(0, 0, h == 0 ? 10 : 0, 0) };
            pic.PartClicked += part => OpenPart(h, part.Comp); // in the drawer: the controllers stay in view
            pic.AddClicked += part => Add(h, part);
            pic.SizeChanged += (s, e) => FitPictures();
            pics[h] = pic;
            grid.Controls.Add(pic, h, 1);
            var configure = new Button
            {
                Text = h == 0 ? "Configure Left Controller" : "Configure Right Controller",
                AutoSize = true, Height = 44, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Anchor = AnchorStyles.Top,
                Font = new Font("Segoe UI", 11.5f, FontStyle.Bold), BackColor = Color.FromArgb(40, 110, 200), ForeColor = Color.White,
                Padding = new Padding(24, 4, 24, 4), Margin = new Padding(0, 12, h == 0 ? 10 : 0, 0),
            };
            configure.FlatAppearance.BorderSize = 0;
            configure.Click += (s, e) => openPart(h, null);
            grid.Controls.Add(configure, h, 2);
        }
        Controls.Add(grid);
        this.grid = grid;
        BuildDrawer();
        DrawerClosed += () => { foreach (var pic in pics) pic.Selected = null; FitPictures(); };
        Resize += (s, e) => FitPictures();
        Controls.Add(new Label
        {
            Text = "Click a button (or its tag) to change what it does. The + on a tag adds something to that button. Blue = your bindings, grey 'Default' = the game's own control.",
            AutoSize = true, MaximumSize = new Size(1000, 0), Dock = DockStyle.Top, ForeColor = Theme.SubText, Font = new Font("Segoe UI", 10.5f), Padding = new Padding(0, 0, 0, 12),
        });
        Controls.Add(new Label { Text = "Controllers  ·  " + layout.Name, AutoSize = true, Dock = DockStyle.Top, Font = new Font("Segoe UI", 22f, FontStyle.Bold), ForeColor = Theme.Text, Padding = new Padding(0, 0, 0, 4) });
        Refresh2();
    }

    TableLayoutPanel grid;
    bool fitting;

    // One button of one controller: each way of using it, its bindings and + Add.
    public void OpenPart(int hand, string comp)
    {
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
            int room = ClientSize.Height - grid.Top - 34 /*captions*/ - 70 /*buttons*/ - Padding.Bottom;
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
    readonly FlowLayoutPanel drawerHead = new() { Dock = DockStyle.Top, Height = 56, WrapContents = false, Padding = new Padding(14, 10, 10, 6) };

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
        drawerHead.Controls.Add(HeadButton("✕  Close", CloseDrawer));
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
