namespace OpenXRCompanionEditor;

// Real controller outlines, traced from the WebXR input profile models (MIT licence, (c) 2019 Amazon) by
// tools/ControllerSilhouettes into ControllerSilhouettes.g.cs. Design units as in ControllerLayouts: the top view
// fits 200 x 260, the side view 130 x 260. Left and right are traced separately (no mirroring needed).
// A loop is x, y pairs; loops are filled together with alternate fill, so inner loops are holes (rings, straps).
public sealed record Silhouette(float[][] Top, float[][] Side, SilhouettePart[] Parts);

// Comp: the part's input name as in ControllerLayouts ("primary", "trigger"...), or "touchpad" / "system".
// Estimated: not a separate part in the model (thumbrest, menu / Meta button): placed from where the others are.
public sealed record SilhouettePart(string Comp, ControllerView View, float[][] Loops, bool Estimated);

public static partial class ControllerSilhouettes
{
    // The outline of a layout for one hand (0 left, 1 right), or null when there is no model (then the layout's own shapes are drawn).
    public static Silhouette For(string layoutId, int hand)
        => Data.TryGetValue(layoutId + "/" + (hand == 0 ? "left" : "right"), out var s) ? s : null;

    // The traced part for a layout part, or null. The Index trackpad is the model's "touchpad".
    public static SilhouettePart PartFor(Silhouette s, ControllerPart part)
    {
        string comp = part.Comp == "trackpad" ? "touchpad" : part.Comp;
        return s.Parts.FirstOrDefault(p => p.Comp == comp && p.View == part.View);
    }
}
