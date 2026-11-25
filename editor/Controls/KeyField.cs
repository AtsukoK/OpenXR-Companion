using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;

namespace OverviewPrototype;

// A key or combo ("Ctrl+S", "F5", "LMB") with Record key and a Mouse menu. Same names as the WinForms KeyBox.
public class KeyField : StackPanel
{
    public event Action Changed;
    readonly TextBox text = new() { Width = 200 };
    readonly Button rec = new() { Content = "Record key" };
    readonly Button mouse = new() { Content = "Mouse ▾" };
    bool recording;
    string lastMod;
    static readonly IBrush Normal = Brush.Parse("#2A2F37"), Error = Brush.Parse("#5A2A2A"), Record = Brush.Parse("#2A4A6A");

    public KeyField()
    {
        Orientation = Orientation.Horizontal;
        Spacing = 6;
        Children.Add(text);
        Children.Add(rec);
        Children.Add(mouse);
        text.TextChanged += (s, e) => { Check(); if (!recording) Changed?.Invoke(); };
        rec.Click += (s, e) => { recording = true; lastMod = null; rec.Content = "Press a key..."; text.Background = Record; text.Focus(); };
        text.LostFocus += (s, e) => Stop();
        text.AddHandler(KeyDownEvent, OnKeyDown, Avalonia.Interactivity.RoutingStrategies.Tunnel);
        text.AddHandler(KeyUpEvent, OnKeyUp, Avalonia.Interactivity.RoutingStrategies.Tunnel);
        var menu = new MenuFlyout();
        foreach (var (code, name) in new[] { ("LMB", "Left click"), ("RMB", "Right click"), ("MMB", "Middle click"), ("Mouse4", "Mouse button 4"),
                                             ("Mouse5", "Mouse button 5"), ("WheelUp", "Scroll up"), ("WheelDown", "Scroll down") })
        {
            var item = new MenuItem { Header = name };
            item.Click += (s, e) => { text.Text = code; Changed?.Invoke(); };
            menu.Items.Add(item);
        }
        mouse.Flyout = menu;
    }

    public string Value { get => (text.Text ?? "").Trim(); set => text.Text = value ?? ""; }

    void Stop()
    {
        if (!recording)
            return;
        recording = false;
        rec.Content = "Record key";
        Check();
    }

    void Check()
    {
        if (recording)
            return;
        var err = EditorApi.CheckKeys(text.Text ?? "", false);
        text.Background = err == null ? Normal : Error;
        ToolTip.SetTip(text, err == null ? null : "Unknown bind: " + err);
    }

    static bool IsModifier(Key k) => k is Key.LeftCtrl or Key.RightCtrl or Key.LeftShift or Key.RightShift or Key.LeftAlt or Key.RightAlt or Key.LWin or Key.RWin;

    static string ModName(Key k) => k switch
    {
        Key.LeftCtrl => "LCtrl", Key.RightCtrl => "RCtrl", Key.LeftShift => "LShift", Key.RightShift => "RShift",
        Key.LeftAlt => "LAlt", Key.RightAlt => "RAlt", _ => "Win",
    };

    void OnKeyDown(object sender, KeyEventArgs e)
    {
        if (!recording)
            return;
        e.Handled = true;
        if (IsModifier(e.Key))
        { lastMod = ModName(e.Key); return; } // alone if released with no other key
        lastMod = null;
        string name = KeyName(e.Key);
        if (name == null)
            return;
        var parts = new List<string>();
        if (e.KeyModifiers.HasFlag(KeyModifiers.Control)) parts.Add("Ctrl");
        if (e.KeyModifiers.HasFlag(KeyModifiers.Shift)) parts.Add("Shift");
        if (e.KeyModifiers.HasFlag(KeyModifiers.Alt)) parts.Add("Alt");
        parts.Add(name);
        text.Text = string.Join("+", parts);
        Stop();
        Changed?.Invoke();
    }

    void OnKeyUp(object sender, KeyEventArgs e)
    {
        if (!recording)
            return;
        e.Handled = true;
        if (lastMod == null || !IsModifier(e.Key))
            return;
        text.Text = lastMod;
        lastMod = null;
        Stop();
        Changed?.Invoke();
    }

    // The layer's key names (the same table as the WinForms KeyBox).
    static string KeyName(Key k)
    {
        if (k >= Key.A && k <= Key.Z) return k.ToString();
        if (k >= Key.D0 && k <= Key.D9) return ((int)(k - Key.D0)).ToString();
        if (k >= Key.F1 && k <= Key.F24) return k.ToString();
        if (k >= Key.NumPad0 && k <= Key.NumPad9) return "Num" + (int)(k - Key.NumPad0);
        return k switch
        {
            Key.Escape => "Esc", Key.Return => "Enter", Key.Space => "Space", Key.Tab => "Tab", Key.Back => "Backspace",
            Key.Delete => "Delete", Key.Insert => "Insert", Key.Home => "Home", Key.End => "End", Key.PageUp => "PageUp",
            Key.PageDown => "PageDown", Key.Up => "Up", Key.Down => "Down", Key.Left => "Left", Key.Right => "Right",
            Key.OemMinus => "minus", Key.OemPlus => "equals", Key.OemComma => "comma", Key.OemPeriod => "period",
            Key.OemQuestion => "slash", Key.OemSemicolon => "semicolon", Key.OemQuotes => "quote",
            Key.OemOpenBrackets => "lbracket", Key.OemCloseBrackets => "rbracket", Key.OemPipe => "backslash",
            Key.OemTilde => "grave", Key.Multiply => "NumMul", Key.Add => "NumAdd", Key.Subtract => "NumSub",
            Key.Divide => "NumDiv", Key.Decimal => "NumDec", Key.CapsLock => "CapsLock", Key.PrintScreen => "PrintScreen",
            Key.Pause => "Pause", Key.Scroll => "ScrollLock",
            _ => null,
        };
    }
}
