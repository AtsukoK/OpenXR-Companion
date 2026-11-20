using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Small helpers: wrist/wheel lookups, renames, buttons, status bar, unsaved changes.
public partial class MainForm
{
    // -1 = not under a wrist panel, 0 = left, 1 = right
    int WristOf(TreeNode n) { while (n?.Parent != null) n = n.Parent; return n == null ? -1 : n == wristRoots[0] ? 0 : n == wristRoots[1] ? 1 : -1; }
    bool InWrist(TreeNode n) => WristOf(n) >= 0;
    Dictionary<string, WheelModel> DictFor(TreeNode n) => WristOf(n) is int h && h >= 0 ? Profile.WristPages(h).Pages : Profile.Wheels;

    void RenamePage(int h, string oldName, string newName)
    {
        var owner = Profile.WristPages(h);
        var pages = owner.Pages;
        var w = pages[oldName];
        pages.Remove(oldName);
        pages[newName] = w;
        if (owner.Start == oldName) owner.Start = newName;
        foreach (var pg in pages.Values) foreach (var i in pg.Items) if (i.Action?.Type == "page" && i.Action.Page == oldName) i.Action.Page = newName;
        RefreshNodeTexts();
    }

    void RenameWheel(string oldName, string newName)
    {
        var w = Profile.Wheels[oldName];
        Profile.Wheels.Remove(oldName);
        Profile.Wheels[newName] = w;
        foreach (var b in Profile.Bindings) if (b.Action?.Type == "wheel" && b.Action.Wheel == oldName) b.Action.Wheel = newName;
        foreach (var ww in Profile.Wheels.Values.Concat(Profile.WristLeft.Pages.Values).Concat(Profile.WristRight.Pages.Values)) foreach (var i in ww.Items) if (i.Action?.Type == "wheel" && i.Action.Wheel == oldName) i.Action.Wheel = newName;
        RefreshNodeTexts();
    }

    static ToolStripButton Btn(string t, Action a) { var b = new ToolStripButton(t); b.Click += (s, e) => a(); return b; }
    static Button PBtn(string t, Action a) { var b = new Button { Text = t, AutoSize = true }; b.Click += (s, e) => a(); return b; }

    void SetStatus(string s, bool error = false) { statusText.Text = s; statusText.ForeColor = error ? (Theme.IsDark ? Color.FromArgb(255, 125, 115) : Color.Firebrick) : (Theme.IsDark ? Theme.Text : SystemColors.ControlText); }
    void MarkDirty() { dirty = true; Text = "OpenXR Companion - Profile Editor - " + Path.GetFileName(currentFile) + " *"; }

    bool ConfirmDiscard()
    {
        if (!dirty) return true;
        var r = MessageBox.Show(this, "Save changes to " + Path.GetFileName(currentFile) + "?", "Unsaved changes", MessageBoxButtons.YesNoCancel);
        if (r == DialogResult.Cancel) return false;
        if (r == DialogResult.Yes) return Save();
        dirty = false;
        return true;
    }
}
