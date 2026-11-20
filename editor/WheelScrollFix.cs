using System.Runtime.InteropServices;

namespace OpenXRCompanionEditor;

// Scrolling the page with the mouse wheel must never change a setting it passes over: over a closed
// dropdown, a number box or a slider, the wheel scrolls the page instead. (An open dropdown list still scrolls.)
sealed class WheelScrollFix : IMessageFilter
{
    const int WM_MOUSEWHEEL = 0x020A;
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point p);
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, int msg, IntPtr w, IntPtr l);

    public static void Install() => Application.AddMessageFilter(new WheelScrollFix());

    public bool PreFilterMessage(ref Message m)
    {
        if (m.Msg != WM_MOUSEWHEEL) return false;
        var c = Control.FromChildHandle(WindowFromPoint(Cursor.Position));
        bool guard = false;
        for (var k = c; k != null && !guard; k = k.Parent)
            guard = k is ComboBox { DroppedDown: false } or NumericUpDown or TrackBar or DomainUpDown;
        if (!guard) return false;
        // hand the wheel to the nearest scrolling page above it
        for (var k = c?.Parent; k != null; k = k.Parent)
            if (k is ScrollableControl { AutoScroll: true } sc && sc.VerticalScroll.Visible && k is not (NumericUpDown or UpDownBase))
            {
                SendMessage(sc.Handle, WM_MOUSEWHEEL, m.WParam, m.LParam);
                return true;
            }
        return true; // nothing to scroll: just don't change the value
    }
}
