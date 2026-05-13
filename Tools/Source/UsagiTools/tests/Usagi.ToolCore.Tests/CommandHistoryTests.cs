using Usagi.ToolCore.Commands;
using Usagi.ToolCore.Entities;
using Xunit;

namespace Usagi.ToolCore.Tests;

public sealed class CommandHistoryTests
{
    [Fact]
    public void ExecuteMarksHistoryDirty()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("Test");
        var command = new RenameEntityCommand(entity, "NewName");

        history.Execute(command);

        Assert.True(history.IsDirty);
        Assert.True(history.CanUndo);
        Assert.False(history.CanRedo);
    }

    [Fact]
    public void UndoRestoresPreviousState()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("OriginalName");
        entity.AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = "OriginalName" });

        var command = new RenameEntityCommand(entity, "NewName");
        history.Execute(command);

        Assert.Equal("NewName", entity.DisplayName);

        history.Undo();

        Assert.Equal("OriginalName", entity.DisplayName);
        Assert.True(history.CanRedo);
        Assert.False(history.CanUndo);
    }

    [Fact]
    public void RedoReappliesCommand()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("OriginalName");
        entity.AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = "OriginalName" });

        history.Execute(new RenameEntityCommand(entity, "NewName"));
        history.Undo();
        history.Redo();

        Assert.Equal("NewName", entity.DisplayName);
        Assert.True(history.CanUndo);
        Assert.False(history.CanRedo);
    }

    [Fact]
    public void ExecuteAfterUndoClearsRedoStack()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("Test");
        entity.AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = "Test" });

        history.Execute(new RenameEntityCommand(entity, "Name1"));
        history.Undo();

        Assert.True(history.CanRedo);

        history.Execute(new RenameEntityCommand(entity, "Name2"));

        Assert.False(history.CanRedo);
        Assert.Equal("Name2", entity.DisplayName);
    }

    [Fact]
    public void MarkSavedClearsDirtyFlag()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("Test");

        history.Execute(new RenameEntityCommand(entity, "NewName"));
        Assert.True(history.IsDirty);

        history.MarkSaved();
        Assert.False(history.IsDirty);
    }

    [Fact]
    public void UndoAfterSaveMakesDirtyAgain()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("Test");
        entity.AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = "Test" });

        history.Execute(new RenameEntityCommand(entity, "NewName"));
        history.MarkSaved();
        history.Undo();

        Assert.True(history.IsDirty);
    }

    [Fact]
    public void AddChildCommandWorks()
    {
        var history = new CommandHistory();
        var parent = new EditableEntityNode("Parent");

        history.Execute(new AddChildCommand(parent, "Child"));

        Assert.Single(parent.Children);
        Assert.Equal("Child", parent.Children[0].DisplayName);

        history.Undo();

        Assert.Empty(parent.Children);
    }

    [Fact]
    public void RemoveChildCommandWorks()
    {
        var history = new CommandHistory();
        var parent = new EditableEntityNode("Parent");
        var child = parent.AddChild("Child");

        history.Execute(new RemoveChildCommand(parent, child));

        Assert.Empty(parent.Children);

        history.Undo();

        Assert.Single(parent.Children);
        Assert.Equal("Child", parent.Children[0].DisplayName);
    }

    [Fact]
    public void AddComponentCommandWorks()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("Entity");

        history.Execute(new AddComponentCommand(entity, "TransformComponent"));

        Assert.Single(entity.Components);
        Assert.Equal("TransformComponent", entity.Components[0].Name);

        history.Undo();

        Assert.Empty(entity.Components);
    }

    [Fact]
    public void RemoveComponentCommandWorks()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("Entity");
        entity.AddComponent("TransformComponent");

        history.Execute(new RemoveComponentCommand(entity, "TransformComponent"));

        Assert.Empty(entity.Components);

        history.Undo();

        Assert.Single(entity.Components);
        Assert.Equal("TransformComponent", entity.Components[0].Name);
    }

    [Fact]
    public void ChangedEventFiresOnOperations()
    {
        var history = new CommandHistory();
        var entity = new EditableEntityNode("Test");
        var changeCount = 0;

        history.Changed += () => changeCount++;

        history.Execute(new RenameEntityCommand(entity, "Name1"));
        history.Undo();
        history.Redo();
        history.MarkSaved();
        history.Clear();

        Assert.Equal(5, changeCount);
    }
}
