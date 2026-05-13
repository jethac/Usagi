namespace Usagi.ToolCore.Assets;

public sealed record AssetItem(string Name, string FullPath, string RelativePath)
{
    public override string ToString() => RelativePath;
}

public static class AssetInventory
{
    public static IReadOnlyList<AssetItem> EnumerateEntityAssets(string entitiesDirectory)
    {
        if (!Directory.Exists(entitiesDirectory))
        {
            return Array.Empty<AssetItem>();
        }

        return Directory.EnumerateFiles(entitiesDirectory, "*.yml", SearchOption.AllDirectories)
            .Concat(Directory.EnumerateFiles(entitiesDirectory, "*.yaml", SearchOption.AllDirectories))
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
            .Select(path =>
            {
                var relative = Path.GetRelativePath(entitiesDirectory, path);
                return new AssetItem(Path.GetFileNameWithoutExtension(path), path, relative);
            })
            .ToArray();
    }
}
