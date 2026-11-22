namespace OpenXRCompanionEditor;

// A popup with big option cards instead of a dropdown: each choice gets a title, what it does, and an example of what
// it is good for. Options are grouped under small headings. Returns the picked option's index, or -1 when closed.
public record PickOption(string Group, string Title, string Does, string Example);

public static class OptionPicker
{
    // Saves a picture of the popup (no window shown), for checking its look.
    public static void Shot(string path, string title, string intro, IReadOnlyList<PickOption> options)
    {
        Show(null, title, intro, options, dlg =>
        {
            dlg.StartPosition = FormStartPosition.Manual;
            dlg.Location = new Point(-5000, -5000);
            dlg.Shown += (s, e) =>
            {
                using var bmp = new Bitmap(dlg.Width, dlg.Height);
                dlg.DrawToBitmap(bmp, new Rectangle(0, 0, dlg.Width, dlg.Height));
                bmp.Save(path);
                dlg.Close();
            };
        });
    }

    public static int Show(IWin32Window owner, string title, string intro, IReadOnlyList<PickOption> options, Action<Form> before = null)
    {
        using var dlg = new Form
        {
            Text = title,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            StartPosition = FormStartPosition.CenterParent,
            MinimizeBox = false,
            MaximizeBox = false,
            ShowInTaskbar = false,
            KeyPreview = true,
            Font = new Font("Segoe UI", 10f),
            BackColor = Theme.Page,
            ForeColor = Theme.Text,
        };
        const int cardW = 400, gap = 12, cols = 2, pad = 20;
        const int innerW = cols * cardW + (cols - 1) * gap;
        int picked = -1;
        // placed by hand: intro, then each group's heading and its cards two by two, the cards of a row equally tall
        var body = new Panel { Location = new Point(0, 0), AutoScroll = true, BackColor = Theme.Page };
        int y = pad;
        var introLabel = new Label { Text = intro, AutoSize = true, MaximumSize = new Size(innerW, 0), ForeColor = Theme.SubText, Location = new Point(pad, y) };
        body.Controls.Add(introLabel);
        y += introLabel.PreferredHeight + 6;
        int at = 0;
        while (at < options.Count)
        {
            string group = options[at].Group;
            if (!string.IsNullOrEmpty(group))
            {
                var head = new Label { Text = group.ToUpperInvariant(), AutoSize = true, ForeColor = Theme.Heading, Font = new Font("Segoe UI", 8.5f, FontStyle.Bold), Location = new Point(pad, y + 8) };
                body.Controls.Add(head);
                y += 8 + head.PreferredHeight + 4;
            }
            int end = at;
            while (end < options.Count && options[end].Group == group)
                end++;
            for (int row = at; row < end; row += cols)
            {
                var cards = new List<Control>();
                for (int c = 0; c < cols && row + c < end; c++)
                {
                    int index = row + c;
                    var card = Card(options[index], cardW, () => { picked = index; dlg.DialogResult = DialogResult.OK; });
                    card.Location = new Point(pad + c * (cardW + gap), y);
                    cards.Add(card);
                }
                int h = cards.Max(NeededHeight);
                foreach (var c in cards)
                {
                    c.AutoSize = false;
                    c.Size = new Size(cardW, h);
                    body.Controls.Add(c);
                }
                y += h + gap;
            }
            at = end;
        }
        int contentH = y + pad - gap;
        dlg.Controls.Add(body);
        var cancel = new Button { Text = "Cancel", DialogResult = DialogResult.Cancel, Size = new Size(100, 34), FlatStyle = FlatStyle.Flat, BackColor = Theme.Button, ForeColor = Theme.Text };
        cancel.FlatAppearance.BorderColor = Theme.CardBorder;
        dlg.Controls.Add(cancel);
        dlg.CancelButton = cancel;
        dlg.AutoScroll = false;
        int maxH = Screen.PrimaryScreen.WorkingArea.Height - 120;
        int bodyH = Math.Min(contentH, maxH - 56);
        int w = innerW + 2 * pad + (contentH > bodyH ? SystemInformation.VerticalScrollBarWidth : 0);
        body.Size = new Size(w, bodyH);
        dlg.ClientSize = new Size(w, bodyH + 56);
        cancel.Location = new Point(w - pad - cancel.Width, bodyH + 11);
        dlg.Load += (s, e) =>
        {
            if (owner != null && Control.FromHandle(owner.Handle) is Form parent) // centred on the editor
            {
                dlg.StartPosition = FormStartPosition.Manual;
                var r = parent.Bounds;
                dlg.Location = new Point(r.X + (r.Width - dlg.Width) / 2, Math.Max(r.Y, r.Y + (r.Height - dlg.Height) / 2));
            }
        };
        ThemeWalker.Apply(dlg);
        dlg.HandleCreated += (s, e) => ThemeWalker.TitleBar(dlg, Theme.IsDark);
        before?.Invoke(dlg);
        return dlg.ShowDialog(owner) == DialogResult.OK ? picked : -1;
    }

    // A card's height from its text, measured the way it will wrap (PreferredSize comes out short before layout).
    static int NeededHeight(Control card)
    {
        int h = card.Padding.Vertical;
        foreach (Control c in card.Controls)
        {
            var size = TextRenderer.MeasureText(c.Text, c.Font, new Size(c.MaximumSize.Width, int.MaxValue), TextFormatFlags.WordBreak);
            h += size.Height + c.Margin.Vertical;
        }
        return h + 20; // a little room: the labels wrap a bit narrower than measured
    }

    // One option: a clickable card that lights up under the mouse.
    static Control Card(PickOption o, int width, Action pick)
    {
        var card = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            Width = width,
            AutoSize = true,
            MaximumSize = new Size(width, 0),
            Padding = new Padding(14, 10, 14, 10),
            BackColor = Theme.Card,
            Cursor = Cursors.Hand,
        };
        int textW = width - 28;
        card.Controls.Add(new Label { Text = o.Title, AutoSize = true, MaximumSize = new Size(textW, 0), Font = new Font("Segoe UI", 11f, FontStyle.Bold), ForeColor = Theme.Text, Margin = new Padding(0, 0, 0, 4) });
        card.Controls.Add(new Label { Text = o.Does, AutoSize = true, MaximumSize = new Size(textW, 0), ForeColor = Theme.Text, Margin = new Padding(0, 0, 0, 4) });
        if (!string.IsNullOrEmpty(o.Example))
            card.Controls.Add(new Label { Text = "Good for: " + o.Example, AutoSize = true, MaximumSize = new Size(textW, 0), ForeColor = Theme.SubText, Font = new Font("Segoe UI", 9f, FontStyle.Italic) });
        void Hover(bool on) => card.BackColor = on ? Theme.Selected : Theme.Card;
        foreach (Control c in new[] { (Control)card }.Concat(card.Controls.Cast<Control>()))
        {
            c.Cursor = Cursors.Hand;
            c.MouseEnter += (s, e) => Hover(true);
            c.MouseLeave += (s, e) => Hover(card.ClientRectangle.Contains(card.PointToClient(Cursor.Position)));
            c.Click += (s, e) => pick();
        }
        return card;
    }
}
