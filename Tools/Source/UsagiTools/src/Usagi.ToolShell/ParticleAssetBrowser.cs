// Usagi.ToolShell - Particle Asset Browser Control

using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Usagi.ToolCore.Particles;
using Usagi.ToolCore.Projects;

namespace Usagi.ToolShell;

/// <summary>
/// Asset browser for particle emitters and effects.
/// </summary>
public sealed class ParticleAssetBrowser : UserControl
{
    private readonly TreeView _treeView = new();
    private readonly ComboBox _filterCombo = new();
    private UsagiProject? _project;

    public event Action<ParticleAssetItem>? AssetSelected;
    public event Action<string>? Message;

    public ParticleAssetBrowser()
    {
        _filterCombo.ItemsSource = new[] { "All", "Emitters", "Effects" };
        _filterCombo.SelectedIndex = 0;
        _filterCombo.SelectionChanged += (_, _) => RefreshTree();
        _filterCombo.Margin = new Avalonia.Thickness(4);

        _treeView.SelectionChanged += OnTreeSelectionChanged;

        var panel = new DockPanel();
        DockPanel.SetDock(_filterCombo, Dock.Top);
        panel.Children.Add(_filterCombo);
        panel.Children.Add(_treeView);

        Content = panel;
    }

    public void SetProject(UsagiProject? project)
    {
        _project = project;
        RefreshTree();
    }

    public void RefreshTree()
    {
        _treeView.ItemsSource = null;

        if (_project is null) return;

        var particlePath = Path.Combine(_project.DataPath, "Particle");
        if (!Directory.Exists(particlePath))
        {
            Message?.Invoke($"Particle data folder not found: {particlePath}");
            return;
        }

        var filter = _filterCombo.SelectedItem?.ToString() ?? "All";
        var roots = new List<TreeViewItem>();

        if (filter is "All" or "Emitters")
        {
            var emittersPath = Path.Combine(particlePath, "Emitters");
            if (Directory.Exists(emittersPath))
            {
                var emitterItem = CreateFolderNode("Emitters", emittersPath, ParticleAssetType.Emitter);
                roots.Add(emitterItem);
            }
        }

        if (filter is "All" or "Effects")
        {
            var effectsPath = Path.Combine(particlePath, "Effects");
            if (Directory.Exists(effectsPath))
            {
                var effectItem = CreateFolderNode("Effects", effectsPath, ParticleAssetType.Effect);
                roots.Add(effectItem);
            }
        }

        _treeView.ItemsSource = roots;

        // Expand root nodes
        foreach (var root in roots)
        {
            root.IsExpanded = true;
        }
    }

    private TreeViewItem CreateFolderNode(string name, string path, ParticleAssetType assetType)
    {
        var children = new List<TreeViewItem>();

        // Add subdirectories
        foreach (var dir in Directory.GetDirectories(path).OrderBy(d => d))
        {
            var subFolder = CreateFolderNode(Path.GetFileName(dir), dir, assetType);
            children.Add(subFolder);
        }

        // Add YAML files
        foreach (var file in Directory.GetFiles(path, "*.yml").OrderBy(f => f))
        {
            var fileName = Path.GetFileNameWithoutExtension(file);
            var item = new TreeViewItem
            {
                Header = fileName,
                Tag = new ParticleAssetItem
                {
                    Name = fileName,
                    FullPath = file,
                    RelativePath = GetRelativePath(file),
                    Type = assetType
                }
            };
            children.Add(item);
        }

        return new TreeViewItem
        {
            Header = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Children =
                {
                    new TextBlock { Text = "\U0001F4C1 ", FontSize = 12 }, // Folder emoji
                    new TextBlock { Text = name }
                }
            },
            ItemsSource = children
        };
    }

    private string GetRelativePath(string fullPath)
    {
        if (_project is null) return fullPath;
        return Path.GetRelativePath(_project.DataPath, fullPath);
    }

    private void OnTreeSelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (_treeView.SelectedItem is TreeViewItem { Tag: ParticleAssetItem item })
        {
            AssetSelected?.Invoke(item);
        }
    }
}

/// <summary>
/// Type of particle asset.
/// </summary>
public enum ParticleAssetType
{
    Emitter,
    Effect
}

/// <summary>
/// Information about a particle asset file.
/// </summary>
public sealed class ParticleAssetItem
{
    public required string Name { get; init; }
    public required string FullPath { get; init; }
    public required string RelativePath { get; init; }
    public required ParticleAssetType Type { get; init; }

    public override string ToString() => Name;
}
