namespace OpenXRCompanionEditor;

// How many bindings a part has: switched on, and switched off.
public readonly record struct PartUse(int On, int Off);

// One line in a part's tag: how it's used ("Press", "Hold", "Up") and what it does ("Space"); Layer when it only
// works in a layer; IsUnused for a way of using the part that does nothing yet (shown greyed, "unused").
public readonly record struct TagLine(string When, string Does, string Layer = null, bool IsUnused = false);

