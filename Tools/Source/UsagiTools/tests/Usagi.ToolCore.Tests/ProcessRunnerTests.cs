using Usagi.ToolCore.Process;
using Xunit;

namespace Usagi.ToolCore.Tests;

public sealed class ProcessRunnerTests
{
    private readonly ProcessRunner _runner = new();

    [Fact]
    public void RunCapturesStandardOutput()
    {
        var result = _runner.Run("cmd.exe", ["/c", "echo hello"]);

        Assert.Equal(0, result.ExitCode);
        Assert.True(result.Success);
        Assert.Contains("hello", result.StandardOutput);
    }

    [Fact]
    public void RunCapturesExitCode()
    {
        var result = _runner.Run("cmd.exe", ["/c", "exit 42"]);

        Assert.Equal(42, result.ExitCode);
        Assert.False(result.Success);
    }

    [Fact]
    public void RunCapturesStandardError()
    {
        var result = _runner.Run("cmd.exe", ["/c", "echo error 1>&2"]);

        Assert.Contains("error", result.StandardError);
    }

    [Fact]
    public void RunPreservesSourceAsset()
    {
        var result = _runner.Run("cmd.exe", ["/c", "echo test"], sourceAsset: "test.yml");

        Assert.Equal("test.yml", result.SourceAsset);
    }

    [Fact]
    public async Task RunAsyncCapturesOutput()
    {
        var result = await _runner.RunAsync("cmd.exe", ["/c", "echo async"]);

        Assert.True(result.Success);
        Assert.Contains("async", result.StandardOutput);
    }

    [Fact]
    public void AllLinesAggregatesOutputAndError()
    {
        var result = _runner.Run("cmd.exe", ["/c", "echo out && echo err 1>&2"]);

        var lines = result.AllLines.ToList();
        Assert.Contains("out", lines[0]);
    }
}
