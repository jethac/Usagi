using Usagi.ToolCore.Entities;
using Xunit;

namespace Usagi.ToolCore.Tests;

public sealed class EntityHierarchyLoaderTests
{
    [Fact]
    public void LoadReadsComponentsAndIdentifier()
    {
        const string yaml = """
Identifier:
  name: RootEntity
MatrixComponent:
CameraComponent:
  fFOV: 50.0
""";

        var document = new EntityHierarchyLoader().Load("Root.yml", yaml);

        Assert.Equal("RootEntity", document.Root.DisplayName);
        Assert.Equal(new[] { "Identifier", "MatrixComponent", "CameraComponent" }, document.Root.Components.Select(component => component.Name));
        Assert.Empty(document.Root.Children);
    }

    [Fact]
    public void LoadReadsChildrenInheritanceOverridesAndInitializerEvents()
    {
        const string yaml = """
Inherits:
  - BaseEnemy
Overrides:
  - EntityWithID: ChildA
    MatrixComponent:
Children:
  - Identifier:
      name: ChildA
    TransformComponent:
  - Identifier:
      name: ChildB
    ModelComponent:
      name: Models/Test.vmdf
InitializerEvents:
  - SpawnEvent:
      count: 1
""";

        var document = new EntityHierarchyLoader().Load("Enemy.yml", yaml);

        Assert.Equal("Enemy", document.Root.DisplayName);
        Assert.Equal(new[] { "BaseEnemy" }, document.Root.Inherits);
        Assert.Equal(1, document.Root.OverrideCount);
        Assert.Equal(1, document.Root.InitializerEventCount);
        Assert.Equal(new[] { "ChildA", "ChildB" }, document.Root.Children.Select(child => child.DisplayName));
        Assert.Contains(document.Root.Children[1].Components, component => component.Name == "ModelComponent");
    }

    [Fact]
    public void LoadFlagsTemplateExpressionsWithoutEvaluatingThem()
    {
        const string yaml = """
Identifier:
  name: Camera
CameraComponent:
  uCamId: <%= Zlib::crc32('Camera') %>
""";

        var document = new EntityHierarchyLoader().Load("Camera.yml", yaml);

        Assert.Contains(document.Diagnostics, diagnostic => diagnostic.Contains("Template expressions", StringComparison.Ordinal));
        Assert.Equal("Camera", document.Root.DisplayName);
    }
}
