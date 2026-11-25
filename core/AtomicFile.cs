using System.IO;

// Writes a whole file safely: a temp file next to it, then swapped in, so a running game
// (which re-reads the profile when it changes) never reads half a file.
public static class AtomicFile
{
    public static void WriteAllText(string path, string text)
    {
        string tmp = path + ".tmp";
        File.WriteAllText(tmp, text);
        File.Move(tmp, path, true);
    }
}
