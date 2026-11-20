using System.Drawing.Drawing2D;

namespace OpenXRCompanionEditor;

public abstract class EditorBase : UserControl
{
    public event Action Changed;
    protected bool loading;
    protected void Fire() { if (!loading) Changed?.Invoke(); }
    protected EditorBase() { AutoScroll = true; Dock = DockStyle.Fill; Padding = new Padding(12, 8, 12, 8); }
    protected void Stack(params Control[] cs) { for (int i = cs.Length - 1; i >= 0; i--) Controls.Add(cs[i]); }
    protected static Label Title(string t) => new() { Text = t, Font = new Font("Segoe UI", 14f, FontStyle.Bold), AutoSize = true, Dock = DockStyle.Top, Padding = new Padding(0, 0, 0, 4) };
}
