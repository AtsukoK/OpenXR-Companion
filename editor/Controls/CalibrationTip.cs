using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.TextFormatting;

namespace OverviewPrototype;

// "Quicker in the headset": a card beside a page's position numbers, with three small drawings of placing the thing by
// hand in Adjust mode (open Adjust, grab with the grip, stick = size). What = "keyboard", "wheel", "screen", "panel".
public class CalibrationTip : Border
{
    protected override Type StyleKeyOverride => typeof(Border); // styled as a card (styles match the exact type)

    // numbersAre: where the numbers it replaces are, as seen from the tip ("on the left", "above")
    public CalibrationTip(string what, string thing, string stickDoes, string numbersAre = "on the left")
    {
        Classes.Add("card");
        Width = 420;
        VerticalAlignment = VerticalAlignment.Top;
        BorderBrush = Brush.Parse("#F08C1E");
        Child = new StackPanel
        {
            Spacing = 10,
            Children =
            {
                new TextBlock { Text = "💡  Quicker in the headset", Classes = { "title" } },
                new TextBlock { Classes = { "desc" }, Text = $"These numbers are easiest to set by hand in VR: put the {thing} exactly where you like it." },
                Step(1, "Open Adjust mode", "Wrist panel › More › Calibrate Positions, or any button set to 'Move panels and wheels'.", new StepPicture(1, what)),
                Step(2, "Grab it with the grip", $"Point at the {thing}, hold the grip, move and turn your hand. Let go: it stays there.", new StepPicture(2, what)),
                Step(3, "Stick: " + stickDoes, "While you hold it, push the thumbstick up or down.", new StepPicture(3, what)),
                new TextBlock { Classes = { "desc" }, Text = $"That spot replaces the numbers {numbersAre} ('Placed by hand: Yes'). 'Use the numbers below again' brings them back." },
            },
        };
    }

    static Control Step(int n, string title, string text, Control picture) => new Grid
    {
        ColumnDefinitions = new ColumnDefinitions("110,*"),
        Children =
        {
            picture,
            new StackPanel
            {
                [Grid.ColumnProperty] = 1, Margin = new Thickness(12, 4, 0, 0), Spacing = 2,
                Children =
                {
                    new TextBlock { Text = $"{n}  {title}", FontWeight = FontWeight.SemiBold },
                    new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#AAB2BF"), FontSize = 12.5 },
                },
            },
        },
    };
}

// The small drawings: 1 forearm + wrist panel with the Adjust button lit, 2 a controller with its grip lit holding the
// thing, 3 the stick with up / down arrows and the thing wider and narrower.
public class StepPicture : Control
{
    readonly int step;
    readonly string what;

    public StepPicture(int step, string what)
    {
        this.step = step;
        this.what = what;
        Width = 110;
        Height = 72;
    }

    static readonly IBrush Back = Brush.Parse("#1E232B"), Body = Brush.Parse("#4A5262"), Edge = Brush.Parse("#6A7385"),
                          Lit = Brush.Parse("#F08C1E"), Thing = Brush.Parse("#2C3A55"), Blue = Brush.Parse("#5A8AE0"), Skin = Brush.Parse("#CDAA8C");

    public override void Render(DrawingContext g)
    {
        g.DrawRectangle(Back, null, new Rect(Bounds.Size), 6, 6);
        switch (step)
        {
            case 1: DrawAdjust(g); break;
            case 2: DrawGrab(g); break;
            default: DrawStick(g); break;
        }
    }

    // the thing being placed: a keyboard (keys), a wheel (ring), a screen or panel (rectangle)
    void DrawThing(DrawingContext g, Rect r)
    {
        if (what == "wheel")
        {
            double rad = Math.Min(r.Width, r.Height) / 2;
            g.DrawEllipse(Thing, new Pen(Blue, 1.5), r.Center, rad, rad);
            g.DrawEllipse(Back, null, r.Center, rad * 0.35, rad * 0.35);
            return;
        }
        g.DrawRectangle(Thing, new Pen(Blue, 1.5), r, 3, 3);
        if (what == "keyboard") // three rows of keys
            for (int row = 0; row < 3; row++)
                for (int k = 0; k < 6; k++)
                    g.DrawRectangle(Edge, null, new Rect(r.X + 3 + k * (r.Width - 6) / 6, r.Y + 3 + row * (r.Height - 6) / 3, (r.Width - 6) / 6 - 1.5, (r.Height - 6) / 3 - 1.5));
    }

    void DrawAdjust(DrawingContext g)
    {
        // forearm, and the wrist panel on it with one button lit
        g.DrawRectangle(Skin, null, new Rect(6, 40, 70, 16), 8, 8);
        var panel = new Rect(30, 12, 44, 30);
        g.DrawRectangle(Brush.Parse("#12161C"), new Pen(Edge, 1), panel, 3, 3);
        for (int i = 0; i < 6; i++)
        {
            var cell = new Rect(panel.X + 3 + (i % 3) * 13.3, panel.Y + 3 + (i / 3) * 12.5, 11.5, 10.5);
            g.DrawRectangle(i == 4 ? Lit : Thing, null, cell, 2, 2);
        }
        // the other hand's finger poking it
        g.DrawLine(new Pen(Skin, 5, lineCap: PenLineCap.Round), new Point(100, 52), new Point(62, 33));
        Label(g, "Adjust", new Point(80, 6));
    }

    void DrawGrab(DrawingContext g)
    {
        // the thing up top, a dashed arrow to the controller holding it
        DrawThing(g, what == "wheel" ? new Rect(58, 6, 34, 34) : new Rect(50, 8, 52, 26));
        g.DrawLine(new Pen(Blue, 1.5, DashStyle.Dash), new Point(38, 38), new Point(52, 28));
        // controller: head, handle, grip lit
        g.DrawEllipse(Body, new Pen(Edge, 1), new Point(26, 34), 14, 11);
        g.DrawRectangle(Body, new Pen(Edge, 1), new Rect(18, 38, 14, 28), 6, 6);
        g.DrawRectangle(Lit, null, new Rect(28, 44, 6, 14), 3, 3);
        Label(g, "grip", new Point(38, 54));
    }

    void DrawStick(DrawingContext g)
    {
        // the stick with up / down arrows
        var c = new Point(24, 36);
        g.DrawEllipse(Body, new Pen(Edge, 1), c, 14, 14);
        g.DrawEllipse(Lit, null, c, 6, 6);
        var pen = new Pen(Brushes.White, 1.8, lineCap: PenLineCap.Round);
        foreach (int dir in new[] { -1, 1 })
        {
            double y = c.Y + dir * 22;
            g.DrawLine(pen, new Point(c.X, c.Y + dir * 15), new Point(c.X, y));
            g.DrawLine(pen, new Point(c.X - 4, y - dir * 4), new Point(c.X, y));
            g.DrawLine(pen, new Point(c.X + 4, y - dir * 4), new Point(c.X, y));
        }
        // the thing smaller and bigger
        if (what == "wheel")
        {
            DrawThing(g, new Rect(48, 26, 20, 20));
            DrawThing(g, new Rect(72, 18, 34, 34));
        }
        else
        {
            DrawThing(g, new Rect(48, 30, 24, 14));
            DrawThing(g, new Rect(76, 22, 30, 28));
        }
    }

    static void Label(DrawingContext g, string text, Point at)
    {
        using var t = new TextLayout(text, new Typeface("Segoe UI", FontStyle.Normal, FontWeight.Bold), 10, Lit);
        t.Draw(g, at);
    }
}
