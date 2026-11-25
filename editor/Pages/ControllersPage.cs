using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Controllers: both controllers drawn with a tag on every button saying what it does (port of the WinForms
// ControllersHub). Clicking a button opens its PartPanel underneath (every way to use it, + Add, Swap, Reset, bindings).
// Not yet:
// the preset dialog, the Steam Frame checkbox.
public class ControllersPage : UserControl
{
    // The picked controller. The WinForms editor saves it app-wide (controller_picture.txt); the port preview keeps it
    // in memory so it never writes outside the profile.
    static string chosen;

    readonly ProfileModel p = Shell.Store.Profile;
    ControllerLayout layout;
    readonly ControllerDrawing[] pics = { new() { AddButtons = true }, new() { AddButtons = true } };
    readonly ContentControl partPanel = new();
    readonly TextBlock title = new() { FontSize = 26, FontWeight = FontWeight.SemiBold };
    readonly TextBlock models = new() { Foreground = Brush.Parse("#AAB2BF"), VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(10, 0, 0, 0) };

    public ControllersPage() : this(-1, null) { }

    // openHand / openComp: a button to open straight away (page key controllers:<hand>.<comp>, for checks).
    public ControllersPage(int openHand, string openComp)
    {
        layout = ControllerLayouts.ById(chosen ?? EditorApi.PictureChoice(p));
        if (openHand is 0 or 1 && layout.PartsFor(openHand).FirstOrDefault(x => x.Comp == openComp) is ControllerPart open)
            Dispatcher.UIThread.Post(() => { pics[openHand].Selected = open.Comp; ShowPart(openHand, open); });
        var chooser = new ChoiceCombo(ControllerLayouts.All.Select(l => new Choice(l.Id, l.Name)), 330) { Code = layout.Id };
        var frameNative = SteamFrameBox.New(); // only for the Steam Frame
        frameNative.IsVisible = layout.Id == "frame";
        chooser.SelectionChanged += (s, e) =>
        {
            if (chooser.Code == null || chooser.Code == layout.Id) return;
            chosen = chooser.Code;
            layout = ControllerLayouts.ById(chosen);
            frameNative.IsVisible = layout.Id == "frame";
            partPanel.Content = null;
            Refresh();
        };
        var offsets = new Button { Content = "Controller offsets  ▶", Classes = { "pill" } };
        offsets.Click += (s, e) => Shell.Go("settings:offsets");
        var preset = new Button { Content = "Apply a controller preset…", Classes = { "go" }, Margin = new Thickness(0) };
        ToolTip.SetTip(preset, "Pick the controllers you use (Quest, Index, Vive, WMR...): their usual buttons are set to open your right and left hand wheels. Other bindings are not changed.");
        preset.Click += async (s, e) => { if (await ChoosePreset(this)) Refresh(); };

        for (int h = 0; h < 2; h++)
        {
            int hand = h;
            pics[h].PartClicked += part => ShowPart(hand, part);
            pics[h].AddClicked += part => AddTo(hand, part);
        }
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("*,*"), ColumnSpacing = 12 };
        for (int h = 0; h < 2; h++)
        {
            var col = new StackPanel
            {
                Spacing = 6,
                Children =
                {
                    new StackPanel { Orientation = Orientation.Horizontal, Spacing = 10, Children =
                    {
                        new TextBlock { Text = h == 0 ? "LEFT CONTROLLER" : "RIGHT CONTROLLER", FontWeight = FontWeight.Bold, Foreground = Brush.Parse("#7FA8F0"), VerticalAlignment = VerticalAlignment.Center },
                        SwitchAll(h),
                    } },
                    new Border { Background = Brush.Parse("#23272E"), CornerRadius = new CornerRadius(6), Padding = new Thickness(6), Child = pics[h] },
                },
            };
            Grid.SetColumn(col, h);
            grid.Children.Add(col);
        }
        Content = new ScrollViewer
        {
            Content = new StackPanel
            {
                Margin = new Thickness(16, 12), Spacing = 8,
                Children =
                {
                    title,
                    new WrapPanel
                    {
                        Children =
                        {
                            new TextBlock { Text = "Controller:", VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 8, 0) },
                            chooser, models, frameNative,
                            new Border { Width = 18 }, preset,
                            new TextBlock { Text = "sets which buttons open your wheels", Foreground = Brush.Parse("#AAB2BF"), VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(8, 0, 18, 0) },
                            offsets,
                        },
                    },
                    new TextBlock { Classes = { "desc" }, Text = "Click a button (or its tag) to see what it does. The + on a tag adds something to that button. Blue = your bindings, grey 'Default' = the game's own control." },
                    grid,
                    partPanel,
                },
            },
        };
        Refresh();
    }

    // "2 of 3 on  [All on] [All off]": every binding on this controller (its last button's hand), like the WinForms page strip.
    readonly TextBlock[] counts = { new(), new() };
    static int HandOf(BindingModel b)
    {
        string last = (b.Input ?? "").Split('+').Last();
        return last.StartsWith("left.") ? 0 : last.StartsWith("right.") ? 1 : -1;
    }
    Control SwitchAll(int hand)
    {
        counts[hand].Foreground = Brush.Parse("#AAB2BF");
        counts[hand].VerticalAlignment = VerticalAlignment.Center;
        Button B(string text, bool v)
        {
            var b = new Button { Content = text, Classes = { "pill" }, Padding = new Thickness(8, 2) };
            ToolTip.SetTip(b, v ? "Switch on every binding on this controller" : "Switch off every binding on this controller (kept, just not used)");
            b.Click += (s, e) =>
            {
                var list = p.Bindings.Where(x => HandOf(x) == hand && x.Enabled != v).ToList();
                if (list.Count == 0) return;
                foreach (var x in list) x.Enabled = v;
                Shell.Store.MarkDirty();
                Refresh();
            };
            return b;
        }
        return new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6, Children = { counts[hand], B("All on", true), B("All off", false) } };
    }

    void Refresh()
    {
        for (int k = 0; k < 2; k++)
        {
            var mine = p.Bindings.Where(x => HandOf(x) == k).ToList();
            counts[k].Text = $"{mine.Count(x => x.Enabled)} of {mine.Count} bindings on";
        }
        title.Text = "Controllers  ·  " + layout.Name;
        models.Text = "For: " + layout.Models;
        for (int h = 0; h < 2; h++)
        {
            int hand = h;
            pics[h].ShowController(layout, hand, null,
                part => QuickBindings.UseOf(p.Bindings, hand, part),
                part => TagLines.Of(p.Bindings, hand, part));
        }
    }

    // The clicked button: every way of using it, what each does, + Add / Swap / Reset and its bindings (PartPanel).
    void ShowPart(int hand, ControllerPart part)
    {
        pics[1 - hand].Selected = null;
        partPanel.Content = new PartPanel(layout, hand, part, Refresh);
    }

    // "Your controllers": the preset rewrites the bindings that open each hand's main wheel (WinForms ChooseControllers).
    // Marks the profile unsaved (Save keeps it). firstRun: the welcome question at the first start, which also writes the
    // app-wide "asked already" flag (controller_chosen, shared with WinForms). True when something changed.
    public static async Task<bool> ChoosePreset(Avalonia.Visual owner, bool firstRun = false)
    {
        var p = Shell.Store.Profile;
        var kinds = EditorApi.ControllerKinds();
        string current = EditorApi.CurrentControllerKind(p);
        var how = new TextBlock { Classes = { "desc" }, MaxWidth = 560 };
        var radios = new StackPanel { Spacing = 4 };
        foreach (var k in kinds)
        {
            var r = new RadioButton { Content = k.label, GroupName = "kind", Tag = k.id, IsChecked = k.id == current };
            r.IsCheckedChanged += (s, e) => { if (r.IsChecked == true) how.Text = k.how; };
            radios.Children.Add(r);
        }
        if (radios.Children.OfType<RadioButton>().All(r => r.IsChecked != true))
            ((RadioButton)radios.Children[0]).IsChecked = true;
        how.Text = kinds.First(k => k.id == (string)radios.Children.OfType<RadioButton>().First(r => r.IsChecked == true).Tag).how;
        bool ok = await Dialogs.Ask(owner, "Your controllers",
                (firstRun ? "Welcome to OpenXR Companion!\n\n" : "") +
                "Which controllers do you use? This sets the buttons that open the right and left hand wheels in '" + Shell.Store.CurrentName + "'. Nothing else changes" +
                (firstRun ? ", and you can change it later on the Controllers page." : "."),
                new StackPanel { Spacing = 10, Children = { radios, how } });
        string kind = (string)radios.Children.OfType<RadioButton>().First(r => r.IsChecked == true).Tag;
        if (firstRun)
            try { File.WriteAllText(Path.Combine(ProfileStore.RealDataDir, "controller_chosen"), ok ? kind : current ?? ""); } catch { }
        if (!ok)
            return false;
        int n = ControllerPresets.Apply(p, kind);
        if (n == 0)
        { Shell.SetStatus("This profile has no controller buttons that open a wheel, so nothing was changed.", true); return false; }
        Shell.Store.MarkDirty();
        Shell.SetStatus("Done: " + kinds.First(k => k.id == kind).how + " (Save to keep it)");
        return true;
    }

    // A new binding on this button (its first way of use), opened on its own page.
    void AddTo(int hand, ControllerPart part)
    {
        if (!part.IsUsable)
            return;
        var b = new BindingModel
        {
            Name = (hand == 0 ? "Left " : "Right ") + part.LabelFor(hand),
            Input = ControllerLayouts.InputOf(hand, part, part.Ways.First()),
            Gesture = GestureKind.Press,
            Action = new ActionModel { Type = ActionType.Key, Keys = "F1" },
        };
        p.Bindings.Add(b);
        Shell.Store.MarkDirty();
        Shell.Go("binding:" + p.Bindings.IndexOf(b));
    }
}
