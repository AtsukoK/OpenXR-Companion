using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using SkiaSharp;

namespace OverviewPrototype;

// Plays an animated GIF (Avalonia's Image shows only the first frame). Frames are decoded once with SkiaSharp, which
// Avalonia already ships, then shown on a timer while the control is on screen. Also shows PNG / JPG (one frame).
public class GifImage : Control
{
    readonly List<(Bitmap bmp, int ms)> frames = new();
    readonly DispatcherTimer timer = new();
    int at;

    public GifImage(Stream data)
    {
        try { Decode(data); } catch { }
        timer.Tick += (s, e) => Next();
        AttachedToVisualTree += (s, e) => { if (frames.Count > 1) { timer.Interval = TimeSpan.FromMilliseconds(frames[at].ms); timer.Start(); } };
        DetachedFromVisualTree += (s, e) => timer.Stop();
    }

    public bool HasPicture => frames.Count > 0;
    public PixelSize PictureSize => frames.Count > 0 ? frames[0].bmp.PixelSize : default;

    void Decode(Stream data)
    {
        using var ms = new MemoryStream();
        data.CopyTo(ms);
        ms.Position = 0;
        using var codec = SKCodec.Create(ms);
        if (codec == null) return;
        var info = new SKImageInfo(codec.Info.Width, codec.Info.Height, SKColorType.Bgra8888, SKAlphaType.Premul);
        int count = Math.Max(1, codec.FrameCount);
        var decoded = new SKBitmap[count];
        for (int i = 0; i < count; i++)
        {
            var bmp = new SKBitmap(info);
            int prior = i < codec.FrameInfo.Length ? codec.FrameInfo[i].RequiredFrame : -1;
            if (prior >= 0 && decoded[prior] != null)
                decoded[prior].CopyTo(bmp);
            else
                bmp.Erase(SKColors.Transparent);
            codec.GetPixels(info, bmp.GetPixels(), new SKCodecOptions(i, prior));
            decoded[i] = bmp;
            int ms2 = i < codec.FrameInfo.Length ? codec.FrameInfo[i].Duration : 0;
            var ab = new Bitmap(PixelFormat.Bgra8888, AlphaFormat.Premul, bmp.GetPixels(), new PixelSize(info.Width, info.Height), new Vector(96, 96), bmp.RowBytes);
            frames.Add((ab, ms2 <= 10 ? 100 : ms2)); // like browsers: 0-10 ms frames play at 100 ms
        }
        foreach (var b in decoded) b?.Dispose();
    }

    void Next()
    {
        at = (at + 1) % frames.Count;
        timer.Interval = TimeSpan.FromMilliseconds(frames[at].ms);
        InvalidateVisual();
    }

    protected override Size MeasureOverride(Size available)
    {
        if (frames.Count == 0) return default;
        var s = frames[0].bmp.Size;
        double w = double.IsInfinity(available.Width) ? s.Width : Math.Min(available.Width, s.Width);
        return new Size(w, w * s.Height / s.Width);
    }

    public override void Render(DrawingContext ctx)
    {
        if (frames.Count == 0) return;
        var b = frames[at].bmp;
        ctx.DrawImage(b, new Rect(b.Size), new Rect(Bounds.Size));
    }
}
