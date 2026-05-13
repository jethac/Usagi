using Usagi.ToolCore.Entities;

namespace Usagi.ToolCore.Commands;

public sealed class RenameEntityCommand : ICommand
{
    private readonly EditableEntityNode _entity;
    private readonly string _oldName;
    private readonly string _newName;

    public string Description => $"Rename to {_newName}";

    public RenameEntityCommand(EditableEntityNode entity, string newName)
    {
        _entity = entity;
        _oldName = entity.DisplayName;
        _newName = newName;
    }

    public void Execute() => _entity.Rename(_newName);
    public void Undo() => _entity.Rename(_oldName);
}

public sealed class AddChildCommand : ICommand
{
    private readonly EditableEntityNode _parent;
    private readonly string _childName;
    private EditableEntityNode? _addedChild;

    public string Description => $"Add child {_childName}";

    public AddChildCommand(EditableEntityNode parent, string childName)
    {
        _parent = parent;
        _childName = childName;
    }

    public void Execute()
    {
        _addedChild = _parent.AddChild(_childName);
    }

    public void Undo()
    {
        if (_addedChild is not null)
        {
            _parent.RemoveChild(_addedChild);
        }
    }
}

public sealed class RemoveChildCommand : ICommand
{
    private readonly EditableEntityNode _parent;
    private readonly EditableEntityNode _child;
    private int _index;

    public string Description => $"Remove {_child.DisplayName}";

    public RemoveChildCommand(EditableEntityNode parent, EditableEntityNode child)
    {
        _parent = parent;
        _child = child;
    }

    public void Execute()
    {
        _index = _parent.Children.IndexOf(_child);
        _parent.RemoveChild(_child);
    }

    public void Undo()
    {
        if (_index >= 0 && _index <= _parent.Children.Count)
        {
            _parent.Children.Insert(_index, _child);
        }
        else
        {
            _parent.Children.Add(_child);
        }
    }
}

public sealed class AddComponentCommand : ICommand
{
    private readonly EditableEntityNode _entity;
    private readonly string _componentName;
    private readonly Dictionary<string, object?>? _fields;
    private EditableComponent? _addedComponent;

    public string Description => $"Add {_componentName}";

    public AddComponentCommand(EditableEntityNode entity, string componentName, Dictionary<string, object?>? fields = null)
    {
        _entity = entity;
        _componentName = componentName;
        _fields = fields;
    }

    public void Execute()
    {
        _addedComponent = _entity.AddComponent(_componentName, _fields);
    }

    public void Undo()
    {
        if (_addedComponent is not null)
        {
            _entity.Components.Remove(_addedComponent);
        }
    }
}

public sealed class RemoveComponentCommand : ICommand
{
    private readonly EditableEntityNode _entity;
    private readonly string _componentName;
    private EditableComponent? _removedComponent;
    private int _index;

    public string Description => $"Remove {_componentName}";

    public RemoveComponentCommand(EditableEntityNode entity, string componentName)
    {
        _entity = entity;
        _componentName = componentName;
    }

    public void Execute()
    {
        _removedComponent = _entity.Components.FirstOrDefault(c => c.Name == _componentName);
        if (_removedComponent is not null)
        {
            _index = _entity.Components.IndexOf(_removedComponent);
            _entity.Components.Remove(_removedComponent);
        }
    }

    public void Undo()
    {
        if (_removedComponent is not null)
        {
            if (_index >= 0 && _index <= _entity.Components.Count)
            {
                _entity.Components.Insert(_index, _removedComponent);
            }
            else
            {
                _entity.Components.Add(_removedComponent);
            }
        }
    }
}
