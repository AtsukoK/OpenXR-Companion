using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace OpenXRCompanionEditor;

class NewProfileDialog : Form
{
    readonly ComboBox exe = new() { Width = 300 };
    readonly ComboBox copy = new() { Width = 300, DropDownStyle = ComboBoxStyle.DropDownList };
    public string ExeName => exe.Text;
    public string CopyFrom => copy.SelectedItem as string;

    public NewProfileDialog(string[] seenExes)
    {
        Text = "New game profile";
        FormBorderStyle = FormBorderStyle.FixedDialog; MaximizeBox = MinimizeBox = false;
        StartPosition = FormStartPosition.CenterParent;
        AutoSize = true; AutoSizeMode = AutoSizeMode.GrowAndShrink; Padding = new Padding(10);
        var t = new TableLayoutPanel { ColumnCount = 3, AutoSize = true, Dock = DockStyle.Fill };
        t.Controls.Add(new Label { Text = "Game exe name:", AutoSize = true, Anchor = AnchorStyles.Left }, 0, 0);
        exe.Items.AddRange(seenExes);
        t.Controls.Add(exe, 1, 0);
        var browse = new Button { Text = "Browse...", AutoSize = true };
        browse.Click += (s, e) => { using var d = new OpenFileDialog { Filter = "Programs|*.exe" }; if (d.ShowDialog(this) == DialogResult.OK) exe.Text = Path.GetFileNameWithoutExtension(d.FileName); };
        t.Controls.Add(browse, 2, 0);
        t.Controls.Add(new Label { Text = "Start from:", AutoSize = true, Anchor = AnchorStyles.Left }, 0, 1);
        foreach (var f in Directory.GetFiles(MainForm.ProfilesPath, "*.json"))
            copy.Items.Add(Path.GetFileName(f));
        copy.SelectedItem = MainForm.StartProfile;
        t.Controls.Add(copy, 1, 1);
        t.Controls.Add(new Label { Text = "Games the layer has already seen are in the list (from its logs).", AutoSize = true, ForeColor = Color.DimGray }, 1, 2);
        var ok = new Button { Text = "Create", DialogResult = DialogResult.OK, AutoSize = true };
        t.Controls.Add(ok, 1, 3);
        AcceptButton = ok;
        Controls.Add(t);
    }
}
