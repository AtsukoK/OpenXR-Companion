using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// The right-hand side: the editor page for the item selected in the list.
public partial class MainForm
{
    internal static StringBuilder PerfLog; // --perf: where the time of a page switch goes
    void ShowEditor(TreeNode n)
    {
        var psw = System.Diagnostics.Stopwatch.StartNew();
        void Mark(string what) { PerfLog?.Append($" {what} {psw.Elapsed.TotalMilliseconds:F0}"); }
        var old = currentEditor;
        Control ed;
        void Changed() { MarkDirty(); RefreshNodeTexts(); }
        switch (n?.Tag)
        {
            case "overview":
                {
                    var ov = new OverviewEditor(Profile, GoTo);
                    ov.Changed += Changed; ed = ov; break;
                }
            case ProfileModel p: { var se = new SettingsEditor(p, -1, "general", Q); se.Changed += Changed; ed = se; break; }
            case SettingsPage sp: { var se = new SettingsEditor(Profile, -1, sp.Key, Q); se.Changed += Changed; ed = se; break; }
            case BindingModel b: { var be = new BindingEditor(b); be.Changed += Changed; ed = be; break; }
            case "wrist0": case "wrist1": { var se = new SettingsEditor(Profile, (string)n.Tag == "wrist1" ? 1 : 0); (bool, bool) lastMirror = (Profile.WristLeft.Mirror, Profile.WristRight.Mirror); se.Changed += () => { Changed(); var m = (Profile.WristLeft.Mirror, Profile.WristRight.Mirror); if (m != lastMirror) { lastMirror = m; BuildTree(); } }; ed = se; break; }
            case WheelModel w:
                {
                    int wh = WristOf(n);
                    var we = wh >= 0 ? new WheelEditor(w, (a, b) => RenamePage(wh, a, b), Profile.WristPages(wh), Profile.WristFor(wh).Columns)
                                     : new WheelEditor(w, RenameWheel);
                    we.Changed += Changed;
                    we.Preview.ItemClicked += it => tree.SelectedNode = Find(tree.Nodes, it);
                    ed = we; break;
                }
            case WheelItemModel i:
                {
                    var w2 = (WheelModel)n.Parent.Tag;
                    int ih = WristOf(n);
                    var ie = ih >= 0 ? new ItemEditor(w2, i, Profile.WristPages(ih), Profile.WristFor(ih).Columns) : new ItemEditor(w2, i);
                    ie.Changed += Changed;
                    ie.Preview.ItemClicked += it => tree.SelectedNode = Find(tree.Nodes, it);
                    ed = ie; break;
                }
            case "bindings_right":
            case "bindings_left":
                ed = InfoWithPic(n, (string)n.Tag == "bindings_left" ? "Left controller" : "Right controller", "A binding makes a controller button (or a combo of buttons) do something: press a key, open a wheel, open the keyboard, switch a layer...\n\nClick '+ Add' on this controller's bar in the list to make one. Click the round dot on the right of a row to switch it on or off without deleting it.\n\nLayers: a binding that switches a layer on lets the same buttons do different things, as long as that layer is on.");
                break;
            case "wheels":
                ed = InfoWithPic(n, "Wheels", "A wheel is a radial menu. A controller binding opens a main wheel; an item on a wheel can open a sub-wheel.\n\nTo make a wheel, click '+ Add' on the WHEELS bar in the list. To add items or a sub-wheel, unfold WHEELS (the arrow on its left) and use the '+ Add item' / '+ Add sub-wheel' row under a wheel.");
                break;
            default: ed = new Panel(); break;
        }
        Mark("built");
        addBtn.Text = n?.Tag switch
        {
            "bindings_right" or "bindings_left" or BindingModel => "Add binding",
            "wheels" => "Add wheel",
            "wrist0" or "wrist1" => "Add page",
            WheelModel or WheelItemModel when InWrist(n) => "Add button",
            WheelModel or WheelItemModel => "Add item",
            _ => "Add",
        };
        addBtn.Enabled = n?.Tag is not (ProfileModel or SettingsPage) && n?.Tag as string != "overview";
        // Everything for the page is made and recoloured before it goes on screen: recolouring controls
        // that already have windows repaints each one (slow), and is visible as a flash.
        ed.Dock = DockStyle.Fill;
        var parts = new List<Control> { ed };
        string tip = TipFor(n);
        if (tip != null) parts.Add(TipBar(tip));
        int stripHand = n?.Tag is WheelModel or WheelItemModel || n?.Tag as string is "wrist0" or "wrist1" ? WristOf(n) : -1;
        if (stripHand >= 0) parts.Add(PanelModeStrip(stripHand, () => { MarkDirty(); BuildTree(); }));
        else if (n?.Tag as string is "bindings_left" or "bindings_right" or "wheels") parts.Add(SectionSwitch((string)n.Tag));
        if (Theme.IsDark) foreach (var c in parts) ThemeWalker.Apply(c);
        Mark("theme");
        Redraw(editorHost, false); // nothing shows until the whole page is ready
        try
        {
            editorHost.SuspendLayout();
            editorHost.Controls.Clear();
            foreach (var c in parts) editorHost.Controls.Add(c);
            Mark("added");
            editorHost.ResumeLayout();
            Mark("layout");
        }
        finally { Redraw(editorHost, true); }
        Mark("painted");
        currentEditor = ed;
        if (old != null && !ReferenceEquals(old, ed)) { if (old.Contains(grid)) old.Controls.Remove(grid); old.Dispose(); }
        Mark("disposed");
    }
}
