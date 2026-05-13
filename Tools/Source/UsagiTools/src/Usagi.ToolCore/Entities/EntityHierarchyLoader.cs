using YamlDotNet.RepresentationModel;

namespace Usagi.ToolCore.Entities;

public sealed class EntityHierarchyLoader
{
    private static readonly HashSet<string> ReservedKeys = new(StringComparer.Ordinal)
    {
        "Children",
        "Inherits",
        "Overrides",
        "InitializerEvents"
    };

    public EntityDocument LoadFile(string path)
    {
        var text = File.ReadAllText(path);
        return Load(path, text);
    }

    public EntityDocument Load(string sourcePath, string text)
    {
        var diagnostics = new List<string>();
        if (text.Contains("<%", StringComparison.Ordinal))
        {
            diagnostics.Add("Template expressions are present; this first slice parses the source YAML without evaluating ERB.");
        }

        var yaml = new YamlStream();
        using var reader = new StringReader(text);
        yaml.Load(reader);

        if (yaml.Documents.Count == 0 || yaml.Documents[0].RootNode is not YamlMappingNode root)
        {
            throw new InvalidDataException($"{sourcePath} must contain a YAML mapping at the document root.");
        }

        var rootName = Path.GetFileNameWithoutExtension(sourcePath);
        var rootNode = BuildEntity(rootName, root, diagnostics);
        return new EntityDocument(sourcePath, rootNode, diagnostics);
    }

    private static EntityNode BuildEntity(string fallbackName, YamlMappingNode node, List<string> diagnostics)
    {
        var name = GetIdentifierName(node) ?? fallbackName;
        var inherits = ReadStringList(node, "Inherits");
        var children = ReadChildren(node, diagnostics);
        var components = ReadComponents(node);
        var overrideCount = CountSequenceItems(node, "Overrides");
        var initializerEventCount = CountSequenceItems(node, "InitializerEvents");

        return new EntityNode(name, components, children, inherits, overrideCount, initializerEventCount);
    }

    private static IReadOnlyList<EntityComponent> ReadComponents(YamlMappingNode node)
    {
        return node.Children
            .Select(pair => (Key: ScalarValue(pair.Key), Value: pair.Value))
            .Where(pair => pair.Key is not null && !ReservedKeys.Contains(pair.Key))
            .Select(pair => new EntityComponent(pair.Key!, Summarize(pair.Value)))
            .ToArray();
    }

    private static IReadOnlyList<EntityNode> ReadChildren(YamlMappingNode node, List<string> diagnostics)
    {
        var childrenNode = GetNode(node, "Children");
        if (childrenNode is null || childrenNode.NodeType == YamlNodeType.Scalar && string.IsNullOrWhiteSpace(ScalarValue(childrenNode)))
        {
            return Array.Empty<EntityNode>();
        }

        if (childrenNode is YamlSequenceNode sequence)
        {
            return sequence.Children
                .OfType<YamlMappingNode>()
                .Select((child, index) => BuildEntity($"Child{index}", child, diagnostics))
                .ToArray();
        }

        if (childrenNode is YamlMappingNode mapping)
        {
            return mapping.Children
                .Where(pair => pair.Value is YamlMappingNode)
                .Select(pair => BuildEntity(ScalarValue(pair.Key) ?? "Child", (YamlMappingNode)pair.Value, diagnostics))
                .ToArray();
        }

        diagnostics.Add("Children is present but is not a sequence or mapping.");
        return Array.Empty<EntityNode>();
    }

    private static IReadOnlyList<string> ReadStringList(YamlMappingNode node, string key)
    {
        var value = GetNode(node, key);
        if (value is null)
        {
            return Array.Empty<string>();
        }

        if (value is YamlSequenceNode sequence)
        {
            return sequence.Children
                .Select(ScalarValue)
                .Where(item => !string.IsNullOrWhiteSpace(item))
                .Select(item => item!)
                .ToArray();
        }

        var scalar = ScalarValue(value);
        return string.IsNullOrWhiteSpace(scalar) ? Array.Empty<string>() : new[] { scalar! };
    }

    private static int CountSequenceItems(YamlMappingNode node, string key)
    {
        return GetNode(node, key) is YamlSequenceNode sequence ? sequence.Children.Count : 0;
    }

    private static string? GetIdentifierName(YamlMappingNode node)
    {
        if (GetNode(node, "Identifier") is not YamlMappingNode identifier)
        {
            return null;
        }

        return GetNode(identifier, "name") is { } name ? ScalarValue(name) : null;
    }

    private static YamlNode? GetNode(YamlMappingNode node, string key)
    {
        foreach (var pair in node.Children)
        {
            if (ScalarValue(pair.Key) == key)
            {
                return pair.Value;
            }
        }

        return null;
    }

    private static string Summarize(YamlNode node)
    {
        return node switch
        {
            YamlMappingNode mapping => $"{mapping.Children.Count} fields",
            YamlSequenceNode sequence => $"{sequence.Children.Count} items",
            YamlScalarNode scalar when string.IsNullOrWhiteSpace(scalar.Value) => "present",
            YamlScalarNode scalar => scalar.Value ?? "present",
            _ => node.NodeType.ToString()
        };
    }

    private static string? ScalarValue(YamlNode node)
    {
        return node is YamlScalarNode scalar ? scalar.Value : null;
    }
}
