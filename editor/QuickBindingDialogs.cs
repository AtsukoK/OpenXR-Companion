namespace OpenXRCompanionEditor;

// The small questions the quick choices on the controller page ask: which key, which of the game's buttons.
public static class QuickBindingDialogs
{
    // Which key the button presses. Null when cancelled or left empty.
    public static string AskKey(IWin32Window owner, string what)
    {
        var keys = new KeyBox();
        string answer = Ask(owner, "Press a key", $"{what}: which key should it press? Click 'Record key' and press the key, or type it (for example Space, F5, Ctrl+S, LMB).", keys, () => keys.Value);
        return string.IsNullOrWhiteSpace(answer) ? null : answer;
    }

    // Which of the game's buttons it acts as: (input, its name), or null when cancelled.
    public static (string input, string name)? AskGameButton(IWin32Window owner, string what, ControllerLayout layout, int hand)
    {
        var choices = new List<Choice>();
        foreach (int h in new[] { hand, 1 - hand }) // this hand first
            foreach (var part in layout.PartsFor(h).Where(p => p.IsUsable))
                foreach (var way in part.Ways)
                    choices.Add(new Choice(ControllerLayouts.InputOf(h, part, way), NameOf(layout, h, part, way)));
        var list = new ChoiceBox(choices, 380);
        string input = Ask(owner, "Swap with another button", $"{what}: which button should it trade places with? Each then does what the other one did in the game.", list, () => list.Code);
        if (input == null)
            return null;
        return (input, choices.First(c => c.Code == input).Text);
    }

    // "Right B (touch)", "Left thumbstick: push up"...
    public static string NameOf(ControllerLayout layout, int hand, ControllerPart part, string way)
    {
        string side = hand == 0 ? "Left " : "Right ";
        string name = side + part.LabelFor(hand);
        return way.Length == 0 && part.Comp != "thumbstick" ? name : name + ": " + ControllerLayouts.WayName(part, way).ToLowerInvariant();
    }

    internal static Action<Form> BeforeShow; // screenshots: called with the dialog before it shows

    // A dialog with one question and OK / Cancel; returns read() on OK, else null. Laid out in rows that size
    // themselves, so the buttons always sit under the input, whatever its height.
    internal static string Ask(IWin32Window owner, string title, string question, Control input, Func<string> read)
    {
        using var dlg = new Form
        {
            Text = title,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            StartPosition = FormStartPosition.CenterParent,
            MinimizeBox = false,
            MaximizeBox = false,
            ShowInTaskbar = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Font = new Font("Segoe UI", 10f),
            Padding = new Padding(16),
        };
        var rows = new TableLayoutPanel { AutoSize = true, ColumnCount = 1, Dock = DockStyle.Fill };
        rows.Controls.Add(new Label { Text = question, AutoSize = true, MaximumSize = new Size(520, 0), Margin = new Padding(0, 0, 0, 10) });
        input.Margin = new Padding(0, 0, 0, 14);
        rows.Controls.Add(input);
        var buttons = new FlowLayoutPanel { AutoSize = true, FlowDirection = FlowDirection.RightToLeft, Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
        var cancel = new Button { Text = "Cancel", DialogResult = DialogResult.Cancel, Size = new Size(110, 36) };
        var ok = new Button { Text = "OK, use this", DialogResult = DialogResult.OK, Size = new Size(130, 36), Font = new Font("Segoe UI", 10f, FontStyle.Bold), Margin = new Padding(0, 0, 8, 0) };
        buttons.Controls.Add(cancel);
        buttons.Controls.Add(ok);
        rows.Controls.Add(buttons);
        dlg.Controls.Add(rows);
        dlg.MinimumSize = new Size(560, 0);
        if (input is not KeyBox) // Enter is a key to record there
            dlg.AcceptButton = ok;
        dlg.CancelButton = cancel;
        ThemeWalker.Apply(dlg);
        dlg.HandleCreated += (s, e) => ThemeWalker.TitleBar(dlg, Theme.IsDark);
        BeforeShow?.Invoke(dlg);
        return dlg.ShowDialog(owner) == DialogResult.OK ? read() : null;
    }
}
