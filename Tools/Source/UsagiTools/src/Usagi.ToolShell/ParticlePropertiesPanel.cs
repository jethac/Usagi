// Usagi.ToolShell - Particle Properties Panel

using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Usagi.ToolCore.Particles;

namespace Usagi.ToolShell;

/// <summary>
/// Property editor panel for particle emitters and effects.
/// </summary>
public sealed class ParticlePropertiesPanel : UserControl
{
    private readonly StackPanel _content = new()
    {
        Spacing = 4,
        Margin = new Thickness(8)
    };

    private readonly ScrollViewer _scrollViewer;
    private EditableEmitterDocument? _emitterDoc;
    private EditableEffectDocument? _effectDoc;

    public event Action? DocumentChanged;

    public ParticlePropertiesPanel()
    {
        _scrollViewer = new ScrollViewer
        {
            Content = _content,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto
        };
        Content = _scrollViewer;
    }

    public void ShowEmitter(EditableEmitterDocument doc)
    {
        _emitterDoc = doc;
        _effectDoc = null;
        RebuildForEmitter(doc.Emitter);
    }

    public void ShowEffect(EditableEffectDocument doc)
    {
        _effectDoc = doc;
        _emitterDoc = null;
        RebuildForEffect(doc.Effect);
    }

    public void Clear()
    {
        _emitterDoc = null;
        _effectDoc = null;
        _content.Children.Clear();
    }

    private void RebuildForEmitter(EditableEmitter emitter)
    {
        _content.Children.Clear();

        AddHeader("Emitter: " + emitter.Name);
        AddSeparator();

        // Emission section
        AddSectionHeader("Emission");
        AddEnumField("Type", emitter.Emission.Type, v => {
            _emitterDoc?.Emitter.Emission.Type = v;
            // Note: Would use command history for proper undo
        });
        AddFloatField("Emission Time", emitter.Emission.EmissionTime, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Emission.EmissionTime = v;
        });
        AddIntField("Max Particles", emitter.Emission.MaxParticles, v => {
            _emitterDoc?.SetMaxParticles(v);
            DocumentChanged?.Invoke();
        });

        AddSeparator();

        // Life section
        AddSectionHeader("Particle Life");
        if (emitter.Life.Frames.Count > 0)
        {
            AddFloatField("Lifespan", emitter.Life.Frames[0].Value, v => {
                _emitterDoc?.SetLifespan(v);
                DocumentChanged?.Invoke();
            });
        }
        AddFloatField("Life Randomness", emitter.LifeRandomness, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.LifeRandomness = v;
        });

        AddSeparator();

        // Shape section
        AddSectionHeader("Shape");
        AddEnumField("Shape", emitter.Shape, v => {
            _emitterDoc?.SetShape(v);
            DocumentChanged?.Invoke();
        });
        AddFloatField("Position Randomness", emitter.PositionRandomness, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.PositionRandomness = v;
        });

        AddSeparator();

        // Velocity section
        AddSectionHeader("Velocity");
        AddFloatField("Cone Angle (deg)", emitter.VelocityDirConeDeg, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.VelocityDirConeDeg = v;
        });
        AddFloatField("Speed Randomness", emitter.SpeedRandomness, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.SpeedRandomness = v;
        });
        AddFloatField("Drag", emitter.Drag, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Drag = v;
        });
        AddFloatField("Gravity", emitter.GravityStrength, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.GravityStrength = v;
        });

        AddSeparator();

        // Texture section
        AddSectionHeader("Textures");
        for (int i = 0; i < emitter.Textures.Count; i++)
        {
            var idx = i;
            AddTextField($"Texture {i}", emitter.Textures[i].Name, v => {
                _emitterDoc?.SetTexture(idx, v);
                DocumentChanged?.Invoke();
            });
        }

        AddSeparator();

        // Rotation section
        AddSectionHeader("Rotation");
        AddFloatField("Base Rotation", emitter.Rotation.BaseRotation, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Rotation.BaseRotation = v;
        });
        AddFloatField("Rotation Randomness", emitter.Rotation.Randomise, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Rotation.Randomise = v;
        });
        AddFloatField("Rotation Speed", emitter.Rotation.Speed, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Rotation.Speed = v;
        });

        AddSeparator();

        // Scale section
        AddSectionHeader("Scale");
        AddFloatField("Scale Randomness", emitter.Scale.Randomness, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Scale.Randomness = v;
        });
        AddFloatField("Initial Scale", emitter.Scale.Initial, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Scale.Initial = v;
        });
        AddFloatField("Ending Scale", emitter.Scale.Ending, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Scale.Ending = v;
        });

        AddSeparator();

        // Alpha section
        AddSectionHeader("Alpha");
        AddFloatField("Initial Alpha", emitter.Alpha.InitialAlpha, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Alpha.InitialAlpha = v;
        });
        AddFloatField("Peak Alpha", emitter.Alpha.IntermediateAlpha, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Alpha.IntermediateAlpha = v;
        });
        AddFloatField("Final Alpha", emitter.Alpha.EndAlpha, v => {
            if (_emitterDoc is not null)
                _emitterDoc.Emitter.Alpha.EndAlpha = v;
        });
    }

    private void RebuildForEffect(EditableEffect effect)
    {
        _content.Children.Clear();

        AddHeader("Effect: " + effect.Name);
        AddSeparator();

        AddIntField("Preload Count", effect.PreloadCount, v => {
            _effectDoc?.SetPreloadCount(v);
            DocumentChanged?.Invoke();
        });

        AddSeparator();
        AddSectionHeader("Emitter Instances");

        for (int i = 0; i < effect.Emitters.Count; i++)
        {
            var em = effect.Emitters[i];
            var idx = i;

            AddSubHeader($"[{i}] {em.EmitterName}");

            AddVec3Field("Position", em.Position, v => {
                _effectDoc?.SetEmitterPosition(em, v);
                DocumentChanged?.Invoke();
            });

            AddVec3Field("Scale", em.Scale, v => {
                _effectDoc?.SetEmitterScale(em, v);
                DocumentChanged?.Invoke();
            });

            AddVec3Field("Rotation", em.Rotation, v => {
                _effectDoc?.SetEmitterRotation(em, v);
                DocumentChanged?.Invoke();
            });

            AddFloatField("Particle Scale", em.ParticleScale, v => {
                if (_effectDoc is not null)
                    _effectDoc.Effect.Emitters[idx].ParticleScale = v;
            });

            AddFloatField("Release Frame", em.ReleaseFrame, v => {
                if (_effectDoc is not null)
                    _effectDoc.Effect.Emitters[idx].ReleaseFrame = v;
            });

            AddSeparator();
        }
    }

    // UI Helpers

    private void AddHeader(string text)
    {
        _content.Children.Add(new TextBlock
        {
            Text = text,
            FontWeight = FontWeight.Bold,
            FontSize = 14,
            Margin = new Thickness(0, 0, 0, 4)
        });
    }

    private void AddSectionHeader(string text)
    {
        _content.Children.Add(new TextBlock
        {
            Text = text,
            FontWeight = FontWeight.SemiBold,
            Foreground = Brushes.DimGray,
            Margin = new Thickness(0, 8, 0, 4)
        });
    }

    private void AddSubHeader(string text)
    {
        _content.Children.Add(new TextBlock
        {
            Text = text,
            FontWeight = FontWeight.Medium,
            Margin = new Thickness(0, 4, 0, 2)
        });
    }

    private void AddSeparator()
    {
        _content.Children.Add(new Border
        {
            Height = 1,
            Background = Brushes.LightGray,
            Margin = new Thickness(0, 4)
        });
    }

    private void AddFloatField(string label, float value, Action<float> onChanged)
    {
        var row = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("120,*")
        };

        var labelBlock = new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(labelBlock, 0);
        row.Children.Add(labelBlock);

        var textBox = new TextBox
        {
            Text = value.ToString("F3"),
            MaxWidth = 100
        };
        textBox.LostFocus += (_, _) =>
        {
            if (float.TryParse(textBox.Text, out var newValue))
            {
                onChanged(newValue);
            }
            else
            {
                textBox.Text = value.ToString("F3");
            }
        };
        Grid.SetColumn(textBox, 1);
        row.Children.Add(textBox);

        _content.Children.Add(row);
    }

    private void AddIntField(string label, int value, Action<int> onChanged)
    {
        var row = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("120,*")
        };

        var labelBlock = new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(labelBlock, 0);
        row.Children.Add(labelBlock);

        var textBox = new TextBox
        {
            Text = value.ToString(),
            MaxWidth = 100
        };
        textBox.LostFocus += (_, _) =>
        {
            if (int.TryParse(textBox.Text, out var newValue))
            {
                onChanged(newValue);
            }
            else
            {
                textBox.Text = value.ToString();
            }
        };
        Grid.SetColumn(textBox, 1);
        row.Children.Add(textBox);

        _content.Children.Add(row);
    }

    private void AddTextField(string label, string value, Action<string> onChanged)
    {
        var row = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("120,*")
        };

        var labelBlock = new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(labelBlock, 0);
        row.Children.Add(labelBlock);

        var textBox = new TextBox
        {
            Text = value
        };
        textBox.LostFocus += (_, _) =>
        {
            if (textBox.Text != value)
            {
                onChanged(textBox.Text ?? "");
            }
        };
        Grid.SetColumn(textBox, 1);
        row.Children.Add(textBox);

        _content.Children.Add(row);
    }

    private void AddEnumField<T>(string label, T value, Action<T> onChanged) where T : struct, Enum
    {
        var row = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("120,*")
        };

        var labelBlock = new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(labelBlock, 0);
        row.Children.Add(labelBlock);

        var combo = new ComboBox
        {
            ItemsSource = Enum.GetValues<T>(),
            SelectedItem = value,
            MaxWidth = 150
        };
        combo.SelectionChanged += (_, _) =>
        {
            if (combo.SelectedItem is T newValue)
            {
                onChanged(newValue);
            }
        };
        Grid.SetColumn(combo, 1);
        row.Children.Add(combo);

        _content.Children.Add(row);
    }

    private void AddVec3Field(string label, Vec3 value, Action<Vec3> onChanged)
    {
        var row = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("120,*")
        };

        var labelBlock = new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(labelBlock, 0);
        row.Children.Add(labelBlock);

        var vecPanel = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 4
        };

        var xBox = CreateVecComponent("X", value.X);
        var yBox = CreateVecComponent("Y", value.Y);
        var zBox = CreateVecComponent("Z", value.Z);

        void UpdateVec()
        {
            if (float.TryParse(xBox.Text, out var x) &&
                float.TryParse(yBox.Text, out var y) &&
                float.TryParse(zBox.Text, out var z))
            {
                onChanged(new Vec3(x, y, z));
            }
        }

        xBox.LostFocus += (_, _) => UpdateVec();
        yBox.LostFocus += (_, _) => UpdateVec();
        zBox.LostFocus += (_, _) => UpdateVec();

        vecPanel.Children.Add(xBox);
        vecPanel.Children.Add(yBox);
        vecPanel.Children.Add(zBox);

        Grid.SetColumn(vecPanel, 1);
        row.Children.Add(vecPanel);

        _content.Children.Add(row);
    }

    private static TextBox CreateVecComponent(string prefix, float value)
    {
        return new TextBox
        {
            Text = value.ToString("F2"),
            Width = 60,
            PlaceholderText = prefix
        };
    }
}
