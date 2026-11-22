namespace OpenXRCompanionEditor;

// The Right / Left controller page: a picture of the controller, and for the button clicked on it, what each way
// of using it does (press, touch, stick directions) with quick choices to change that, like SteamVR's controller
// bindings. Everything it makes is an ordinary binding in the profile.
public class ControllerButtonsPage : UserControl
{
    static readonly Dictionary<int, string> lastSelected = new(); // per hand, so the page keeps its button when rebuilt

    readonly ProfileModel profile;
    readonly int hand;
    readonly Action<BindingModel> open;
    readonly Action<BindingModel, bool> added;
    readonly Func<string> loadLayoutId;
    readonly Action<string> saveLayoutId;
    readonly ControllerPicture picture = new() { Dock = DockStyle.Top, Height = 360 };
    readonly FlowLayoutPanel details = new() { Dock = DockStyle.Top, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Padding = new Padding(0, 6, 0, 12) };
    readonly Label models = new() { AutoSize = true, Margin = new Padding(10, 7, 0, 0) };
    ControllerLayout layout;

    // open: show a binding in its editor. added: a new binding was put in the profile (true = show it in its editor).
    // The owner updates the list after this page's click is done, so the page may be rebuilt then.
    public ControllerButtonsPage(ProfileModel profile, int hand, Action<BindingModel> open, Action<BindingModel, bool> added,
                                 Func<string> loadLayoutId, Action<string> saveLayoutId)
    {
        this.profile = profile;
        this.hand = hand;
        this.open = open;
        this.added = added;
        this.loadLayoutId = loadLayoutId;
        this.saveLayoutId = saveLayoutId;
        Dock = DockStyle.Fill;
        AutoScroll = true;
        Padding = new Padding(12, 8, 12, 8);
        BackColor = Theme.Page;
        layout = ControllerLayouts.ById(loadLayoutId());

        var chooser = new ChoiceBox(ControllerLayouts.All.Select(l => new Choice(l.Id, l.Name)), 330) { Code = layout.Id };
        chooser.SelectedIndexChanged += (s, e) =>
        {
            layout = ControllerLayouts.ById(chooser.Code);
            saveLayoutId(layout.Id);
            ShowAll();
        };
        var chooserRow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = false, Padding = new Padding(0, 4, 0, 4) };
        chooserRow.Controls.Add(new Label { Text = "Controller:", AutoSize = true, Margin = new Padding(0, 7, 8, 0) });
        chooserRow.Controls.Add(chooser);
        chooserRow.Controls.Add(models);

        picture.BackColor = Theme.Card;
        picture.PartClicked += part =>
        {
            lastSelected[hand] = part.Comp;
            ShowDetails();
        };

        // Docked to the top in reverse order: the last added sits highest.
        Controls.Add(details);
        Controls.Add(picture);
        Controls.Add(chooserRow);
        Controls.Add(new Label
        {
            Text = "Click a button on the picture to see what each way of using it does: pressing, touching, pushing the stick. " +
                   "Make it press a key, act as another of the game's buttons, or turn it off for the game. " +
                   "Buttons with a blue number already do something.",
            AutoSize = true,
            MaximumSize = new Size(760, 0),
            Dock = DockStyle.Top,
            ForeColor = Theme.SubText,
            Padding = new Padding(0, 0, 0, 6),
        });
        Controls.Add(new Label { Text = (hand == 0 ? "Left" : "Right") + " controller buttons", AutoSize = true, Dock = DockStyle.Top, Font = new Font("Segoe UI", 14f, FontStyle.Bold), Padding = new Padding(0, 0, 0, 4) });
        ShowAll();
    }

    // Which part a page for this hand starts with (screenshots; otherwise the last one clicked).
    internal static void StartWith(int hand, string comp) => lastSelected[hand] = comp;

    ControllerPart SelectedPart
    {
        get
        {
            var parts = layout.PartsFor(hand).ToList();
            string comp = lastSelected.GetValueOrDefault(hand);
            return parts.FirstOrDefault(p => p.Comp == comp) ?? parts.FirstOrDefault(p => p.IsUsable);
        }
    }

    void ShowAll()
    {
        models.Text = "For: " + layout.Models;
        picture.ShowController(layout, hand, SelectedPart?.Comp, p => QuickBindings.UseOf(profile.Bindings, hand, p));
        ShowDetails();
    }

    void ShowDetails()
    {
        var part = SelectedPart;
        picture.Selected = part?.Comp;
        SuspendLayout();
        details.SuspendLayout();
        foreach (Control c in details.Controls.Cast<Control>().ToList())
            c.Dispose();
        details.Controls.Clear();
        if (part != null)
        {
            details.Controls.Add(new Label { Text = part.LabelFor(hand), AutoSize = true, Font = new Font("Segoe UI", 12.5f, FontStyle.Bold), Margin = new Padding(0, 4, 0, 4) });
            if (!part.IsUsable)
                details.Controls.Add(Note(part.NotUsable));
            else
                foreach (var way in part.Ways)
                    details.Controls.Add(WayCard(part, way));
        }
        if (Theme.IsDark)
            ThemeWalker.Apply(details);
        details.ResumeLayout();
        ResumeLayout();
    }

    static Label Note(string text) => new() { Text = text, AutoSize = true, MaximumSize = new Size(720, 0), ForeColor = Theme.SubText, Margin = new Padding(0, 2, 0, 8) };

    // One way of using the part: what it does now, and the quick choices.
    Control WayCard(ControllerPart part, string way)
    {
        string input = ControllerLayouts.InputOf(hand, part, way);
        string wayName = ControllerLayouts.WayName(part, way);
        string what = QuickBindingDialogs.NameOf(layout, hand, part, way);
        var card = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            BackColor = Theme.Card,
            Padding = new Padding(12, 8, 12, 8),
            Margin = new Padding(0, 0, 0, 8),
            MinimumSize = new Size(720, 0),
        };
        card.Controls.Add(new Label { Text = wayName, AutoSize = true, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Margin = new Padding(0, 0, 0, 2) });
        // What the button normally does first; bindings that only work in a layer (shift...) after, then switched-off ones.
        var bound = profile.Bindings.Where(b => QuickBindings.FiredBy(b, input))
                                    .OrderBy(b => IsLayerOnly(b))
                                    .ThenBy(b => !b.Enabled)
                                    .ToList();
        if (bound.Count == 0)
            card.Controls.Add(Note("Nothing set: the game gets it as usual."));
        foreach (var b in bound)
            card.Controls.Add(BindingLink(b));
        bool isTouchOnly = IsTouchOnly(part, way);
        var holdOnly = new CheckBox
        {
            Text = "Only when held for a moment (a quick tap still reaches the game as usual)",
            AutoSize = true,
            Margin = new Padding(0, 6, 0, 0),
            Visible = !isTouchOnly, // a touch has no tap of its own to give back
        };
        FormPanel.Tips.SetToolTip(holdOnly, "For the next choice you make below. While you hold the button the game doesn't see it;\n" +
                                            "after " + QuickBindings.DefaultHoldMs + " ms your choice starts. Let go sooner and the game gets a normal press.");
        card.Controls.Add(holdOnly);
        card.Controls.Add(QuickChoices(input, what, isTouchOnly, () => holdOnly.Checked));
        return card;
    }

    static bool IsTouchOnly(ControllerPart part, string way) => way == "touch" || part.Comp == "thumbrest";

    static bool IsLayerOnly(BindingModel b) => !string.IsNullOrWhiteSpace(b.Layers);

    LinkLabel BindingLink(BindingModel b)
    {
        string gesture = Choices.Gestures.FirstOrDefault(g => g.Code == b.Gesture)?.Text.Split("  (")[0].Split(" (")[0] ?? b.Gesture;
        var others = (b.Input ?? "").Split('+').Select(p => p.Trim()).Where(p => p.Length > 0).ToList();
        string combo = others.Count > 1 ? " (with " + InputText.Friendly(string.Join("+", others.Take(others.Count - 1))) + " held)" : "";
        string layerNote = IsLayerOnly(b) ? $"In layer {b.Layers}: " : "";
        string hidden = b.Block == "none" ? "" : " Hidden from the game.";
        var link = new LinkLabel
        {
            Text = $"{layerNote}{b.Name}: {gesture}{combo} → {MainForm.Plain(b.Action)}.{hidden}{(b.Enabled ? "" : " (switched off)")}",
            AutoSize = true,
            MaximumSize = new Size(700, 0),
            LinkColor = b.Enabled ? Theme.Link : Theme.OffText,
            Margin = new Padding(IsLayerOnly(b) ? 18 : 0, 2, 0, 2), // layer ones indented under the normal ones
        };
        link.LinkClicked += (s, e) => open(b);
        return link;
    }

    // isHoldOnly: read when a choice is clicked; makes it a hold binding that gives quick taps back to the game.
    Control QuickChoices(string input, string what, bool isTouchOnly, Func<bool> isHoldOnly)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = true, Margin = new Padding(0, 6, 0, 0), MaximumSize = new Size(700, 0) };
        void Choice(string text, Func<BindingModel> make)
        {
            var b = new Button { Text = text, AutoSize = true, Height = 30, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(0, 0, 6, 4), BackColor = Theme.Button, ForeColor = Theme.Text };
            b.FlatAppearance.BorderColor = Theme.CardBorder;
            b.Click += (s, e) =>
            {
                var made = make();
                if (made == null)
                    return;
                if (isHoldOnly())
                    QuickBindings.OnlyWhenHeld(made);
                profile.Bindings.Add(made);
                added(made, false);
                ShowAll();
            };
            row.Controls.Add(b);
        }
        Choice("+ Press a key…", () => QuickBindingDialogs.AskKey(FindForm(), what) is string keys ? QuickBindings.Key(input, what, keys) : null);
        if (!isTouchOnly) // touching can't stand in for another button or be hidden without hiding its presses
        {
            Choice("+ Act as another button…", () => QuickBindingDialogs.AskGameButton(FindForm(), what, layout, hand) is var (target, name) ? QuickBindings.GameButton(input, what, target, name) : null);
            Choice("Turn off in the game", () => QuickBindings.OffInGame(input, what));
        }
        Choice("+ Something else…", () =>
        {
            var b = QuickBindings.Blank(input, what);
            if (isHoldOnly())
                QuickBindings.OnlyWhenHeld(b);
            profile.Bindings.Add(b);
            added(b, true); // finished in the binding editor
            return null;
        });
        return row;
    }
}
