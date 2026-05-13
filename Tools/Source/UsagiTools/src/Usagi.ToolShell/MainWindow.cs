using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Usagi.ToolCore.Assets;
using Usagi.ToolCore.Entities;
using Usagi.ToolCore.Projects;

namespace Usagi.ToolShell;

public sealed class MainWindow : Window
{
    private readonly EntityHierarchyLoader _loader = new();
    private readonly ListBox _assetList = new();
    private readonly TreeView _hierarchyTree = new();
    private readonly TextBlock _properties = new();
    private readonly TextBox _output = new();

    private UsagiProject? _project;
    private EntityDocument? _document;

    public MainWindow()
    {
        Title = "Usagi Tools";
        Width = 1280;
        Height = 760;
        MinWidth = 900;
        MinHeight = 560;
        Content = BuildLayout();

        Opened += (_, _) => InitializeProject();
        _assetList.SelectionChanged += (_, _) => LoadSelectedAsset();
        _hierarchyTree.SelectionChanged += (_, _) => ShowSelectedEntity();
    }

    private Control BuildLayout()
    {
        var root = new Grid
        {
            RowDefinitions = new RowDefinitions("*,170"),
            ColumnDefinitions = new ColumnDefinitions("280,*,360")
        };

        root.Children.Add(Wrap("Assets", _assetList, 0, 0));
        root.Children.Add(Wrap("Hierarchy", _hierarchyTree, 1, 0));

        _properties.TextWrapping = TextWrapping.Wrap;
        _properties.FontFamily = FontFamily.Parse("Consolas");
        root.Children.Add(Wrap("Properties", new ScrollViewer { Content = _properties }, 2, 0));

        _output.IsReadOnly = true;
        _output.AcceptsReturn = true;
        _output.FontFamily = FontFamily.Parse("Consolas");
        _output.TextWrapping = TextWrapping.NoWrap;

        var outputPanel = Wrap("Output", _output, 0, 1);
        Grid.SetColumnSpan(outputPanel, 3);
        root.Children.Add(outputPanel);

        return root;
    }

    private static Border Wrap(string title, Control content, int column, int row)
    {
        var panel = new DockPanel();
        panel.Children.Add(new TextBlock
        {
            Text = title,
            FontWeight = FontWeight.SemiBold,
            Margin = new Avalonia.Thickness(8, 6),
            VerticalAlignment = VerticalAlignment.Center
        });
        DockPanel.SetDock(panel.Children[0], Dock.Top);
        panel.Children.Add(content);

        var border = new Border
        {
            BorderBrush = Brushes.Gray,
            BorderThickness = new Avalonia.Thickness(0.5),
            Child = panel
        };

        Grid.SetColumn(border, column);
        Grid.SetRow(border, row);
        return border;
    }

    private void InitializeProject()
    {
        try
        {
            _project = UsagiProjectLocator.Locate(AppContext.BaseDirectory);
            if (!Directory.Exists(_project.EntitiesPath))
            {
                WriteOutput($"No entity directory found: {_project.EntitiesPath}");
                return;
            }

            var assets = AssetInventory.EnumerateEntityAssets(_project.EntitiesPath);
            _assetList.ItemsSource = assets;
            WriteOutput($"Project: {_project.RootPath}{Environment.NewLine}Entity assets: {assets.Count}");

            var standardRoot = assets.FirstOrDefault(asset => asset.Name.Equals("StandardRoot", StringComparison.OrdinalIgnoreCase));
            _assetList.SelectedItem = standardRoot ?? assets.FirstOrDefault();
        }
        catch (Exception ex)
        {
            WriteOutput(ex.Message);
        }
    }

    private void LoadSelectedAsset()
    {
        if (_assetList.SelectedItem is not AssetItem asset)
        {
            return;
        }

        try
        {
            _document = _loader.LoadFile(asset.FullPath);
            var rootItem = new EntityTreeItem(_document.Root);
            _hierarchyTree.ItemsSource = new[] { BuildTreeItem(rootItem) };
            _properties.Text = Describe(_document.Root);
            WriteOutput($"Loaded {asset.RelativePath}{Environment.NewLine}{string.Join(Environment.NewLine, _document.Diagnostics)}");
        }
        catch (Exception ex)
        {
            _hierarchyTree.ItemsSource = Array.Empty<TreeViewItem>();
            _properties.Text = string.Empty;
            WriteOutput($"Failed to load {asset.RelativePath}:{Environment.NewLine}{ex.Message}");
        }
    }

    private void ShowSelectedEntity()
    {
        if (_hierarchyTree.SelectedItem is TreeViewItem { Tag: EntityTreeItem item })
        {
            _properties.Text = Describe(item.Node);
        }
    }

    private static TreeViewItem BuildTreeItem(EntityTreeItem item)
    {
        return new TreeViewItem
        {
            Header = item.Node.DisplayName,
            Tag = item,
            ItemsSource = item.Children.Select(BuildTreeItem).ToArray()
        };
    }

    private static string Describe(EntityNode node)
    {
        var lines = new List<string>
        {
            $"Name: {node.DisplayName}",
            $"Components: {node.Components.Count}",
            $"Children: {node.Children.Count}",
            $"Inherits: {(node.Inherits.Count == 0 ? "(none)" : string.Join(", ", node.Inherits))}",
            $"Overrides: {node.OverrideCount}",
            $"Initializer events: {node.InitializerEventCount}",
            string.Empty,
            "Component List:"
        };

        lines.AddRange(node.Components.Select(component => $"  {component.Name}: {component.Summary}"));
        return string.Join(Environment.NewLine, lines);
    }

    private void WriteOutput(string message)
    {
        _output.Text = message;
    }
}
