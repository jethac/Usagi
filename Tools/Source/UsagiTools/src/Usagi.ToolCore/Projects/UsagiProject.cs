namespace Usagi.ToolCore.Projects;

public sealed record UsagiProject(string RootPath)
{
    public string DataPath => Path.Combine(RootPath, "Data");
    public string EntitiesPath => Path.Combine(DataPath, "Entities");
}
