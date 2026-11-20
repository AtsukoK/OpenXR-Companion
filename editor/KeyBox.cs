using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// ------------------------------------------------------------ chord (button combo) editor
public class ChordEditor : FlowLayoutPanel
{
    public event Action Changed;
    readonly LinkLabel addLink = new() { Text = "+ add another button (makes a combo)", AutoSize = true, Margin = new Padding(3, 4, 3, 2) };
    bool loading;

    public ChordEditor()
    {
        FlowDirection = FlowDirection.TopDown;
        AutoSize = true;
        WrapContents = false;
        Margin = Padding.Empty;
        addLink.LinkClicked += (s, e) => { AddRow("left", "grip", true); Fire(); };
        Controls.Add(addLink);
    }
    void Fire() { if (!loading) Changed?.Invoke(); Relabel(); }

    void AddRow(string hand, string button, bool atFront)
    {
        var row = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 0, 0, 2) };
        var lead = new Label { AutoSize = true, Padding = new Padding(0, 6, 0, 0), Width = 60 };
        var h = new ChoiceBox(Choices.Hands, 110) { Code = hand };
        var b = new ChoiceBox(Choices.Buttons, 250) { Code = button };
        var x = new Button { Text = "✕", Width = 28, Height = 26 };
        h.SelectedIndexChanged += (s, e) => Fire();
        b.SelectedIndexChanged += (s, e) => Fire();
        x.Click += (s, e) => { if (Rows().Count() > 1) { Controls.Remove(row); row.Dispose(); Fire(); } };
        row.Controls.AddRange(new Control[] { lead, h, b, x });
        Controls.Add(row);
        // Combo modifiers go first, the button that fires stays last.
        Controls.SetChildIndex(row, atFront ? 0 : Controls.Count - 2);
        Controls.SetChildIndex(addLink, Controls.Count - 1);
    }
    IEnumerable<FlowLayoutPanel> Rows() => Controls.OfType<FlowLayoutPanel>();

    void Relabel()
    {
        var rows = Rows().ToList();
        for (int i = 0; i < rows.Count; i++)
        {
            ((Label)rows[i].Controls[0]).Text = rows.Count == 1 ? "" : i == rows.Count - 1 ? "then" : (i == 0 ? "Hold" : "and");
            rows[i].Controls[3].Visible = rows.Count > 1;
        }
    }

    public string Value
    {
        get => string.Join("+", Rows().Select(r => InputText.Join(((ChoiceBox)r.Controls[1]).Code, ((ChoiceBox)r.Controls[2]).Code)));
        set
        {
            loading = true;
            foreach (var r in Rows().ToList())
            { Controls.Remove(r); r.Dispose(); }
            var parts = (value ?? "").Split('+', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 0)
                parts = new[] { "right.trigger" };
            foreach (var p in parts)
            { var (h, b) = InputText.Split(p); AddRow(h, b, false); }
            loading = false;
            Relabel();
        }
    }
}

// ------------------------------------------------------------ key combo box with recorder
public class KeyBox : FlowLayoutPanel
{
    public static bool Recording;
    public event Action Changed;
    readonly TextBox text = new() { Width = 200 };
    readonly Button rec = new() { Text = "Record key", AutoSize = true };
    readonly Button mouse = new() { Text = "Mouse ▾", AutoSize = true };

    public KeyBox()
    {
        AutoSize = true;
        WrapContents = false;
        Margin = Padding.Empty;
        Controls.AddRange(new Control[] { text, rec, mouse });
        text.TextChanged += (s, e) => { Check(); if (!Recording) Changed?.Invoke(); };
        rec.Click += (s, e) => StartRecording();
        text.KeyDown += OnKey;
        text.KeyUp += OnKeyUp;
        text.PreviewKeyDown += (s, e) => { if (Recording) e.IsInputKey = true; };
        text.LostFocus += (s, e) => StopRecording();
        var menu = new ContextMenuStrip();
        foreach (var (code, name) in new[] { ("LMB", "Left click"), ("RMB", "Right click"), ("MMB", "Middle click"), ("Mouse4", "Mouse button 4"), ("Mouse5", "Mouse button 5"), ("WheelUp", "Scroll up"), ("WheelDown", "Scroll down") })
            menu.Items.Add(name, null, (s, e) => { text.Text = code; Changed?.Invoke(); });
        mouse.Click += (s, e) => menu.Show(mouse, 0, mouse.Height);
    }
    public string Value { get => text.Text.Trim(); set => text.Text = value ?? ""; }
    static readonly ToolTip tip = new();
    void Check()
    {
        if (Recording)
            return;
        var err = Native.CheckKeys(text.Text, false);
        text.BackColor = err == null ? Theme.InputBack : Theme.ErrorBack;
        tip.SetToolTip(text, err == null ? null : "Unknown bind: " + err);
    }

    void StartRecording() { Recording = true; lastMod = null; rec.Text = "Press a key..."; text.BackColor = Theme.RecordBack; text.Focus(); }
    void StopRecording() { if (!Recording) return; Recording = false; rec.Text = "Record key"; text.BackColor = Theme.InputBack; Check(); }

    void OnKey(object s, KeyEventArgs e)
    {
        if (!Recording)
            return;
        e.SuppressKeyPress = true;
        e.Handled = true;
        if (e.KeyCode is Keys.ControlKey or Keys.ShiftKey or Keys.Menu or Keys.LWin or Keys.RWin)
        {   // remember which modifier; if it is released with no other key, it is recorded alone
            lastMod = ModName(e.KeyCode);
            return;
        }
        lastMod = null;
        string name = KeyName(e.KeyCode);
        if (name == null)
            return;
        var parts = new List<string>();
        if (e.Control)
            parts.Add("Ctrl");
        if (e.Shift)
            parts.Add("Shift");
        if (e.Alt)
            parts.Add("Alt");
        parts.Add(name);
        text.Text = string.Join("+", parts);
        StopRecording();
        Changed?.Invoke();
    }

    string lastMod;
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern short GetKeyState(int vk);
    static bool Down(int vk) => (GetKeyState(vk) & 0x8000) != 0;
    static string ModName(Keys k) => k switch
    {
        Keys.ControlKey => Down(0xA3) ? "RCtrl" : "LCtrl",
        Keys.ShiftKey => Down(0xA1) ? "RShift" : "LShift",
        Keys.Menu => Down(0xA5) ? "RAlt" : "LAlt",
        _ => "Win",
    };
    void OnKeyUp(object s, KeyEventArgs e)
    {
        if (!Recording)
            return;
        e.SuppressKeyPress = true;
        e.Handled = true;
        if (lastMod == null || e.KeyCode is not (Keys.ControlKey or Keys.ShiftKey or Keys.Menu or Keys.LWin or Keys.RWin))
            return;
        text.Text = lastMod;
        lastMod = null;
        StopRecording();
        Changed?.Invoke();
    }

    static string KeyName(Keys k)
    {
        if (k >= Keys.A && k <= Keys.Z)
            return k.ToString();
        if (k >= Keys.D0 && k <= Keys.D9)
            return ((int)(k - Keys.D0)).ToString();
        if (k >= Keys.F1 && k <= Keys.F24)
            return k.ToString();
        if (k >= Keys.NumPad0 && k <= Keys.NumPad9)
            return "Num" + (int)(k - Keys.NumPad0);
        return k switch
        {
            Keys.Escape => "Esc",
            Keys.Return => "Enter",
            Keys.Space => "Space",
            Keys.Tab => "Tab",
            Keys.Back => "Backspace",
            Keys.Delete => "Delete",
            Keys.Insert => "Insert",
            Keys.Home => "Home",
            Keys.End => "End",
            Keys.PageUp => "PageUp",
            Keys.PageDown => "PageDown",
            Keys.Up => "Up",
            Keys.Down => "Down",
            Keys.Left => "Left",
            Keys.Right => "Right",
            Keys.OemMinus => "minus",
            Keys.Oemplus => "equals",
            Keys.Oemcomma => "comma",
            Keys.OemPeriod => "period",
            Keys.OemQuestion => "slash",
            Keys.OemSemicolon => "semicolon",
            Keys.OemQuotes => "quote",
            Keys.OemOpenBrackets => "lbracket",
            Keys.OemCloseBrackets => "rbracket",
            Keys.OemPipe => "backslash",
            Keys.Oemtilde => "grave",
            Keys.Multiply => "NumMul",
            Keys.Add => "NumAdd",
            Keys.Subtract => "NumSub",
            Keys.Divide => "NumDiv",
            Keys.Decimal => "NumDec",
            Keys.CapsLock => "CapsLock",
            Keys.PrintScreen => "PrintScreen",
            Keys.Pause => "Pause",
            Keys.Scroll => "ScrollLock",
            _ => null,
        };
    }
}
