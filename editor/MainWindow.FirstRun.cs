using Avalonia.Controls;

namespace OverviewPrototype;

// The first start (only a normal start on the real data folder): which controllers you use, then the offer of the
// main tour. The same app-wide flag files as WinForms (controller_chosen, tutorial_seen), so a user of either editor is
// asked once.
public partial class MainWindow
{
    static string Flag(string name) => Path.Combine(ProfileStore.RealDataDir, name);

    async Task FirstRunAsync()
    {
        if (!File.Exists(Flag("controller_chosen")))
        {
            if (!File.Exists(SetupCard.FilePath))
                SetupCard.Pending = true; // a new user: the first game starts with the setup of positions
            if (await ControllersPage.ChoosePreset(this, firstRun: true))
                Shell.Refresh();
        }
        if (File.Exists(Flag("tutorial_seen")))
            return;
        int pick = await Dialogs.Choose(this, "First time here?",
            "Welcome to OpenXR Companion!\n\nTake a short guided tour? You'll make a radial wheel, put something on it and pick the controller button that opens it in VR, then see every page. Anything you make can be thrown away at the end.\n\n" +
            "Later: the Tutorials page in the left bar has this tour, short how-tos, and a tour of each page.",
            "Start the tour", "Don't ask again", "Not now");
        void Seen() { try { File.WriteAllText(Flag("tutorial_seen"), DateTime.Now.ToString("s")); } catch { } }
        if (pick == 0) { Seen(); StartTutorial("main"); }
        else if (pick == 1) Seen(); // "Not now" asks again next start
    }
}
