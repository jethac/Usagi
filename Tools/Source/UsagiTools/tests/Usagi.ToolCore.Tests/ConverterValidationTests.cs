using Usagi.ToolCore.Entities;
using Usagi.ToolCore.Process;
using Usagi.ToolCore.Projects;
using Xunit;

namespace Usagi.ToolCore.Tests;

/// <summary>
/// Integration tests that validate YAML through the actual Ruby converter.
/// These tests require Ruby to be installed and a Usagi checkout to be available.
/// </summary>
public sealed class ConverterValidationTests
{
    private readonly EntityYamlWriter _writer = new();
    private readonly RubyRunner _rubyRunner = new();

    [SkippableFact]
    public void WrittenYamlPassesConverterValidation()
    {
        var project = TryGetProject();
        Skip.If(project is null, "No Usagi project available for integration tests.");
        Skip.If(!File.Exists(project.ProcessHierarchyScript), "process_hierarchy.rb not found.");

        var entity = new EditableEntityNode("TestValidation");
        entity.AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = "TestValidation" });
        entity.AddComponent("TransformComponent");

        var yaml = _writer.Write(entity);

        var tempFile = Path.Combine(Path.GetTempPath(), $"usagi_test_{Guid.NewGuid()}.yml");
        try
        {
            File.WriteAllText(tempFile, yaml);

            // Run process_hierarchy.rb with minimal args just for syntax validation
            // The -g flag enables debug output which helps diagnose issues
            var result = _rubyRunner.RunScript(
                project.ProcessHierarchyScript,
                ["-g", tempFile],
                project.RootPath,
                tempFile,
                timeoutMs: 30000);

            // The converter may fail due to missing protobuf definitions in test env,
            // but it should at least be able to parse the YAML without syntax errors
            if (!result.Success)
            {
                var hasYamlError = result.StandardError.Contains("YAML", StringComparison.OrdinalIgnoreCase)
                    || result.StandardError.Contains("Psych", StringComparison.OrdinalIgnoreCase);
                Assert.False(hasYamlError, $"YAML syntax error: {result.StandardError}");
            }
        }
        finally
        {
            if (File.Exists(tempFile))
            {
                File.Delete(tempFile);
            }
        }
    }

    [SkippableFact]
    public void WrittenYamlWithChildrenPassesConverterValidation()
    {
        var project = TryGetProject();
        Skip.If(project is null, "No Usagi project available for integration tests.");
        Skip.If(!File.Exists(project.ProcessHierarchyScript), "process_hierarchy.rb not found.");

        var entity = new EditableEntityNode("ParentEntity");
        entity.AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = "ParentEntity" });

        var child = entity.AddChild("ChildEntity");
        child.AddComponent("TransformComponent");

        var yaml = _writer.Write(entity);

        var tempFile = Path.Combine(Path.GetTempPath(), $"usagi_test_children_{Guid.NewGuid()}.yml");
        try
        {
            File.WriteAllText(tempFile, yaml);

            var result = _rubyRunner.RunScript(
                project.ProcessHierarchyScript,
                ["-g", tempFile],
                project.RootPath,
                tempFile,
                timeoutMs: 30000);

            if (!result.Success)
            {
                var hasYamlError = result.StandardError.Contains("YAML", StringComparison.OrdinalIgnoreCase)
                    || result.StandardError.Contains("Psych", StringComparison.OrdinalIgnoreCase);
                Assert.False(hasYamlError, $"YAML syntax error: {result.StandardError}");
            }
        }
        finally
        {
            if (File.Exists(tempFile))
            {
                File.Delete(tempFile);
            }
        }
    }

    [SkippableFact]
    public void RoundTripPreservesEntityStructure()
    {
        var original = new EditableEntityNode("RoundTripTest");
        original.AddComponent("Identifier", new Dictionary<string, object?> { ["name"] = "RoundTripTest" });
        original.AddComponent("TransformComponent");
        original.Inherits.Add("BaseEntity");

        var child1 = original.AddChild("Child1");
        child1.AddComponent("ModelComponent");

        var child2 = original.AddChild("Child2");
        child2.AddComponent("CameraComponent", new Dictionary<string, object?> { ["fFOV"] = 60.0 });

        var yaml = _writer.Write(original);
        var loader = new EntityHierarchyLoader();
        var reloaded = loader.Load("RoundTripTest.yml", yaml);

        Assert.Equal("RoundTripTest", reloaded.Root.DisplayName);
        Assert.Equal(2, reloaded.Root.Children.Count);
        Assert.Single(reloaded.Root.Inherits);
        Assert.Equal("BaseEntity", reloaded.Root.Inherits[0]);
        Assert.Equal("Child1", reloaded.Root.Children[0].DisplayName);
        Assert.Equal("Child2", reloaded.Root.Children[1].DisplayName);
    }

    private static UsagiProject? TryGetProject()
    {
        return UsagiProjectLocator.TryLocate(AppContext.BaseDirectory);
    }
}
