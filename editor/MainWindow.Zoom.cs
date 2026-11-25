using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;

namespace OverviewPrototype;

// The "Size" slider: scales the whole editor (on top of Windows' own display scaling), 60% - 200%.
// Remembered for the editor only, in avalonia_editor_size.txt next to the profiles folder; written only when the user
// changes it (tests pass zoom=... to --shot, which never saves).
public partial class MainWindow
{
    public static string ZoomFile => Path.Combine(ProfileStore.RealDataDir, "avalonia_editor_size.txt");
    bool zoomLoading;

    void SetupZoom()
    {
        var slider = this.FindControl<Slider>("ZoomSlider")!;
        var reset = this.FindControl<Button>("ZoomResetButton")!;
        // While dragging, only the % label follows: zooming mid-drag resizes the slider under the mouse and makes it jump.
        // The size is applied when the mouse is let go.
        bool dragging = false;
        void Commit()
        {
            ApplyZoom(slider.Value / 100.0);
            if (!zoomLoading)
                try { File.WriteAllText(ZoomFile, Math.Round(slider.Value).ToString(System.Globalization.CultureInfo.InvariantCulture)); } catch { }
        }
        slider.AddHandler(PointerPressedEvent, (object s, PointerPressedEventArgs e) => dragging = true, Avalonia.Interactivity.RoutingStrategies.Tunnel, true);
        void EndDrag() { if (dragging) { dragging = false; Commit(); } }
        slider.AddHandler(PointerReleasedEvent, (object s, PointerReleasedEventArgs e) => EndDrag(), Avalonia.Interactivity.RoutingStrategies.Tunnel | Avalonia.Interactivity.RoutingStrategies.Bubble, true);
        slider.AddHandler(PointerCaptureLostEvent, (object s, PointerCaptureLostEventArgs e) => EndDrag(), Avalonia.Interactivity.RoutingStrategies.Bubble, true);
        slider.PropertyChanged += (s, e) =>
        {
            if (e.Property != Slider.ValueProperty) return;
            reset.Content = Math.Round(slider.Value) + "%";
            if (!dragging) Commit();
        };
        this.FindControl<Button>("ZoomDownButton")!.Click += (s, e) => slider.Value = Math.Max(60, slider.Value - 5);
        this.FindControl<Button>("ZoomUpButton")!.Click += (s, e) => slider.Value = Math.Min(200, slider.Value + 5);
        reset.Click += (s, e) => slider.Value = 100;
        KeyDown += (s, e) =>
        {
            if (!e.KeyModifiers.HasFlag(KeyModifiers.Control)) return;
            if (e.Key is Key.OemPlus or Key.Add) { slider.Value = Math.Min(200, slider.Value + 10); e.Handled = true; }
            else if (e.Key is Key.OemMinus or Key.Subtract) { slider.Value = Math.Max(60, slider.Value - 10); e.Handled = true; }
            else if (e.Key is Key.D0 or Key.NumPad0) { slider.Value = 100; e.Handled = true; }
        };
        // Ctrl + mouse wheel, like a browser
        AddHandler(PointerWheelChangedEvent, (object s, PointerWheelEventArgs e) =>
        {
            if (!e.KeyModifiers.HasFlag(KeyModifiers.Control)) return;
            slider.Value = Math.Clamp(slider.Value + (e.Delta.Y > 0 ? 10 : -10), 60, 200);
            e.Handled = true;
        }, Avalonia.Interactivity.RoutingStrategies.Tunnel);

        double saved = 100;
        try { if (File.Exists(ZoomFile)) saved = double.Parse(File.ReadAllText(ZoomFile).Trim(), System.Globalization.CultureInfo.InvariantCulture); } catch { }
        SetZoomPercent(saved, false);
    }

    // Test hook too (--shot zoom=1.5): set without saving.
    public void SetZoomPercent(double percent, bool save)
    {
        var slider = this.FindControl<Slider>("ZoomSlider")!;
        zoomLoading = !save;
        slider.Value = Math.Clamp(percent, 60, 200);
        ApplyZoom(slider.Value / 100.0);
        this.FindControl<Button>("ZoomResetButton")!.Content = Math.Round(slider.Value) + "%";
        zoomLoading = false;
    }

    void ApplyZoom(double z) =>
        this.FindControl<LayoutTransformControl>("ZoomHost")!.LayoutTransform = Math.Abs(z - 1) < 0.001 ? null : new ScaleTransform(z, z);

    // The window opens at 1400 x 900 (before Windows scaling); on a small or highly scaled screen shrink it to fit
    // the work area and centre it, so it never starts bigger than the screen.
    void FitToScreen()
    {
        var screen = Screens.ScreenFromWindow(this) ?? Screens.Primary;
        if (screen == null) return;
        double s = screen.Scaling;
        var area = screen.WorkingArea;
        double maxW = area.Width / s * 0.95, maxH = area.Height / s * 0.92;
        if (Width <= maxW && Height <= maxH) return;
        Width = Math.Max(MinWidth, Math.Min(Width, maxW));
        Height = Math.Max(MinHeight, Math.Min(Height, maxH));
        Position = new PixelPoint(area.X + (int)((area.Width - Width * s) / 2), area.Y + (int)((area.Height - Height * s) / 2));
    }

    // Headset mirror switch, first on the toolbar like in WinForms: app-wide (HeadsetMirrorFile, shared with the layer,
    // saved straight away). Polled every second because the layer turns it off when its window is closed.
    void SetupMirrorButton()
    {
        var mb = this.FindControl<Button>("MirrorButton")!;
        void Show()
        {
            bool on = OpenXRCompanionEditor.HeadsetMirrorFile.On;
            mb.Content = on ? "● Headset mirror: ON" : "○ Headset mirror: off";
            mb.Foreground = new SolidColorBrush(on ? Color.Parse("#4CC27A") : Color.Parse("#8A93A0"));
        }
        ToolTip.SetTip(mb, "A window on your desktop showing what the headset shows, with the wheel, wrist panels, screen and lasers in it " +
            "(the game's own mirror can't show those). Turns on in a running game within a second; closing the window turns it off. " +
            "Eye, zoom and frame rate: Settings > Headset mirror. D3D11 games.");
        mb.Click += (s, e) => { OpenXRCompanionEditor.HeadsetMirrorFile.On = !OpenXRCompanionEditor.HeadsetMirrorFile.On; Show(); };
        var t = new Avalonia.Threading.DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        t.Tick += (s, e) => Show();
        t.Start();
        Show();
    }

    // ---- the window's size: maximized the first time; after that as the user left it (size, place, maximized).
    // Kept in avalonia_editor_window.txt: "max|normal x y w h" (x, y in screen pixels, w, h in window units).
    public static string WindowFile => Path.Combine(ProfileStore.RealDataDir, "avalonia_editor_window.txt");
    PixelPoint normalPos;
    Size normalSize;

    void RestoreWindow()
    {
        string[] v = null;
        try { if (File.Exists(WindowFile)) v = File.ReadAllText(WindowFile).Trim().Split(' '); } catch { }
        var inv = System.Globalization.CultureInfo.InvariantCulture;
        if (v is { Length: 5 } && int.TryParse(v[1], out int x) && int.TryParse(v[2], out int y)
            && double.TryParse(v[3], System.Globalization.NumberStyles.Float, inv, out double w)
            && double.TryParse(v[4], System.Globalization.NumberStyles.Float, inv, out double h))
        {
            WindowStartupLocation = WindowStartupLocation.Manual;
            Width = Math.Max(MinWidth, w);
            Height = Math.Max(MinHeight, h);
            Position = new PixelPoint(x, y);
            if (v[0] == "max") WindowState = WindowState.Maximized;
            // a monitor that is gone, or a smaller screen: back onto one that is there
            Opened += (s, e) =>
            {
                var scr = Screens.All.FirstOrDefault(sc => sc.WorkingArea.Contains(new PixelPoint(x + 40, y + 20)));
                if (scr == null) { Position = Screens.Primary?.WorkingArea.Position ?? default; FitToScreen(); }
                else if (WindowState != WindowState.Maximized) FitToScreen();
            };
        }
        else
            WindowState = WindowState.Maximized; // first start: use the whole screen
        normalPos = Position;
        normalSize = new Size(Width, Height);
        PositionChanged += (s, e) => { if (WindowState == WindowState.Normal) normalPos = Position; };
        Resized += (s, e) => { if (WindowState == WindowState.Normal) normalSize = new Size(Width, Height); };
        Closing += (s, e) =>
        {
            var inv2 = System.Globalization.CultureInfo.InvariantCulture;
            string state = WindowState == WindowState.Maximized ? "max" : "normal";
            if (WindowState == WindowState.Normal) { normalPos = Position; normalSize = new Size(Width, Height); }
            try { File.WriteAllText(WindowFile, $"{state} {normalPos.X} {normalPos.Y} {normalSize.Width.ToString(inv2)} {normalSize.Height.ToString(inv2)}"); } catch { }
        };
    }
}
