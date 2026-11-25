using System.Text.Json.Nodes;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.VisualTree;

namespace OverviewPrototype;

// --data <copy> --clicktest <result.txt>: presses real buttons (their Click event, what a mouse click raises) on several
// pages and writes, for each press, every JSON path of the profile that changed. Then saves and checks the saved file
// equals the profile in memory. Only ever on a copy of the data folder (App refuses the real one).
public static class ClickTest
{
    public static async Task Run(Window w, string resultFile)
    {
        var lines = new List<string>();
        var store = Shell.Store;
        string last = store.Serialize();

        Control Page() => ((ContentControl)w.FindControl<ContentControl>("PageHost")!).Content as Control;

        async Task Go(string key)
        {
            Shell.Go(key);
            await Task.Delay(500);
        }

        // the n-th button on the page whose text starts with this
        Button Find(string text, int n = 0) =>
            Page()!.GetVisualDescendants().OfType<Button>()
                .Where(b => (b.Content as string ?? (b.Content as TextBlock)?.Text ?? "").StartsWith(text, StringComparison.Ordinal))
                .Skip(n).FirstOrDefault();

        async Task Press(string what, Button b)
        {
            if (b == null)
            { lines.Add("NOT FOUND: " + what); return; }
            b.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await Task.Delay(400);
            string now = store.Serialize();
            var diffs = Diff(last, now);
            lines.Add($"{what}: {diffs.Count} change(s); unsaved: {store.Dirty}; page now: {Shell.Current}");
            lines.AddRange(diffs.Take(12).Select(d => "    " + d));
            if (diffs.Count > 12)
                lines.Add($"    ... {diffs.Count - 12} more");
            last = now;
        }

        // a dialog window opened by the last press: click its button with this text (what the user would do)
        async Task Answer(string dialogTitle, string buttonText)
        {
            await Task.Delay(400);
            var dlg = (Avalonia.Application.Current!.ApplicationLifetime as Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime)!
                .Windows.FirstOrDefault(x => x != w && x.Title == dialogTitle);
            var b = dlg?.GetVisualDescendants().OfType<Button>().FirstOrDefault(x => (x.Content as string ?? "").StartsWith(buttonText, StringComparison.Ordinal)
                || (x.Content as Control)?.GetVisualDescendants().OfType<TextBlock>().Any(t => (t.Text ?? "").StartsWith(buttonText, StringComparison.Ordinal)) == true);
            if (b == null) { lines.Add("NOT FOUND: dialog '" + dialogTitle + "' button '" + buttonText + "'"); dlg?.Close(); return; }
            b.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await Task.Delay(400);
        }
        async Task Note(string what)
        {
            string now = store.Serialize();
            var diffs = Diff(last, now);
            lines.Add($"{what}: {diffs.Count} change(s); unsaved: {store.Dirty}; page now: {Shell.Current}");
            lines.AddRange(diffs.Take(12).Select(d => "    " + d));
            last = now;
            await Task.CompletedTask;
        }

        await Go("bindings");
        await Press("All bindings: on/off of the first row", Find("On") ?? Find("Off"));
        await Press("All bindings: duplicate the first row", Find("⧉"));
        int before = store.Profile.Bindings.Count;
        Find("✕")?.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        await Answer("Delete binding", "Delete");
        await Note($"All bindings: delete the first row, answered Delete (bindings {before} -> {store.Profile.Bindings.Count})");

        string wk = store.Profile.Wheels.Keys.First();
        await Go("wheel:" + wk);
        await Press($"Wheel '{wk}': + Add item", Find("+ Add item"));
        await Press($"Wheel '{wk}': Move ◀ (the new item)", Find("◀ Move"));
        // What it does: the picker window, pick "Press a key"
        Find("Change…")?.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        await Answer("What should this item do?", "Close the wheel");
        await Note($"Wheel '{wk}': Change… -> picker window -> 'Close the wheel'");
        Find("Change…")?.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        await Answer("What should this item do?", "Press a key");
        await Note($"Wheel '{wk}': Change… -> picker window -> back to 'Press a key'");
        // Record key: click it, then press F8 on the keyboard
        var rec = Find("Record key");
        rec?.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        await Task.Delay(200);
        lines.Add("  Record key button now says: " + (rec?.Content ?? "(not found)"));
        var keyBox = (rec?.Parent as Avalonia.Visual)?.GetVisualDescendants().OfType<TextBox>().FirstOrDefault();
        keyBox?.RaiseEvent(new Avalonia.Input.KeyEventArgs { RoutedEvent = Avalonia.Input.InputElement.KeyDownEvent, Key = Avalonia.Input.Key.F8, Source = keyBox });
        await Task.Delay(300);
        await Note($"Wheel '{wk}': Record key, then F8 pressed (key box now '{keyBox?.Text}')");
        // dragging a row: the first item dropped on the bottom half of the third (what a drop runs), then back on top of the second
        {
            var items = store.Profile.Wheels[wk].Items;
            var a = items[0]; var c = items[2];
            string before0 = string.Join(" | ", items.Select(x => x.Label));
            RowDrag.Drop(items, a, c, after: true);
            string after1 = string.Join(" | ", items.Select(x => x.Label));
            RowDrag.Drop(items, a, items[0], after: false);
            string back = string.Join(" | ", items.Select(x => x.Label));
            lines.Add($"Row drag: '{a.Label}' dropped below '{c.Label}': now at {items.IndexOf(a) >= 0} [{after1}]; dropped back on top: same as before = {back == before0}");
            bool ok1 = after1.Split(" | ").ToList().IndexOf(a.Label) == 2;
            lines.Add("  " + (ok1 && back == before0 ? "drag order OK" : "drag order WRONG"));
        }
        // the colour picker: click the swatch, pick a colour in the picker
        var colBtn = Page()!.GetVisualDescendants().OfType<Button>().FirstOrDefault(b => b.Flyout is Flyout { Content: ColorView });
        if (colBtn == null) lines.Add("NOT FOUND: colour swatch button");
        else
        {
            var fl = (Flyout)colBtn.Flyout!;
            fl.ShowAt(colBtn);
            await Task.Delay(400);
            ((ColorView)fl.Content!).Color = Avalonia.Media.Color.Parse("#FF336699");
            await Task.Delay(300);
            fl.Hide();
            await Note($"Wheel '{wk}': colour swatch -> picker -> #336699");
        }
        await Press($"Wheel '{wk}': the All buttons tab (no change expected)", Find("🔍"));
        await Press($"Wheel '{wk}': library, first + Add here", Find("+ Add here"));

        // Wrist panel: ticking Mirror switches the page to the other panel's pages straight away
        await Go("wrist1:settings");
        var mirror = Page()!.GetVisualDescendants().OfType<CheckBox>().FirstOrDefault(c => ((c.Content as TextBlock)?.Text ?? "").StartsWith("Mirror the"));
        if (mirror == null) lines.Add("NOT FOUND: Mirror tick box");
        else
        {
            bool was = mirror.IsChecked == true;
            mirror.IsChecked = !was;
            await Task.Delay(700);
            bool notice = Page()!.GetVisualDescendants().OfType<TextBlock>().Any(t => (t.Text ?? "").StartsWith("This panel mirrors"));
            bool onSettings = Page()!.GetVisualDescendants().OfType<CheckBox>().Any(c => ((c.Content as TextBlock)?.Text ?? "").StartsWith("Mirror the") && c.IsEffectivelyVisible);
            await Note($"Right wrist: Mirror {(was ? "unticked" : "ticked")} -> page rebuilt at once: shows the mirror notice = {notice} (expected {!was}), still on the settings tab = {onSettings}");
            var again = Page()!.GetVisualDescendants().OfType<CheckBox>().FirstOrDefault(c => ((c.Content as TextBlock)?.Text ?? "").StartsWith("Mirror the"));
            if (again != null) { again.IsChecked = was; await Task.Delay(700); await Note("Right wrist: Mirror back as it was"); }
        }
        await Go("wrist0");
        await Press("Left wrist: + Add button", Find("+ Add button"));
        await Go("controllers:1.primary");
        await Press("Controllers, right A (primary): first + Add…", Find("+ Add…"));
        await Go("settings:pointing");
        var steady = Page()!.GetVisualDescendants().OfType<NumericUpDown>().ElementAtOrDefault(6); // "Steady the laser"
        if (steady != null)
        {
            steady.Value = (steady.Value ?? 0) + 0.05m;
            await Press("Settings > Pointing: Steady the laser + 0.05 (typed, then a harmless page button)", Find("Use the numbers") ?? new Button());
        }

        // a binding's own page: Duplicate binding, then Delete binding (answered Delete)
        await Go("binding:0");
        int nb = store.Profile.Bindings.Count;
        await Press("Binding page: Duplicate binding", Find("Duplicate binding"));
        lines.Add($"  bindings {nb} -> {store.Profile.Bindings.Count}, now on the copy's page: {Shell.Current}");
        Find("Delete binding")?.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        await Answer("Delete binding", "Delete");
        await Note($"Binding page: Delete binding (the copy), answered Delete -> bindings {store.Profile.Bindings.Count}");
        // a wheel page: Duplicate wheel, then Delete wheel on the copy
        await Go("wheel:" + wk);
        int nw = store.Profile.Wheels.Count;
        await Press($"Wheel '{wk}': Duplicate wheel", Find("Duplicate wheel"));
        lines.Add($"  wheels {nw} -> {store.Profile.Wheels.Count}, now on: {Shell.Current}");
        Find("Delete wheel")?.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        await Answer("Delete wheel", "Delete");
        await Note($"Wheel copy: Delete wheel, answered Delete -> wheels {store.Profile.Wheels.Count}, page now {Shell.Current}");

        string why = store.Save();
        string saved = File.ReadAllText(Path.Combine(store.ProfilesDir, store.CurrentName));
        lines.Add("save: " + (why ?? "ok") + " to " + store.CurrentName + "; saved file equals the profile in memory: " + (saved == store.Serialize()));
        File.WriteAllLines(resultFile, lines);
    }

    static List<string> Diff(string before, string after)
    {
        var diffs = new List<string>();
        void D(JsonNode x, JsonNode y, string path)
        {
            if (x is JsonObject ox && y is JsonObject oy)
            {
                foreach (var k in ox.Select(kv => kv.Key).Union(oy.Select(kv => kv.Key)))
                    D(ox[k], oy[k], path + "." + k);
            }
            else if (x is JsonArray ax && y is JsonArray ay)
            {
                for (int i = 0; i < Math.Max(ax.Count, ay.Count); i++)
                    D(i < ax.Count ? ax[i] : null, i < ay.Count ? ay[i] : null, path + "[" + i + "]");
            }
            else
            {
                string a = x?.ToJsonString() ?? "null", b = y?.ToJsonString() ?? "null";
                if (a != b)
                    diffs.Add(path + ": " + Short(a) + " -> " + Short(b));
            }
        }
        D(JsonNode.Parse(before), JsonNode.Parse(after), "$");
        return diffs;
    }

    static string Short(string s) => s.Length > 90 ? s[..90] + "…" : s;
}
