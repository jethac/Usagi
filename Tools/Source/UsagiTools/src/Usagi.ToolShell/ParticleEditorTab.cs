// Usagi.ToolShell - Particle Editor Tab

using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Usagi.ToolCore.Particles;
using Usagi.ToolCore.Projects;

namespace Usagi.ToolShell;

/// <summary>
/// Complete particle editor tab with browser, preview, and properties.
/// </summary>
public sealed class ParticleEditorTab : UserControl
{
    private readonly ParticleAssetBrowser _browser = new();
    private readonly ParticlePropertiesPanel _properties = new();
    private readonly PreviewPane _preview = new();
    private readonly TextBox _output = new();

    private UsagiProject? _project;
    private EditableEmitterDocument? _emitterDoc;
    private EditableEffectDocument? _effectDoc;

    public event Action? DocumentChanged;

    public ParticleEditorTab()
    {
        _browser.AssetSelected += OnAssetSelected;
        _browser.Message += WriteOutput;
        _properties.DocumentChanged += OnPropertiesChanged;

        _output.IsReadOnly = true;
        _output.AcceptsReturn = true;
        _output.FontFamily = FontFamily.Parse("Consolas");
        _output.TextWrapping = TextWrapping.NoWrap;

        Content = BuildLayout();
    }

    public void SetProject(UsagiProject? project)
    {
        _project = project;
        _browser.SetProject(project);
        _preview.SetProject(project);
        _emitterDoc = null;
        _effectDoc = null;
        _properties.Clear();
    }

    public bool IsDirty => _emitterDoc?.IsDirty == true || _effectDoc?.IsDirty == true;

    public void Save()
    {
        try
        {
            if (_emitterDoc?.IsDirty == true)
            {
                _emitterDoc.Save();
                WriteOutput($"Saved emitter: {_emitterDoc.Emitter.SourcePath}");
                DocumentChanged?.Invoke();
            }
            else if (_effectDoc?.IsDirty == true)
            {
                _effectDoc.Save();
                WriteOutput($"Saved effect: {_effectDoc.Effect.SourcePath}");
                DocumentChanged?.Invoke();
            }
        }
        catch (Exception ex)
        {
            WriteOutput($"Save failed: {ex.Message}");
        }
    }

    public void Undo()
    {
        _emitterDoc?.History.Undo();
        _effectDoc?.History.Undo();
        RefreshProperties();
    }

    public void Redo()
    {
        _emitterDoc?.History.Redo();
        _effectDoc?.History.Redo();
        RefreshProperties();
    }

    public bool CanUndo => _emitterDoc?.History.CanUndo == true || _effectDoc?.History.CanUndo == true;
    public bool CanRedo => _emitterDoc?.History.CanRedo == true || _effectDoc?.History.CanRedo == true;

    private Control BuildLayout()
    {
        var mainGrid = new Grid
        {
            RowDefinitions = new RowDefinitions("*,150"),
            ColumnDefinitions = new ColumnDefinitions("220,*,300")
        };

        // Left: Browser
        mainGrid.Children.Add(Wrap("Particles", _browser, 0, 0));

        // Center: Preview + Actions
        var centerPanel = new DockPanel();

        var actionBar = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Thickness(4),
            Spacing = 4
        };

        var playButton = new Button { Content = "Play" };
        playButton.Click += (_, _) => PlayPreview();

        var stopButton = new Button { Content = "Stop" };
        stopButton.Click += (_, _) => StopPreview();

        var resetButton = new Button { Content = "Reset" };
        resetButton.Click += (_, _) => ResetPreview();

        actionBar.Children.Add(playButton);
        actionBar.Children.Add(stopButton);
        actionBar.Children.Add(resetButton);

        DockPanel.SetDock(actionBar, Dock.Top);
        centerPanel.Children.Add(actionBar);
        centerPanel.Children.Add(_preview);

        var centerWrapped = Wrap("Preview", centerPanel, 1, 0);
        mainGrid.Children.Add(centerWrapped);

        // Right: Properties
        mainGrid.Children.Add(Wrap("Properties", _properties, 2, 0));

        // Bottom: Output
        var outputPanel = Wrap("Output", _output, 0, 1);
        Grid.SetColumnSpan(outputPanel, 3);
        mainGrid.Children.Add(outputPanel);

        return mainGrid;
    }

    private static Border Wrap(string title, Control content, int column, int row)
    {
        var panel = new DockPanel();
        panel.Children.Add(new TextBlock
        {
            Text = title,
            FontWeight = FontWeight.SemiBold,
            Margin = new Thickness(8, 6),
            VerticalAlignment = VerticalAlignment.Center
        });
        DockPanel.SetDock(panel.Children[0], Dock.Top);
        panel.Children.Add(content);

        var border = new Border
        {
            BorderBrush = Brushes.Gray,
            BorderThickness = new Thickness(0.5),
            Child = panel
        };

        Grid.SetColumn(border, column);
        Grid.SetRow(border, row);
        return border;
    }

    private void OnAssetSelected(ParticleAssetItem asset)
    {
        try
        {
            switch (asset.Type)
            {
                case ParticleAssetType.Emitter:
                    _emitterDoc = EditableEmitterDocument.Load(asset.FullPath);
                    _effectDoc = null;
                    _properties.ShowEmitter(_emitterDoc);
                    WriteOutput($"Loaded emitter: {asset.Name}");

                    // Send to preview host
                    _ = _preview.LoadParticleAsync(asset.FullPath, null);
                    break;

                case ParticleAssetType.Effect:
                    _effectDoc = EditableEffectDocument.Load(asset.FullPath);
                    _emitterDoc = null;
                    _properties.ShowEffect(_effectDoc);
                    WriteOutput($"Loaded effect: {asset.Name}");

                    // For effects, we need the emitter path too
                    if (_effectDoc.Effect.Emitters.Count > 0)
                    {
                        var emitterName = _effectDoc.Effect.Emitters[0].EmitterName;
                        var emitterPath = FindEmitterPath(emitterName);
                        _ = _preview.LoadParticleAsync(emitterPath, asset.FullPath);
                    }
                    break;
            }

            DocumentChanged?.Invoke();
        }
        catch (Exception ex)
        {
            WriteOutput($"Failed to load {asset.Name}: {ex.Message}");
            _properties.Clear();
        }
    }

    private string? FindEmitterPath(string emitterName)
    {
        if (_project is null) return null;

        var emittersPath = Path.Combine(_project.DataPath, "Particle", "Emitters");
        var filePath = Path.Combine(emittersPath, emitterName + ".yml");

        return File.Exists(filePath) ? filePath : null;
    }

    private void OnPropertiesChanged()
    {
        DocumentChanged?.Invoke();
    }

    private void RefreshProperties()
    {
        if (_emitterDoc is not null)
        {
            _properties.ShowEmitter(_emitterDoc);
        }
        else if (_effectDoc is not null)
        {
            _properties.ShowEffect(_effectDoc);
        }
        DocumentChanged?.Invoke();
    }

    private void PlayPreview()
    {
        // The preview host should start emitting particles
        WriteOutput("Play preview");
    }

    private void StopPreview()
    {
        WriteOutput("Stop preview");
    }

    private void ResetPreview()
    {
        WriteOutput("Reset preview");
    }

    private void WriteOutput(string message)
    {
        _output.Text = message;
    }
}
