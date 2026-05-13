using System.Text.Json;

namespace Usagi.ToolCore.Settings;

public sealed class WorkspaceSettings
{
    private static readonly string SettingsDirectory =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "UsagiTools");

    private static readonly string SettingsPath =
        Path.Combine(SettingsDirectory, "workspace.json");

    public string? LastProjectPath { get; set; }
    public List<string> RecentProjects { get; set; } = [];
    public string? RubyExecutable { get; set; }

    public static WorkspaceSettings Load()
    {
        if (!File.Exists(SettingsPath))
        {
            return new WorkspaceSettings();
        }

        try
        {
            var json = File.ReadAllText(SettingsPath);
            return JsonSerializer.Deserialize<WorkspaceSettings>(json) ?? new WorkspaceSettings();
        }
        catch
        {
            return new WorkspaceSettings();
        }
    }

    public void Save()
    {
        Directory.CreateDirectory(SettingsDirectory);

        var options = new JsonSerializerOptions { WriteIndented = true };
        var json = JsonSerializer.Serialize(this, options);
        File.WriteAllText(SettingsPath, json);
    }

    public void AddRecentProject(string projectPath)
    {
        RecentProjects.Remove(projectPath);
        RecentProjects.Insert(0, projectPath);

        if (RecentProjects.Count > 10)
        {
            RecentProjects.RemoveRange(10, RecentProjects.Count - 10);
        }

        LastProjectPath = projectPath;
    }
}
