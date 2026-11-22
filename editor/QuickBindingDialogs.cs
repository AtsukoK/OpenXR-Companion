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
        string input = Ask(owner, "Act as another button", $"{what}: which of the game's buttons should it be? While you use it, the game sees that button instead.", list, () => list.Code);
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

    // A dialog with one question and OK / Cancel; returns read() on OK, else null.
    static string Ask(IWin32Window owner, string title, string question, Control input, Func<string> read)
    {
        using var dlg = new Form
        {
            Text = title,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            StartPosition = FormStartPosition.CenterParent,
            MinimizeBox = false,
            MaximizeBox = false,
            ShowInTaskbar = false,
            ClientSize = new Size(520, 190),
            Font = new Font("Segoe UI", 10f),
        };
        dlg.Controls.Add(new Label { Text = question, Location = new Point(16, 14), Size = new Size(488, 62) });
        input.Location = new Point(16, 82);
        dlg.Controls.Add(input);
        var ok = new Button { Text = "OK", DialogResult = DialogResult.OK, Size = new Size(96, 32), Location = new Point(300, 140) };
        var cancel = new Button { Text = "Cancel", DialogResult = DialogResult.Cancel, Size = new Size(96, 32), Location = new Point(408, 140) };
        dlg.Controls.Add(ok);
        dlg.Controls.Add(cancel);
        if (input is not KeyBox) // Enter is a key to record there
            dlg.AcceptButton = ok;
        dlg.CancelButton = cancel;
        ThemeWalker.Apply(dlg);
        dlg.HandleCreated += (s, e) => ThemeWalker.TitleBar(dlg, Theme.IsDark);
        return dlg.ShowDialog(owner) == DialogResult.OK ? read() : null;
    }
}
