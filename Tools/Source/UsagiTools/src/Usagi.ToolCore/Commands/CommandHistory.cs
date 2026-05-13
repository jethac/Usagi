namespace Usagi.ToolCore.Commands;

public sealed class CommandHistory
{
    private readonly Stack<ICommand> _undoStack = new();
    private readonly Stack<ICommand> _redoStack = new();
    private int _savePoint;

    public event Action? Changed;

    public bool CanUndo => _undoStack.Count > 0;
    public bool CanRedo => _redoStack.Count > 0;
    public bool IsDirty => _undoStack.Count != _savePoint;

    public string? UndoDescription => _undoStack.TryPeek(out var cmd) ? cmd.Description : null;
    public string? RedoDescription => _redoStack.TryPeek(out var cmd) ? cmd.Description : null;

    public void Execute(ICommand command)
    {
        command.Execute();
        _undoStack.Push(command);
        _redoStack.Clear();
        Changed?.Invoke();
    }

    public void Undo()
    {
        if (!CanUndo) return;

        var command = _undoStack.Pop();
        command.Undo();
        _redoStack.Push(command);
        Changed?.Invoke();
    }

    public void Redo()
    {
        if (!CanRedo) return;

        var command = _redoStack.Pop();
        command.Execute();
        _undoStack.Push(command);
        Changed?.Invoke();
    }

    public void MarkSaved()
    {
        _savePoint = _undoStack.Count;
        Changed?.Invoke();
    }

    public void Clear()
    {
        _undoStack.Clear();
        _redoStack.Clear();
        _savePoint = 0;
        Changed?.Invoke();
    }
}
