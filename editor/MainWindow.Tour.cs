using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Running a tour (port of the WinForms coach card): an orange frame around the step's control, a coach card in the
// bottom corner away from it with Back / Next / Stop, "Your turn" steps that wait (checked 5 times a second), and at
// the end the choice to keep or throw away what was made (the profile is snapshotted first).
public partial class MainWindow : ITourHost
{
    Canvas TourCanvas => this.FindControl<Canvas>("TourLayer")!;
    List<TourStep> tour;
    int tourAt = -1;
    string tourSnapshot;
    bool tourWasDirty;
    DispatcherTimer tourTimer;
    Border coach, frame;
    TextBlock coachTitle, coachText, coachCount;
    Button coachNext, coachBack;
    bool advancing;

    // ---- ITourHost: controls to frame
    Control ITourHost.PageArea() => this.FindControl<ContentControl>("PageHost");
    Control ITourHost.RailArea() => this.FindControl<ItemsControl>("Rail");
    Control ITourHost.Picture() => Shown().OfType<ControllerDrawing>().FirstOrDefault();
    bool ITourHost.Shows(string text) => ((ITourHost)this).Find(text) != null;

    // Everything on screen except the tour's own layer, that is really visible.
    IEnumerable<Control> Shown() =>
        this.GetVisualDescendants().OfType<Control>().Where(c => c.IsEffectivelyVisible && !TourCanvas.IsVisualAncestorOf(c) && c != TourCanvas);

    static string TextOf(Control c) => c switch
    {
        TextBlock t => t.Text,
        Button { Content: string s } => s,
        _ => null,
    };

    // A button whose text (or a text inside it) contains this; else any text containing it.
    Control ITourHost.Find(string text)
    {
        var shown = Shown().ToList();
        var hit = shown.FirstOrDefault(c => TextOf(c)?.Contains(text, StringComparison.OrdinalIgnoreCase) == true);
        if (hit == null)
            return null;
        return hit as Button ?? hit.FindAncestorOfType<Button>() ?? hit;
    }

    // ---- starting and stopping
    public void TutorialMenu() => _ = TutorialMenuAsync();

    async Task TutorialMenuAsync()
    {
        string here = Tours.SectionTour(Shell.Current);
        var list = new List<TourInfo>();
        if (here != null && Tours.All.FirstOrDefault(t => t.Id == here) is TourInfo h)
            list.Add(h with { Group = "This page", Title = "This page: " + h.Title });
        list.AddRange(Tours.All);
        int pick = await OptionPicker.Show(this, "Tutorials", "Pick a tour. You can stop any time; what you make can be thrown away at the end.",
                                           list.Select(t => new PickOption(t.Group, t.Title, t.Does, t.Example)).ToList());
        if (pick >= 0)
            StartTutorial(list[pick].Id);
    }

    public void StartTutorial(string id)
    {
        if (tour != null)
            EndTutorial(keep: true);
        tour = Tours.Build(id, this);
        if (tour == null || tour.Count == 0)
        { tour = null; return; }
        tourSnapshot = Shell.Store.Serialize();
        tourWasDirty = Shell.Store.Dirty;
        BuildCoach();
        tourTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
        tourTimer.Tick += (s, e) => TourTick();
        tourTimer.Start();
        TourGo(0);
    }

    void BuildCoach()
    {
        coachTitle = new TextBlock { FontSize = 18, FontWeight = FontWeight.Bold, Foreground = Brushes.White, TextWrapping = TextWrapping.Wrap };
        coachText = new TextBlock { FontSize = 14, Foreground = Brush.Parse("#DCE4F0"), TextWrapping = TextWrapping.Wrap };
        coachCount = new TextBlock { Foreground = Brush.Parse("#96A0B4"), VerticalAlignment = VerticalAlignment.Center };
        Button B(string t, string bg, Action a)
        {
            var b = new Button { Content = t, Background = Brush.Parse(bg), Foreground = Brushes.White, FontWeight = FontWeight.Bold };
            b.Click += (s, e) => a();
            return b;
        }
        coachNext = B("Next  ▶", "#F08C1E", () => { if (tourAt >= tour.Count - 1) EndTutorial(keep: null); else TourGo(tourAt + 1); });
        coachBack = B("◀  Back", "#465268", () => TourGo(Math.Max(0, tourAt - 1)));
        var stop = B("Stop", "#465268", () => EndTutorial(keep: null));
        coach = new Border
        {
            Width = 460, Background = Brush.Parse("#1C2434"), CornerRadius = new CornerRadius(8), Padding = new Thickness(16),
            BorderBrush = Brush.Parse("#F08C1E"), BorderThickness = new Thickness(1),
            Child = new StackPanel
            {
                Spacing = 10,
                Children =
                {
                    coachTitle, coachText,
                    new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, HorizontalAlignment = HorizontalAlignment.Right, Children = { coachCount, stop, coachBack, coachNext } },
                },
            },
        };
        frame = new Border { BorderBrush = Brush.Parse("#FF8C14"), BorderThickness = new Thickness(4), CornerRadius = new CornerRadius(4), IsHitTestVisible = false };
        TourCanvas.Children.Clear();
        TourCanvas.Children.Add(frame);
        TourCanvas.Children.Add(coach);
        TourCanvas.IsVisible = true;
    }

    void TourGo(int i)
    {
        tourAt = i;
        var st = tour[i];
        try { st.Enter?.Invoke(); } catch (Exception) { }
        coachTitle.Text = st.Title;
        coachText.Text = st.Text;
        coachCount.Text = $"{i + 1} / {tour.Count}";
        coachBack.IsEnabled = i > 0;
        // after the page it went to is laid out: scroll what the step points at into view, then frame it
        Dispatcher.UIThread.Post(() =>
        {
            try { tour?[tourAt].Target?.Invoke()?.BringIntoView(); } catch (Exception) { }
            TourTick();
        }, DispatcherPriority.Background);
    }

    static bool SafeDone(TourStep st) { try { return st.Done(); } catch (Exception) { return false; } }

    void TourTick()
    {
        if (tour == null || tourAt < 0)
            return;
        var st = tour[tourAt];
        bool waiting = st.Done != null && !SafeDone(st);
        // a "your turn" step that is done moves on by itself (the last one just offers Finish)
        if (st.Done != null && !waiting && !advancing && tourAt + 1 < tour.Count)
        {
            advancing = true;
            TourGo(tourAt + 1);
            advancing = false;
            return;
        }
        coachNext.IsEnabled = !waiting;
        coachNext.Content = waiting ? "Waiting for you..." : tourAt >= tour.Count - 1 ? "Finish  ✓" : "Next  ▶";
        TourLayout();
    }

    void TourLayout()
    {
        Control target = null;
        try { target = tour[tourAt].Target?.Invoke(); } catch (Exception) { }
        Rect? r = null;
        if (target != null && target.IsEffectivelyVisible && target.TranslatePoint(new Point(0, 0), TourCanvas) is Point at)
            r = new Rect(at, target.Bounds.Size).Inflate(4).Intersect(new Rect(TourCanvas.Bounds.Size));
        frame.IsVisible = r is { Width: > 0, Height: > 0 };
        if (frame.IsVisible)
        {
            Canvas.SetLeft(frame, r.Value.X);
            Canvas.SetTop(frame, r.Value.Y);
            frame.Width = r.Value.Width;
            frame.Height = r.Value.Height;
        }
        // the coach card in the bottom corner away from the target
        double W = TourCanvas.Bounds.Width, H = TourCanvas.Bounds.Height;
        bool targetRight = r is Rect tr && tr.Center.X > W / 2 && tr.Width < W * 0.6;
        coach.Measure(Size.Infinity);
        Canvas.SetLeft(coach, targetRight ? 90 : W - coach.DesiredSize.Width - 24);
        Canvas.SetTop(coach, H - coach.DesiredSize.Height - 40);
    }

    // Test: see App --tourtest.
    public async Task TourTest(string resultFile, string shot)
    {
        var lines = new List<string>();
        int framed = 0, steps = 0;
        foreach (var info in Tours.All)
        {
            string before = Shell.Store.Serialize();
            StartTutorial(info.Id);
            if (tour == null) { lines.Add(info.Id + ": NO TOUR"); continue; }
            for (int i = 0; i < tour.Count; i++)
            {
                tourTimer.Stop(); // step by hand: no moving on by itself
                TourGo(i);
                await Task.Delay(350);
                TourTick();
                bool hasTarget = tour[i].Target != null;
                bool shownFrame = frame.IsVisible;
                steps++;
                if (!hasTarget || shownFrame) framed++;
                lines.Add($"{info.Id} {i + 1}/{tour.Count} {(hasTarget ? (shownFrame ? "framed " : "MISSING") : "no frame")}  {tour[i].Title}  [{Shell.Current}]");
                if (info.Id == "main" && i == 3)
                    await ShotAsync(shot);
                // a "your turn" step: do what the user would (click the framed button), unless it opens a dialog
                if (tour[i].Done != null && !SafeDone(tour[i]) && tour[i].Target?.Invoke() is Button b
                    && !(TextOf(b) ?? "").StartsWith("+ Add a button") && !(TextOf(b) ?? "").StartsWith("Change"))
                {
                    b.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Button.ClickEvent));
                    await Task.Delay(400);
                    lines.Add($"    clicked '{TextOf(b)}': done = {SafeDone(tour[i])}");
                }
            }
            EndTutorial(keep: false);
            await Task.Delay(200);
            lines.Add($"{info.Id}: after throwing away, profile as before: {Shell.Store.Serialize() == before}");
        }
        lines.Insert(0, $"steps: {steps}, targets found (or none needed): {framed}");
        File.WriteAllLines(resultFile, lines);
    }

    // keep: true = keep, false = throw away, null = ask when something changed
    async void EndTutorial(bool? keep)
    {
        tourTimer?.Stop();
        TourCanvas.IsVisible = false;
        TourCanvas.Children.Clear();
        var made = tour != null && Shell.Store.Serialize() != tourSnapshot;
        tour = null;
        tourAt = -1;
        if (!made)
            return;
        bool k = keep ?? await Dialogs.Confirm(this, "Tutorial", "Keep what you made during the tutorial?\n\nOK keeps it (save to use it in your games). Cancel puts the profile back as it was before the tutorial.", "Keep it");
        if (!k)
        {
            Shell.Store.Restore(tourSnapshot, tourWasDirty);
            Shell.Refresh();
            Shell.SetStatus("Put back as it was before the tutorial.");
        }
    }
}
