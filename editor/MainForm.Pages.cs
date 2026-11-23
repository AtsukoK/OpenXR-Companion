using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Info pages, tips and quick switches shown above the editors.
public partial class MainForm
{
    // Run after the current event; before the window exists, wait until it is shown.
    void Later(Action a)
    {
        if (IsHandleCreated)
            BeginInvoke(a);
        else
        { EventHandler h = null; h = (s, e) => { Shown -= h; a(); }; Shown += h; }
    }

    Control InfoWithPic(TreeNode n, string title, string text)
    {
        Image pic = n.Tag switch
        {
            "bindings_left" => MiniPics.Wheel(200, 140, 0, Profile.Bindings.Where(b => HandOf(b) == 0 && b.Action?.Type == ActionType.Wheel).Select(b => Profile.Wheels.GetValueOrDefault(b.Action.Wheel ?? "")).FirstOrDefault(w => w != null), true),
            "bindings_right" => MiniPics.Wheel(200, 140, 1, Profile.Bindings.Where(b => HandOf(b) == 1 && b.Action?.Type == ActionType.Wheel).Select(b => Profile.Wheels.GetValueOrDefault(b.Action.Wheel ?? "")).FirstOrDefault(w => w != null), true),
            "wheels" => MiniPics.Wheel(200, 140, 1, Profile.Wheels.Values.FirstOrDefault(), true),
            _ => null,
        };
        return Info(title, text, pic);
    }

    static Control Info(string title, string text, Image pic = null)
    {
        var p = new Panel { Padding = new Padding(14) };
        if (pic != null)
            p.Controls.Add(new PictureBox { Image = pic, Dock = DockStyle.Top, Height = pic.Height + 12, SizeMode = PictureBoxSizeMode.Normal, Padding = new Padding(0, 8, 0, 0) });
        p.Controls.Add(new Label { Text = text, AutoSize = true, MaximumSize = new Size(620, 0), Dock = DockStyle.Top, Font = new Font("Segoe UI", 10f) });
        p.Controls.Add(new Label { Text = title, AutoSize = true, Dock = DockStyle.Top, Font = new Font("Segoe UI", 14f, FontStyle.Bold), Padding = new Padding(0, 0, 0, 8) });
        return p;
    }


    // Tips shown in a soft bar under the top strip.
    string TipFor(TreeNode n)
    {
        string t = TipText(n);
        if (t != null && n != null && IsSection(n) && !n.IsExpanded && RealCount(n) > 0)
            t = "This section is folded: click the arrow left of its name in the list to show what's in it. " + t;
        return t;
    }

    string TipText(TreeNode n)
    {
        bool wrist = n != null && WristOf(n) >= 0;
        return n?.Tag switch
        {
            ControllersPage => "Tip: click a button on the picture (or its tag) to see and change what it does; the + on a tag adds something to it.",
            BindingsPage => "Tip: everything in this profile. The circle switches one on or off; click a line for all its settings.",
            "overview" => "Tip: the switches on each card turn things on or off right here. The blue buttons jump to where you edit them.",
            ProfileModel or SettingsPage => "Tip: cards tagged 'Saved with profile' take effect when you save (Ctrl+S); 'Applies straight away' ones work at once. Hover or click \u24D8 for help on a setting.",
            "bindings_left" or "bindings_right" => "Tip: click a button on the picture to change what it does; the + on a tag adds something to it.",
            "wheels" => "Tip: click a wheel to open it. On its page, '+ Add sub-wheel' makes a submenu that is linked to it automatically.",
            "wrist0" or "wrist1" => "Tip: turn your wrist toward your face and look at it to show the panel. Poke a button with your other hand's finger, or point at it and pull the trigger.",
            BindingModel => "Tip: pick the controller button and gesture first, then what it does. 'Record key' then press a key on your keyboard to bind it.",
            WheelModel when wrist => "Tip: click a button in the panel picture or the list on the right to change it. '+ Add page' makes a page with a 'Next page' button to it.",
            WheelModel => "Tip: click an item in the wheel or the list on the right to change it; drag \u2261 to reorder. The tabs at the top go to its sub-wheels.",
            WheelItemModel => "Tip: click any other " + (wrist ? "button" : "item") + " in the picture or the list to change it. Del deletes, Ctrl+D duplicates, Alt+arrows move it.",
            _ => null,
        };
    }

    // The tip, and a "Show me this page" button that starts this page's short tour.
    Control TipBar(string text, string tourId)
    {
        var p = new Panel { Dock = DockStyle.Top, Height = text.Length > 300 ? 62 : 44, BackColor = Color.FromArgb(255, 248, 225), Padding = new Padding(14, 0, 8, 0) };
        p.Controls.Add(new Label { Text = text.Length > 0 ? "💡  " + text : "", Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft, ForeColor = Color.FromArgb(110, 80, 10), Font = new Font("Segoe UI", 9.5f) });
        if (tourId != null)
        {
            var b = new Button { Text = "🎓  Show me this page", AutoSize = true, Dock = DockStyle.Right, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Tag = "swatch",
                                 BackColor = Color.FromArgb(240, 140, 30), ForeColor = Color.White, Font = new Font("Segoe UI", 10f, FontStyle.Bold), Padding = new Padding(10, 0, 10, 0) };
            b.FlatAppearance.BorderSize = 0;
            FormPanel.Tips.SetToolTip(b, "A short guided tour of this page. More tours (and how-tos like sprinting with a double flick): the Tutorial button at the top.");
            b.Click += (s, e) => StartTutorial(tourId);
            var wrap = new Panel { Dock = DockStyle.Right, Width = 230, Padding = new Padding(0, 6, 0, 6), BackColor = p.BackColor };
            b.Dock = DockStyle.Fill;
            wrap.Controls.Add(b);
            p.Controls.Add(wrap);
        }
        return p;
    }

    // Switch a whole section on/off from its page: every binding on that controller, or every binding that opens a wheel.
    Control SectionSwitch(string tag)
    {
        List<BindingModel> list = tag switch
        {
            "bindings_left" => Profile.Bindings.Where(b => HandOf(b) == 0).ToList(),
            "bindings_right" => Profile.Bindings.Where(b => HandOf(b) == 1).ToList(),
            _ => Profile.Bindings.Where(b => b.Action?.Type == ActionType.Wheel).ToList(),
        };
        string what = tag == "wheels" ? "Bindings that open a wheel" : (tag == "bindings_left" ? "Left" : "Right") + " controller bindings";
        var strip = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 48, Padding = new Padding(12, 8, 0, 0), BackColor = Color.FromArgb(236, 240, 247), WrapContents = false };
        int on = list.Count(b => b.Enabled);
        strip.Controls.Add(new Label { Text = $"{what}: {on} of {list.Count} on", AutoSize = true, Font = new Font("Segoe UI", 10f, FontStyle.Bold), Margin = new Padding(0, 7, 12, 0), ForeColor = Color.FromArgb(40, 60, 100) });
        void Opt(string text, bool v)
        {
            var b = new Button { Text = text, AutoSize = true, Height = 32, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Margin = new Padding(0, 0, 6, 0), Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), BackColor = Color.White, ForeColor = Color.FromArgb(40, 50, 70), Enabled = list.Count > 0 };
            b.FlatAppearance.BorderColor = Color.FromArgb(170, 185, 210);
            b.Click += (s, e) => { foreach (var x in list) x.Enabled = v; MarkDirty(); BuildTree(); };
            strip.Controls.Add(b);
        }
        Opt("Switch all on", true);
        Opt("Switch all off", false);
        return strip;
    }

    // Quick switch above a wrist page / button: Off, own pages, or mirror the other panel.
    Control PanelModeStrip(int h, Action changed)
    {
        var wm = Profile.WristFor(h);
        var other = Profile.WristFor(1 - h);
        string side = h == 0 ? "Left" : "Right", otherSide = h == 0 ? "right" : "left";
        var strip = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 48, Padding = new Padding(12, 8, 0, 0), BackColor = Color.FromArgb(232, 244, 241), WrapContents = false };
        strip.Controls.Add(new Label { Text = $"{side} wrist panel:", AutoSize = true, Font = new Font("Segoe UI", 10f, FontStyle.Bold), Margin = new Padding(0, 7, 10, 0), ForeColor = Color.FromArgb(30, 90, 80) });
        void Opt(string text, bool active, Action apply)
        {
            var b = new Button
            {
                Text = text,
                AutoSize = true,
                Height = 32,
                FlatStyle = FlatStyle.Flat,
                Cursor = Cursors.Hand,
                Margin = new Padding(0, 0, 6, 0),
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                BackColor = active ? Color.FromArgb(30, 130, 110) : Color.White,
                ForeColor = active ? Color.White : Color.FromArgb(40, 50, 70)
            };
            b.FlatAppearance.BorderColor = Color.FromArgb(150, 190, 180);
            b.Click += (s, e) => { if (active) return; apply(); changed(); };
            strip.Controls.Add(b);
        }
        Opt("Off", !wm.Enabled, () => wm.Enabled = false);
        Opt("On, own pages", wm.Enabled && !wm.Mirror, () => { wm.Enabled = true; wm.Mirror = false; });
        Opt($"On, mirror the {otherSide} panel", wm.Enabled && wm.Mirror, () => { wm.Enabled = true; wm.Mirror = true; other.Mirror = false; });
        return strip;
    }
}
