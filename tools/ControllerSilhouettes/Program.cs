// Traces controller outlines from the WebXR input profile models into drawing data for the editor and the layer.
//
//   dotnet run -- <models folder> <repo root>
//
// <models folder> holds one folder per profile (meta-quest-touch-plus, valve-index...) with left.glb and right.glb,
// downloaded from https://github.com/immersive-web/webxr-input-profiles (packages/assets/profiles, MIT licence,
// (c) 2019 Amazon). Only outlines are kept: no textures, no logos. Writes:
//   editor/ControllerSilhouettes.g.cs, layer/src/controller_silhouettes.inc, tools/ControllerSilhouettes/preview/*.png
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Text.Json;

namespace ControllerSilhouettes;

static class Program
{
    // WebXR profile folder -> our layout id (editor/ControllerLayouts.cs)
    // topFromSide: for controllers whose buttons face along the handle (the Index): looking at the buttons shows only
    // the head and strap, so the "top" picture looks across the controller instead (along its Y axis): head on
    // top, handle down, strap beside it, which is how it is usually pictured.
    static readonly (string profile, string id, bool hasThumbrest, bool topFromSide)[] Profiles =
    {
        ("meta-quest-touch-plus", "quest3", true, false),
        ("oculus-touch-v3", "quest2", true, false),
        ("oculus-touch-v2", "rifts", true, false),
        ("meta-quest-touch-pro", "questpro", true, false),
        ("pico-4", "pico4", false, false),
        ("valve-index", "index", false, true),
        ("microsoft-mixed-reality", "wmr", false, false),
        ("hp-mixed-reality", "reverbg2", false, false),
    };

    static int Main(string[] args)
    {
        if (args.Length != 2)
        {
            Console.WriteLine("usage: ControllerSilhouettes <models folder> <repo root>");
            return 1;
        }
        string models = args[0], repo = args[1];
        string preview = Path.Combine(repo, "tools", "ControllerSilhouettes", "preview");
        if (Environment.GetEnvironmentVariable("SIL_AXES") is string axesOf) // debug: the six straight-on views of one model
        {
            Directory.CreateDirectory(preview);
            string glb = Path.Combine(models, axesOf, "right.glb"), dae = Path.Combine(models, axesOf, "model", "model.dae");
            Tracer.AxisViews(File.Exists(glb) ? Glb.Load(glb) : Dae.Load(dae), Path.Combine(preview, "axes_" + axesOf + ".png"));
            return 0;
        }
        Directory.CreateDirectory(preview);
        var results = new List<Traced>();
        foreach (var (profile, id, hasThumbrest, topFromSide) in Profiles)
            foreach (string hand in new[] { "left", "right" })
            {
                string file = Path.Combine(models, profile, hand + ".glb");
                if (!File.Exists(file))
                {
                    Console.WriteLine($"skipped {profile}/{hand}: no model");
                    continue;
                }
                var tris = Glb.Load(file);
                var traced = Tracer.Trace(id, hand, tris, hasThumbrest, topFromSide);
                results.Add(traced);
                Preview.Save(traced, Path.Combine(preview, $"{id}_{hand}.png"));
                Console.WriteLine($"{id}/{hand}: {tris.Count} triangles, parts {string.Join(", ", traced.Parts.Select(p => p.Comp + (p.Estimated ? "*" : "")))}");
            }
        // The Vive wands: not in the WebXR set. A CC BY model when it is there (HTC Vive Controller by iShoNz on
        // Sketchfab), else photos.
        string viveModel = Path.Combine(models, "htc-vive", "model", "model.dae");
        if (File.Exists(viveModel))
        {
            var viveTris = Dae.Load(viveModel);
            foreach (string hand in new[] { "left", "right" })
            {
                var traced = Tracer.TraceVive(hand, viveTris);
                results.Add(traced);
                Preview.Save(traced, Path.Combine(preview, $"vive_{hand}.png"));
                Console.WriteLine($"vive/{hand}: from the model, {viveTris.Count} triangles");
            }
        }
        string photo = Path.Combine(models, "photos", "vive_back.png"); // the back shows the crown's ring best
        bool fromBack = File.Exists(photo);
        if (!fromBack)
            photo = Path.Combine(models, "photos", "vive_wands.png");
        if (File.Exists(photo) && !File.Exists(viveModel))
            foreach (string hand in new[] { "left", "right" })
            {
                var traced = PhotoTracer.Trace("vive", hand, photo, fromBack);
                results.Add(traced);
                Preview.Save(traced, Path.Combine(preview, $"vive_{hand}.png"));
                Console.WriteLine($"vive/{hand}: from the photo, parts {string.Join(", ", traced.Parts.Select(p => p.Comp))}");
            }
        if (results.Count < Profiles.Length * 2) // never replace the outlines with fewer than before
        {
            Console.WriteLine($"only {results.Count} of {Profiles.Length * 2} models found: nothing written (see README.md for where to get them)");
            return 2;
        }
        Emit.CSharp(results, Path.Combine(repo, "editor", "ControllerSilhouettes.g.cs"));
        Emit.Cpp(results, Path.Combine(repo, "layer", "src", "controller_silhouettes.inc"));
        return 0;
    }
}

// ------------------------------------------------------------ glTF binary: triangles in model space, with the part they belong to
record struct Tri(Vector3 A, Vector3 B, Vector3 C, string Part);

static class Glb
{
    public static List<Tri> Load(string path)
    {
        byte[] data = File.ReadAllBytes(path);
        int jsonLen = BitConverter.ToInt32(data, 12);
        using var doc = JsonDocument.Parse(new ReadOnlyMemory<byte>(data, 20, jsonLen));
        var root = doc.RootElement;
        int binStart = 20 + jsonLen + 8; // after the BIN chunk header
        var nodes = root.GetProperty("nodes").EnumerateArray().ToArray();
        var parent = new Dictionary<int, int>();
        for (int i = 0; i < nodes.Length; i++)
            if (nodes[i].TryGetProperty("children", out var kids))
                foreach (var k in kids.EnumerateArray())
                    parent[k.GetInt32()] = i;
        var world = new Matrix4x4?[nodes.Length];
        Matrix4x4 World(int i)
        {
            if (world[i] is Matrix4x4 w)
                return w;
            var local = Local(nodes[i]);
            var m = parent.TryGetValue(i, out int p) ? local * World(p) : local;
            world[i] = m;
            return m;
        }
        var tris = new List<Tri>();
        for (int i = 0; i < nodes.Length; i++)
        {
            if (!nodes[i].TryGetProperty("mesh", out var meshIndex))
                continue;
            string part = PartOf(i, nodes, parent);
            var m = World(i);
            var mesh = root.GetProperty("meshes")[meshIndex.GetInt32()];
            foreach (var prim in mesh.GetProperty("primitives").EnumerateArray())
            {
                if (prim.TryGetProperty("mode", out var mode) && mode.GetInt32() != 4)
                    continue; // triangles only
                var pos = ReadVec3(root, data, binStart, prim.GetProperty("attributes").GetProperty("POSITION").GetInt32());
                var idx = prim.TryGetProperty("indices", out var ix) ? ReadIndices(root, data, binStart, ix.GetInt32())
                                                                      : Enumerable.Range(0, pos.Length).ToArray();
                for (int t = 0; t + 2 < idx.Length; t += 3)
                    tris.Add(new Tri(Vector3.Transform(pos[idx[t]], m), Vector3.Transform(pos[idx[t + 1]], m), Vector3.Transform(pos[idx[t + 2]], m), part));
            }
        }
        return tris;
    }

    // The part a mesh belongs to: the first of its node and its parents whose name says what it is.
    static string PartOf(int i, JsonElement[] nodes, Dictionary<int, int> parent)
    {
        for (int k = i; ; k = parent[k])
        {
            string name = nodes[k].TryGetProperty("name", out var n) ? n.GetString().ToLowerInvariant() : "";
            string part = name switch
            {
                "a" or "x" => "primary",
                "b" or "y" => "secondary",
                "select" => "trigger",
                "grasp" => "grip",
                "home" => "system",
                _ when name.StartsWith("a_button") || name.StartsWith("x_button") => "primary",
                _ when name.StartsWith("b_button") || name.StartsWith("y_button") => "secondary",
                _ when name.Contains("thumbstick") || name.Contains("joystick") => "thumbstick",
                _ when name.Contains("touchpad") => "touchpad",
                _ when name.Contains("trigger") => "trigger",
                _ when name.Contains("squeeze") || name == "grip" => "grip",
                _ when name.Contains("thumbrest") => "thumbrest",
                _ when name.Contains("menu") => "menu",
                _ when name.Contains("power") || name.Contains("system") => "system",
                _ => null,
            };
            if (part != null)
                return part;
            if (!parent.ContainsKey(k))
                return "body";
        }
    }

    static Matrix4x4 Local(JsonElement node)
    {
        if (node.TryGetProperty("matrix", out var mat))
        {
            var f = mat.EnumerateArray().Select(e => e.GetSingle()).ToArray(); // column-major = row-vector rows
            return new Matrix4x4(f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
        }
        var s = node.TryGetProperty("scale", out var sc) ? sc.EnumerateArray().Select(e => e.GetSingle()).ToArray() : new[] { 1f, 1f, 1f };
        var r = node.TryGetProperty("rotation", out var ro) ? ro.EnumerateArray().Select(e => e.GetSingle()).ToArray() : new[] { 0f, 0f, 0f, 1f };
        var t = node.TryGetProperty("translation", out var tr) ? tr.EnumerateArray().Select(e => e.GetSingle()).ToArray() : new[] { 0f, 0f, 0f };
        return Matrix4x4.CreateScale(s[0], s[1], s[2]) * Matrix4x4.CreateFromQuaternion(new Quaternion(r[0], r[1], r[2], r[3])) *
               Matrix4x4.CreateTranslation(t[0], t[1], t[2]);
    }

    static (int offset, int stride) View(JsonElement root, int binStart, JsonElement accessor, int elementSize)
    {
        var view = root.GetProperty("bufferViews")[accessor.GetProperty("bufferView").GetInt32()];
        int offset = binStart + (view.TryGetProperty("byteOffset", out var vo) ? vo.GetInt32() : 0) +
                     (accessor.TryGetProperty("byteOffset", out var ao) ? ao.GetInt32() : 0);
        int stride = view.TryGetProperty("byteStride", out var bs) ? bs.GetInt32() : elementSize;
        return (offset, stride);
    }

    static Vector3[] ReadVec3(JsonElement root, byte[] data, int binStart, int index)
    {
        var acc = root.GetProperty("accessors")[index];
        int count = acc.GetProperty("count").GetInt32();
        var (offset, stride) = View(root, binStart, acc, 12);
        var v = new Vector3[count];
        for (int i = 0; i < count; i++)
        {
            int o = offset + i * stride;
            v[i] = new Vector3(BitConverter.ToSingle(data, o), BitConverter.ToSingle(data, o + 4), BitConverter.ToSingle(data, o + 8));
        }
        return v;
    }

    static int[] ReadIndices(JsonElement root, byte[] data, int binStart, int index)
    {
        var acc = root.GetProperty("accessors")[index];
        int count = acc.GetProperty("count").GetInt32(), type = acc.GetProperty("componentType").GetInt32();
        int size = type == 5125 ? 4 : type == 5123 ? 2 : 1;
        var (offset, stride) = View(root, binStart, acc, size);
        var r = new int[count];
        for (int i = 0; i < count; i++)
        {
            int o = offset + i * stride;
            r[i] = size == 4 ? (int)BitConverter.ToUInt32(data, o) : size == 2 ? BitConverter.ToUInt16(data, o) : data[o];
        }
        return r;
    }
}

// ------------------------------------------------------------ Collada (.dae): one mesh's triangles (the Vive wand model)
static class Dae
{
    public static List<Tri> Load(string path)
    {
        var doc = System.Xml.Linq.XDocument.Load(path);
        System.Xml.Linq.XNamespace ns = doc.Root.Name.Namespace;
        var tris = new List<Tri>();
        foreach (var mesh in doc.Descendants(ns + "mesh"))
        {
            var sources = mesh.Elements(ns + "source").ToDictionary(e => (string)e.Attribute("id"),
                e => e.Element(ns + "float_array").Value.Split((char[])null, StringSplitOptions.RemoveEmptyEntries).Select(x => float.Parse(x, CultureInfo.InvariantCulture)).ToArray());
            string posId = ((string)mesh.Element(ns + "vertices").Elements(ns + "input").First(i => (string)i.Attribute("semantic") == "POSITION").Attribute("source")).TrimStart('#');
            var pos = sources[posId];
            Vector3 P(int i) => new(pos[3 * i], pos[3 * i + 1], pos[3 * i + 2]);
            foreach (var prim in mesh.Elements().Where(e => e.Name.LocalName is "polylist" or "triangles"))
            {
                int stride = prim.Elements(ns + "input").Max(i => (int)i.Attribute("offset")) + 1;
                int vertexOffset = (int)prim.Elements(ns + "input").First(i => (string)i.Attribute("semantic") == "VERTEX").Attribute("offset");
                var p = prim.Element(ns + "p").Value.Split((char[])null, StringSplitOptions.RemoveEmptyEntries).Select(int.Parse).ToArray();
                var counts = prim.Element(ns + "vcount") is { } vc ? vc.Value.Split((char[])null, StringSplitOptions.RemoveEmptyEntries).Select(int.Parse).ToArray()
                                                                   : Enumerable.Repeat(3, p.Length / stride / 3).ToArray();
                int at = 0;
                foreach (int n in counts)
                {
                    for (int k = 1; k + 1 < n; k++) // a fan of triangles per polygon
                        tris.Add(new Tri(P(p[(at) * stride + vertexOffset]), P(p[(at + k) * stride + vertexOffset]), P(p[(at + k + 1) * stride + vertexOffset]), "body"));
                    at += n;
                }
            }
        }
        return tris;
    }
}

// ------------------------------------------------------------ tracing
// Outlines in design units: the top view fits 200 x 260, the side view 130 x 260 (like editor/ControllerLayouts.cs).
record TracedPart(string Comp, bool Top, List<List<PointF>> Loops, bool Estimated);
record Traced(string Id, string Hand, List<List<PointF>> Top, List<List<PointF>> Side, List<TracedPart> Parts);

static class Tracer
{
    static readonly string[] TopParts = { "primary", "secondary", "thumbstick", "touchpad", "thumbrest", "menu", "system" };
    static readonly string[] SideParts = { "trigger", "grip" };
    const int Grid = 640; // mask size along the longer side

    public static Traced Trace(string id, string hand, List<Tri> tris, bool hasThumbrest, bool topFromSide)
    {
        Vector3 Centroid(IEnumerable<Tri> ts)
        {
            var l = ts.ToList();
            if (l.Count == 0)
                return Vector3.Zero;
            var s = Vector3.Zero;
            foreach (var t in l)
                s += (t.A + t.B + t.C) / 3;
            return s / l.Count;
        }
        var top = tris.Where(t => t.Part is "primary" or "secondary" or "thumbstick").ToList();
        var topC = Centroid(top);
        var bodyC = Centroid(tris);
        // the buttons face: area-weighted normals of the top parts, pointing away from the body's middle
        var faceN = Vector3.Zero;
        foreach (var t in top)
        {
            var n = Vector3.Cross(t.B - t.A, t.C - t.A);
            if (Vector3.Dot(n, topC - bodyC) < 0)
                n = -n;
            faceN += n;
        }
        faceN = Vector3.Normalize(faceN);
        // up the handle: the models are in WebXR grip space, where the handle runs along Z; pointed toward the buttons
        var handleAxis = Vector3.Dot(topC - bodyC, Vector3.UnitZ) >= 0 ? Vector3.UnitZ : -Vector3.UnitZ;
        var up = Vector3.Normalize(handleAxis - Vector3.Dot(handleAxis, faceN) * faceN);
        // the side view keeps the handle upright and shows how the face leans
        var sideAcross = Vector3.Normalize(faceN - Vector3.Dot(faceN, handleAxis) * handleAxis);
        var across = Vector3.Normalize(Vector3.Cross(up, faceN));

        // top view: looking at the face; side view: looking from the side, front (the face's direction... trigger) on the left
        if (topFromSide) // across the controller (along Y), handle upright
        {
            faceN = -Vector3.UnitY;
            up = handleAxis;
            across = Vector3.Normalize(Vector3.Cross(up, faceN));
        }
        var topView = Project(tris, across, up, TopParts, Grid);
        var sideView = Project(tris, sideAcross, handleAxis, SideParts, Grid);
        // the stick sits toward the middle of the body (left on a right controller): flip so it does
        bool rightHand = hand == "right";
        float StickX(View v) => v.PartBoxes.TryGetValue("thumbstick", out var b) ? b.X + b.Width / 2 : 0;
        float FaceX(View v) => v.PartBoxes.TryGetValue("primary", out var b) ? b.X + b.Width / 2 : StickX(v);
        bool stickLeft = StickX(topView) < FaceX(topView);
        if (stickLeft != rightHand)
            topView.FlipX();
        // side view: the trigger at the front, on the left for the right hand (mirrored for the left)
        float TriggerX(View v) => v.PartBoxes.TryGetValue("trigger", out var b) ? b.X + b.Width / 2 : 0;
        float BodyX(View v) => v.Body.SelectMany(l => l).Average(p => p.X);
        bool triggerLeft = TriggerX(sideView) < BodyX(sideView);
        if (triggerLeft != rightHand)
            sideView.FlipX();

        var traced = new Traced(id, hand, null, null, new List<TracedPart>());
        var fitTop = Fit(topView, 200, 260);
        var fitSide = Fit(sideView, 130, 260);
        traced = traced with { Top = topView.Body.Select(l => l.Select(fitTop).ToList()).ToList(), Side = sideView.Body.Select(l => l.Select(fitSide).ToList()).ToList() };
        foreach (var (name, loops) in topView.Parts)
            traced.Parts.Add(new TracedPart(name, true, loops.Select(l => l.Select(fitTop).ToList()).ToList(), false));
        foreach (var (name, loops) in sideView.Parts)
            traced.Parts.Add(new TracedPart(name, false, loops.Select(l => l.Select(fitSide).ToList()).ToList(), false));
        Estimate(traced, hasThumbrest);
        return traced;
    }

    // Debug: the model seen along +X, -X, +Y, -Y, +Z, -Z (image up = the next axis), parts in blue.
    public static void AxisViews(List<Tri> tris, string file)
    {
        var axes = new[] { Vector3.UnitX, -Vector3.UnitX, Vector3.UnitY, -Vector3.UnitY, Vector3.UnitZ, -Vector3.UnitZ };
        var ups = new[] { Vector3.UnitY, Vector3.UnitY, -Vector3.UnitZ, -Vector3.UnitZ, Vector3.UnitY, Vector3.UnitY };
        using var bmp = new Bitmap(6 * 220, 260);
        using var g = Graphics.FromImage(bmp);
        g.Clear(Color.FromArgb(30, 34, 40));
        for (int i = 0; i < 6; i++)
        {
            var across = Vector3.Cross(ups[i], axes[i]);
            var v = Project(tris, across, ups[i], new[] { "primary", "secondary", "thumbstick", "trigger", "grip" }, 200);
            using var path = new GraphicsPath(FillMode.Alternate);
            foreach (var l in v.Body)
                path.AddPolygon(l.Select(p => new PointF(i * 220 + 10 + p.X, 30 + p.Y)).ToArray());
            g.FillPath(Brushes.SlateGray, path);
            foreach (var (name, loops) in v.Parts)
                foreach (var l in loops)
                    g.FillPolygon(name == "trigger" ? Brushes.Orange : Brushes.DodgerBlue, l.Select(p => new PointF(i * 220 + 10 + p.X, 30 + p.Y)).ToArray());
            g.DrawString(new[] { "+X", "-X", "+Y", "-Y", "+Z", "-Z" }[i], SystemFonts.DefaultFont, Brushes.White, i * 220 + 10, 8);
        }
        bmp.Save(file);
    }

    // The Vive wand model is one piece (no separate buttons), handle along Z with the crown toward -Z, the
    // trackpad's face toward +Y. Its parts are placed by proportion (checked against photos of real wands).
    // cx, cy: shares of the outline's width and height; w, h: shares of its width (so circles stay round).
    static readonly (string comp, float cx, float cy, float w, float h)[] ViveTop =
    {
        ("menu", 0.5f, 0.31f, 0.06f, 0.06f), ("thumbstick", 0.5f, 0.45f, 0.3f, 0.3f), ("system", 0.5f, 0.6f, 0.05f, 0.05f),
    };
    static readonly (string comp, float cx, float cy, float w, float h)[] ViveSide =
    {
        ("trigger", 0.6f, 0.37f, 0.14f, 0.22f), ("grip", 0.79f, 0.49f, 0.13f, 0.2f),
    };

    public static Traced TraceVive(string hand, List<Tri> tris)
    {
        var up = -Vector3.UnitZ;
        var top = Project(tris, Vector3.UnitX, up, Array.Empty<string>(), Grid);  // from the trackpad's side
        var side = Project(tris, Vector3.UnitY, up, Array.Empty<string>(), Grid); // trigger (-Y) on the left
        if (hand == "left")
        {
            top.FlipX();
            side.FlipX();
        }
        var fitTop = Fit(top, 200, 260);
        var fitSide = Fit(side, 130, 260);
        var t = new Traced("vive", hand, top.Body.Select(l => l.Select(fitTop).ToList()).ToList(),
                           side.Body.Select(l => l.Select(fitSide).ToList()).ToList(), new List<TracedPart>());
        void Place(List<List<PointF>> body, bool isTop, (string comp, float cx, float cy, float w, float h)[] parts)
        {
            var pts = body.SelectMany(l => l).ToList();
            float x0 = pts.Min(q => q.X), y0 = pts.Min(q => q.Y), bw = pts.Max(q => q.X) - x0, bh = pts.Max(q => q.Y) - y0;
            foreach (var (comp, cx, cy, w, h) in parts)
            {
                float x = isTop || hand == "right" ? cx : 1 - cx; // the side view's front is on the right for the left hand
                t.Parts.Add(new TracedPart(comp, isTop, new() { Pill(x0 + bw * x, y0 + bh * cy, bw * w, bw * h) }, true));
            }
        }
        Place(t.Top, true, ViveTop);
        Place(t.Side, false, ViveSide);
        return t;
    }

    // Parts most models don't have as meshes (they are part of the body): a fair guess from where the others are.
    static void Estimate(Traced t, bool hasThumbrest)
    {
        RectangleF Box(string comp)
        {
            var p = t.Parts.FirstOrDefault(x => x.Comp == comp && x.Top);
            if (p == null)
                return RectangleF.Empty;
            var pts = p.Loops.SelectMany(l => l).ToList();
            float x0 = pts.Min(q => q.X), y0 = pts.Min(q => q.Y), x1 = pts.Max(q => q.X), y1 = pts.Max(q => q.Y);
            return new RectangleF(x0, y0, x1 - x0, y1 - y0);
        }
        // A grip that is part of the body (the Index's squeeze sensor): along the front of the handle, below the trigger.
        var trigger = t.Parts.FirstOrDefault(x => x.Comp == "trigger" && !x.Top);
        if (trigger != null && !t.Parts.Any(x => x.Comp == "grip"))
        {
            var tp = trigger.Loops.SelectMany(l => l).ToList();
            var body = t.Side.SelectMany(l => l).ToList();
            float tx = tp.Average(q => q.X), ty = tp.Max(q => q.Y), bottom = body.Max(q => q.Y), width = body.Max(q => q.X) - body.Min(q => q.X);
            float gx = tx + (t.Hand == "right" ? 1 : -1) * width * 0.12f, gTop = ty + (bottom - ty) * 0.08f, gBottom = ty + (bottom - ty) * 0.75f;
            t.Parts.Add(new TracedPart("grip", false, new() { Pill(gx, (gTop + gBottom) / 2, width * 0.18f, gBottom - gTop) }, true));
        }
        var a = Box("primary");
        var b = Box("secondary");
        var stick = Box("thumbstick");
        if (a.IsEmpty || stick.IsEmpty)
            return;
        bool rightHand = t.Hand == "right";
        float outward = rightHand ? 1 : -1; // away from the stick
        float button = Math.Max(a.Width, a.Height);
        if (hasThumbrest && !t.Parts.Any(p => p.Comp == "thumbrest"))
        {
            float faceEdge = rightHand ? Math.Max(a.Right, b.IsEmpty ? a.Right : b.Right) : Math.Min(a.Left, b.IsEmpty ? a.Left : b.Left);
            float cx = faceEdge + outward * button * 0.75f, cy = b.IsEmpty ? a.Y + a.Height / 2 : (a.Y + a.Height / 2 + b.Y + b.Height / 2) / 2;
            t.Parts.Add(new TracedPart("thumbrest", true, new() { Pill(cx, cy, button * 0.5f, button * 1.5f) }, true));
        }
        string below = rightHand ? "system" : "menu"; // Meta / Pico button on the right, menu on the left
        if (!t.Parts.Any(p => p.Comp == below) && (t.Id.StartsWith("quest") || t.Id is "rifts" or "pico4"))
        {
            float cx = stick.X + stick.Width / 2, cy = stick.Bottom + stick.Height * 0.5f; // just below the stick
            t.Parts.Add(new TracedPart(below, true, new() { Pill(cx, cy, stick.Width * 0.32f, stick.Width * 0.32f) }, true));
        }
    }

    static List<PointF> Pill(float cx, float cy, float w, float h)
    {
        var pts = new List<PointF>();
        float r = Math.Min(w, h) / 2;
        for (int i = 0; i < 24; i++)
        {
            double a = i * Math.PI * 2 / 24;
            float x = (float)Math.Cos(a) * r, y = (float)Math.Sin(a) * r;
            y += Math.Sign(y) * (h / 2 - r); // stretch a circle into a pill
            x += Math.Sign(x) * (w / 2 - r);
            pts.Add(new PointF(cx + x, cy + y));
        }
        return pts;
    }

    // Grid pixels -> design units: the body fills the box, centred, with a small margin.
    static Func<PointF, PointF> Fit(View v, float boxW, float boxH)
    {
        var pts = v.Body.SelectMany(l => l).ToList();
        float x0 = pts.Min(p => p.X), y0 = pts.Min(p => p.Y), x1 = pts.Max(p => p.X), y1 = pts.Max(p => p.Y);
        float s = Math.Min((boxW - 12) / (x1 - x0), (boxH - 12) / (y1 - y0));
        float ox = (boxW - (x1 - x0) * s) / 2 - x0 * s, oy = (boxH - (y1 - y0) * s) / 2 - y0 * s;
        return p => new PointF((float)Math.Round(p.X * s + ox, 1), (float)Math.Round(p.Y * s + oy, 1));
    }

    class View
    {
        public List<List<PointF>> Body = new();
        public Dictionary<string, List<List<PointF>>> Parts = new();
        public Dictionary<string, RectangleF> PartBoxes = new();
        public int Width;

        public void FlipX()
        {
            void Flip(List<List<PointF>> loops)
            {
                foreach (var l in loops)
                    for (int i = 0; i < l.Count; i++)
                        l[i] = new PointF(Width - l[i].X, l[i].Y);
            }
            Flip(Body);
            foreach (var p in Parts.Values)
                Flip(p);
            foreach (var k in PartBoxes.Keys.ToList())
            {
                var b = PartBoxes[k];
                PartBoxes[k] = new RectangleF(Width - b.Right, b.Y, b.Width, b.Height);
            }
        }
    }

    // Orthographic projection onto (xAxis, upAxis), rasterised into masks, traced into loops.
    static View Project(List<Tri> tris, Vector3 xAxis, Vector3 upAxis, string[] partNames, int grid)
    {
        PointF P(Vector3 v) => new(Vector3.Dot(v, xAxis), -Vector3.Dot(v, upAxis));
        var all = tris.SelectMany(t => new[] { P(t.A), P(t.B), P(t.C) }).ToList();
        float x0 = all.Min(p => p.X), y0 = all.Min(p => p.Y), x1 = all.Max(p => p.X), y1 = all.Max(p => p.Y);
        float s = (grid - 8) / Math.Max(x1 - x0, y1 - y0);
        int w = (int)((x1 - x0) * s) + 8, h = (int)((y1 - y0) * s) + 8;
        PointF G(Vector3 v)
        {
            var p = P(v);
            return new PointF((p.X - x0) * s + 4, (p.Y - y0) * s + 4);
        }
        var view = new View { Width = w };
        view.Body = Contours.Trace(Mask(tris, G, w, h));
        foreach (string name in partNames)
        {
            var mine = tris.Where(t => t.Part == name).ToList();
            if (mine.Count == 0)
                continue;
            var loops = Contours.Trace(Mask(mine, G, w, h));
            if (loops.Count == 0)
                continue;
            view.Parts[name] = loops;
            var pts = loops.SelectMany(l => l).ToList();
            view.PartBoxes[name] = RectangleF.FromLTRB(pts.Min(p => p.X), pts.Min(p => p.Y), pts.Max(p => p.X), pts.Max(p => p.Y));
        }
        return view;
    }

    static bool[,] Mask(List<Tri> tris, Func<Vector3, PointF> toGrid, int w, int h)
    {
        using var bmp = new Bitmap(w, h);
        using (var g = Graphics.FromImage(bmp))
        {
            g.Clear(Color.Black);
            g.SmoothingMode = SmoothingMode.None;
            foreach (var t in tris)
                g.FillPolygon(Brushes.White, new[] { toGrid(t.A), toGrid(t.B), toGrid(t.C) });
        }
        var m = new bool[w, h];
        var data = bmp.LockBits(new Rectangle(0, 0, w, h), System.Drawing.Imaging.ImageLockMode.ReadOnly, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        var row = new byte[w * 4];
        for (int y = 0; y < h; y++)
        {
            System.Runtime.InteropServices.Marshal.Copy(data.Scan0 + y * data.Stride, row, 0, w * 4);
            for (int x = 0; x < w; x++)
                m[x, y] = row[x * 4] > 127;
        }
        bmp.UnlockBits(data);
        return m;
    }
}

// ------------------------------------------------------------ an outline from a photo (dark controller on a lighter floor)
static class PhotoTracer
{
    // Where the wand's parts are, as shares of the wand's width and height in the photo (measured by eye).
    static readonly (string comp, float cx, float cy, float r)[] ViveParts =
    {
        ("menu", 0.5f, 0.2f, 0.05f), ("thumbstick", 0.5f, 0.36f, 0.22f), ("system", 0.5f, 0.55f, 0.045f),
    };
    // The same parts on the outline from the back photo (its crown is taller): carried over from the front photo
    // along the handle (just under the crown, then down it).
    static readonly (string comp, float cx, float cy, float r)[] ViveFromBack =
    {
        ("menu", 0.5f, 0.37f, 0.045f), ("thumbstick", 0.5f, 0.53f, 0.18f), ("system", 0.5f, 0.72f, 0.04f),
    };

    // Each point moved to the average of its neighbours (a closed loop), then thinned again.
    static List<PointF> Smooth(List<PointF> loop, int k)
    {
        int n = loop.Count;
        var o = new List<PointF>(n);
        for (int i = 0; i < n; i++)
        {
            float x = 0, y = 0;
            for (int d = -k; d <= k; d++)
            {
                var p = loop[((i + d) % n + n) % n];
                x += p.X;
                y += p.Y;
            }
            o.Add(new PointF(x / (2 * k + 1), y / (2 * k + 1)));
        }
        return o.Where((p, i) => i % 2 == 0).ToList();
    }

    static bool[,] Dilate(bool[,] m, int r) => Morph(m, r, true);
    static bool[,] Erode(bool[,] m, int r) => Morph(m, r, false);
    static bool[,] Morph(bool[,] m, int r, bool grow)
    {
        int w = m.GetLength(0), h = m.GetLength(1);
        var o = new bool[w, h];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                bool any = false, all = true;
                for (int dy = -r; dy <= r; dy++)
                    for (int dx = -r; dx <= r; dx++)
                    {
                        if (dx * dx + dy * dy > r * r)
                            continue;
                        int xx = x + dx, yy = y + dy;
                        bool v = xx >= 0 && yy >= 0 && xx < w && yy < h && m[xx, yy];
                        any |= v;
                        all &= v;
                    }
                o[x, y] = grow ? any : all;
            }
        return o;
    }

    // The photo's dark shapes in its right half (the right-hand wand), smoothed and made symmetric, as loops.
    static List<List<PointF>> TraceAt(Bitmap bmp, float threshold)
    {
        int w = bmp.Width, h = bmp.Height, x0 = w / 2;
        var mask = new bool[w - x0, h];
        for (int y = 0; y < h; y++)
            for (int x = x0; x < w; x++)
            {
                var c = bmp.GetPixel(x, y);
                mask[x - x0, y] = c.R * 0.3f + c.G * 0.59f + c.B * 0.11f < threshold;
            }
        // smooth the floor's speckle off the edge, and close the wand's lighter reflections
        mask = Erode(Dilate(mask, 3), 3);
        mask = Dilate(Erode(mask, 2), 2);
        // the wand is symmetric: keep only what is dark on both sides of its middle (drops floor stuck to one side)
        var first = Contours.Trace(mask);
        var big = first.OrderByDescending(l => l.Count).First();
        float cx = (big.Min(p => p.X) + big.Max(p => p.X)) / 2;
        int mw = mask.GetLength(0);
        var sym = new bool[mw, h];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < mw; x++)
            {
                int mx = (int)Math.Round(2 * cx - x - 1);
                sym[x, y] = mask[x, y] && mx >= 0 && mx < mw && mask[mx, y];
            }
        return Contours.Trace(sym).Select(l => Smooth(l, 3)).ToList(); // pixel steps smoothed
    }

    // fromBack: the photo shows the back: the outline is mirrored to be the front.
    public static Traced Trace(string id, string hand, string file, bool fromBack)
    {
        using var bmp = new Bitmap(file);
        // Darker than this counts as the wand. On the back photo the grey handle is lighter than the crown (the outline
        // needs a lighter limit), while the shadow inside the crown's ring needs a darker one to stay an opening.
        var loops = TraceAt(bmp, fromBack ? 118 : 90);
        var holeLoops = fromBack ? TraceAt(bmp, 90) : loops;
        float Area(List<PointF> l)
        {
            float a = 0;
            for (int i = 0; i < l.Count; i++)
                a += l[i].X * l[(i + 1) % l.Count].Y - l[(i + 1) % l.Count].X * l[i].Y;
            return Math.Abs(a) / 2;
        }
        var outer = loops.OrderByDescending(Area).First();
        var ob = RectangleF.FromLTRB(outer.Min(p => p.X), outer.Min(p => p.Y), outer.Max(p => p.X), outer.Max(p => p.Y));
        var body = new List<List<PointF>> { outer };
        var holeOuter = holeLoops.OrderByDescending(Area).First();
        body.AddRange(holeLoops.Where(l => l != holeOuter && l != outer && Area(l) > Area(outer) * 0.005f && l.All(p => ob.Contains(p)) &&
                                           l.Average(p => p.Y) < ob.Y + ob.Height * 0.4f)); // the opening in the tracking crown
        float s = Math.Min((200 - 12) / ob.Width, (260 - 12) / ob.Height);
        float ox = (200 - ob.Width * s) / 2 - ob.X * s, oy = (260 - ob.Height * s) / 2 - ob.Y * s;
        PointF Fit(PointF p)
        {
            float x = p.X * s + ox;
            if (fromBack)
                x = 200 - x;
            return new((float)Math.Round(x, 1), (float)Math.Round(p.Y * s + oy, 1));
        }
        var traced = new Traced(id, hand, body.Select(l => l.Select(Fit).ToList()).ToList(), new List<List<PointF>>(), new List<TracedPart>());
        foreach (var (comp, cx, cy, r) in fromBack ? ViveFromBack : ViveParts)
        {
            var c = Fit(new PointF(ob.X + ob.Width * cx, ob.Y + ob.Height * cy));
            float rad = ob.Width * r * s;
            var circle = Enumerable.Range(0, 32).Select(i => new PointF(c.X + rad * MathF.Cos(i * MathF.PI / 16), c.Y + rad * MathF.Sin(i * MathF.PI / 16))).ToList();
            traced.Parts.Add(new TracedPart(comp, true, new() { circle }, false));
        }
        return traced;
    }
}

// ------------------------------------------------------------ mask -> closed outlines (marching squares), simplified
static class Contours
{
    public static List<List<PointF>> Trace(bool[,] mask)
    {
        int w = mask.GetLength(0), h = mask.GetLength(1);
        bool At(int x, int y) => x >= 0 && y >= 0 && x < w && y < h && mask[x, y];
        // edges between a filled and an empty pixel, as unit segments on the pixel grid, linked into loops
        var next = new Dictionary<(int, int), List<(int, int)>>();
        void Edge((int, int) a, (int, int) b)
        {
            if (!next.TryGetValue(a, out var l))
                next[a] = l = new();
            l.Add(b);
        }
        for (int y = -1; y < h; y++)
            for (int x = -1; x < w; x++)
            {
                // a filled pixel's border, walked so the filled side is on the right
                if (!At(x, y))
                    continue;
                if (!At(x, y - 1))
                    Edge((x, y), (x + 1, y));
                if (!At(x + 1, y))
                    Edge((x + 1, y), (x + 1, y + 1));
                if (!At(x, y + 1))
                    Edge((x + 1, y + 1), (x, y + 1));
                if (!At(x - 1, y))
                    Edge((x, y + 1), (x, y));
            }
        var loops = new List<List<PointF>>();
        while (next.Count > 0)
        {
            var start = next.Keys.First();
            var loop = new List<PointF>();
            var at = start;
            while (next.TryGetValue(at, out var outs))
            {
                var to = outs[0];
                outs.RemoveAt(0);
                if (outs.Count == 0)
                    next.Remove(at);
                loop.Add(new PointF(at.Item1, at.Item2));
                at = to;
                if (at == start)
                    break;
            }
            if (loop.Count >= 12) // specks are dropped
                loops.Add(Simplify(loop, 1.2f));
        }
        return loops.Where(l => l.Count >= 3).ToList();
    }

    // Douglas-Peucker on a closed loop.
    static List<PointF> Simplify(List<PointF> loop, float tolerance)
    {
        int far = 0;
        float best = -1;
        for (int i = 1; i < loop.Count; i++)
        {
            float d = Dist(loop[0], loop[i]);
            if (d > best)
            {
                best = d;
                far = i;
            }
        }
        var a = Dp(loop.GetRange(0, far + 1), tolerance);
        var b = Dp(loop.GetRange(far, loop.Count - far).Append(loop[0]).ToList(), tolerance);
        a.RemoveAt(a.Count - 1);
        b.RemoveAt(b.Count - 1);
        return a.Concat(b).ToList();
    }

    static List<PointF> Dp(List<PointF> pts, float tol)
    {
        if (pts.Count < 3)
            return pts.ToList();
        int index = 0;
        float max = 0;
        for (int i = 1; i < pts.Count - 1; i++)
        {
            float d = LineDist(pts[i], pts[0], pts[^1]);
            if (d > max)
            {
                max = d;
                index = i;
            }
        }
        if (max <= tol)
            return new List<PointF> { pts[0], pts[^1] };
        var left = Dp(pts.GetRange(0, index + 1), tol);
        var right = Dp(pts.GetRange(index, pts.Count - index), tol);
        left.RemoveAt(left.Count - 1);
        return left.Concat(right).ToList();
    }

    static float Dist(PointF a, PointF b) => MathF.Sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y));
    static float LineDist(PointF p, PointF a, PointF b)
    {
        float len = Dist(a, b);
        if (len < 1e-6f)
            return Dist(p, a);
        return MathF.Abs((b.X - a.X) * (a.Y - p.Y) - (a.X - p.X) * (b.Y - a.Y)) / len;
    }
}

// ------------------------------------------------------------ pictures for checking by eye
static class Preview
{
    public static void Save(Traced t, string file)
    {
        using var bmp = new Bitmap(700, 560);
        using var g = Graphics.FromImage(bmp);
        g.Clear(Color.FromArgb(30, 34, 40));
        g.SmoothingMode = SmoothingMode.AntiAlias;
        void Draw(List<List<PointF>> loops, float ox, Brush fill, Pen edge)
        {
            if (loops.Count == 0)
                return;
            using var path = new GraphicsPath(FillMode.Alternate);
            foreach (var l in loops)
                path.AddPolygon(l.Select(p => new PointF(ox + p.X * 2, 20 + p.Y * 2)).ToArray());
            g.FillPath(fill, path);
            g.DrawPath(edge, path);
        }
        using var body = new SolidBrush(Color.FromArgb(70, 78, 90));
        using var part = new SolidBrush(Color.FromArgb(60, 140, 255));
        using var guess = new SolidBrush(Color.FromArgb(240, 140, 30));
        using var edge = new Pen(Color.FromArgb(150, 160, 175), 1.5f);
        Draw(t.Top, 20, body, edge);
        Draw(t.Side, 440, body, edge);
        foreach (var p in t.Parts)
            Draw(p.Loops, p.Top ? 20 : 440, p.Estimated ? guess : part, edge);
        g.DrawString($"{t.Id} {t.Hand}  (orange = estimated)", SystemFonts.DefaultFont, Brushes.White, 10, 540);
        bmp.Save(file);
    }
}

// ------------------------------------------------------------ generated code
static class Emit
{
    static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
    const string Header = "Generated by tools/ControllerSilhouettes from the WebXR input profile models\n" +
                          "(github.com/immersive-web/webxr-input-profiles, MIT licence, Copyright (c) 2019 Amazon), and the Vive wand from\n" +
                          "\"HTC Vive Controller\" by iShoNz (sketchfab.com/Jonabafor), CC BY 4.0. Outlines only. Do not edit.";

    static string Floats(List<PointF> loop) => string.Join(", ", loop.Select(p => $"{p.X.ToString("0.0##", Inv)}f, {p.Y.ToString("0.0##", Inv)}f"));

    public static void CSharp(List<Traced> all, string file)
    {
        var sb = new StringBuilder();
        foreach (var line in Header.Split('\n'))
            sb.AppendLine("// " + line);
        sb.AppendLine("namespace OpenXRCompanionEditor;\n");
        sb.AppendLine("public static partial class ControllerSilhouettes\n{");
        sb.AppendLine("    static readonly Dictionary<string, Silhouette> Data = new()\n    {");
        string Loops(List<List<PointF>> loops) => "new float[][] { " + string.Join(", ", loops.Select(l => "new float[] { " + Floats(l) + " }")) + " }";
        foreach (var t in all)
        {
            sb.AppendLine($"        [\"{t.Id}/{t.Hand}\"] = new Silhouette({Loops(t.Top)},");
            sb.AppendLine($"            {Loops(t.Side)},");
            sb.AppendLine("            new SilhouettePart[] {");
            foreach (var p in t.Parts)
                sb.AppendLine($"                new(\"{p.Comp}\", {(p.Top ? "ControllerView.Top" : "ControllerView.Side")}, {Loops(p.Loops)}, {(p.Estimated ? "true" : "false")}),");
            sb.AppendLine("            }),");
        }
        sb.AppendLine("    };\n}");
        File.WriteAllText(file, sb.ToString());
    }

    public static void Cpp(List<Traced> all, string file)
    {
        var sb = new StringBuilder();
        foreach (var line in Header.Split('\n'))
            sb.AppendLine("// " + line);
        sb.AppendLine("// Included once by controller_silhouettes.cpp. Top view only (the layer's pictures).");
        int n = 0;
        var tables = new List<string>();
        foreach (var t in all)
        {
            string Loop(List<PointF> l)
            {
                string name = $"kL{n++}";
                sb.AppendLine($"static const float {name}[] = {{ {Floats(l)} }};");
                return $"{{ {name}, {l.Count} }}";
            }
            string body = string.Join(", ", t.Top.Select(Loop));
            var parts = new List<string>();
            foreach (var p in t.Parts.Where(p => p.Top))
            {
                string loops = string.Join(", ", p.Loops.Select(Loop));
                string partName = $"kP{n++}";
                sb.AppendLine($"static const SilLoop {partName}[] = {{ {loops} }};");
                parts.Add($"{{ \"{p.Comp}\", {partName}, {p.Loops.Count} }}");
            }
            string bodyName = $"kB{n++}", partsName = $"kS{n++}";
            sb.AppendLine($"static const SilLoop {bodyName}[] = {{ {body} }};");
            sb.AppendLine($"static const SilPart {partsName}[] = {{ {string.Join(", ", parts)} }};");
            tables.Add($"{{ \"{t.Id}\", {(t.Hand == "left" ? 0 : 1)}, {bodyName}, {t.Top.Count}, {partsName}, {parts.Count} }}");
        }
        sb.AppendLine("static const Silhouette kSilhouettes[] = {");
        foreach (var tb in tables)
            sb.AppendLine("    " + tb + ",");
        sb.AppendLine("};");
        File.WriteAllText(file, sb.ToString());
    }
}
