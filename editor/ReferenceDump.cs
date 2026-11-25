using System.Text;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// --reference <data folder> <out.txt>: everything the data side produces for the profiles there, as text, so a
// restructured data side can be compared with this one line by line. Reads only.
public static class ReferenceDump
{
    public static void Write(string dataDir, string outFile)
    {
        var sb = new StringBuilder();
        var store = new ProfileStore(dataDir, false);
        sb.AppendLine("## choices");
        foreach (var (name, list) in new (string, Choice[])[] { ("Hands", Choices.Hands), ("Buttons", Choices.Buttons), ("Blocks", Choices.Blocks), ("KeyModes", Choices.KeyModes),
                     ("ScreenModes", Choices.ScreenModes), ("OffsetModes", Choices.OffsetModes), ("MirrorModes", Choices.MirrorModes), ("ScreenPlaces", Choices.ScreenPlaces),
                     ("LayerModes", Choices.LayerModes), ("Confirms", Choices.Confirms), ("ActionTypes", Choices.ActionTypes), ("Anchors", EditorApi.Anchors), ("Monitors", Monitors.Choices(true)) })
            sb.AppendLine(name + ": " + string.Join(" | ", list.Select(c => c.Code + "=" + c.Text)));
        foreach (var kind in new[] { "binding", "wheel", "wrist" })
            sb.AppendLine("options " + kind + ": " + string.Join(" | ", EditorApi.ActionOptions(kind).Select(o => $"{o.code}={o.option}")));
        sb.AppendLine("kinds: " + string.Join(" | ", EditorApi.ControllerKinds().Select(k => $"{k.id}={k.label}={k.how}")));
        sb.AppendLine("settings pages: " + string.Join(" | ", SettingsPage.All.Select(s => $"{s.Key}={s.Title}={s.Sub}={s.Live}")));
        sb.AppendLine("check keys: " + string.Join(" | ", new[] { "F5", "Ctrl+S", "Qq", "LMB", "wait 100; Ctrl+C" }.Select(k => k + "=" + (EditorApi.CheckKeys(k, k.Contains(';')) ?? "ok"))));
        foreach (var layout in ControllerLayouts.All)
            for (int h = 0; h < 2; h++)
                foreach (var part in layout.PartsFor(h))
                    foreach (var way in part.Ways)
                        sb.AppendLine($"name {layout.Id} {h} {part.Comp} {way}: {QuickBindings.NameOf(layout, h, part, way)} / {ControllerLayouts.InputOf(h, part, way)} / {ControllerLayouts.WayName(part, way)}");
        foreach (var name in store.ProfileNames())
        {
            store.Load(name);
            var p = store.Profile;
            sb.AppendLine("## profile " + name);
            sb.AppendLine(store.Serialize());
            sb.AppendLine("picture: " + EditorApi.PictureChoice(p) + "; kind: " + EditorApi.CurrentControllerKind(p) + "; layers: " + string.Join(",", EditorApi.LayerNames(p)));
            sb.AppendLine("validate: " + (ProfileStore.Validate(store.Serialize()) ?? "ok"));
            foreach (var b in p.Bindings)
                sb.AppendLine("binding " + b.Name + ": " + EditorApi.Plain(b.Action) + " / " + InputText.Friendly(b.Input));
            foreach (var w in p.Wheels.Values.Concat(p.WristLeft.Pages.Values).Concat(p.WristRight.Pages.Values))
                foreach (var i in w.Items)
                    sb.AppendLine("item " + w.Name + "/" + i.Label + ": " + EditorApi.Plain(i.Action));
            foreach (var layout in ControllerLayouts.All)
                for (int h = 0; h < 2; h++)
                    foreach (var part in layout.PartsFor(h))
                    {
                        var use = QuickBindings.UseOf(p.Bindings, h, part);
                        var lines = TagLines.Of(p.Bindings, h, part);
                        sb.AppendLine($"tag {layout.Id} {h} {part.Comp}: {use.On}/{use.Off} " + string.Join(" | ", lines.Select(l => $"{l.Layer}:{l.When}:{l.Does}:{l.IsUnused}")));
                    }
        }
        File.WriteAllText(outFile, sb.ToString());
    }
}
