using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Usagi.ToolCore.Assets;
using Usagi.ToolCore.Entities;
using Usagi.ToolCore.Projects;
using Usagi.ToolCore.Settings;

namespace Usagi.ToolShell;

public enum EditorMode
{
    Entities,
    Particles,
    Audio
}

public sealed class MainWindow : Window
{
    private readonly EntityHierarchyLoader _loader = new();
    private readonly ListBox _assetList = new();
    private readonly TreeView _hierarchyTree = new();
    private readonly TextBlock _properties = new();
    private readonly TextBox _output = new();
    private readonly PreviewPane _previewPane = new();
    private readonly ParticleEditorTab _particleTab = new();
    private readonly AudioEditorTab _audioTab = new();
    private readonly WorkspaceSettings _settings;

    // Layout containers
    private Control _entityContent = null!;
    private readonly ContentControl _mainContent = new();

    private MenuItem _undoItem = null!;
    private MenuItem _redoItem = null!;
    private MenuItem _saveItem = null!;
    private MenuItem _entityModeItem = null!;
    private MenuItem _particleModeItem = null!;
    private MenuItem _audioModeItem = null!;

    private UsagiProject? _project;
    private EditableEntityDocument? _document;
    private EditorMode _mode = EditorMode.Entities;

    public MainWindow()
    {
        Title = "Usagi Tools";
        Width = 1280;
        Height = 760;
        MinWidth = 900;
        MinHeight = 560;

        _settings = WorkspaceSettings.Load();
        Content = BuildLayout();

        _particleTab.DocumentChanged += OnParticleDocChanged;
        _audioTab.DocumentChanged += OnAudioDocChanged;

        Opened += (_, _) => InitializeProject();
        _assetList.SelectionChanged += (_, _) => LoadSelectedAsset();
        _hierarchyTree.SelectionChanged += (_, _) => ShowSelectedEntity();

        KeyDown += OnKeyDown;
    }

    private void OnParticleDocChanged()
    {
        UpdateTitle();
        UpdateEditMenuState();
    }

    private void OnAudioDocChanged()
    {
        UpdateTitle();
        UpdateEditMenuState();
    }

    private void OnKeyDown(object? sender, KeyEventArgs e)
    {
        var handled = (e.KeyModifiers, e.Key) switch
        {
            (KeyModifiers.Control, Key.O) => DoAsync(OpenProjectAsync),
            (KeyModifiers.Control, Key.S) => DoSync(SaveDocument),
            (KeyModifiers.Control, Key.Z) => DoSync(Undo),
            (KeyModifiers.Control, Key.Y) => DoSync(Redo),
            (KeyModifiers.Control | KeyModifiers.Shift, Key.Z) => DoSync(Redo),
            (KeyModifiers.Control, Key.D1) => DoSync(() => SwitchMode(EditorMode.Entities)),
            (KeyModifiers.Control, Key.D2) => DoSync(() => SwitchMode(EditorMode.Particles)),
            (KeyModifiers.Control, Key.D3) => DoSync(() => SwitchMode(EditorMode.Audio)),
            _ => false
        };
        e.Handled = handled;

        static bool DoSync(Action action)
        {
            action();
            return true;
        }
        static bool DoAsync(Func<Task> action)
        {
            _ = action();
            return true;
        }
    }

    private async Task OpenProjectAsync()
    {
        var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "Select Usagi Project Root",
            AllowMultiple = false
        });

        if (folders.Count == 0) return;

        var folderPath = folders[0].Path.LocalPath;
        var project = UsagiProjectLocator.FromPath(folderPath);

        if (project is null)
        {
            WriteOutput($"Invalid project: {folderPath}\nExpected Engine, Data, and Tools directories.");
            return;
        }

        _settings.AddRecentProject(project.RootPath);
        _settings.Save();

        LoadProject(project);
    }

    private Control BuildLayout()
    {
        var menuBar = BuildMenuBar();

        // Build entity editor content
        _entityContent = BuildEntityContent();

        // Initially show entity mode
        _mainContent.Content = _entityContent;

        var root = new DockPanel();
        DockPanel.SetDock(menuBar, Dock.Top);
        root.Children.Add(menuBar);
        root.Children.Add(_mainContent);

        return root;
    }

    private Control BuildEntityContent()
    {
        var contentGrid = new Grid
        {
            RowDefinitions = new RowDefinitions("*,170"),
            ColumnDefinitions = new ColumnDefinitions("240,*,320")
        };

        contentGrid.Children.Add(Wrap("Assets", _assetList, 0, 0));

        // Split center area between hierarchy and preview
        var centerSplit = new Grid
        {
            RowDefinitions = new RowDefinitions("*,*")
        };
        var hierarchyPanel = Wrap("Hierarchy", BuildHierarchyPanel(), 0, 0);
        Grid.SetRow(hierarchyPanel, 0);
        centerSplit.Children.Add(hierarchyPanel);

        var previewPanel = Wrap("Preview", _previewPane, 0, 0);
        Grid.SetRow(previewPanel, 1);
        centerSplit.Children.Add(previewPanel);

        Grid.SetColumn(centerSplit, 1);
        Grid.SetRow(centerSplit, 0);
        contentGrid.Children.Add(centerSplit);

        _properties.TextWrapping = TextWrapping.Wrap;
        _properties.FontFamily = FontFamily.Parse("Consolas");
        contentGrid.Children.Add(Wrap("Properties", new ScrollViewer { Content = _properties }, 2, 0));

        _output.IsReadOnly = true;
        _output.AcceptsReturn = true;
        _output.FontFamily = FontFamily.Parse("Consolas");
        _output.TextWrapping = TextWrapping.NoWrap;

        var outputPanel = Wrap("Output", _output, 0, 1);
        Grid.SetColumnSpan(outputPanel, 3);
        contentGrid.Children.Add(outputPanel);

        return contentGrid;
    }

    private void SwitchMode(EditorMode mode)
    {
        if (_mode == mode) return;
        _mode = mode;

        _mainContent.Content = mode switch
        {
            EditorMode.Entities => _entityContent,
            EditorMode.Particles => _particleTab,
            EditorMode.Audio => _audioTab,
            _ => _entityContent
        };

        _entityModeItem.Icon = mode == EditorMode.Entities ? new TextBlock { Text = "\u2713" } : null;
        _particleModeItem.Icon = mode == EditorMode.Particles ? new TextBlock { Text = "\u2713" } : null;
        _audioModeItem.Icon = mode == EditorMode.Audio ? new TextBlock { Text = "\u2713" } : null;

        UpdateTitle();
        UpdateEditMenuState();
    }

    private Control BuildHierarchyPanel()
    {
        _hierarchyTree.ContextMenu = new ContextMenu
        {
            ItemsSource = BuildEntityContextMenu()
        };
        return _hierarchyTree;
    }

    private Control[] BuildEntityContextMenu()
    {
        var addChildItem = new MenuItem { Header = "Add _Child" };
        addChildItem.Click += (_, _) => AddChildToSelected();

        var renameItem = new MenuItem { Header = "_Rename..." };
        renameItem.Click += (_, _) => RenameSelected();

        var addComponentItem = new MenuItem { Header = "Add C_omponent..." };
        addComponentItem.Click += (_, _) => AddComponentToSelected();

        var removeComponentItem = new MenuItem { Header = "Remove Com_ponent..." };
        removeComponentItem.Click += (_, _) => RemoveComponentFromSelected();

        var deleteItem = new MenuItem { Header = "_Delete" };
        deleteItem.Click += (_, _) => DeleteSelected();

        return [addChildItem, renameItem, new Separator(), addComponentItem, removeComponentItem, new Separator(), deleteItem];
    }

    private Menu BuildMenuBar()
    {
        // File menu
        var openProjectItem = new MenuItem { Header = "_Open Project...", InputGesture = new KeyGesture(Key.O, KeyModifiers.Control) };
        openProjectItem.Click += async (_, _) => await OpenProjectAsync();

        _saveItem = new MenuItem { Header = "_Save", InputGesture = new KeyGesture(Key.S, KeyModifiers.Control), IsEnabled = false };
        _saveItem.Click += (_, _) => SaveDocument();

        var recentMenu = new MenuItem { Header = "_Recent Projects" };
        UpdateRecentProjectsMenu(recentMenu);

        var exitItem = new MenuItem { Header = "E_xit" };
        exitItem.Click += (_, _) => Close();

        var fileMenu = new MenuItem
        {
            Header = "_File",
            ItemsSource = new Control[] { openProjectItem, _saveItem, new Separator(), recentMenu, new Separator(), exitItem }
        };

        // Edit menu
        _undoItem = new MenuItem { Header = "_Undo", InputGesture = new KeyGesture(Key.Z, KeyModifiers.Control), IsEnabled = false };
        _undoItem.Click += (_, _) => Undo();

        _redoItem = new MenuItem { Header = "_Redo", InputGesture = new KeyGesture(Key.Y, KeyModifiers.Control), IsEnabled = false };
        _redoItem.Click += (_, _) => Redo();

        var addChildItem = new MenuItem { Header = "Add _Child" };
        addChildItem.Click += (_, _) => AddChildToSelected();

        var renameItem = new MenuItem { Header = "Re_name..." };
        renameItem.Click += (_, _) => RenameSelected();

        var addComponentItem = new MenuItem { Header = "Add C_omponent..." };
        addComponentItem.Click += (_, _) => AddComponentToSelected();

        var removeComponentItem = new MenuItem { Header = "Remove Com_ponent..." };
        removeComponentItem.Click += (_, _) => RemoveComponentFromSelected();

        var editMenu = new MenuItem
        {
            Header = "_Edit",
            ItemsSource = new Control[]
            {
                _undoItem, _redoItem, new Separator(),
                addChildItem, renameItem, new Separator(),
                addComponentItem, removeComponentItem
            }
        };

        // View menu
        _entityModeItem = new MenuItem
        {
            Header = "_Entity Editor",
            Icon = new TextBlock { Text = "\u2713" }, // Checkmark
            InputGesture = new KeyGesture(Key.D1, KeyModifiers.Control)
        };
        _entityModeItem.Click += (_, _) => SwitchMode(EditorMode.Entities);

        _particleModeItem = new MenuItem
        {
            Header = "_Particle Editor",
            InputGesture = new KeyGesture(Key.D2, KeyModifiers.Control)
        };
        _particleModeItem.Click += (_, _) => SwitchMode(EditorMode.Particles);

        _audioModeItem = new MenuItem
        {
            Header = "_Audio Editor",
            InputGesture = new KeyGesture(Key.D3, KeyModifiers.Control)
        };
        _audioModeItem.Click += (_, _) => SwitchMode(EditorMode.Audio);

        var viewMenu = new MenuItem
        {
            Header = "_View",
            ItemsSource = new Control[] { _entityModeItem, _particleModeItem, _audioModeItem }
        };

        return new Menu { ItemsSource = new[] { fileMenu, editMenu, viewMenu } };
    }

    private void UpdateRecentProjectsMenu(MenuItem recentMenu)
    {
        var items = new List<Control>();

        foreach (var path in _settings.RecentProjects.Take(5))
        {
            var item = new MenuItem { Header = path };
            var projectPath = path;
            item.Click += (_, _) =>
            {
                var project = UsagiProjectLocator.FromPath(projectPath);
                if (project is not null)
                {
                    LoadProject(project);
                }
                else
                {
                    WriteOutput($"Project no longer valid: {projectPath}");
                }
            };
            items.Add(item);
        }

        if (items.Count == 0)
        {
            items.Add(new MenuItem { Header = "(none)", IsEnabled = false });
        }

        recentMenu.ItemsSource = items;
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
        if (!string.IsNullOrEmpty(_settings.LastProjectPath))
        {
            var project = UsagiProjectLocator.FromPath(_settings.LastProjectPath);
            if (project is not null)
            {
                LoadProject(project);
                return;
            }
        }

        var discovered = UsagiProjectLocator.TryLocate(AppContext.BaseDirectory);
        if (discovered is not null)
        {
            LoadProject(discovered);
        }
        else
        {
            WriteOutput("No project loaded. Use File > Open Project (Ctrl+O) to select a Usagi checkout.");
        }
    }

    private void LoadProject(UsagiProject project)
    {
        _project = project;
        UpdateTitle();
        _previewPane.SetProject(project);
        _particleTab.SetProject(project);
        _audioTab.SetProject(project);

        _settings.AddRecentProject(project.RootPath);
        _settings.Save();

        if (!Directory.Exists(_project.EntitiesPath))
        {
            WriteOutput($"Project: {_project.RootPath}\nNo entity directory found: {_project.EntitiesPath}");
            _assetList.ItemsSource = Array.Empty<AssetItem>();
            return;
        }

        var assets = AssetInventory.EnumerateEntityAssets(_project.EntitiesPath);
        _assetList.ItemsSource = assets;
        WriteOutput($"Project: {_project.RootPath}\nEntity assets: {assets.Count}");

        var standardRoot = assets.FirstOrDefault(asset => asset.Name.Equals("StandardRoot", StringComparison.OrdinalIgnoreCase));
        _assetList.SelectedItem = standardRoot ?? assets.FirstOrDefault();
    }

    private void LoadSelectedAsset()
    {
        if (_assetList.SelectedItem is not AssetItem asset)
        {
            return;
        }

        try
        {
            var readOnly = _loader.LoadFile(asset.FullPath);
            _document = EditableEntityDocument.FromReadOnly(readOnly);
            _document.Changed += OnDocumentChanged;

            RefreshHierarchy();
            UpdateTitle();
            UpdateEditMenuState();

            WriteOutput($"Loaded {asset.RelativePath}{Environment.NewLine}{string.Join(Environment.NewLine, _document.Diagnostics)}");
        }
        catch (Exception ex)
        {
            _hierarchyTree.ItemsSource = Array.Empty<TreeViewItem>();
            _properties.Text = string.Empty;
            WriteOutput($"Failed to load {asset.RelativePath}:{Environment.NewLine}{ex.Message}");
        }
    }

    private void OnDocumentChanged()
    {
        UpdateTitle();
        UpdateEditMenuState();
        RefreshHierarchy();
    }

    private void UpdateTitle()
    {
        var projectName = _project is not null ? Path.GetFileName(_project.RootPath) : "No Project";
        var modeName = _mode switch
        {
            EditorMode.Particles => " [Particles]",
            EditorMode.Audio => " [Audio]",
            _ => ""
        };
        var dirty = (_mode == EditorMode.Entities && _document?.IsDirty == true) ||
                    (_mode == EditorMode.Particles && _particleTab.IsDirty) ||
                    (_mode == EditorMode.Audio && _audioTab.IsDirty) ? "*" : "";
        Title = $"Usagi Tools - {projectName}{modeName}{dirty}";
    }

    private void UpdateEditMenuState()
    {
        if (_mode == EditorMode.Entities)
        {
            _undoItem.IsEnabled = _document?.History.CanUndo == true;
            _redoItem.IsEnabled = _document?.History.CanRedo == true;
            _saveItem.IsEnabled = _document?.IsDirty == true;
            _undoItem.Header = _document?.History.UndoDescription is { } undoDesc ? $"_Undo {undoDesc}" : "_Undo";
            _redoItem.Header = _document?.History.RedoDescription is { } redoDesc ? $"_Redo {redoDesc}" : "_Redo";
        }
        else if (_mode == EditorMode.Particles)
        {
            _undoItem.IsEnabled = _particleTab.CanUndo;
            _redoItem.IsEnabled = _particleTab.CanRedo;
            _saveItem.IsEnabled = _particleTab.IsDirty;
            _undoItem.Header = "_Undo";
            _redoItem.Header = "_Redo";
        }
        else
        {
            _undoItem.IsEnabled = false;
            _redoItem.IsEnabled = false;
            _saveItem.IsEnabled = _audioTab.IsDirty;
            _undoItem.Header = "_Undo";
            _redoItem.Header = "_Redo";
        }
    }

    private void RefreshHierarchy()
    {
        if (_document is null) return;

        var rootItem = new EditableEntityTreeItem(_document.Root);
        _hierarchyTree.ItemsSource = new[] { BuildEditableTreeItem(rootItem) };

        if (_hierarchyTree.ItemsSource is TreeViewItem[] items && items.Length > 0)
        {
            items[0].IsExpanded = true;
        }
    }

    private void ShowSelectedEntity()
    {
        if (_hierarchyTree.SelectedItem is TreeViewItem { Tag: EditableEntityTreeItem item })
        {
            _properties.Text = Describe(item.Node);
        }
    }

    private static TreeViewItem BuildEditableTreeItem(EditableEntityTreeItem item)
    {
        return new TreeViewItem
        {
            Header = item.Node.DisplayName,
            Tag = item,
            ItemsSource = item.Children.Select(BuildEditableTreeItem).ToArray(),
            IsExpanded = true
        };
    }

    private static string Describe(EditableEntityNode node)
    {
        var lines = new List<string>
        {
            $"Name: {node.DisplayName}",
            $"Components: {node.Components.Count}",
            $"Children: {node.Children.Count}",
            $"Inherits: {(node.Inherits.Count == 0 ? "(none)" : string.Join(", ", node.Inherits))}",
            $"Overrides: {node.Overrides.Count}",
            $"Initializer events: {node.InitializerEvents.Count}",
            string.Empty,
            "Component List:"
        };

        lines.AddRange(node.Components.Select(component =>
            $"  {component.Name}: {(component.Fields.Count > 0 ? $"{component.Fields.Count} fields" : "present")}"));
        return string.Join(Environment.NewLine, lines);
    }

    private EditableEntityNode? GetSelectedEntity()
    {
        return _hierarchyTree.SelectedItem is TreeViewItem { Tag: EditableEntityTreeItem item } ? item.Node : null;
    }

    private EditableEntityNode? FindParent(EditableEntityNode target, EditableEntityNode? current = null)
    {
        current ??= _document?.Root;
        if (current is null) return null;

        if (current.Children.Contains(target))
        {
            return current;
        }

        foreach (var child in current.Children)
        {
            var found = FindParent(target, child);
            if (found is not null) return found;
        }

        return null;
    }

    private void Undo()
    {
        if (_mode == EditorMode.Entities)
        {
            _document?.History.Undo();
        }
        else if (_mode == EditorMode.Particles)
        {
            _particleTab.Undo();
        }
    }

    private void Redo()
    {
        if (_mode == EditorMode.Entities)
        {
            _document?.History.Redo();
        }
        else if (_mode == EditorMode.Particles)
        {
            _particleTab.Redo();
        }
    }

    private void SaveDocument()
    {
        if (_mode == EditorMode.Particles)
        {
            _particleTab.Save();
            return;
        }

        if (_mode == EditorMode.Audio)
        {
            _audioTab.Save();
            return;
        }

        if (_document is null || !_document.IsDirty) return;

        try
        {
            _document.Save();
            WriteOutput($"Saved: {_document.SourcePath}");
        }
        catch (Exception ex)
        {
            WriteOutput($"Save failed: {ex.Message}");
        }
    }

    private async void AddChildToSelected()
    {
        var parent = GetSelectedEntity();
        if (_document is null || parent is null) return;

        var name = await PromptForName("Add Child Entity", "Entity name:", "NewEntity");
        if (string.IsNullOrWhiteSpace(name)) return;

        _document.AddChild(parent, name);
        WriteOutput($"Added child: {name}");
    }

    private async void RenameSelected()
    {
        var entity = GetSelectedEntity();
        if (_document is null || entity is null) return;

        var name = await PromptForName("Rename Entity", "New name:", entity.DisplayName);
        if (string.IsNullOrWhiteSpace(name) || name == entity.DisplayName) return;

        _document.Rename(entity, name);
        WriteOutput($"Renamed to: {name}");
    }

    private async void AddComponentToSelected()
    {
        var entity = GetSelectedEntity();
        if (_document is null || entity is null) return;

        var name = await PromptForName("Add Component", "Component name:", "TransformComponent");
        if (string.IsNullOrWhiteSpace(name)) return;

        _document.AddComponent(entity, name);
        WriteOutput($"Added component: {name}");
        _properties.Text = Describe(entity);
    }

    private async void RemoveComponentFromSelected()
    {
        var entity = GetSelectedEntity();
        if (_document is null || entity is null || entity.Components.Count == 0) return;

        var components = entity.Components.Select(c => c.Name).ToArray();
        var selected = await PromptForSelection("Remove Component", "Select component:", components);
        if (string.IsNullOrWhiteSpace(selected)) return;

        _document.RemoveComponent(entity, selected);
        WriteOutput($"Removed component: {selected}");
        _properties.Text = Describe(entity);
    }

    private void DeleteSelected()
    {
        var entity = GetSelectedEntity();
        if (_document is null || entity is null) return;

        var parent = FindParent(entity);
        if (parent is null)
        {
            WriteOutput("Cannot delete root entity.");
            return;
        }

        _document.RemoveChild(parent, entity);
        WriteOutput($"Deleted: {entity.DisplayName}");
    }

    private async Task<string?> PromptForName(string title, string prompt, string defaultValue)
    {
        var dialog = new Window
        {
            Title = title,
            Width = 400,
            Height = 140,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            CanResize = false
        };

        var textBox = new TextBox { Text = defaultValue, Margin = new Avalonia.Thickness(10) };
        var okButton = new Button { Content = "OK", Width = 80, Margin = new Avalonia.Thickness(5), IsDefault = true };
        var cancelButton = new Button { Content = "Cancel", Width = 80, Margin = new Avalonia.Thickness(5), IsCancel = true };

        string? result = null;

        okButton.Click += (_, _) => { result = textBox.Text; dialog.Close(); };
        cancelButton.Click += (_, _) => dialog.Close();

        var buttonPanel = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Margin = new Avalonia.Thickness(10, 0, 10, 10),
            Children = { okButton, cancelButton }
        };

        dialog.Content = new StackPanel
        {
            Children =
            {
                new TextBlock { Text = prompt, Margin = new Avalonia.Thickness(10, 10, 10, 0) },
                textBox,
                buttonPanel
            }
        };

        await dialog.ShowDialog(this);
        return result;
    }

    private async Task<string?> PromptForSelection(string title, string prompt, string[] options)
    {
        var dialog = new Window
        {
            Title = title,
            Width = 400,
            Height = 200,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            CanResize = false
        };

        var listBox = new ListBox { ItemsSource = options, Margin = new Avalonia.Thickness(10), Height = 100 };
        if (options.Length > 0) listBox.SelectedIndex = 0;

        var okButton = new Button { Content = "OK", Width = 80, Margin = new Avalonia.Thickness(5), IsDefault = true };
        var cancelButton = new Button { Content = "Cancel", Width = 80, Margin = new Avalonia.Thickness(5), IsCancel = true };

        string? result = null;

        okButton.Click += (_, _) => { result = listBox.SelectedItem?.ToString(); dialog.Close(); };
        cancelButton.Click += (_, _) => dialog.Close();

        var buttonPanel = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Margin = new Avalonia.Thickness(10, 0, 10, 10),
            Children = { okButton, cancelButton }
        };

        dialog.Content = new StackPanel
        {
            Children =
            {
                new TextBlock { Text = prompt, Margin = new Avalonia.Thickness(10, 10, 10, 0) },
                listBox,
                buttonPanel
            }
        };

        await dialog.ShowDialog(this);
        return result;
    }

    private void WriteOutput(string message)
    {
        _output.Text = message;
    }
}

internal sealed class EditableEntityTreeItem
{
    public EditableEntityNode Node { get; }
    public IReadOnlyList<EditableEntityTreeItem> Children { get; }

    public EditableEntityTreeItem(EditableEntityNode node)
    {
        Node = node;
        Children = node.Children.Select(child => new EditableEntityTreeItem(child)).ToArray();
    }
}
