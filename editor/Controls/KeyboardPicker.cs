using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;

namespace OverviewPrototype;

// A whole keyboard (and the mouse buttons) to click instead of recording a key: easy with a VR pointer on the
// mirrored editor. Ctrl / Shift / Alt / Win stay lit when clicked and join the next key ("Ctrl+S"); "Use the
// lit keys" binds modifiers alone. Names are the layer's (keys.cpp). Returns the combo, or null when closed.
public static class KeyboardPicker
{
    const double U = 46, Pad = 3; // one key unit in pixels, gap

    static readonly IBrush KeyBg = Brush.Parse("#2A2F37"), KeyLit = Brush.Parse("#2A4A6A"), KeyNow = Brush.Parse("#4A3A6A");

    // (label, layer name, width in units); null name = a gap
    sealed record K(string Label, string Code, double W = 1);

    static readonly K[][] Main =
    {
        new K[] { new("Esc", "Esc"), new("", null, 1), new("F1", "F1"), new("F2", "F2"), new("F3", "F3"), new("F4", "F4"), new("", null, 0.5),
                  new("F5", "F5"), new("F6", "F6"), new("F7", "F7"), new("F8", "F8"), new("", null, 0.5),
                  new("F9", "F9"), new("F10", "F10"), new("F11", "F11"), new("F12", "F12") },
        new K[] { new("`", "grave"), new("1", "1"), new("2", "2"), new("3", "3"), new("4", "4"), new("5", "5"), new("6", "6"), new("7", "7"),
                  new("8", "8"), new("9", "9"), new("0", "0"), new("-", "minus"), new("=", "equals"), new("Backspace", "Backspace", 2) },
        new K[] { new("Tab", "Tab", 1.5), new("Q", "Q"), new("W", "W"), new("E", "E"), new("R", "R"), new("T", "T"), new("Y", "Y"), new("U", "U"),
                  new("I", "I"), new("O", "O"), new("P", "P"), new("[", "lbracket"), new("]", "rbracket"), new("\\", "backslash", 1.5) },
        new K[] { new("Caps", "CapsLock", 1.75), new("A", "A"), new("S", "S"), new("D", "D"), new("F", "F"), new("G", "G"), new("H", "H"),
                  new("J", "J"), new("K", "K"), new("L", "L"), new(";", "semicolon"), new("'", "quote"), new("Enter", "Enter", 2.25) },
        new K[] { new("Shift", "LShift", 2.25), new("Z", "Z"), new("X", "X"), new("C", "C"), new("V", "V"), new("B", "B"), new("N", "N"),
                  new("M", "M"), new(",", "comma"), new(".", "period"), new("/", "slash"), new("Shift", "RShift", 2.75) },
        new K[] { new("Ctrl", "LCtrl", 1.5), new("Win", "Win", 1.25), new("Alt", "LAlt", 1.25), new("Space", "Space", 6.25),
                  new("Alt", "RAlt", 1.25), new("Menu", "Apps", 1.25), new("Ctrl", "RCtrl", 1.25) },
    };

    static readonly K[][] Nav =
    {
        new K[] { new("PrtSc", "PrintScreen"), new("ScrLk", "ScrollLock"), new("Pause", "Pause") },
        new K[] { new("Ins", "Insert"), new("Home", "Home"), new("PgUp", "PageUp") },
        new K[] { new("Del", "Delete"), new("End", "End"), new("PgDn", "PageDown") },
        new K[] { },
        new K[] { new("", null), new("↑", "Up") },
        new K[] { new("←", "Left"), new("↓", "Down"), new("→", "Right") },
    };

    static readonly K[][] Numpad =
    {
        new K[] { },
        new K[] { new("Num", "NumLock"), new("/", "NumDiv"), new("*", "NumMul"), new("-", "NumSub") },
        new K[] { new("7", "Num7"), new("8", "Num8"), new("9", "Num9"), new("+", "NumAdd") },
        new K[] { new("4", "Num4"), new("5", "Num5"), new("6", "Num6") },
        new K[] { new("1", "Num1"), new("2", "Num2"), new("3", "Num3"), new("Enter", "NumEnter") },
        new K[] { new("0", "Num0", 2), new(".", "NumDec") },
    };

    static readonly K[] Mouse =
    {
        new("Left click", "LMB", 2), new("Right click", "RMB", 2), new("Middle click", "MMB", 2.25), new("Mouse 4", "Mouse4", 1.75),
        new("Mouse 5", "Mouse5", 1.75), new("Scroll up", "WheelUp", 2), new("Scroll down", "WheelDown", 2.25),
    };

    // modifier key -> the name used in a combo (left ones as the plain name, which the layer reads as the left key)
    static string ModOf(string code) => code switch
    {
        "LCtrl" => "Ctrl", "RCtrl" => "RCtrl", "LShift" => "Shift", "RShift" => "RShift",
        "LAlt" => "Alt", "RAlt" => "RAlt", "Win" => "Win", _ => null,
    };
    static readonly string[] ModOrder = { "Ctrl", "RCtrl", "Shift", "RShift", "Alt", "RAlt", "Win" };

    public static async Task<string> Show(Visual owner, string current)
    {
        string result = null;
        var lit = new List<string>(); // modifier codes, in click order
        var buttons = new List<(Button b, string code)>();
        var nowParts = new HashSet<string>((current ?? "").Split('+', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries),
                                           StringComparer.OrdinalIgnoreCase);
        var win = new Window
        {
            Title = "Pick a key", Width = 1180, Height = 560, WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = Brush.Parse("#1B1E23"),
        };
        var status = new TextBlock { Classes = { "desc" }, VerticalAlignment = VerticalAlignment.Center };
        var useLit = new Button { Content = "Use the lit keys", IsEnabled = false };

        void Paint()
        {
            foreach (var (b, code) in buttons)
                b.Background = lit.Contains(code) ? KeyLit : nowParts.Contains(code) || nowParts.Contains(ModOf(code) ?? "\0") ? KeyNow : KeyBg;
            string mods = string.Join("+", lit.Select(ModOf).Distinct().OrderBy(m => Array.IndexOf(ModOrder, m)));
            status.Text = lit.Count == 0 ? "Click a key. Click Ctrl, Shift, Alt or Win first for a combo like Ctrl+S."
                                         : $"Held: {mods}  ·  now click the key, or 'Use the lit keys' for just these.";
            useLit.IsEnabled = lit.Count > 0;
        }

        void Click(string code)
        {
            if (ModOf(code) != null)
            {
                if (!lit.Remove(code))
                    lit.Add(code);
                Paint();
                return;
            }
            var parts = lit.Select(ModOf).Distinct().OrderBy(m => Array.IndexOf(ModOrder, m)).ToList();
            parts.Add(code);
            result = string.Join("+", parts);
            win.Close();
        }

        useLit.Click += (s, e) =>
        {
            // one modifier alone keeps its side (LShift); several are a combo of them
            result = lit.Count == 1 ? lit[0] : string.Join("+", lit.Select(ModOf).Distinct().OrderBy(m => Array.IndexOf(ModOrder, m)));
            win.Close();
        };

        Canvas Block(K[][] rows)
        {
            var c = new Canvas();
            double maxW = 0;
            for (int r = 0; r < rows.Length; r++)
            {
                double x = 0, y = r * U + (r > 0 ? U * 0.3 : 0);
                foreach (var k in rows[r])
                {
                    if (k.Code != null)
                    {
                        var b = new Button
                        {
                            Content = k.Label, Width = k.W * U - Pad, Height = U - Pad, Padding = new Thickness(2),
                            HorizontalContentAlignment = HorizontalAlignment.Center, VerticalContentAlignment = VerticalAlignment.Center,
                            FontSize = k.Label.Length > 3 ? 11 : 14,
                        };
                        ToolTip.SetTip(b, k.Code);
                        string code = k.Code;
                        b.Click += (s, e) => Click(code);
                        Canvas.SetLeft(b, x);
                        Canvas.SetTop(b, y);
                        c.Children.Add(b);
                        buttons.Add((b, code));
                    }
                    x += k.W * U;
                }
                maxW = Math.Max(maxW, x);
            }
            c.Width = maxW;
            c.Height = rows.Length * U + U * 0.3;
            return c;
        }

        var keyboard = new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = U * 0.4,
            Children = { Block(Main), Block(Nav), Block(Numpad) },
        };
        var body = new StackPanel { Margin = new Thickness(20), Spacing = 10 };
        body.Children.Add(new TextBlock { Text = "Pick a key", FontSize = 22, FontWeight = FontWeight.SemiBold });
        body.Children.Add(status);
        body.Children.Add(keyboard);
        body.Children.Add(new TextBlock { Text = "MOUSE", FontWeight = FontWeight.Bold, FontSize = 12, Foreground = Brush.Parse("#7FA8F0") });
        body.Children.Add(Block(new[] { Mouse }));
        var cancel = new Button { Content = "Cancel" };
        cancel.Click += (s, e) => win.Close();
        body.Children.Add(new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { useLit, cancel } });
        body.Children.Add(new TextBlock { Text = "Purple = the key it has now.", Classes = { "desc" } });
        win.Content = new ScrollViewer { Content = body, HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto };
        Paint();
        if (TopLevel.GetTopLevel(owner) is Window parent)
            await win.ShowDialog(parent);
        return result;
    }
}
