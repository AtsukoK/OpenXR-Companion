using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Dragging list rows by their ≡ handle (the WinForms lists' drag), for the wheel item list and the wrist button list.
// Each row is a Border whose Tag is its item. Dropping on a row puts the dragged item where that row's item is in the
// real order (so it works whatever the list's sort view), above it when dropped on its top half, below on the bottom.
// An orange line shows where it will go.
public static class RowDrag
{
    public static Control Handle(Panel rows, Border row, WheelItemModel it, Func<List<WheelItemModel>> list, Action moved)
    {
        var grip = new TextBlock
        {
            Text = "≡", FontSize = 20, Foreground = Brush.Parse("#C4CCD8"), VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center, Cursor = new Cursor(StandardCursorType.SizeNorthSouth),
            Background = Brushes.Transparent, Width = 26,
        };
        ToolTip.SetTip(grip, "Drag to move it up or down the list");
        bool dragging = false;
        Border target = null;
        bool below = false;

        void Mark(Border b, bool under)
        {
            if (target != null) target.BorderThickness = new Thickness(0);
            target = b;
            below = under;
            if (b != null)
            {
                b.BorderBrush = Brush.Parse("#F08C1E");
                b.BorderThickness = under ? new Thickness(0, 0, 0, 3) : new Thickness(0, 3, 0, 0);
            }
        }

        grip.PointerPressed += (s, e) =>
        {
            if (!e.GetCurrentPoint(grip).Properties.IsLeftButtonPressed) return;
            dragging = true;
            e.Pointer.Capture(grip);
            row.Opacity = 0.5;
            e.Handled = true;
        };
        grip.PointerMoved += (s, e) =>
        {
            if (!dragging) return;
            var y = e.GetPosition(rows).Y;
            Border hit = null;
            bool under = false;
            foreach (var c in rows.Children.OfType<Border>().Where(b => b.Tag is WheelItemModel))
            {
                var top = c.TranslatePoint(new Point(0, 0), rows)?.Y ?? 0;
                if (y >= top && y < top + c.Bounds.Height) { hit = c; under = y > top + c.Bounds.Height / 2; break; }
            }
            Mark(ReferenceEquals(hit, row) ? null : hit, under);
        };
        grip.PointerReleased += (s, e) =>
        {
            if (!dragging) return;
            dragging = false;
            e.Pointer.Capture(null);
            row.Opacity = 1;
            var dropOn = target;
            bool after = below;
            Mark(null, false);
            if (dropOn?.Tag is not WheelItemModel other || ReferenceEquals(other, it)) return;
            Drop(list(), it, other, after);
            moved();
            e.Handled = true;
        };
        grip.PointerCaptureLost += (s, e) => { if (dragging) { dragging = false; row.Opacity = 1; Mark(null, false); } };
        return grip;
    }

    // Tests: the same move a drop does (drag 'it' onto 'other', its bottom half when after).
    public static void Drop(List<WheelItemModel> l, WheelItemModel it, WheelItemModel other, bool after)
    {
        int from = l.IndexOf(it);
        if (from < 0 || ReferenceEquals(it, other)) return;
        l.RemoveAt(from);
        int to = l.IndexOf(other) + (after ? 1 : 0);
        l.Insert(Math.Clamp(to, 0, l.Count), it);
    }
}
