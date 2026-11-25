using System.Text.Json;
using System.Text.Json.Nodes;

namespace OpenXRCompanionEditor;

// Reading and writing profile text: the one way both editors do it.
public static class ProfileIO
{
    public static readonly JsonSerializerOptions JsonOpts = new() { Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping, WriteIndented = true, PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true };

    public static ProfileModel FromText(string text)
    {
        var root = JsonNode.Parse(text, documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
        // Accept a chord given as an array as well.
        foreach (var b in root?["bindings"]?.AsArray() ?? new JsonArray())
            if (b?["input"] is JsonArray arr)
                b["input"] = string.Join("+", arr.Select(x => x.ToString()));
        foreach (var b in root?["bindings"]?.AsArray() ?? new JsonArray())
            foreach (var k in new[] { "layers", "not_layers" })
                if (b?[k] is JsonArray la)
                    b[k] = string.Join(",", la.Select(x => x.ToString()));
        return root.Deserialize<ProfileModel>(JsonOpts) ?? new ProfileModel();
    }

    // wheels and wrist pages re-keyed by their names first (a renamed one is saved under its new name)
    public static string Serialize(ProfileModel Profile)
    {
        var wheels = new Dictionary<string, WheelModel>();
        foreach (var w in Profile.Wheels.Values)
            wheels[w.Name] = w;
        Profile.Wheels = wheels;
        foreach (var wm in new[] { Profile.WristLeft, Profile.WristRight })
        {
            var pages = new Dictionary<string, WheelModel>();
            foreach (var w in wm.Pages.Values)
                pages[w.Name] = w;
            wm.Pages = pages;
        }
        return JsonSerializer.Serialize(Profile, JsonOpts);
    }
}
