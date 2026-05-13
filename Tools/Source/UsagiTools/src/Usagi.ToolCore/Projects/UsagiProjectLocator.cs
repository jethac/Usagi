namespace Usagi.ToolCore.Projects;

public static class UsagiProjectLocator
{
    public static UsagiProject Locate(string? startDirectory = null)
    {
        var environmentRoot = Environment.GetEnvironmentVariable("USAGI_REPO_DIR");
        if (!string.IsNullOrWhiteSpace(environmentRoot) && IsUsagiRoot(environmentRoot))
        {
            return new UsagiProject(Path.GetFullPath(environmentRoot));
        }

        var current = new DirectoryInfo(startDirectory ?? Environment.CurrentDirectory);
        while (current is not null)
        {
            if (IsUsagiRoot(current.FullName))
            {
                return new UsagiProject(current.FullName);
            }

            current = current.Parent;
        }

        throw new DirectoryNotFoundException("Could not locate a Usagi checkout. Set USAGI_REPO_DIR or launch from inside the repo.");
    }

    private static bool IsUsagiRoot(string path)
    {
        return Directory.Exists(Path.Combine(path, "Engine")) &&
               Directory.Exists(Path.Combine(path, "Data")) &&
               Directory.Exists(Path.Combine(path, "Tools"));
    }
}
