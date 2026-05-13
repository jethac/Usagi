using System.Globalization;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Usagi.ToolCore.Audio;
using Usagi.ToolCore.Projects;

namespace Usagi.ToolShell;

public sealed class AudioEditorTab : UserControl
{
    private readonly ListBox _soundList = new();
    private readonly TextBox _diagnostics = new();
    private readonly TextBox _enumNameBox = new();
    private readonly TextBox _filenameBox = new();
    private readonly CheckBox _streamBox = new();
    private readonly CheckBox _loopBox = new();
    private readonly TextBox _volumeBox = new();
    private readonly TextBox _minDistanceBox = new();
    private readonly TextBox _maxDistanceBox = new();
    private readonly TextBox _audioTypeBox = new();
    private readonly TextBox _falloffBox = new();
    private readonly TextBox _pitchRandomisationBox = new();
    private readonly TextBox _priorityBox = new();
    private readonly TextBox _crossfadeBox = new();
    private readonly TextBox _basePitchBox = new();
    private readonly TextBox _dopplerFactorBox = new();
    private readonly CheckBox _localizedBox = new();
    private readonly TextBox _crcBox = new();
    private readonly TextBox _filterCrcBox = new();
    private readonly TextBox _effectCrcsBox = new();
    private readonly TextBox _roomNameCrcBox = new();

    private UsagiProject? _project;
    private AudioBank? _bank;
    private string? _sourcePath;

    public event Action? DocumentChanged;

    public bool IsDirty { get; private set; }

    public AudioEditorTab()
    {
        Content = BuildLayout();
        _soundList.SelectionChanged += (_, _) => ShowSelectedSound();
        SetEditorEnabled(false);
    }

    public void SetProject(UsagiProject project)
    {
        _project = project;

        if (_bank is not null)
        {
            return;
        }

        var audioDir = Path.Combine(project.RootPath, "Data", "VPB", "Audio");
        var banks = Directory.Exists(audioDir)
            ? Directory.EnumerateFiles(audioDir, "*.yml").Concat(Directory.EnumerateFiles(audioDir, "*.yaml")).Count()
            : 0;

        WriteDiagnostics($"Project: {project.RootPath}{Environment.NewLine}Audio banks: {banks}");
    }

    public void Save()
    {
        if (_bank is null || _sourcePath is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        File.WriteAllText(_sourcePath, AudioBankYamlWriter.Write(_bank));
        IsDirty = false;
        DocumentChanged?.Invoke();
        WriteDiagnostics($"Saved: {_sourcePath}");
    }

    private Control BuildLayout()
    {
        var root = new DockPanel();

        var toolbar = BuildToolbar();
        DockPanel.SetDock(toolbar, Dock.Top);
        root.Children.Add(toolbar);

        _diagnostics.IsReadOnly = true;
        _diagnostics.AcceptsReturn = true;
        _diagnostics.FontFamily = FontFamily.Parse("Consolas");
        _diagnostics.TextWrapping = TextWrapping.NoWrap;
        _diagnostics.Height = 160;

        var diagnosticsPanel = Wrap("Diagnostics", _diagnostics);
        DockPanel.SetDock(diagnosticsPanel, Dock.Bottom);
        root.Children.Add(diagnosticsPanel);

        var body = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("360,*")
        };
        body.Children.Add(Wrap("Sound Files", _soundList, 0, 0));
        body.Children.Add(Wrap("Sound", BuildEditor(), 1, 0));
        root.Children.Add(body);

        return root;
    }

    private Control BuildToolbar()
    {
        var openButton = new Button { Content = "Open Bank", Margin = new Avalonia.Thickness(4) };
        openButton.Click += async (_, _) => await OpenBankAsync();

        var saveButton = new Button { Content = "Save YAML", Margin = new Avalonia.Thickness(4) };
        saveButton.Click += (_, _) => Save();

        var validateButton = new Button { Content = "Validate", Margin = new Avalonia.Thickness(4) };
        validateButton.Click += (_, _) => ValidateBank();

        var protoButton = new Button { Content = "Generate Proto", Margin = new Avalonia.Thickness(4) };
        protoButton.Click += async (_, _) => await GenerateProtoAsync();

        var headerButton = new Button { Content = "Generate Header", Margin = new Avalonia.Thickness(4) };
        headerButton.Click += async (_, _) => await GenerateHeaderAsync();

        return new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Avalonia.Thickness(6, 4),
            Children = { openButton, saveButton, validateButton, protoButton, headerButton }
        };
    }

    private Control BuildEditor()
    {
        var panel = new StackPanel { Margin = new Avalonia.Thickness(10), Spacing = 6 };

        AddTextRow(panel, "Enum Name", _enumNameBox);
        AddTextRow(panel, "Filename", _filenameBox);
        AddCheckRow(panel, "Stream", _streamBox);
        AddCheckRow(panel, "Loop", _loopBox);
        AddTextRow(panel, "Volume", _volumeBox);
        AddTextRow(panel, "Min Distance", _minDistanceBox);
        AddTextRow(panel, "Max Distance", _maxDistanceBox);
        AddTextRow(panel, "Audio Type", _audioTypeBox);
        AddTextRow(panel, "Falloff", _falloffBox);
        AddTextRow(panel, "Pitch Random", _pitchRandomisationBox);
        AddTextRow(panel, "Priority", _priorityBox);
        AddTextRow(panel, "Crossfade", _crossfadeBox);
        AddTextRow(panel, "Base Pitch", _basePitchBox);
        AddTextRow(panel, "Doppler Factor", _dopplerFactorBox);
        AddCheckRow(panel, "Localized", _localizedBox);
        AddTextRow(panel, "CRC", _crcBox);
        AddTextRow(panel, "Filter CRC", _filterCrcBox);
        AddTextRow(panel, "Effect CRCs", _effectCrcsBox);
        AddTextRow(panel, "Room CRC", _roomNameCrcBox);

        var applyButton = new Button
        {
            Content = "Apply Changes",
            HorizontalAlignment = HorizontalAlignment.Left,
            Margin = new Avalonia.Thickness(0, 8, 0, 0)
        };
        applyButton.Click += (_, _) => ApplySoundChanges();
        panel.Children.Add(applyButton);

        return new ScrollViewer { Content = panel };
    }

    private async Task OpenBankAsync()
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel is null)
        {
            return;
        }

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Open Audio Bank",
            AllowMultiple = false,
            FileTypeFilter = [YamlFileType()]
        });

        if (files.Count == 0)
        {
            return;
        }

        OpenBank(files[0].Path.LocalPath);
    }

    private void OpenBank(string path)
    {
        try
        {
            _bank = AudioBankYamlParser.ParseFile(path);
            _sourcePath = path;
            IsDirty = false;

            RefreshSoundList(0);
            SetEditorEnabled(true);
            DocumentChanged?.Invoke();
            WriteDiagnostics(DescribeBank(path, _bank));
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            WriteDiagnostics($"Open failed: {ex.Message}");
        }
    }

    private void ValidateBank()
    {
        if (_bank is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        var diagnostics = AudioBankValidator.Validate(_bank, _project?.RootPath);
        if (diagnostics.Count == 0)
        {
            WriteDiagnostics("Validation passed.");
            return;
        }

        WriteDiagnostics(string.Join(
            Environment.NewLine,
            diagnostics.Select(diagnostic => $"{diagnostic.Severity}: {diagnostic.Field}: {diagnostic.Message}")));
    }

    private async Task GenerateProtoAsync()
    {
        if (_bank is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        var path = await PickSavePathAsync("Save FSID Proto", SuggestedBankName() + ".proto", ProtoFileType());
        if (path is null)
        {
            return;
        }

        try
        {
            File.WriteAllText(path, FsidBuilder.WriteProto(_bank, SuggestedEnumName()));
            WriteDiagnostics($"Generated: {path}");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            WriteDiagnostics($"Proto generation failed: {ex.Message}");
        }
    }

    private async Task GenerateHeaderAsync()
    {
        if (_bank is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        var baseName = SuggestedBankName().ToUpperInvariant();
        var path = await PickSavePathAsync("Save FSID Header", SuggestedBankName() + ".h", HeaderFileType());
        if (path is null)
        {
            return;
        }

        try
        {
            File.WriteAllText(path, FsidBuilder.WriteHeader(_bank, $"_CLR_{baseName}_FSID_", SuggestedEnumName()));
            WriteDiagnostics($"Generated: {path}");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            WriteDiagnostics($"Header generation failed: {ex.Message}");
        }
    }

    private async Task<string?> PickSavePathAsync(string title, string suggestedFileName, FilePickerFileType fileType)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel is null)
        {
            return null;
        }

        var file = await topLevel.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = title,
            SuggestedFileName = suggestedFileName,
            FileTypeChoices = [fileType]
        });

        return file?.Path.LocalPath;
    }

    private void ShowSelectedSound()
    {
        if (_soundList.SelectedItem is not AudioSoundListItem item)
        {
            return;
        }

        var sound = item.Sound;
        _enumNameBox.Text = sound.EnumName;
        _filenameBox.Text = sound.Filename;
        _streamBox.IsChecked = sound.Stream;
        _loopBox.IsChecked = sound.Loop;
        _volumeBox.Text = FormatFloat(sound.Volume);
        _minDistanceBox.Text = FormatFloat(sound.MinDistance);
        _maxDistanceBox.Text = FormatFloat(sound.MaxDistance);
        _audioTypeBox.Text = sound.AudioType.ToString(CultureInfo.InvariantCulture);
        _falloffBox.Text = sound.Falloff.ToString(CultureInfo.InvariantCulture);
        _pitchRandomisationBox.Text = FormatFloat(sound.PitchRandomisation);
        _priorityBox.Text = sound.Priority.ToString(CultureInfo.InvariantCulture);
        _crossfadeBox.Text = sound.Crossfade;
        _basePitchBox.Text = FormatFloat(sound.BasePitch);
        _dopplerFactorBox.Text = FormatFloat(sound.DopplerFactor);
        _localizedBox.IsChecked = sound.Localized;
        _crcBox.Text = sound.Crc.ToString(CultureInfo.InvariantCulture);
        _filterCrcBox.Text = sound.FilterCrc.ToString(CultureInfo.InvariantCulture);
        _effectCrcsBox.Text = string.Join(", ", sound.EffectCrcs.Select(crc => crc.ToString(CultureInfo.InvariantCulture)));
        _roomNameCrcBox.Text = sound.RoomNameCrc.ToString(CultureInfo.InvariantCulture);
    }

    private void ApplySoundChanges()
    {
        if (_soundList.SelectedItem is not AudioSoundListItem item)
        {
            return;
        }

        var errors = new List<string>();
        var volume = ParseFloat(_volumeBox.Text, "Volume", errors);
        var minDistance = ParseFloat(_minDistanceBox.Text, "Min Distance", errors);
        var maxDistance = ParseFloat(_maxDistanceBox.Text, "Max Distance", errors);
        var audioType = ParseInt(_audioTypeBox.Text, "Audio Type", errors);
        var falloff = ParseInt(_falloffBox.Text, "Falloff", errors);
        var pitchRandomisation = ParseFloat(_pitchRandomisationBox.Text, "Pitch Random", errors);
        var priority = ParseInt(_priorityBox.Text, "Priority", errors);
        var basePitch = ParseFloat(_basePitchBox.Text, "Base Pitch", errors);
        var dopplerFactor = ParseFloat(_dopplerFactorBox.Text, "Doppler Factor", errors);
        var crc = ParseUInt(_crcBox.Text, "CRC", errors);
        var filterCrc = ParseUInt(_filterCrcBox.Text, "Filter CRC", errors);
        var effectCrcs = ParseUIntList(_effectCrcsBox.Text, "Effect CRCs", errors);
        var roomNameCrc = ParseUInt(_roomNameCrcBox.Text, "Room CRC", errors);

        if (errors.Count > 0)
        {
            WriteDiagnostics(string.Join(Environment.NewLine, errors));
            return;
        }

        var sound = item.Sound;
        sound.EnumName = _enumNameBox.Text?.Trim() ?? "";
        sound.Filename = _filenameBox.Text?.Trim() ?? "";
        sound.Stream = _streamBox.IsChecked == true;
        sound.Loop = _loopBox.IsChecked == true;
        sound.Volume = volume;
        sound.MinDistance = minDistance;
        sound.MaxDistance = maxDistance;
        sound.AudioType = audioType;
        sound.Falloff = falloff;
        sound.PitchRandomisation = pitchRandomisation;
        sound.Priority = priority;
        sound.Crossfade = _crossfadeBox.Text?.Trim() ?? "";
        sound.BasePitch = basePitch;
        sound.DopplerFactor = dopplerFactor;
        sound.Localized = _localizedBox.IsChecked == true;
        sound.Crc = crc;
        sound.FilterCrc = filterCrc;
        sound.EffectCrcs.Clear();
        sound.EffectCrcs.AddRange(effectCrcs);
        sound.RoomNameCrc = roomNameCrc;

        IsDirty = true;
        RefreshSoundList(item.Index);
        DocumentChanged?.Invoke();
        WriteDiagnostics($"Updated: {sound.EnumName}");
    }

    private void RefreshSoundList(int selectedIndex)
    {
        var items = _bank?.SoundFiles.Select((sound, index) => new AudioSoundListItem(index, sound)).ToArray()
            ?? [];
        _soundList.ItemsSource = items;

        if (items.Length > 0)
        {
            _soundList.SelectedIndex = Math.Clamp(selectedIndex, 0, items.Length - 1);
        }
    }

    private void SetEditorEnabled(bool isEnabled)
    {
        foreach (var control in new Control[]
        {
            _soundList,
            _enumNameBox,
            _filenameBox,
            _streamBox,
            _loopBox,
            _volumeBox,
            _minDistanceBox,
            _maxDistanceBox,
            _audioTypeBox,
            _falloffBox,
            _pitchRandomisationBox,
            _priorityBox,
            _crossfadeBox,
            _basePitchBox,
            _dopplerFactorBox,
            _localizedBox,
            _crcBox,
            _filterCrcBox,
            _effectCrcsBox,
            _roomNameCrcBox
        })
        {
            control.IsEnabled = isEnabled;
        }
    }

    private string SuggestedBankName()
    {
        return _sourcePath is null
            ? "Audio"
            : Path.GetFileNameWithoutExtension(_sourcePath);
    }

    private string SuggestedEnumName()
    {
        var name = SuggestedBankName().Replace(" ", "", StringComparison.Ordinal);
        return string.IsNullOrWhiteSpace(name) ? "Audio" : name + "Audio";
    }

    private static string DescribeBank(string path, AudioBank bank)
    {
        return string.Join(
            Environment.NewLine,
            $"Opened: {path}",
            $"Sound files: {bank.SoundFiles.Count}",
            $"Filters: {bank.Filters.Count}",
            $"Reverbs: {bank.Reverbs.Count}",
            $"Rooms: {bank.Rooms.Count}");
    }

    private static void AddTextRow(StackPanel panel, string label, TextBox textBox)
    {
        textBox.MinWidth = 240;
        panel.Children.Add(new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("140,*"),
            Children =
            {
                new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center },
                AtColumn(textBox, 1)
            }
        });
    }

    private static void AddCheckRow(StackPanel panel, string label, CheckBox checkBox)
    {
        panel.Children.Add(new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("140,*"),
            Children =
            {
                new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center },
                AtColumn(checkBox, 1)
            }
        });
    }

    private static T AtColumn<T>(T control, int column)
        where T : Control
    {
        Grid.SetColumn(control, column);
        return control;
    }

    private static Border Wrap(string title, Control content, int column = 0, int row = 0)
    {
        var panel = new DockPanel();
        var header = new TextBlock
        {
            Text = title,
            FontWeight = FontWeight.SemiBold,
            Margin = new Avalonia.Thickness(8, 6),
            VerticalAlignment = VerticalAlignment.Center
        };
        DockPanel.SetDock(header, Dock.Top);
        panel.Children.Add(header);
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

    private static float ParseFloat(string? value, string field, List<string> errors)
    {
        if (float.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed))
        {
            return parsed;
        }

        errors.Add($"{field} must be a floating-point number.");
        return 0;
    }

    private static int ParseInt(string? value, string field, List<string> errors)
    {
        if (int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out var parsed))
        {
            return parsed;
        }

        errors.Add($"{field} must be an integer.");
        return 0;
    }

    private static uint ParseUInt(string? value, string field, List<string> errors)
    {
        if (uint.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out var parsed))
        {
            return parsed;
        }

        errors.Add($"{field} must be an unsigned integer.");
        return 0;
    }

    private static List<uint> ParseUIntList(string? value, string field, List<string> errors)
    {
        var values = new List<uint>();
        if (string.IsNullOrWhiteSpace(value))
        {
            return values;
        }

        foreach (var part in value.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            if (uint.TryParse(part, NumberStyles.Integer, CultureInfo.InvariantCulture, out var parsed))
            {
                values.Add(parsed);
            }
            else
            {
                errors.Add($"{field} contains an invalid unsigned integer: {part}");
            }
        }

        return values;
    }

    private static string FormatFloat(float value) =>
        value.ToString("G9", CultureInfo.InvariantCulture);

    private void WriteDiagnostics(string message)
    {
        _diagnostics.Text = message;
    }

    private static FilePickerFileType YamlFileType() =>
        new("YAML files") { Patterns = ["*.yml", "*.yaml"] };

    private static FilePickerFileType ProtoFileType() =>
        new("Protocol Buffers") { Patterns = ["*.proto"] };

    private static FilePickerFileType HeaderFileType() =>
        new("C/C++ headers") { Patterns = ["*.h", "*.hpp"] };

    private sealed record AudioSoundListItem(int Index, SoundFileDefinition Sound)
    {
        public override string ToString()
        {
            return $"{Index + 1,3}  {Sound.EnumName}  {Sound.Filename}";
        }
    }
}
