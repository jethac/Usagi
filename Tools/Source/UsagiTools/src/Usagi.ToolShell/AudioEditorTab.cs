using System.Globalization;
using System.Text;
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
    private readonly ListBox _filterList = new();
    private readonly ListBox _reverbList = new();
    private readonly ListBox _roomList = new();
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
    private readonly TextBox _wavMetadataBox = new();

    private readonly TextBox _filterNameBox = new();
    private readonly TextBox _filterCrcValueBox = new();
    private readonly TextBox _filterTypeBox = new();
    private readonly TextBox _filterFrequencyBox = new();
    private readonly TextBox _filterOneOverQBox = new();

    private readonly TextBox _reverbNameBox = new();
    private readonly TextBox _reverbCrcBox = new();
    private readonly TextBox _reverbEffectTypeBox = new();
    private readonly TextBox _wetDryMixBox = new();
    private readonly TextBox _reflectionsDelayBox = new();
    private readonly TextBox _reverbDelayBox = new();
    private readonly TextBox _roomFilterFreqBox = new();
    private readonly TextBox _roomFilterMainBox = new();
    private readonly TextBox _roomFilterHfBox = new();
    private readonly TextBox _reflectionsGainBox = new();
    private readonly TextBox _reverbGainBox = new();
    private readonly TextBox _decayTimeBox = new();
    private readonly TextBox _densityBox = new();
    private readonly TextBox _roomSizeBox = new();

    private readonly TextBox _roomNameBox = new();
    private readonly TextBox _roomCrcBox = new();
    private readonly TextBox _roomFilterCrcBox = new();
    private readonly TextBox _roomEffectCrcsBox = new();

    private UsagiProject? _project;
    private AudioBank? _bank;
    private string? _sourcePath;

    public event Action? DocumentChanged;

    public bool IsDirty { get; private set; }

    public AudioEditorTab()
    {
        Content = BuildLayout();
        _soundList.SelectionChanged += (_, _) => ShowSelectedSound();
        _filterList.SelectionChanged += (_, _) => ShowSelectedFilter();
        _reverbList.SelectionChanged += (_, _) => ShowSelectedReverb();
        _roomList.SelectionChanged += (_, _) => ShowSelectedRoom();
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
        _diagnostics.Height = 150;

        var diagnosticsPanel = Wrap("Diagnostics", _diagnostics);
        DockPanel.SetDock(diagnosticsPanel, Dock.Bottom);
        root.Children.Add(diagnosticsPanel);

        root.Children.Add(new TabControl
        {
            ItemsSource = new Control[]
            {
                new TabItem { Header = "Sounds", Content = BuildSoundTab() },
                new TabItem { Header = "Filters", Content = BuildFilterTab() },
                new TabItem { Header = "Reverbs", Content = BuildReverbTab() },
                new TabItem { Header = "Rooms", Content = BuildRoomTab() }
            }
        });

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

    private Control BuildSoundTab()
    {
        return BuildListEditorTab("Sound Files", _soundList, BuildSoundListButtons(), "Sound", BuildSoundEditor());
    }

    private Control BuildFilterTab()
    {
        return BuildListEditorTab("Filters", _filterList, BuildFilterListButtons(), "Filter", BuildFilterEditor());
    }

    private Control BuildReverbTab()
    {
        return BuildListEditorTab("Reverbs", _reverbList, BuildReverbListButtons(), "Reverb", BuildReverbEditor());
    }

    private Control BuildRoomTab()
    {
        return BuildListEditorTab("Rooms", _roomList, BuildRoomListButtons(), "Room", BuildRoomEditor());
    }

    private static Control BuildListEditorTab(string listTitle, ListBox list, Control buttons, string editorTitle, Control editor)
    {
        var listPanel = new DockPanel();
        DockPanel.SetDock(buttons, Dock.Top);
        listPanel.Children.Add(buttons);
        listPanel.Children.Add(list);

        var body = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("360,*")
        };
        body.Children.Add(Wrap(listTitle, listPanel, 0, 0));
        body.Children.Add(Wrap(editorTitle, editor, 1, 0));
        return body;
    }

    private Control BuildSoundListButtons()
    {
        var addButton = new Button { Content = "Add", Margin = new Avalonia.Thickness(4) };
        addButton.Click += (_, _) => AddSound();

        var duplicateButton = new Button { Content = "Duplicate", Margin = new Avalonia.Thickness(4) };
        duplicateButton.Click += (_, _) => DuplicateSound();

        var removeButton = new Button { Content = "Remove", Margin = new Avalonia.Thickness(4) };
        removeButton.Click += (_, _) => RemoveSound();

        return new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Avalonia.Thickness(4),
            Children = { addButton, duplicateButton, removeButton }
        };
    }

    private Control BuildSoundEditor()
    {
        _wavMetadataBox.IsReadOnly = true;
        _wavMetadataBox.AcceptsReturn = true;
        _wavMetadataBox.Height = 110;
        _wavMetadataBox.FontFamily = FontFamily.Parse("Consolas");

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

        var browseButton = new Button { Content = "Browse WAV", Margin = new Avalonia.Thickness(0, 8, 4, 0) };
        browseButton.Click += async (_, _) => await BrowseSoundWavAsync();

        var metadataButton = new Button { Content = "Read Metadata", Margin = new Avalonia.Thickness(4, 8, 4, 0) };
        metadataButton.Click += (_, _) => UpdateSoundMetadata(_filenameBox.Text);

        var crcButton = new Button { Content = "Refresh CRC", Margin = new Avalonia.Thickness(4, 8, 4, 0) };
        crcButton.Click += (_, _) => RefreshSoundCrc();

        var previewButton = new Button { Content = "Preview", Margin = new Avalonia.Thickness(4, 8, 4, 0) };
        previewButton.Click += (_, _) => PreviewSound();

        var stopButton = new Button { Content = "Stop", Margin = new Avalonia.Thickness(4, 8, 0, 0) };
        stopButton.Click += (_, _) => StopPreview();

        panel.Children.Add(new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Children = { browseButton, metadataButton, crcButton, previewButton, stopButton }
        });
        panel.Children.Add(_wavMetadataBox);

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

    private Control BuildFilterListButtons()
    {
        var addButton = new Button { Content = "Add", Margin = new Avalonia.Thickness(4) };
        addButton.Click += (_, _) => AddFilter();

        var duplicateButton = new Button { Content = "Duplicate", Margin = new Avalonia.Thickness(4) };
        duplicateButton.Click += (_, _) => DuplicateFilter();

        var removeButton = new Button { Content = "Remove", Margin = new Avalonia.Thickness(4) };
        removeButton.Click += (_, _) => RemoveFilter();

        return new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Avalonia.Thickness(4),
            Children = { addButton, duplicateButton, removeButton }
        };
    }

    private Control BuildFilterEditor()
    {
        var panel = new StackPanel { Margin = new Avalonia.Thickness(10), Spacing = 6 };
        AddTextRow(panel, "Enum Name", _filterNameBox);
        AddTextRow(panel, "CRC", _filterCrcValueBox);
        AddTextRow(panel, "Filter Type", _filterTypeBox);
        AddTextRow(panel, "Frequency", _filterFrequencyBox);
        AddTextRow(panel, "One Over Q", _filterOneOverQBox);
        AddActionRow(panel, ("Refresh CRC", RefreshFilterCrc), ("Apply Changes", ApplyFilterChanges));
        return new ScrollViewer { Content = panel };
    }

    private Control BuildReverbListButtons()
    {
        var addButton = new Button { Content = "Add", Margin = new Avalonia.Thickness(4) };
        addButton.Click += (_, _) => AddReverb();

        var duplicateButton = new Button { Content = "Duplicate", Margin = new Avalonia.Thickness(4) };
        duplicateButton.Click += (_, _) => DuplicateReverb();

        var removeButton = new Button { Content = "Remove", Margin = new Avalonia.Thickness(4) };
        removeButton.Click += (_, _) => RemoveReverb();

        return new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Avalonia.Thickness(4),
            Children = { addButton, duplicateButton, removeButton }
        };
    }

    private Control BuildReverbEditor()
    {
        var panel = new StackPanel { Margin = new Avalonia.Thickness(10), Spacing = 6 };
        AddTextRow(panel, "Enum Name", _reverbNameBox);
        AddTextRow(panel, "CRC", _reverbCrcBox);
        AddTextRow(panel, "Effect Type", _reverbEffectTypeBox);
        AddTextRow(panel, "Wet Dry Mix", _wetDryMixBox);
        AddTextRow(panel, "Reflections Delay", _reflectionsDelayBox);
        AddTextRow(panel, "Reverb Delay", _reverbDelayBox);
        AddTextRow(panel, "Room Filter Freq", _roomFilterFreqBox);
        AddTextRow(panel, "Room Filter Main", _roomFilterMainBox);
        AddTextRow(panel, "Room Filter HF", _roomFilterHfBox);
        AddTextRow(panel, "Reflections Gain", _reflectionsGainBox);
        AddTextRow(panel, "Reverb Gain", _reverbGainBox);
        AddTextRow(panel, "Decay Time", _decayTimeBox);
        AddTextRow(panel, "Density", _densityBox);
        AddTextRow(panel, "Room Size", _roomSizeBox);
        AddActionRow(panel, ("Refresh CRC", RefreshReverbCrc), ("Apply Changes", ApplyReverbChanges));
        return new ScrollViewer { Content = panel };
    }

    private Control BuildRoomListButtons()
    {
        var addButton = new Button { Content = "Add", Margin = new Avalonia.Thickness(4) };
        addButton.Click += (_, _) => AddRoom();

        var duplicateButton = new Button { Content = "Duplicate", Margin = new Avalonia.Thickness(4) };
        duplicateButton.Click += (_, _) => DuplicateRoom();

        var removeButton = new Button { Content = "Remove", Margin = new Avalonia.Thickness(4) };
        removeButton.Click += (_, _) => RemoveRoom();

        return new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Avalonia.Thickness(4),
            Children = { addButton, duplicateButton, removeButton }
        };
    }

    private Control BuildRoomEditor()
    {
        var panel = new StackPanel { Margin = new Avalonia.Thickness(10), Spacing = 6 };
        AddTextRow(panel, "Room Name", _roomNameBox);
        AddTextRow(panel, "Room CRC", _roomCrcBox);
        AddTextRow(panel, "Filter CRC", _roomFilterCrcBox);
        AddTextRow(panel, "Effect CRCs", _roomEffectCrcsBox);
        AddActionRow(panel, ("Refresh CRC", RefreshRoomCrc), ("Apply Changes", ApplyRoomChanges));
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

            RefreshAllLists();
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

    private async Task BrowseSoundWavAsync()
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel is null)
        {
            return;
        }

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Select WAV",
            AllowMultiple = false,
            FileTypeFilter = [WavFileType()]
        });

        if (files.Count == 0)
        {
            return;
        }

        var wavPath = files[0].Path.LocalPath;
        _filenameBox.Text = ToAudioFilename(wavPath);
        UpdateSoundMetadata(_filenameBox.Text);
    }

    private void ShowSelectedSound()
    {
        if (_soundList.SelectedItem is not AudioSoundListItem item)
        {
            ClearSoundEditor();
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
        UpdateSoundMetadata(sound.Filename, false);
    }

    private void AddSound()
    {
        if (_bank is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        var sound = new SoundFileDefinition
        {
            EnumName = UniqueName("NewSound", _bank.SoundFiles.Select(item => item.EnumName)),
            Filename = "new_sound"
        };
        sound.Crc = FsidBuilder.ComputeSoundCrc(sound.EnumName);
        _bank.SoundFiles.Add(sound);
        MarkDirty();
        RefreshSoundList(_bank.SoundFiles.Count - 1);
        WriteDiagnostics($"Added sound: {sound.EnumName}");
    }

    private void DuplicateSound()
    {
        if (_bank is null || _soundList.SelectedItem is not AudioSoundListItem item)
        {
            return;
        }

        var sound = CloneSound(item.Sound);
        sound.EnumName = UniqueName(item.Sound.EnumName + "_Copy", _bank.SoundFiles.Select(entry => entry.EnumName));
        sound.Filename = string.IsNullOrWhiteSpace(item.Sound.Filename) ? "new_sound" : item.Sound.Filename + "_copy";
        sound.Crc = FsidBuilder.ComputeSoundCrc(sound.EnumName);
        _bank.SoundFiles.Insert(item.Index + 1, sound);
        MarkDirty();
        RefreshSoundList(item.Index + 1);
        WriteDiagnostics($"Duplicated sound: {sound.EnumName}");
    }

    private void RemoveSound()
    {
        if (_bank is null || _soundList.SelectedItem is not AudioSoundListItem item)
        {
            return;
        }

        var removed = item.Sound.EnumName;
        _bank.SoundFiles.RemoveAt(item.Index);
        MarkDirty();
        RefreshSoundList(Math.Min(item.Index, _bank.SoundFiles.Count - 1));
        WriteDiagnostics($"Removed sound: {removed}");
    }

    private void RefreshSoundCrc()
    {
        _crcBox.Text = FsidBuilder.ComputeSoundCrc(_enumNameBox.Text ?? "").ToString(CultureInfo.InvariantCulture);
    }

    private void PreviewSound()
    {
        var wavPath = ResolveWavPath(_filenameBox.Text);
        if (wavPath is null)
        {
            WriteDiagnostics($"WAV not found: {_filenameBox.Text}");
            return;
        }

        try
        {
            WavePreviewPlayer.Play(wavPath);
            WriteDiagnostics($"Previewing: {wavPath}");
        }
        catch (Exception ex) when (ex is InvalidOperationException or IOException)
        {
            WriteDiagnostics($"Preview failed: {ex.Message}");
        }
    }

    private void StopPreview()
    {
        WavePreviewPlayer.Stop();
        WriteDiagnostics("Preview stopped.");
    }

    private void UpdateSoundMetadata(string? filename, bool reportMissing = true)
    {
        var wavPath = ResolveWavPath(filename);
        if (wavPath is null)
        {
            _wavMetadataBox.Text = reportMissing ? $"WAV not found: {filename}" : "";
            return;
        }

        try
        {
            var metadata = WaveMetadataReader.ReadFile(wavPath);
            _wavMetadataBox.Text = string.Join(
                Environment.NewLine,
                wavPath,
                $"Format: {metadata.FormatId}",
                $"Channels: {metadata.ChannelCount}",
                $"Sample Rate: {metadata.SampleRate}",
                $"Bits: {metadata.BitsPerSample}",
                $"Data Bytes: {metadata.DataSize}",
                $"Duration: {metadata.Duration.TotalSeconds:0.###}s",
                metadata.ForwardLoop is null
                    ? "Forward Loop: none"
                    : $"Forward Loop: {metadata.ForwardLoop.Start}-{metadata.ForwardLoop.End} ({metadata.ForwardLoop.Length})");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            _wavMetadataBox.Text = $"Metadata failed: {ex.Message}";
        }
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

        MarkDirty();
        RefreshSoundList(item.Index);
        WriteDiagnostics($"Updated sound: {sound.EnumName}");
    }

    private void ShowSelectedFilter()
    {
        if (_filterList.SelectedItem is not AudioFilterListItem item)
        {
            ClearTextBoxes(_filterNameBox, _filterCrcValueBox, _filterTypeBox, _filterFrequencyBox, _filterOneOverQBox);
            return;
        }

        var filter = item.Filter;
        _filterNameBox.Text = filter.EnumName;
        _filterCrcValueBox.Text = filter.Crc.ToString(CultureInfo.InvariantCulture);
        _filterTypeBox.Text = filter.FilterType.ToString(CultureInfo.InvariantCulture);
        _filterFrequencyBox.Text = FormatFloat(filter.Frequency);
        _filterOneOverQBox.Text = FormatFloat(filter.OneOverQ);
    }

    private void AddFilter()
    {
        if (_bank is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        var filter = new AudioFilterDefinition
        {
            EnumName = UniqueName("NewFilter", _bank.Filters.Select(item => item.EnumName))
        };
        filter.Crc = ComputeNameCrc(filter.EnumName);
        _bank.Filters.Add(filter);
        MarkDirty();
        RefreshFilterList(_bank.Filters.Count - 1);
        WriteDiagnostics($"Added filter: {filter.EnumName}");
    }

    private void DuplicateFilter()
    {
        if (_bank is null || _filterList.SelectedItem is not AudioFilterListItem item)
        {
            return;
        }

        var filter = new AudioFilterDefinition
        {
            EnumName = UniqueName(item.Filter.EnumName + "_Copy", _bank.Filters.Select(entry => entry.EnumName)),
            FilterType = item.Filter.FilterType,
            Frequency = item.Filter.Frequency,
            OneOverQ = item.Filter.OneOverQ
        };
        filter.Crc = ComputeNameCrc(filter.EnumName);
        _bank.Filters.Insert(item.Index + 1, filter);
        MarkDirty();
        RefreshFilterList(item.Index + 1);
        WriteDiagnostics($"Duplicated filter: {filter.EnumName}");
    }

    private void RemoveFilter()
    {
        if (_bank is null || _filterList.SelectedItem is not AudioFilterListItem item)
        {
            return;
        }

        var removed = item.Filter.EnumName;
        _bank.Filters.RemoveAt(item.Index);
        MarkDirty();
        RefreshFilterList(Math.Min(item.Index, _bank.Filters.Count - 1));
        WriteDiagnostics($"Removed filter: {removed}");
    }

    private void RefreshFilterCrc()
    {
        _filterCrcValueBox.Text = ComputeNameCrc(_filterNameBox.Text ?? "").ToString(CultureInfo.InvariantCulture);
    }

    private void ApplyFilterChanges()
    {
        if (_filterList.SelectedItem is not AudioFilterListItem item)
        {
            return;
        }

        var errors = new List<string>();
        var crc = ParseUInt(_filterCrcValueBox.Text, "CRC", errors);
        var type = ParseInt(_filterTypeBox.Text, "Filter Type", errors);
        var frequency = ParseFloat(_filterFrequencyBox.Text, "Frequency", errors);
        var oneOverQ = ParseFloat(_filterOneOverQBox.Text, "One Over Q", errors);
        if (errors.Count > 0)
        {
            WriteDiagnostics(string.Join(Environment.NewLine, errors));
            return;
        }

        item.Filter.EnumName = _filterNameBox.Text?.Trim() ?? "";
        item.Filter.Crc = crc;
        item.Filter.FilterType = type;
        item.Filter.Frequency = frequency;
        item.Filter.OneOverQ = oneOverQ;
        MarkDirty();
        RefreshFilterList(item.Index);
        WriteDiagnostics($"Updated filter: {item.Filter.EnumName}");
    }

    private void ShowSelectedReverb()
    {
        if (_reverbList.SelectedItem is not AudioReverbListItem item)
        {
            ClearTextBoxes(
                _reverbNameBox, _reverbCrcBox, _reverbEffectTypeBox, _wetDryMixBox, _reflectionsDelayBox,
                _reverbDelayBox, _roomFilterFreqBox, _roomFilterMainBox, _roomFilterHfBox, _reflectionsGainBox,
                _reverbGainBox, _decayTimeBox, _densityBox, _roomSizeBox);
            return;
        }

        var reverb = item.Reverb;
        _reverbNameBox.Text = reverb.Effect.EnumName;
        _reverbCrcBox.Text = reverb.Effect.Crc.ToString(CultureInfo.InvariantCulture);
        _reverbEffectTypeBox.Text = reverb.Effect.EffectType.ToString(CultureInfo.InvariantCulture);
        _wetDryMixBox.Text = FormatFloat(reverb.WetDryMix);
        _reflectionsDelayBox.Text = reverb.ReflectionsDelay.ToString(CultureInfo.InvariantCulture);
        _reverbDelayBox.Text = reverb.ReverbDelay.ToString(CultureInfo.InvariantCulture);
        _roomFilterFreqBox.Text = FormatFloat(reverb.RoomFilterFreq);
        _roomFilterMainBox.Text = FormatFloat(reverb.RoomFilterMain);
        _roomFilterHfBox.Text = FormatFloat(reverb.RoomFilterHf);
        _reflectionsGainBox.Text = FormatFloat(reverb.ReflectionsGain);
        _reverbGainBox.Text = FormatFloat(reverb.ReverbGain);
        _decayTimeBox.Text = FormatFloat(reverb.DecayTime);
        _densityBox.Text = FormatFloat(reverb.Density);
        _roomSizeBox.Text = FormatFloat(reverb.RoomSize);
    }

    private void AddReverb()
    {
        if (_bank is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        var reverb = new ReverbEffectDefinition();
        reverb.Effect.EnumName = UniqueName("NewReverb", _bank.Reverbs.Select(item => item.Effect.EnumName));
        reverb.Effect.Crc = ComputeNameCrc(reverb.Effect.EnumName);
        _bank.Reverbs.Add(reverb);
        MarkDirty();
        RefreshReverbList(_bank.Reverbs.Count - 1);
        WriteDiagnostics($"Added reverb: {reverb.Effect.EnumName}");
    }

    private void DuplicateReverb()
    {
        if (_bank is null || _reverbList.SelectedItem is not AudioReverbListItem item)
        {
            return;
        }

        var reverb = CloneReverb(item.Reverb);
        reverb.Effect.EnumName = UniqueName(item.Reverb.Effect.EnumName + "_Copy", _bank.Reverbs.Select(entry => entry.Effect.EnumName));
        reverb.Effect.Crc = ComputeNameCrc(reverb.Effect.EnumName);
        _bank.Reverbs.Insert(item.Index + 1, reverb);
        MarkDirty();
        RefreshReverbList(item.Index + 1);
        WriteDiagnostics($"Duplicated reverb: {reverb.Effect.EnumName}");
    }

    private void RemoveReverb()
    {
        if (_bank is null || _reverbList.SelectedItem is not AudioReverbListItem item)
        {
            return;
        }

        var removed = item.Reverb.Effect.EnumName;
        _bank.Reverbs.RemoveAt(item.Index);
        MarkDirty();
        RefreshReverbList(Math.Min(item.Index, _bank.Reverbs.Count - 1));
        WriteDiagnostics($"Removed reverb: {removed}");
    }

    private void RefreshReverbCrc()
    {
        _reverbCrcBox.Text = ComputeNameCrc(_reverbNameBox.Text ?? "").ToString(CultureInfo.InvariantCulture);
    }

    private void ApplyReverbChanges()
    {
        if (_reverbList.SelectedItem is not AudioReverbListItem item)
        {
            return;
        }

        var errors = new List<string>();
        var crc = ParseUInt(_reverbCrcBox.Text, "CRC", errors);
        var effectType = ParseInt(_reverbEffectTypeBox.Text, "Effect Type", errors);
        var wetDryMix = ParseFloat(_wetDryMixBox.Text, "Wet Dry Mix", errors);
        var reflectionsDelay = ParseInt(_reflectionsDelayBox.Text, "Reflections Delay", errors);
        var reverbDelay = ParseInt(_reverbDelayBox.Text, "Reverb Delay", errors);
        var roomFilterFreq = ParseFloat(_roomFilterFreqBox.Text, "Room Filter Freq", errors);
        var roomFilterMain = ParseFloat(_roomFilterMainBox.Text, "Room Filter Main", errors);
        var roomFilterHf = ParseFloat(_roomFilterHfBox.Text, "Room Filter HF", errors);
        var reflectionsGain = ParseFloat(_reflectionsGainBox.Text, "Reflections Gain", errors);
        var reverbGain = ParseFloat(_reverbGainBox.Text, "Reverb Gain", errors);
        var decayTime = ParseFloat(_decayTimeBox.Text, "Decay Time", errors);
        var density = ParseFloat(_densityBox.Text, "Density", errors);
        var roomSize = ParseFloat(_roomSizeBox.Text, "Room Size", errors);
        if (errors.Count > 0)
        {
            WriteDiagnostics(string.Join(Environment.NewLine, errors));
            return;
        }

        var reverb = item.Reverb;
        reverb.Effect.EnumName = _reverbNameBox.Text?.Trim() ?? "";
        reverb.Effect.Crc = crc;
        reverb.Effect.EffectType = effectType;
        reverb.WetDryMix = wetDryMix;
        reverb.ReflectionsDelay = reflectionsDelay;
        reverb.ReverbDelay = reverbDelay;
        reverb.RoomFilterFreq = roomFilterFreq;
        reverb.RoomFilterMain = roomFilterMain;
        reverb.RoomFilterHf = roomFilterHf;
        reverb.ReflectionsGain = reflectionsGain;
        reverb.ReverbGain = reverbGain;
        reverb.DecayTime = decayTime;
        reverb.Density = density;
        reverb.RoomSize = roomSize;
        MarkDirty();
        RefreshReverbList(item.Index);
        WriteDiagnostics($"Updated reverb: {reverb.Effect.EnumName}");
    }

    private void ShowSelectedRoom()
    {
        if (_roomList.SelectedItem is not AudioRoomListItem item)
        {
            ClearTextBoxes(_roomNameBox, _roomCrcBox, _roomFilterCrcBox, _roomEffectCrcsBox);
            return;
        }

        var room = item.Room;
        _roomNameBox.Text = room.RoomName;
        _roomCrcBox.Text = room.RoomCrc.ToString(CultureInfo.InvariantCulture);
        _roomFilterCrcBox.Text = room.FilterCrc.ToString(CultureInfo.InvariantCulture);
        _roomEffectCrcsBox.Text = string.Join(", ", room.EffectCrcs.Select(crc => crc.ToString(CultureInfo.InvariantCulture)));
    }

    private void AddRoom()
    {
        if (_bank is null)
        {
            WriteDiagnostics("No audio bank is open.");
            return;
        }

        var room = new AudioRoomDefinition
        {
            RoomName = UniqueName("NewRoom", _bank.Rooms.Select(item => item.RoomName))
        };
        room.RoomCrc = ComputeNameCrc(room.RoomName);
        _bank.Rooms.Add(room);
        MarkDirty();
        RefreshRoomList(_bank.Rooms.Count - 1);
        WriteDiagnostics($"Added room: {room.RoomName}");
    }

    private void DuplicateRoom()
    {
        if (_bank is null || _roomList.SelectedItem is not AudioRoomListItem item)
        {
            return;
        }

        var room = new AudioRoomDefinition
        {
            RoomName = UniqueName(item.Room.RoomName + "_Copy", _bank.Rooms.Select(entry => entry.RoomName)),
            FilterCrc = item.Room.FilterCrc
        };
        room.RoomCrc = ComputeNameCrc(room.RoomName);
        room.EffectCrcs.AddRange(item.Room.EffectCrcs);
        _bank.Rooms.Insert(item.Index + 1, room);
        MarkDirty();
        RefreshRoomList(item.Index + 1);
        WriteDiagnostics($"Duplicated room: {room.RoomName}");
    }

    private void RemoveRoom()
    {
        if (_bank is null || _roomList.SelectedItem is not AudioRoomListItem item)
        {
            return;
        }

        var removed = item.Room.RoomName;
        _bank.Rooms.RemoveAt(item.Index);
        MarkDirty();
        RefreshRoomList(Math.Min(item.Index, _bank.Rooms.Count - 1));
        WriteDiagnostics($"Removed room: {removed}");
    }

    private void RefreshRoomCrc()
    {
        _roomCrcBox.Text = ComputeNameCrc(_roomNameBox.Text ?? "").ToString(CultureInfo.InvariantCulture);
    }

    private void ApplyRoomChanges()
    {
        if (_roomList.SelectedItem is not AudioRoomListItem item)
        {
            return;
        }

        var errors = new List<string>();
        var roomCrc = ParseUInt(_roomCrcBox.Text, "Room CRC", errors);
        var filterCrc = ParseUInt(_roomFilterCrcBox.Text, "Filter CRC", errors);
        var effectCrcs = ParseUIntList(_roomEffectCrcsBox.Text, "Effect CRCs", errors);
        if (errors.Count > 0)
        {
            WriteDiagnostics(string.Join(Environment.NewLine, errors));
            return;
        }

        var room = item.Room;
        room.RoomName = _roomNameBox.Text?.Trim() ?? "";
        room.RoomCrc = roomCrc;
        room.FilterCrc = filterCrc;
        room.EffectCrcs.Clear();
        room.EffectCrcs.AddRange(effectCrcs);
        MarkDirty();
        RefreshRoomList(item.Index);
        WriteDiagnostics($"Updated room: {room.RoomName}");
    }

    private void RefreshAllLists()
    {
        RefreshSoundList(0);
        RefreshFilterList(0);
        RefreshReverbList(0);
        RefreshRoomList(0);
    }

    private void RefreshSoundList(int selectedIndex)
    {
        var items = _bank?.SoundFiles.Select((sound, index) => new AudioSoundListItem(index, sound)).ToArray()
            ?? [];
        RefreshList(_soundList, items, selectedIndex);
    }

    private void RefreshFilterList(int selectedIndex)
    {
        var items = _bank?.Filters.Select((filter, index) => new AudioFilterListItem(index, filter)).ToArray()
            ?? [];
        RefreshList(_filterList, items, selectedIndex);
    }

    private void RefreshReverbList(int selectedIndex)
    {
        var items = _bank?.Reverbs.Select((reverb, index) => new AudioReverbListItem(index, reverb)).ToArray()
            ?? [];
        RefreshList(_reverbList, items, selectedIndex);
    }

    private void RefreshRoomList(int selectedIndex)
    {
        var items = _bank?.Rooms.Select((room, index) => new AudioRoomListItem(index, room)).ToArray()
            ?? [];
        RefreshList(_roomList, items, selectedIndex);
    }

    private static void RefreshList<T>(ListBox list, T[] items, int selectedIndex)
    {
        list.ItemsSource = items;
        list.SelectedIndex = items.Length > 0 ? Math.Clamp(selectedIndex, 0, items.Length - 1) : -1;
    }

    private void SetEditorEnabled(bool isEnabled)
    {
        foreach (var control in new Control[]
        {
            _soundList, _filterList, _reverbList, _roomList,
            _enumNameBox, _filenameBox, _streamBox, _loopBox, _volumeBox, _minDistanceBox, _maxDistanceBox,
            _audioTypeBox, _falloffBox, _pitchRandomisationBox, _priorityBox, _crossfadeBox, _basePitchBox,
            _dopplerFactorBox, _localizedBox, _crcBox, _filterCrcBox, _effectCrcsBox, _roomNameCrcBox,
            _filterNameBox, _filterCrcValueBox, _filterTypeBox, _filterFrequencyBox, _filterOneOverQBox,
            _reverbNameBox, _reverbCrcBox, _reverbEffectTypeBox, _wetDryMixBox, _reflectionsDelayBox,
            _reverbDelayBox, _roomFilterFreqBox, _roomFilterMainBox, _roomFilterHfBox, _reflectionsGainBox,
            _reverbGainBox, _decayTimeBox, _densityBox, _roomSizeBox,
            _roomNameBox, _roomCrcBox, _roomFilterCrcBox, _roomEffectCrcsBox
        })
        {
            control.IsEnabled = isEnabled;
        }
    }

    private string? ResolveWavPath(string? filename)
    {
        if (string.IsNullOrWhiteSpace(filename))
        {
            return null;
        }

        var candidate = filename.Trim();
        if (Path.IsPathRooted(candidate) && File.Exists(candidate))
        {
            return candidate;
        }

        if (Path.HasExtension(candidate) && File.Exists(candidate))
        {
            return Path.GetFullPath(candidate);
        }

        if (_project is null)
        {
            return null;
        }

        var relative = candidate.Replace('/', Path.DirectorySeparatorChar);
        var wavPath = Path.Combine(_project.RootPath, "Data", "Audio", relative);
        if (!Path.HasExtension(wavPath))
        {
            wavPath += ".wav";
        }

        return File.Exists(wavPath) ? wavPath : null;
    }

    private string ToAudioFilename(string wavPath)
    {
        var withoutExtension = Path.ChangeExtension(wavPath, null);
        if (_project is null)
        {
            return Path.GetFileName(withoutExtension);
        }

        var audioRoot = Path.Combine(_project.RootPath, "Data", "Audio");
        var relative = Path.GetRelativePath(audioRoot, withoutExtension);
        if (!relative.StartsWith("..", StringComparison.Ordinal) && !Path.IsPathRooted(relative))
        {
            return relative.Replace(Path.DirectorySeparatorChar, '/');
        }

        return Path.GetFileName(withoutExtension);
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

    private static SoundFileDefinition CloneSound(SoundFileDefinition source)
    {
        var sound = new SoundFileDefinition
        {
            EnumName = source.EnumName,
            Filename = source.Filename,
            Stream = source.Stream,
            Loop = source.Loop,
            Volume = source.Volume,
            MinDistance = source.MinDistance,
            MaxDistance = source.MaxDistance,
            AudioType = source.AudioType,
            Falloff = source.Falloff,
            PitchRandomisation = source.PitchRandomisation,
            Priority = source.Priority,
            Crossfade = source.Crossfade,
            BasePitch = source.BasePitch,
            DopplerFactor = source.DopplerFactor,
            Localized = source.Localized,
            Crc = source.Crc,
            FilterCrc = source.FilterCrc,
            RoomNameCrc = source.RoomNameCrc
        };
        sound.EffectCrcs.AddRange(source.EffectCrcs);
        return sound;
    }

    private static ReverbEffectDefinition CloneReverb(ReverbEffectDefinition source)
    {
        var reverb = new ReverbEffectDefinition
        {
            WetDryMix = source.WetDryMix,
            ReflectionsDelay = source.ReflectionsDelay,
            ReverbDelay = source.ReverbDelay,
            RoomFilterFreq = source.RoomFilterFreq,
            RoomFilterMain = source.RoomFilterMain,
            RoomFilterHf = source.RoomFilterHf,
            ReflectionsGain = source.ReflectionsGain,
            ReverbGain = source.ReverbGain,
            DecayTime = source.DecayTime,
            Density = source.Density,
            RoomSize = source.RoomSize
        };
        reverb.Effect.EnumName = source.Effect.EnumName;
        reverb.Effect.Crc = source.Effect.Crc;
        reverb.Effect.EffectType = source.Effect.EffectType;
        return reverb;
    }

    private static string UniqueName(string baseName, IEnumerable<string> existingNames)
    {
        var names = existingNames.ToHashSet(StringComparer.OrdinalIgnoreCase);
        var normalizedBase = string.IsNullOrWhiteSpace(baseName) ? "NewItem" : baseName.Trim();
        if (!names.Contains(normalizedBase))
        {
            return normalizedBase;
        }

        for (var index = 2; ; index++)
        {
            var candidate = normalizedBase + index.ToString(CultureInfo.InvariantCulture);
            if (!names.Contains(candidate))
            {
                return candidate;
            }
        }
    }

    private static uint ComputeNameCrc(string name)
    {
        return Crc32.Compute(Encoding.UTF8.GetBytes(FsidBuilder.NormalizeSoundName(name)));
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

    private static void AddActionRow(StackPanel panel, params (string Label, Action Handler)[] actions)
    {
        var row = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Avalonia.Thickness(0, 8, 0, 0)
        };

        foreach (var action in actions)
        {
            var button = new Button { Content = action.Label, Margin = new Avalonia.Thickness(0, 0, 8, 0) };
            button.Click += (_, _) => action.Handler();
            row.Children.Add(button);
        }

        panel.Children.Add(row);
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

    private void ClearSoundEditor()
    {
        ClearTextBoxes(
            _enumNameBox, _filenameBox, _volumeBox, _minDistanceBox, _maxDistanceBox, _audioTypeBox, _falloffBox,
            _pitchRandomisationBox, _priorityBox, _crossfadeBox, _basePitchBox, _dopplerFactorBox, _crcBox,
            _filterCrcBox, _effectCrcsBox, _roomNameCrcBox, _wavMetadataBox);
        _streamBox.IsChecked = false;
        _loopBox.IsChecked = false;
        _localizedBox.IsChecked = false;
    }

    private static void ClearTextBoxes(params TextBox[] textBoxes)
    {
        foreach (var textBox in textBoxes)
        {
            textBox.Text = "";
        }
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

    private static string FormatFloat(float value)
    {
        return value.ToString("G9", CultureInfo.InvariantCulture);
    }

    private void MarkDirty()
    {
        IsDirty = true;
        DocumentChanged?.Invoke();
    }

    private void WriteDiagnostics(string message)
    {
        _diagnostics.Text = message;
    }

    private static FilePickerFileType YamlFileType() =>
        new("YAML files") { Patterns = ["*.yml", "*.yaml"] };

    private static FilePickerFileType WavFileType() =>
        new("WAV files") { Patterns = ["*.wav"] };

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

    private sealed record AudioFilterListItem(int Index, AudioFilterDefinition Filter)
    {
        public override string ToString()
        {
            return $"{Index + 1,3}  {Filter.EnumName}  {Filter.Crc}";
        }
    }

    private sealed record AudioReverbListItem(int Index, ReverbEffectDefinition Reverb)
    {
        public override string ToString()
        {
            return $"{Index + 1,3}  {Reverb.Effect.EnumName}  {Reverb.Effect.Crc}";
        }
    }

    private sealed record AudioRoomListItem(int Index, AudioRoomDefinition Room)
    {
        public override string ToString()
        {
            return $"{Index + 1,3}  {Room.RoomName}  {Room.RoomCrc}";
        }
    }
}
