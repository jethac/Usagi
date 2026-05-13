using Usagi.ToolCore.Entities;

namespace Usagi.ToolShell;

internal sealed class EntityTreeItem
{
    public EntityTreeItem(EntityNode node)
    {
        Node = node;
        Children = node.Children.Select(child => new EntityTreeItem(child)).ToArray();
    }

    public EntityNode Node { get; }
    public IReadOnlyList<EntityTreeItem> Children { get; }

    public override string ToString() => Node.DisplayName;
}
