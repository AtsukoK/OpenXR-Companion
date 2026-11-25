using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Text.Json.Nodes;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

// Plain classes the page binds to. When a property changes, the page redraws that bit by itself:
// no "rebuild the panel" code like the WinForms version needs.
public abstract class Notify : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler PropertyChanged;
    protected void Changed([CallerMemberName] string name = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}

public class SwitchRow : Notify
{
    readonly Func<bool> get;
    readonly Action<bool> set;
    readonly Action after;
    public SwitchRow(string text, string detail, Func<bool> get, Action<bool> set, Action after)
    { Text = text; Detail = detail; this.get = get; this.set = set; this.after = after; }
    public string Text { get; }
    public string Detail { get; }
    public bool HasDetail => !string.IsNullOrEmpty(Detail);
    public bool IsOn
    {
        get => get();
        set { set(value); Changed(); after?.Invoke(); Shell.Store?.MarkDirty(); }
    }
}

// A blue button: its text and the page it opens (a Shell key).
public record GoLink(string Label, string Target)
{
    public string Text => Label + "  ▶";
}

public class Card : Notify
{
    readonly Func<(string pill, bool on)> state;
    public Card(string icon, string title, string description, Func<(string, bool)> state)
    { Icon = icon; Title = title; Description = description; this.state = state; }
    public string Icon { get; }
    public string Title { get; }
    public string Description { get; }
    public string Pill => state().pill;
    public bool IsOn => state().on;
    public ObservableCollection<SwitchRow> Switches { get; } = new();
    public ObservableCollection<GoLink> Buttons { get; } = new();
    public void Refresh() { Changed(nameof(Pill)); Changed(nameof(IsOn)); }
}

public static class Overview
{
    static readonly JsonSerializerOptions Opts = new() { PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true };

    // Same reading rules as the editor (chords and layers given as arrays become strings).
    public static ProfileModel Load(string path)
    {
        var root = JsonNode.Parse(File.ReadAllText(path), documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
        foreach (var b in root?["bindings"]?.AsArray() ?? new JsonArray())
        {
            if (b?["input"] is JsonArray arr)
                b["input"] = string.Join("+", arr.Select(x => x.ToString()));
            foreach (var k in new[] { "layers", "not_layers" })
                if (b?[k] is JsonArray la)
                    b[k] = string.Join(",", la.Select(x => x.ToString()));
        }
        return root.Deserialize<ProfileModel>(Opts) ?? new ProfileModel();
    }

    static string HowTo(BindingModel b)
    {
        string verb = b.Gesture switch { "double_tap" => "Double-tap", "double_tap_hold" => "Double-tap and hold", "hold" or "while_held" => "Hold", "tap" => "Tap", _ => "Press" };
        string s = verb + " " + InputText.Friendly(b.Input);
        if (!string.IsNullOrWhiteSpace(b.Layers))
            s += " (while layer " + b.Layers + " is on)";
        return s;
    }

    static string HandOf(BindingModel b) => (b.Input ?? "").StartsWith("left", StringComparison.OrdinalIgnoreCase) ? "left" : "right";

    // The cards, in the order the page shows them (pairs: left-hand things first).
    public static List<Card> Build(ProfileModel p)
    {
        var cards = new List<Card>();

        var master = new Card("", "Master switch",
            "Turns the whole OpenXR Companion layer on or off for this profile. Everything below only works while it is on.",
            () => (p.Enabled ? "ON" : "OFF", p.Enabled));
        master.Switches.Add(new SwitchRow("Layer on for this profile", null, () => p.Enabled, v => p.Enabled = v, master.Refresh));
        master.Buttons.Add(new("Settings (look, picking items)", "settings:wheel")); // the wheel's look and how items are picked
        cards.Add(master);

        var kbOpeners = p.Bindings.Where(b => b.Action?.Type == "keyboard").ToList();
        var keyboard = new Card("", "VR keyboard",
            kbOpeners.Count > 0 ? "Opened by:" : "Nothing opens it yet. Add a Keyboard button to a wheel or wrist panel, or a binding.",
            () => kbOpeners.Any(b => b.Enabled) ? ("ON", true) : ("OFF", false));
        foreach (var b in kbOpeners)
            keyboard.Switches.Add(new SwitchRow(HowTo(b), null, () => b.Enabled, v => b.Enabled = v, keyboard.Refresh));
        keyboard.Buttons.Add(new("Keyboard size and position", "settings:keyboard"));
        cards.Add(keyboard);

        foreach (var hand in new[] { "left", "right" })
        {
            var openers = p.Bindings.Where(b => b.Action?.Type == "wheel" && HandOf(b) == hand).ToList();
            string title = hand == "left" ? "Left controller wheel" : "Right controller wheel";
            var card = new Card("", title,
                openers.Count > 0 ? "How to open it in the headset:" : $"No binding opens a wheel from the {hand} controller. Add one under Bindings (action: Open a wheel).",
                () => openers.Any(b => b.Enabled) ? ("ON", true) : ("OFF", false));
            foreach (var b in openers)
                card.Switches.Add(new SwitchRow(HowTo(b), $"opens wheel '{b.Action.Wheel}'", () => b.Enabled, v => b.Enabled = v, card.Refresh));
            if (openers.Count > 0)
            {
                card.Buttons.Add(new($"Edit wheel '{openers[0].Action.Wheel}'", "wheel:" + openers[0].Action.Wheel));
                card.Buttons.Add(new("Edit the button that opens it", "binding:" + p.Bindings.IndexOf(openers[0])));
            }
            else
                card.Buttons.Add(new("All bindings", "bindings"));
            cards.Add(card);
        }

        foreach (var h in new[] { 0, 1 })
        {
            var wm = p.WristFor(h);
            var pages = p.WristPages(h);
            string side = h == 0 ? "Left" : "Right", other = h == 0 ? "right" : "left";
            int first = pages.Pages.Values.FirstOrDefault()?.Items.Count ?? 0;
            var card = new Card("", $"{side} wrist panel",
                wm.Enabled
                    ? (wm.Mirror ? $"Shows the {other} panel's buttons." : $"Own pages: {pages.Pages.Count} page(s), {first} buttons on the first.") +
                      $" Pressed with your {other} hand."
                    : $"Hidden. Turn it on to get buttons on your {side.ToLowerInvariant()} forearm.",
                () => wm.Enabled ? ("ON", true) : ("OFF", false));
            card.Switches.Add(new SwitchRow("Panel shown", null, () => wm.Enabled, v => wm.Enabled = v, card.Refresh));
            card.Buttons.Add(new("Panel settings (size, side, text)", "wrist" + h + ":settings")); // the panel's settings tab, not its buttons
            card.Buttons.Add(new(wm.Mirror ? $"Edit the {other} panel's buttons" : "Edit its buttons", "wrist" + (wm.Mirror ? 1 - h : h)));
            cards.Add(card);
        }

        var rest = p.Bindings.Where(b => b.Action?.Type is not ("wheel" or "keyboard")).ToList();
        Card others = null;
        others = new Card("", "Other bindings", "Keys, macros, layers and game inputs on controller buttons.",
            () => { int on = rest.Count(b => b.Enabled); return ($"{on} of {rest.Count} on", on > 0); });
        foreach (var b in rest)
            others.Switches.Add(new SwitchRow(b.Name, HowTo(b), () => b.Enabled, v => b.Enabled = v, () => others.Refresh()));
        others.Buttons.Add(new("All bindings", "bindings"));
        cards.Add(others);

        return cards;
    }
}
