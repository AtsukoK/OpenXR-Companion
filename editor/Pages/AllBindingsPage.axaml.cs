using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.VisualTree;
using OpenXRCompanionEditor;

namespace OverviewPrototype;

public partial class AllBindingsPage : UserControl
{
    static int sort;             // kept while the editor runs, like the WinForms page
    static string show = "all";

    readonly ProfileModel p = Shell.Store.Profile;
    readonly TextBox search;
    readonly ListBox list;

    public AllBindingsPage() : this(null) { }

    public AllBindingsPage(string query)
    {
        AvaloniaXamlLoader.Load(this);
        if (query?.StartsWith("@") == true) { show = query[1..]; query = null; } // bindings:@layers opens that view
        search = this.FindControl<TextBox>("Search")!;
        list = this.FindControl<ListBox>("List")!;
        var sortBox = this.FindControl<ComboBox>("SortBox")!;
        var showBox = this.FindControl<ComboBox>("ShowBox")!;
        sortBox.SelectedIndex = sort;
        showBox.SelectedItem = showBox.Items.OfType<ComboBoxItem>().FirstOrDefault(i => (string)i.Tag == show);
        search.Text = query ?? "";

        search.TextChanged += (s, e) => Fill();
        sortBox.SelectionChanged += (s, e) => { sort = Math.Max(0, sortBox.SelectedIndex); Fill(); };
        showBox.SelectionChanged += (s, e) => { show = (showBox.SelectedItem as ComboBoxItem)?.Tag as string ?? "all"; Fill(); };
        this.FindControl<Button>("AddButton")!.Click += (s, e) =>
        {
            var b = new BindingModel { Name = "New binding", Input = "right.primary", Gesture = GestureKind.Press,
                                       Action = new ActionModel { Type = ActionType.Key, Keys = "F1" } };
            p.Bindings.Add(b);
            Shell.Store.MarkDirty();
            Shell.Go("binding:" + p.Bindings.IndexOf(b));
        };
        // a click on a row (not on its buttons) opens it
        list.AddHandler(PointerReleasedEvent, (s, e) =>
        {
            if (e.Source is Control c && c.FindAncestorOfType<Button>(true) == null)
                switch (c.DataContext)
                {
                    case BindingRow br: Shell.Go("binding:" + p.Bindings.IndexOf(br.Binding)); break;
                    case ItemRow ir: Shell.Go(ir.Open); break;
                    case SettingsHitRow sh: SettingsPageView.Highlight = search.Text?.Trim(); Shell.Go("settings:" + sh.Key); break;
                }
        }, RoutingStrategies.Bubble, true);
        Fill();
    }

    void Fill() => list.ItemsSource = AllBindings.Rows(p, search.Text, sort, show);

    void Changed()
    {
        Shell.Store.MarkDirty();
        Fill();
    }

    void Toggle_Click(object sender, RoutedEventArgs e)
    {
        switch ((sender as Control)?.DataContext)
        {
            case BindingRow br: br.Binding.Enabled = !br.Binding.Enabled; break;
            case ItemRow ir: ir.Item.Enabled = !ir.Item.Enabled; break;
            default: return;
        }
        Changed();
    }

    void Duplicate_Click(object sender, RoutedEventArgs e)
    {
        switch ((sender as Control)?.DataContext)
        {
            case BindingRow br:
                var c = ItemLists.Clone(br.Binding);
                c.Name += " copy";
                c.Enabled = false; // two of the same would both fire
                p.Bindings.Insert(p.Bindings.IndexOf(br.Binding) + 1, c);
                break;
            case ItemRow ir:
                if (ir.Owner.Count >= 16)
                { Shell.SetStatus("That holds at most 16.", true); return; }
                ir.Owner.Insert(ir.Owner.IndexOf(ir.Item) + 1, ItemLists.Clone(ir.Item));
                break;
            default: return;
        }
        Changed();
    }

    async void Delete_Click(object sender, RoutedEventArgs e)
    {
        switch ((sender as Control)?.DataContext)
        {
            case BindingRow br:
                if (await Dialogs.Confirm(this, "Delete binding", $"Delete '{br.Binding.Name}'?", "Delete"))
                { p.Bindings.Remove(br.Binding); Changed(); }
                break;
            case ItemRow ir:
                if (await Dialogs.Confirm(this, "Delete", $"Delete '{ir.Label}'?", "Delete"))
                { ir.Owner.Remove(ir.Item); Changed(); }
                break;
        }
    }
}
