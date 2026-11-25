using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.TextFormatting;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// A forearm and hand in the pose the wrist panel's text direction is meant for, with the panel drawn to scale on the
// arm (its size, its distance back from the controller, the real buttons of its first page). Port of the WinForms
// ArmPosePreview. GDI+'s stacked transforms are written out as one matrix per drawing step (Avalonia row vectors:
// the first transform applied is the leftmost factor).
public class ArmPoseView : Control
{
    public bool RightArm { get; set; }
    public ProfileModel Profile { get; set; }
    const double K = 500;       // drawing units per metre: the arm is about 25 cm, the palm about 9 cm
    const double GripX = 131;   // controller grip origin (centre of the palm) along the arm

    public ArmPoseView()
    {
        Width = 380;
        Height = 300;
    }

    WristModel Panel => Profile?.WristFor(RightArm ? 1 : 0);

    static Matrix Rot(double deg) => Matrix.CreateRotation(deg * Math.PI / 180);
    static Matrix Move(double x, double y) => Matrix.CreateTranslation(x, y);

    public override void Render(DrawingContext g)
    {
        g.DrawRectangle(new SolidColorBrush(Color.FromRgb(38, 44, 52)), null, new Rect(Bounds.Size), 6, 6);
        var wp = Panel;
        string m = wp?.TextUp ?? "fingers";
        double sizeM = wp?.Size ?? 0.11, elbowM = wp?.TowardElbow ?? 0.13;
        int columns = wp?.Columns ?? 2;
        double armAngle;
        string caption;
        switch (m)
        {
            case "between": armAngle = -45; caption = "Arm half raised: text turned halfway"; break;
            case "along_arm": armAngle = 0; caption = "Arm across the body: text along the forearm"; break;
            case "custom": armAngle = -(wp?.RotateDeg ?? 0); caption = $"Custom: text turned {wp?.RotateDeg ?? 0:0} degrees from along the forearm"; break;
            default: armAngle = -90; caption = "Hand held up, palm to face: top toward the fingers"; break;
        }
        if (RightArm)
            armAngle = -180 - armAngle;
        double textTurn = -armAngle; // every pose reads upright from where you look

        // fit the hand and the panel into the view at any angle, centred on them; the sleeve runs off the edge
        double W = Bounds.Width, H = Bounds.Height - 40;
        double S = sizeM * K, px = GripX - elbowM * K;
        var pts = new List<Point> { new(180, -18), new(180, 18), new(108, -26), new(108, 26), new(px - S / 2 - 15, -16), new(px - S / 2 - 15, 16) };
        var pm = Rot(textTurn) * Move(px, 0);
        foreach (var c in new[] { new Point(-S / 2, -S / 2), new Point(S / 2, -S / 2), new Point(S / 2, S * 0.6), new Point(-S / 2, S * 0.6) })
            pts.Add(pm.Transform(c));
        var fit = pts.Select(p => Rot(armAngle).Transform(p)).ToList();
        double minX = fit.Min(p => p.X), maxX = fit.Max(p => p.X), minY = fit.Min(p => p.Y), maxY = fit.Max(p => p.Y);
        double scale = Math.Min(1.8, Math.Min((W - 20) / (maxX - minX), (H - 12) / (maxY - minY)));
        // GDI+: Translate(W/2,H/2), Scale, Translate(-centre), Rotate(arm): the rotation is applied first
        var world = Rot(armAngle) * Move(-(minX + maxX) / 2, -(minY + maxY) / 2) * Matrix.CreateScale(scale, scale) * Move(W / 2, H / 2);

        var skin = new SolidColorBrush(Color.FromRgb(205, 170, 140));
        var sleeve = new SolidColorBrush(Color.FromRgb(90, 96, 110));
        var outline = new Pen(new SolidColorBrush(Color.FromRgb(30, 30, 34)), 1.5);
        using (g.PushTransform(world))
        {
            Capsule(g, sleeve, outline, -400, -15, 515, 30);
            Capsule(g, skin, outline, 108, -17, 46, 34);
            for (int i = 0; i < 4; i++)
                Capsule(g, skin, outline, 146, -15 + i * 8.5, i == 0 || i == 3 ? 26 : 32, 7.5);
        }
        double thumbSide = RightArm ? 1 : -1;
        using (g.PushTransform(Rot(thumbSide * 35) * Move(122, thumbSide * 15) * world))
            Capsule(g, skin, outline, 0, -4, 24, 8);
        // the panel, to scale, centred 'toward the elbow' back from the grip
        using (g.PushTransform(Rot(textTurn) * Move(GripX - elbowM * K, 0) * world))
            DrawPanel(g, S, columns);

        using var cap = new TextLayout(caption, new Typeface("Segoe UI"), 11.5, new SolidColorBrush(Color.FromRgb(220, 220, 220)), TextAlignment.Center, maxWidth: Bounds.Width - 8);
        cap.Draw(g, new Point(4, Bounds.Height - 38));
        if (wp != null)
        {
            string side = wp.Face == "back" ? "back of the wrist" : "inner side";
            string info = $"{side}, {wp.Size * 100:0.#} cm panel, {wp.TowardElbow * 100:0.#} cm back from the controller, {wp.Lift * 100:0.#} cm off the arm";
            using var small = new TextLayout(info, new Typeface("Segoe UI"), 10, new SolidColorBrush(Color.FromRgb(192, 192, 192)), TextAlignment.Center, maxWidth: Bounds.Width - 8);
            small.Draw(g, new Point(4, Bounds.Height - 20));
        }
    }

    static void Capsule(DrawingContext g, IBrush fill, Pen edge, double x, double y, double w, double h)
    {
        double r = Math.Min(w, h) / 2;
        g.DrawRectangle(fill, edge, new Rect(x, y, w, h), r, r);
    }

    void DrawPanel(DrawingContext g, double S, int columns)
    {
        // the real first page when a profile is attached, sample buttons otherwise
        string title = "TOP";
        var labels = new List<string> { "Keys", "Esc", "Map", "More" };
        var colors = new List<string> { null, null, null, null };
        if (Profile != null)
        {
            var owner = Profile.WristPages(RightArm ? 1 : 0);
            if (owner.Pages.TryGetValue(owner.Start ?? "", out var page))
            {
                title = string.IsNullOrEmpty(page.Title) ? page.Name : page.Title;
                var on = page.Items.Where(i => i.Enabled).ToList();
                labels = on.Select(i => i.Label).ToList();
                colors = on.Select(i => i.Color).ToList();
            }
        }
        var cells = WheelView.GridLayout(labels.Count, columns, out var hd);
        double bottom = cells.Count == 0 ? hd.Bottom : Math.Max(hd.Bottom, cells.Max(c => c.Bottom));
        var pr = new Rect(-S / 2, -(bottom + 0.03) * S / 2, S, S);
        g.DrawRectangle(new SolidColorBrush(Color.FromArgb(235, 16, 20, 24)), new Pen(new SolidColorBrush(Color.FromArgb(200, 255, 255, 255)), 1.2),
            new Rect(pr.X, pr.Y, S, (bottom + 0.03) * S), S * 0.04, S * 0.04);
        Rect Px(Rect r) => new(pr.X + r.X * S, pr.Y + r.Y * S, r.Width * S, r.Height * S);
        Text(g, title, Math.Max(4, S * 0.075), Color.FromRgb(127, 196, 255), Px(hd));
        double cellPx = cells.Count > 0 ? cells[0].Height * S : 10;
        for (int i = 0; i < cells.Count; i++)
        {
            var r = Px(cells[i]);
            var fill = WheelView.TryColor(colors[i], out var col) ? new SolidColorBrush(col) : new SolidColorBrush(Color.FromRgb(44, 52, 70));
            g.DrawRectangle(fill, null, r, r.Height * 0.11, r.Height * 0.11);
            Text(g, labels[i], Math.Max(3.5, cellPx * 0.2), Colors.White, r);
        }
    }

    static void Text(DrawingContext g, string text, double size, Color color, Rect box)
    {
        if (string.IsNullOrEmpty(text) || box.Width <= 1)
            return;
        using var t = new TextLayout(text, new Typeface("Segoe UI", FontStyle.Normal, FontWeight.Bold), size, new SolidColorBrush(color),
            TextAlignment.Center, TextWrapping.NoWrap, TextTrimming.CharacterEllipsis, maxWidth: box.Width);
        t.Draw(g, new Point(box.X, box.Y + (box.Height - t.Height) / 2));
    }
}
