namespace Usagi.ToolCore.Entities;

public sealed record EntityDocument(
    string SourcePath,
    EntityNode Root,
    IReadOnlyList<string> Diagnostics);
