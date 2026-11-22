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
    // topTurnDeg: an extra turn of the top view, for controllers whose buttons face along the handle (the head and
    // strap are all you see from above, so there is no handle to stand upright).
    static readonly (string profile, string id, bool hasThumbrest, float topTurnDeg)[] Profiles =
    {
        ("meta-quest-touch-plus", "quest3", true, 0),
        ("oculus-touch-v3", "quest2", true, 0),
        ("oculus-touch-v2", "rifts", true, 0),
        ("meta-quest-touch-pro", "questpro", true, 0),
        ("pico-4", "pico4", false, 0),
        ("valve-index", "index", false, TopTurnIndex),
        ("microsoft-mixed-reality", "wmr", false, 0),
        ("hp-mixed-reality", "reverbg2", false, 0),
    };
    static float TopTurnIndex => float.TryParse(Environment.GetEnvironmentVariable("SIL_INDEX_TURN"), out var t) ? t : -50;

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
            Tracer.AxisViews(Glb.Load(Path.Combine(models, axesOf, "right.glb")), Path.Combine(preview, "axes_" + axesOf + ".png"));
            return 0;
        }
        Directory.CreateDirectory(preview);
        var results = new List<Traced>();
        foreach (var (profile, id, hasThumbrest, topTurnDeg) in Profiles)
            foreach (string hand in new[] { "left", "right" })
            {
                string file = Path.Combine(models, profile, hand + ".glb");
                if (!File.Exists(file))
                {
                    Console.WriteLine($"skipped {profile}/{hand}: no model");
                    continue;
                }
                var tris = Glb.Load(file);
                var traced = Tracer.Trace(id, hand, tris, hasThumbrest, hand == "right" ? topTurnDeg : -topTurnDeg);
                results.Add(traced);
                Preview.Save(traced, Path.Combine(preview, $"{id}_{hand}.png"));
                Console.WriteLine($"{id}/{hand}: {tris.Count} triangles, parts {string.Join(", ", traced.Parts.Select(p => p.Comp + (p.Estimated ? "*" : "")))}");
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

// ------------------------------------------------------------ tracing
// Outlines in design units: the top view fits 200 x 260, the side view 130 x 260 (like editor/ControllerLayouts.cs).
record TracedPart(string Comp, bool Top, List<List<PointF>> Loops, bool Estimated);
record Traced(string Id, string Hand, List<List<PointF>> Top, List<List<PointF>> Side, List<TracedPart> Parts);

static class Tracer
{
    static readonly string[] TopParts = { "primary", "secondary", "thumbstick", "touchpad", "thumbrest", "menu", "system" };
    static readonly string[] SideParts = { "trigger", "grip" };
    const int Grid = 640; // mask size along the longer side

    public static Traced Trace(string id, string hand, List<Tri> tris, bool hasThumbrest, float topTurnDeg)
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
        if (topTurnDeg != 0) // turn the top view's axes about the face
        {
            var turn = Quaternion.CreateFromAxisAngle(faceN, topTurnDeg * MathF.PI / 180);
            across = Vector3.Transform(across, turn);
            up = Vector3.Transform(up, turn);
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
                          "(github.com/immersive-web/webxr-input-profiles, MIT licence, Copyright (c) 2019 Amazon). Outlines only. Do not edit.";

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
