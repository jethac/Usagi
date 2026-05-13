using System.Globalization;

namespace Usagi.ToolCore.Audio;

public sealed class AudioBank
{
    public List<SoundFileDefinition> SoundFiles { get; } = [];
}

public sealed class SoundFileDefinition
{
    public string EnumName { get; set; } = "";
    public string Filename { get; set; } = "";
    public bool Stream { get; set; }
    public bool Loop { get; set; }
    public float Volume { get; set; } = 1.0f;
    public float MinDistance { get; set; } = 1.0f;
    public float MaxDistance { get; set; } = 1000.0f;
    public int AudioType { get; set; } = 1;
    public int Falloff { get; set; }
    public float PitchRandomisation { get; set; }
    public int Priority { get; set; } = 128;
    public string Crossfade { get; set; } = "";
    public float BasePitch { get; set; } = 1.0f;
    public float DopplerFactor { get; set; }
    public bool Localized { get; set; }
    public uint Crc { get; set; }

    internal static float ParseFloat(string value, float fallback)
    {
        return float.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var parsed)
            ? parsed
            : fallback;
    }
}
