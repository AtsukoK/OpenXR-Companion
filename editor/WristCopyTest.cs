using OpenXRCompanionEditor;

namespace OverviewPrototype;

// --wristcopytest <result.txt>: for every profile, every panel the Wrist panels page offers is copied onto both arms,
// both ways (replace, add as next pages), and from the other arm also mirrored, each on a fresh copy of the profile in
// memory. Checks: wrist page names unique over both arms, every "go to page" and first page exists, the layer accepts
// the profile (so the wheels the buttons open came along); when adding, the arm's own pages are all still there; when
// mirrored, no copied screen button stays on the source's side. Nothing is written.
public static class WristCopyTest
{
    public static void Run(string resultFile)
    {
        var store = Shell.Store;
        var lines = new List<string>();
        int runs = 0, bad = 0;
        foreach (var name in store.ProfileNames().ToList())
        {
            store.Load(name);
            string text = store.Serialize();
            foreach (var src in WristsPage.AllSources(store.Profile))
                for (int hand = 0; hand < 2; hand++)
                    foreach (bool append in new[] { false, true })
                        foreach (bool mirror in src.hand != hand ? new[] { false, true } : new[] { false })
                        {
                            var p = ProfileIO.FromText(text);
                            foreach (var (k, w) in p.Wheels) w.Name = k;
                            foreach (var wm in new[] { p.WristLeft, p.WristRight })
                                foreach (var (k, w) in wm.Pages) w.Name = k;
                            var ownBefore = p.WristFor(hand).Mirror ? new List<string>() : p.WristFor(hand).Pages.Keys.ToList();
                            bool linked = true;
                            if (append)
                            {
                                if (p.WristFor(hand).Mirror || p.WristFor(hand).Pages.Count == 0)
                                    continue; // the page offers "add" only for a panel with pages of its own
                                linked = WristsPage.Append(p, src.profile, src.panel, hand, mirror);
                            }
                            else
                                WristsPage.Copy(p, src.profile, src.panel, hand, mirror);
                            runs++;
                            var problems = new List<string>();
                            var all = p.WristLeft.Pages.Keys.Concat(p.WristRight.Pages.Keys).ToList();
                            if (all.Count != all.Distinct().Count())
                                problems.Add("duplicate page names");
                            foreach (var wm in new[] { p.WristLeft, p.WristRight })
                            {
                                if (!wm.Mirror && wm.Pages.Count > 0 && !wm.Pages.ContainsKey(wm.Start ?? ""))
                                    problems.Add("first page missing: " + wm.Start);
                                foreach (var pg in wm.Pages.Values)
                                    foreach (var it in pg.Items.Where(i => i.Action?.Type == ActionType.Page))
                                        if (!wm.Pages.ContainsKey(it.Action.Page ?? ""))
                                            problems.Add($"'{it.Label}' goes to a missing page '{it.Action.Page}'");
                            }
                            if (append)
                            {
                                if (ownBefore.Any(k => !p.WristFor(hand).Pages.ContainsKey(k)))
                                    problems.Add("an own page was lost");
                                if (!linked)
                                    problems.Add("(first page full: no button added)");
                            }
                            if (mirror)
                            {
                                string wrong = src.hand == 0 ? "wrist_left" : "wrist_right";
                                var copied = p.WristFor(hand).Pages.Keys.Except(ownBefore);
                                if (copied.SelectMany(k => p.WristFor(hand).Pages[k].Items).Any(i => i.Action?.Type == ActionType.Screen && i.Action.Place == wrong))
                                    problems.Add("a screen button still shows on the " + wrong);
                            }
                            string layer = Native.Validate(ProfileIO.Serialize(p));
                            if (layer != null)
                                problems.Add("layer: " + layer);
                            bool ok = problems.All(x => x.StartsWith("("));
                            if (!ok) bad++;
                            lines.Add($"{(ok ? "ok " : "BAD")} {name}: {(append ? "add" : "replace")}{(mirror ? " mirrored" : "")} {(src.hand == 0 ? "left" : "right")} from {src.from} -> {(hand == 0 ? "left" : "right")}" +
                                      (problems.Count > 0 ? "  " + string.Join("; ", problems) : ""));
                        }
        }
        lines.Insert(0, $"copies: {runs}, bad: {bad}");
        File.WriteAllLines(resultFile, lines);
    }
}
