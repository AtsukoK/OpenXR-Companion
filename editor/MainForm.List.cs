using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Filling the list on the left from the profile (rows, groups, search).
public partial class MainForm
{
    readonly TreeNode[] bindingRoots = new TreeNode[2]; // [0] left controller, [1] right controller
    TreeNode wheelsRoot;
    readonly TreeNode[] wristRoots = new TreeNode[2];

    public void RebuildList() => BuildTree();
    void BuildTree()
    {
        subDepth.Clear();
        if (Profile == null)
            return;
        object sel = tree.SelectedNode?.Tag;
        string q = Q;
        int topRow = VisibleRows().IndexOf(tree.TopNode); // the list stays scrolled where it was
        tree.BeginUpdate();
        Redraw(tree, false);
        tree.Nodes.Clear();
        var overview = tree.Nodes.Add("Overview");
        overview.Tag = "overview";
        var settings = tree.Nodes.Add("Settings");
        settings.Tag = Profile;
        foreach (var sp in SettingsPage.All) // its pages; while searching, only those with a matching setting
            if ((sp.Key != "experimental" || HeadSmoothingFile.ShowPage || HeadSmoothingFile.On) && (string.IsNullOrWhiteSpace(q) || Matches(q, sp.Title, sp.Sub, SettingsEditor.WordsOf(Profile, sp.Key))))
                settings.Nodes.Add(new TreeNode(sp.Title) { Tag = sp });
        // Bindings grouped by the controller whose button fires them.
        foreach (int h in new[] { 1, 0 })
        {
            bindingRoots[h] = tree.Nodes.Add(h == 1 ? "Right controller" : "Left controller");
            bindingRoots[h].Tag = h == 1 ? "bindings_right" : "bindings_left";
            foreach (var bd in Profile.Bindings.Where(x => HandOf(x) == h))
                if (Matches(q, bd.Name, bd.Input, InputText.Friendly(bd.Input), Plain(bd.Action), bd.Action?.Keys, bd.Action?.Wheel))
                    bindingRoots[h].Nodes.Add(new TreeNode(bd.Name) { Tag = bd });
            Slot(bindingRoots[h], "Add binding");
        }
        wheelsRoot = tree.Nodes.Add("Wheels");
        wheelsRoot.Tag = "wheels";
        AddPages(wheelsRoot, Profile.Wheels.Values, q);
        Slot(wheelsRoot, "New wheel or sub-wheel");
        for (int h = 0; h < 2; h++)
        {
            wristRoots[h] = tree.Nodes.Add(h == 0 ? "Left wrist panel" : "Right wrist panel");
            wristRoots[h].Tag = h == 0 ? "wrist0" : "wrist1"; // literals, so the selection survives a rebuild
            AddPages(wristRoots[h], Profile.WristFor(h).Pages.Values, q);
            Slot(wristRoots[h], "New sub-page");
        }
        ApplyFolds();
        RefreshNodeTexts();
        tree.EndUpdate();
        TreeNode found = Find(tree.Nodes, sel);
        try
        {
            tree.SelectedNode = found ?? overview;
            var rows = VisibleRows();
            if (topRow >= 0 && rows.Count > 0)
            {
                tree.TopNode = rows[Math.Min(topRow, rows.Count - 1)];
                // keep the selected row on screen if it moved out of view
                if (tree.SelectedNode != null && (tree.SelectedNode.Bounds.Bottom > tree.ClientSize.Height || tree.SelectedNode.Bounds.Top < 0))
                    tree.SelectedNode.EnsureVisible();
            }
        }
        finally { Redraw(tree, true, false); } // no wipe: the double-buffered list paints over itself
    }

    // Rows of the list as shown (children of folded rows left out), top to bottom.
    List<TreeNode> VisibleRows()
    {
        var l = new List<TreeNode>();
        void Walk(TreeNodeCollection c)
        { foreach (TreeNode n in c) { l.Add(n); if (n.IsExpanded) Walk(n.Nodes); } }
        Walk(tree.Nodes);
        return l;
    }

    void AddPages(TreeNode root, IEnumerable<WheelModel> pages, string q)
    {
        // order: each main wheel/page followed by its sub-wheels (depth-first), then anything left over
        var all = pages.ToList();
        bool wrist = root != wheelsRoot;
        WheelModel Find(string name) => all.FirstOrDefault(x => x.Name == name);
        IEnumerable<WheelModel> Children(WheelModel w) => w.Items
            .Select(i => wrist ? (i.Action?.Type == ActionType.Page ? i.Action.Page : null) : (i.Action?.Type == ActionType.Wheel ? i.Action.Wheel : null))
            .Where(nm => nm != null).Distinct().Select(Find).Where(x => x != null && x != w);
        var parentOf = new Dictionary<WheelModel, WheelModel>();
        foreach (var w in all)
            foreach (var c in Children(w))
                parentOf.TryAdd(c, w);
        int startH = root == wristRoots[0] ? 0 : 1;
        bool IsMain(WheelModel w) => wrist ? w.Name == Profile.WristFor(startH).Start || !parentOf.ContainsKey(w)
                                           : Profile.Bindings.Any(b => b.Action?.Type == ActionType.Wheel && b.Action.Wheel == w.Name) || !parentOf.ContainsKey(w);
        var ordered = new List<WheelModel>();
        var seen = new HashSet<WheelModel>();
        void Visit(WheelModel w, int d)
        {
            if (!seen.Add(w))
                return;
            ordered.Add(w);
            subDepth[w] = Math.Min(d, 3);
            foreach (var c in Children(w))
                if (!IsMain(c))
                    Visit(c, d + 1);
        }
        foreach (var w in all)
            if (IsMain(w))
                Visit(w, 0);
        foreach (var w in all)
            Visit(w, 0); // cycles with no main wheel
        foreach (var w in ordered)
        {
            bool self = Matches(q, w.Name, w.Title);
            var items = w.Items.Where(i => self || Matches(q, i.Label, Plain(i.Action), i.Action?.Keys)).ToList();
            if (!self && items.Count == 0)
                continue;
            var wn = root.Nodes.Add(w.Name);
            wn.Tag = w;
            foreach (var i in items)
                wn.Nodes.Add(new TreeNode(i.Label) { Tag = i });
            Slot(wn, root == wheelsRoot ? "Add item" : "Add button", root == wheelsRoot ? "Add sub-wheel" : "Add sub-page");
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    struct TVITEM { public int mask; public IntPtr hItem; public int state; public int stateMask; public IntPtr pszText; public int cchTextMax; public int iImage; public int iSelectedImage; public int cChildren; public IntPtr lParam; }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr h, int msg, IntPtr w, ref TVITEM l);

    void HideChecks(TreeNodeCollection nodes) { } // the list is drawn by hand now (no checkboxes)
    void HideChecksOld(TreeNodeCollection nodes)
    {
        foreach (TreeNode n in nodes)
        {
            if (n.Tag is not (BindingModel or WheelItemModel))
            {
                var it = new TVITEM { mask = 0x8 /*TVIF_STATE*/, hItem = n.Handle, stateMask = 0xF000 /*TVIS_STATEIMAGEMASK*/, state = 0 };
                SendMessage(tree.Handle, 0x113F /*TVM_SETITEMW*/, IntPtr.Zero, ref it);
            }
            HideChecks(n.Nodes);
        }
    }

    static TreeNode Find(TreeNodeCollection nodes, object tag)
    {
        if (tag == null)
            return null;
        foreach (TreeNode n in nodes)
        { if (ReferenceEquals(n.Tag, tag)) return n; var f = Find(n.Nodes, tag); if (f != null) return f; }
        return null;
    }

    void RefreshNodeTexts()
    {
        if (wheelsRoot == null)
            return;
        tree.Invalidate(); // rows are drawn from the live model
    }

    WheelModel SelectedWheel(out TreeNode wheelNode)
    {
        wheelNode = tree.SelectedNode;
        if (wheelNode?.Tag is WheelItemModel)
            wheelNode = wheelNode.Parent;
        return wheelNode?.Tag as WheelModel;
    }

    void Add()
    {
        var n = tree.SelectedNode;
        if (n == null)
            return;
        if (n == bindingRoots[0] || n == bindingRoots[1] || n.Tag is BindingModel)
        {
            int hand = n == bindingRoots[0] ? 0 : n == bindingRoots[1] ? 1 : HandOf((BindingModel)n.Tag);
            var b = new BindingModel { Name = "New binding", Input = hand == 0 ? "left.x" : "right.a", Gesture = GestureKind.Press, Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
            int at = n.Tag is BindingModel cur ? Profile.Bindings.IndexOf(cur) + 1 : Profile.Bindings.Count;
            Profile.Bindings.Insert(at, b);
            Rebuild(b);
        }
        else if (n == wristRoots[0] || n == wristRoots[1])
        {
            int h = n == wristRoots[0] ? 0 : 1;
            var owner = Profile.WristFor(h);
            var pages = owner.Pages;
            WheelModel w;
            if (pages.Count == 0)
            { w = DefaultPage(h); owner.Start = w.Name; }
            else
            {
                string name = "page" + (pages.Count + 1);
                while (pages.ContainsKey(name))
                    name += "_";
                w = new WheelModel { Name = name, Title = name };
            }
            string name0 = w.Name;
            pages[name0] = w;
            Rebuild(w);
        }
        else if (n == wheelsRoot)
        {
            string name = "wheel" + (Profile.Wheels.Count + 1);
            while (Profile.Wheels.ContainsKey(name))
                name += "_";
            var w = new WheelModel { Name = name, Title = name };
            Profile.Wheels[name] = w;
            Rebuild(w);
        }
        else if (SelectedWheel(out _) is WheelModel w)
        {
            bool wristPage = InWrist(n);
            if (w.Items.Count >= 16)
            { MessageBox.Show(this, wristPage ? "A page holds at most 16 buttons. Add another page." : "A wheel holds at most 16 items. Use a submenu wheel."); return; }
            var it = new WheelItemModel { Label = wristPage ? "New button" : "New item", Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
            int at = n.Tag is WheelItemModel ci ? w.Items.IndexOf(ci) + 1 : w.Items.Count;
            w.Items.Insert(at, it);
            Rebuild(it);
        }
    }

    // A new sub-wheel (or wrist sub-page) plus the item on this wheel/page that opens it; the new one starts with a Back item.
    void AddSub(TreeNode wheelNode)
    {
        if (wheelNode.Tag is not WheelModel w)
            return;
        bool wrist = InWrist(wheelNode);
        if (w.Items.Count >= 16)
        { MessageBox.Show(this, wrist ? "This page is full (16 buttons)." : "This wheel is full (16 items)."); return; }
        var dict = DictFor(wheelNode);
        string name = w.Name + (wrist ? " sub-page" : " sub-wheel");
        for (int k = 2; dict.ContainsKey(name); k++)
            name = w.Name + (wrist ? " sub-page " : " sub-wheel ") + k;
        var sub = new WheelModel { Name = name, Title = "More" };
        sub.Items.Add(new WheelItemModel { Label = "Back", Action = new ActionModel { Type = ActionType.Back } });
        dict[name] = sub;
        unfolded.Add(sub);
        unfolded.Add(w);
        w.Items.Add(new WheelItemModel { Label = "More", Action = wrist ? new ActionModel { Type = ActionType.Page, Page = name } : new ActionModel { Type = ActionType.Wheel, Wheel = name } });
        Rebuild(sub);
    }

    static T Clone<T>(T o) => JsonSerializer.Deserialize<T>(JsonSerializer.Serialize(o, JsonOpts), JsonOpts);

    void Duplicate()
    {
        var n = tree.SelectedNode;
        switch (n?.Tag)
        {
            case BindingModel b:
                { var c = Clone(b); c.Name += " copy"; Profile.Bindings.Insert(Profile.Bindings.IndexOf(b) + 1, c); Rebuild(c); break; }
            case WheelItemModel i:
                { var w = (WheelModel)n.Parent.Tag; var c = Clone(i); w.Items.Insert(w.Items.IndexOf(i) + 1, c); Rebuild(c); break; }
            case WheelModel w:
                {
                    var dict = DictFor(n);
                    var c = Clone(w);
                    c.Name = w.Name + "_copy";
                    while (dict.ContainsKey(c.Name))
                        c.Name += "_";
                    dict[c.Name] = c;
                    Rebuild(c);
                    break;
                }
        }
    }

    void Remove()
    {
        var n = tree.SelectedNode;
        switch (n?.Tag)
        {
            case BindingModel b:
                Profile.Bindings.Remove(b);
                Rebuild(null);
                break;
            case WheelItemModel i:
                ((WheelModel)n.Parent.Tag).Items.Remove(i);
                Rebuild(n.Parent.Tag);
                break;
            case WheelModel w:
                {
                    var dict = DictFor(n);
                    if (MessageBox.Show(this, InWrist(n) ? $"Delete wrist page {w.Name} and its {w.Items.Count} buttons?" : $"Delete wheel {w.Name} and its {w.Items.Count} items?", "Delete", MessageBoxButtons.YesNo) != DialogResult.Yes)
                        return;
                    foreach (var k in dict.Where(kv => ReferenceEquals(kv.Value, w)).Select(kv => kv.Key).ToList())
                        dict.Remove(k);
                }
                Rebuild(null);
                break;
        }
    }

    void MoveItem(int d)
    {
        var n = tree.SelectedNode;
        void Swap<T>(List<T> l, T x)
        { int i = l.IndexOf(x), j = i + d; if (j < 0 || j >= l.Count) return; (l[i], l[j]) = (l[j], l[i]); Rebuild(x); }
        if (n?.Tag is BindingModel b)
        {
            var same = Profile.Bindings.Where(x => HandOf(x) == HandOf(b)).ToList();
            int i = same.IndexOf(b), j = i + d;
            if (j < 0 || j >= same.Count)
                return;
            int a1 = Profile.Bindings.IndexOf(b), a2 = Profile.Bindings.IndexOf(same[j]);
            (Profile.Bindings[a1], Profile.Bindings[a2]) = (Profile.Bindings[a2], Profile.Bindings[a1]);
            Rebuild(b);
        }
        else if (n?.Tag is WheelItemModel i)
            Swap(((WheelModel)n.Parent.Tag).Items, i);
    }

    void Rebuild(object select)
    {
        MarkDirty();
        BuildTree();
        var f = Find(tree.Nodes, select);
        if (f != null)
            tree.SelectedNode = f;
    }
}
