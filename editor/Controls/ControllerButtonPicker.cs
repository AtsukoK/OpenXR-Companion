using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Both controllers as pictures: click a button, stick, trigger... then how (press, touch, push up...) to pick it,
// instead of searching the dropdowns. Returns (hand "left"/"right", button code like "thumbstick.up"), or null.
public static class ControllerButtonPicker
{
    static string WayText(ControllerPart part, string way) => way switch
    {
        "" => part.Shape switch
        {
            PartShape.Trigger => "Pull", PartShape.Grip => "Squeeze", PartShape.Stick => "Click (press in)",
            _ => "Press",
        },
        "touch" => "Just touching",
        "full" => part.Shape == PartShape.Grip ? "Squeezed all the way" : "Pulled all the way",
        "up" => part.Shape == PartShape.Pad ? "Swipe up" : "Push up",
        "down" => part.Shape == PartShape.Pad ? "Swipe down" : "Push down",
        "left" => part.Shape == PartShape.Pad ? "Swipe left" : "Push left",
        "right" => part.Shape == PartShape.Pad ? "Swipe right" : "Push right",
        _ => way,
    };

    public static async Task<(string hand, string button)?> Show(Visual owner, string currentHand = null, string currentButton = null)
    {
        (string, string)? result = null;
        var win = new Window
        {
            Title = "Pick a controller button", Width = 1200, Height = 820, WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = Brush.Parse("#1B1E23"),
        };
        var layouts = ControllerLayouts.All;
        var layoutBox = new ChoiceCombo(layouts.Select(l => new Choice(l.Id, l.Name)), 320)
        {
            Code = ControllerLayouts.ById(EditorApi.PictureChoice(Shell.Store.Profile)).Id,
        };
        var pics = new[] { new ControllerDrawing { Stacked = true }, new ControllerDrawing { Stacked = true } };
        var ways = new WrapPanel();
        var prompt = new TextBlock { Text = "Click a button, stick, trigger or grip on either controller.", Classes = { "desc" } };
        string curComp = currentButton?.Split('.')[0];

        void ShowLayouts()
        {
            var layout = ControllerLayouts.ById(layoutBox.Code);
            for (int h = 0; h < 2; h++)
                pics[h].ShowController(layout, h, (h == 0) == (currentHand == "left") ? curComp : null, _ => default);
        }

        void Picked(int hand, ControllerPart part)
        {
            pics[1 - hand].Selected = null;
            pics[hand].Selected = part.Comp;
            ways.Children.Clear();
            string name = (hand == 0 ? "Left " : "Right ") + part.LabelFor(hand);
            if (!part.IsUsable)
            {
                prompt.Text = name + ": " + part.NotUsable;
                return;
            }
            if (part.Ways.Length == 1)
            {
                result = (hand == 0 ? "left" : "right", part.Comp);
                win.Close();
                return;
            }
            prompt.Text = name + ": how?";
            foreach (var way in part.Ways)
            {
                var b = new Button { Content = WayText(part, way), MinWidth = 150, MinHeight = 44, Margin = new Thickness(0, 0, 8, 8), FontSize = 15 };
                string w = way;
                b.Click += (s, e) =>
                {
                    result = (hand == 0 ? "left" : "right", w == "" ? part.Comp : part.Comp + "." + w);
                    win.Close();
                };
                ways.Children.Add(b);
            }
        }

        for (int h = 0; h < 2; h++)
        {
            int hand = h;
            pics[h].PartClicked += part => Picked(hand, part);
        }
        layoutBox.SelectionChanged += (s, e) => { ways.Children.Clear(); ShowLayouts(); };
        ShowLayouts();

        var cancel = new Button { Content = "Cancel" };
        cancel.Click += (s, e) => win.Close();
        var grid = new Grid { ColumnDefinitions = new ColumnDefinitions("*,*"), Margin = new Thickness(0, 6) };
        for (int h = 0; h < 2; h++)
        {
            var col = new StackPanel { Spacing = 4, Margin = new Thickness(6, 0) };
            col.Children.Add(new TextBlock { Text = h == 0 ? "LEFT" : "RIGHT", FontWeight = FontWeight.Bold, Foreground = Brush.Parse("#7FA8F0"),
                                             HorizontalAlignment = HorizontalAlignment.Center });
            col.Children.Add(pics[h]);
            Grid.SetColumn(col, h);
            grid.Children.Add(col);
        }
        var body = new DockPanel { Margin = new Thickness(20) };
        var top = new StackPanel { Spacing = 8 };
        top.Children.Add(new TextBlock { Text = "Pick a controller button", FontSize = 22, FontWeight = FontWeight.SemiBold });
        top.Children.Add(new StackPanel
        {
            Orientation = Orientation.Horizontal, Spacing = 8,
            Children = { new TextBlock { Text = "Controller:", VerticalAlignment = VerticalAlignment.Center }, layoutBox, cancel },
        });
        top.Children.Add(prompt);
        top.Children.Add(ways);
        DockPanel.SetDock(top, Dock.Top);
        body.Children.Add(top);
        body.Children.Add(new ScrollViewer { Content = grid });
        win.Content = body;
        if (TopLevel.GetTopLevel(owner) is Window parent)
            await win.ShowDialog(parent);
        return result;
    }
}
