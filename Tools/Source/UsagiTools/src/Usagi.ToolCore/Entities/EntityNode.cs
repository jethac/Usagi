namespace Usagi.ToolCore.Entities;

public sealed class EntityNode
{
    public EntityNode(
        string displayName,
        IReadOnlyList<EntityComponent> components,
        IReadOnlyList<EntityNode> children,
        IReadOnlyList<string> inherits,
        int overrideCount,
        int initializerEventCount)
    {
        DisplayName = displayName;
        Components = components;
        Children = children;
        Inherits = inherits;
        OverrideCount = overrideCount;
        InitializerEventCount = initializerEventCount;
    }

    public string DisplayName { get; }
    public IReadOnlyList<EntityComponent> Components { get; }
    public IReadOnlyList<EntityNode> Children { get; }
    public IReadOnlyList<string> Inherits { get; }
    public int OverrideCount { get; }
    public int InitializerEventCount { get; }

    public string ComponentSummary =>
        Components.Count == 0 ? "No components" : string.Join(", ", Components.Select(component => component.Name));
}
