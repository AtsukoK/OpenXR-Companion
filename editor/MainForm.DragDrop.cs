using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

// Dragging items in the list to reorder or move them.
public partial class MainForm
{
    const int TVM_SETINSERTMARK = 0x111A;
    [DllImport("user32.dll", EntryPoint = "SendMessageW")] static extern IntPtr SendMsg(IntPtr h, int msg, IntPtr w, IntPtr l);

    void SetupDragDrop()
    {
        tree.AllowDrop = true;
        tree.ItemDrag += (s, e) =>
        {
            if (e.Item is TreeNode n && n.Tag is BindingModel or WheelItemModel)
            {
                tree.SelectedNode = n;
                tree.DoDragDrop(n, DragDropEffects.Move);
            }
        };
        tree.DragOver += (s, e) =>
        {
            var pt = tree.PointToClient(new Point(e.X, e.Y));
            var (target, after) = DropTarget(e.Data.GetData(typeof(TreeNode)) as TreeNode, pt);
            e.Effect = target != null ? DragDropEffects.Move : DragDropEffects.None;
            ShowDropMark(target, after);
            // Scroll when dragging near the top or bottom edge.
            var under = tree.GetNodeAt(pt);
            if (under != null && pt.Y < 16)
                under.PrevVisibleNode?.EnsureVisible();
            else if (under != null && pt.Y > tree.ClientSize.Height - 16)
                under.NextVisibleNode?.EnsureVisible();
        };
        tree.DragLeave += (s, e) => ShowDropMark(null, false);
        tree.DragDrop += (s, e) =>
        {
            ShowDropMark(null, false);
            var src = e.Data.GetData(typeof(TreeNode)) as TreeNode;
            var (target, after) = DropTarget(src, tree.PointToClient(new Point(e.X, e.Y)));
            if (target != null)
                DoDrop(src, target, after);
        };
    }

    // Where a dragged node would land: (node to insert next to, after it?). A wheel node means "append to that wheel".
    (TreeNode, bool) DropTarget(TreeNode src, Point pt)
    {
        var n = tree.GetNodeAt(pt);
        if (src == null || n == null || n == src)
            return (null, false);
        if (n.Tag is AddSlot)
        {
            if (src.Tag is WheelItemModel && n.Parent?.Tag is WheelModel)
                return (n.Parent, true); // onto "+ Add item" = append to that wheel
            return (null, false);
        }
        bool lowerHalf = pt.Y > n.Bounds.Top + n.Bounds.Height / 2;
        if (src.Tag is BindingModel)
        {
            if (n.Tag is BindingModel)
                return (n, lowerHalf);
            if ((n == bindingRoots[0] || n == bindingRoots[1]) && n.Nodes.Count > 0)
                return (n.Nodes[0], false);
        }
        else if (src.Tag is WheelItemModel)
        {
            if (n.Tag is WheelItemModel)
                return (n, lowerHalf);
            if (n.Tag is WheelModel w)
                return ReferenceEquals(w, src.Parent.Tag) && n.Nodes.Count > 0 ? (n.Nodes[0], false) : (n, true);
        }
        return (null, false);
    }

    // Insertion line between nodes; dropping onto another wheel highlights that wheel.
    void ShowDropMark(TreeNode n, bool after)
    {
        var wheel = n?.Tag is WheelModel ? n : null;
        var line = wheel == null ? n : null;
        if (ReferenceEquals(wheel, dropWheel) && ReferenceEquals(line, dropNode) && after == dropAfter)
            return;
        dropWheel = wheel;
        dropNode = line;
        dropAfter = after;
        tree.Invalidate();
    }

    void DoDrop(TreeNode src, TreeNode target, bool after)
    {
        if (src.Tag is BindingModel b && target.Tag is BindingModel tb)
        {
            Profile.Bindings.Remove(b);
            Profile.Bindings.Insert(Profile.Bindings.IndexOf(tb) + (after ? 1 : 0), b);
            Rebuild(b);
        }
        else if (src.Tag is WheelItemModel it)
        {
            var fromWheel = (WheelModel)src.Parent.Tag;
            var toWheel = target.Tag as WheelModel ?? (WheelModel)target.Parent.Tag;
            if (!ReferenceEquals(fromWheel, toWheel) && toWheel.Items.Count >= 16)
            { MessageBox.Show(this, $"Wheel {toWheel.Name} already has 16 items."); return; }
            fromWheel.Items.Remove(it);
            int i = target.Tag is WheelItemModel ti ? toWheel.Items.IndexOf(ti) + (after ? 1 : 0) : toWheel.Items.Count;
            toWheel.Items.Insert(Math.Clamp(i, 0, toWheel.Items.Count), it);
            Rebuild(it);
        }
    }
}
