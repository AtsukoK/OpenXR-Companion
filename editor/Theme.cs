using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

// Colours used across the editor (one place, so a dark theme can swap them).
public static class Theme
{
    public static bool IsDark { get; private set; }
    public static Color Page, Card, CardBorder, Text, SubText, Link, Heading, Live, Saved, Highlight;
    public static Color Input, Button, Bar, Surface, Selected, Chevron, OffText;
    public static Color InputBack => IsDark ? Input : SystemColors.Window;
    public static Color ErrorBack => IsDark ? Color.FromArgb(90, 40, 40) : Color.MistyRose;
    public static Color RecordBack => IsDark ? Color.FromArgb(85, 75, 30) : Color.LightYellow;
    static Theme() => Set(false);
    public static void Set(bool dark)
    {
        IsDark = dark;
        Live = Color.FromArgb(46, 158, 91);
        Saved = dark ? Color.FromArgb(88, 96, 110) : Color.FromArgb(120, 128, 140);
        if (!dark)
        {
            Page = Color.FromArgb(243, 245, 248); Card = Color.White; CardBorder = Color.FromArgb(222, 226, 232);
            Text = Color.FromArgb(30, 32, 36); SubText = Color.FromArgb(105, 110, 120); Link = Color.FromArgb(60, 110, 190);
            Heading = Color.FromArgb(40, 90, 170); Highlight = Color.FromArgb(255, 236, 160);
            Input = SystemColors.Window; Button = SystemColors.Control; Bar = SystemColors.Control;
            Surface = Color.FromArgb(248, 249, 251); Selected = Color.FromArgb(222, 234, 252); Chevron = Color.FromArgb(120, 125, 135); OffText = Color.FromArgb(150, 150, 150);
        }
        else
        {
            Page = Color.FromArgb(24, 26, 30); Card = Color.FromArgb(36, 39, 45); CardBorder = Color.FromArgb(58, 63, 72);
            Text = Color.FromArgb(226, 229, 235); SubText = Color.FromArgb(150, 156, 168); Link = Color.FromArgb(120, 170, 245);
            Heading = Color.FromArgb(125, 175, 250); Highlight = Color.FromArgb(105, 88, 30);
            Input = Color.FromArgb(46, 50, 58); Button = Color.FromArgb(52, 57, 66); Bar = Color.FromArgb(32, 34, 39);
            Surface = Color.FromArgb(28, 30, 35); Selected = Color.FromArgb(44, 62, 92); Chevron = Color.FromArgb(150, 156, 168); OffText = Color.FromArgb(110, 114, 122);
        }
    }
}
