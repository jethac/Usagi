using YamlDotNet.RepresentationModel;

namespace Usagi.ToolCore.Audio;

public static class AudioBankYamlParser
{
    public static AudioBank ParseFile(string path)
    {
        var content = File.ReadAllText(path);
        return Parse(content);
    }

    public static AudioBank Parse(string yaml)
    {
        var stream = new YamlStream();
        stream.Load(new StringReader(yaml));

        var bank = new AudioBank();
        if (stream.Documents.Count == 0 ||
            stream.Documents[0].RootNode is not YamlMappingNode root ||
            !TryGetMapping(root, "AudioBank", out var audioBankNode))
        {
            return bank;
        }

        if (!TryGetSequence(audioBankNode, "soundFiles", out var soundFiles))
        {
            return bank;
        }

        foreach (var item in soundFiles.Children)
        {
            if (item is YamlMappingNode soundFileNode)
            {
                bank.SoundFiles.Add(ParseSoundFile(soundFileNode));
            }
        }

        return bank;
    }

    private static SoundFileDefinition ParseSoundFile(YamlMappingNode node)
    {
        var sound = new SoundFileDefinition();
        foreach (var entry in node.Children)
        {
            var key = Scalar(entry.Key);
            var value = Scalar(entry.Value);
            switch (key)
            {
                case "enumName":
                    sound.EnumName = value;
                    break;
                case "filename":
                    sound.Filename = Path.GetFileNameWithoutExtension(value);
                    break;
                case "stream":
                    sound.Stream = ParseBool(value);
                    break;
                case "loop":
                    sound.Loop = ParseBool(value);
                    break;
                case "volume":
                    sound.Volume = SoundFileDefinition.ParseFloat(value, sound.Volume);
                    break;
                case "minDistance":
                    sound.MinDistance = SoundFileDefinition.ParseFloat(value, sound.MinDistance);
                    break;
                case "maxDistance":
                    sound.MaxDistance = SoundFileDefinition.ParseFloat(value, sound.MaxDistance);
                    break;
                case "eType":
                    sound.AudioType = ParseInt(value, sound.AudioType);
                    break;
                case "eFalloff":
                    sound.Falloff = ParseInt(value, sound.Falloff);
                    break;
                case "pitchRandomisation":
                    sound.PitchRandomisation = SoundFileDefinition.ParseFloat(value, sound.PitchRandomisation);
                    break;
                case "priority":
                    sound.Priority = ParseInt(value, sound.Priority);
                    break;
                case "crossfade":
                    sound.Crossfade = value;
                    break;
                case "basePitch":
                    sound.BasePitch = SoundFileDefinition.ParseFloat(value, sound.BasePitch);
                    break;
                case "dopplerFactor":
                    sound.DopplerFactor = SoundFileDefinition.ParseFloat(value, sound.DopplerFactor);
                    break;
                case "localized":
                    sound.Localized = ParseBool(value);
                    break;
                case "crc":
                    sound.Crc = ParseUInt(value, sound.Crc);
                    break;
            }
        }

        return sound;
    }

    private static bool TryGetMapping(YamlMappingNode node, string key, out YamlMappingNode mapping)
    {
        if (node.Children.TryGetValue(new YamlScalarNode(key), out var value) && value is YamlMappingNode found)
        {
            mapping = found;
            return true;
        }

        mapping = new YamlMappingNode();
        return false;
    }

    private static bool TryGetSequence(YamlMappingNode node, string key, out YamlSequenceNode sequence)
    {
        if (node.Children.TryGetValue(new YamlScalarNode(key), out var value) && value is YamlSequenceNode found)
        {
            sequence = found;
            return true;
        }

        sequence = new YamlSequenceNode();
        return false;
    }

    private static string Scalar(YamlNode node) =>
        node is YamlScalarNode scalar ? scalar.Value ?? "" : "";

    private static bool ParseBool(string value) =>
        value.Equals("true", StringComparison.OrdinalIgnoreCase);

    private static int ParseInt(string value, int fallback) =>
        int.TryParse(value, out var parsed) ? parsed : fallback;

    private static uint ParseUInt(string value, uint fallback) =>
        uint.TryParse(value, out var parsed) ? parsed : fallback;
}
