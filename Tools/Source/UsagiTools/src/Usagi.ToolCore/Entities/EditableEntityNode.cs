namespace Usagi.ToolCore.Entities;

public sealed class EditableEntityNode
{
    public string DisplayName { get; set; }
    public List<EditableComponent> Components { get; } = [];
    public List<EditableEntityNode> Children { get; } = [];
    public List<string> Inherits { get; } = [];
    public List<EditableOverride> Overrides { get; } = [];
    public List<Dictionary<string, object?>> InitializerEvents { get; } = [];

    public EditableEntityNode(string displayName)
    {
        DisplayName = displayName;
    }

    public static EditableEntityNode FromReadOnly(EntityNode node)
    {
        var editable = new EditableEntityNode(node.DisplayName);

        foreach (var component in node.Components)
        {
            editable.Components.Add(new EditableComponent(component.Name));
        }

        editable.Inherits.AddRange(node.Inherits);

        foreach (var child in node.Children)
        {
            editable.Children.Add(FromReadOnly(child));
        }

        return editable;
    }

    public EditableEntityNode AddChild(string name)
    {
        var child = new EditableEntityNode(name);
        child.Components.Add(new EditableComponent("Identifier", new Dictionary<string, object?> { ["name"] = name }));
        Children.Add(child);
        return child;
    }

    public void RemoveChild(EditableEntityNode child)
    {
        Children.Remove(child);
    }

    public EditableComponent AddComponent(string name, Dictionary<string, object?>? fields = null)
    {
        var component = new EditableComponent(name, fields);
        Components.Add(component);
        return component;
    }

    public bool RemoveComponent(string name)
    {
        var component = Components.FirstOrDefault(c => c.Name == name);
        if (component is null)
        {
            return false;
        }

        Components.Remove(component);
        return true;
    }

    public void Rename(string newName)
    {
        DisplayName = newName;
        var identifier = Components.FirstOrDefault(c => c.Name == "Identifier");
        if (identifier is not null)
        {
            identifier.Fields["name"] = newName;
        }
        else
        {
            AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = newName });
        }
    }
}

public sealed class EditableComponent
{
    public string Name { get; set; }
    public Dictionary<string, object?> Fields { get; }

    public EditableComponent(string name, Dictionary<string, object?>? fields = null)
    {
        Name = name;
        Fields = fields ?? [];
    }
}

public sealed class EditableOverride
{
    public string TargetEntityId { get; set; }
    public Dictionary<string, Dictionary<string, object?>> ComponentOverrides { get; } = [];

    public EditableOverride(string targetEntityId)
    {
        TargetEntityId = targetEntityId;
    }
}
