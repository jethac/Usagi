using System.Diagnostics;
using System.Text;

namespace Usagi.ToolCore.Process;

public sealed record ProcessResult(
    int ExitCode,
    string StandardOutput,
    string StandardError,
    string? SourceAsset = null)
{
    public bool Success => ExitCode == 0;

    public IEnumerable<string> AllLines =>
        StandardOutput.Split('\n', StringSplitOptions.RemoveEmptyEntries)
            .Concat(StandardError.Split('\n', StringSplitOptions.RemoveEmptyEntries));
}

public sealed class ProcessRunner
{
    public async Task<ProcessResult> RunAsync(
        string executable,
        IEnumerable<string> arguments,
        string? workingDirectory = null,
        string? sourceAsset = null,
        CancellationToken cancellationToken = default)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = executable,
            WorkingDirectory = workingDirectory ?? Environment.CurrentDirectory,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        foreach (var arg in arguments)
        {
            startInfo.ArgumentList.Add(arg);
        }

        var stdout = new StringBuilder();
        var stderr = new StringBuilder();

        using var process = new System.Diagnostics.Process { StartInfo = startInfo };

        process.OutputDataReceived += (_, e) =>
        {
            if (e.Data is not null)
            {
                stdout.AppendLine(e.Data);
            }
        };

        process.ErrorDataReceived += (_, e) =>
        {
            if (e.Data is not null)
            {
                stderr.AppendLine(e.Data);
            }
        };

        process.Start();
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        await process.WaitForExitAsync(cancellationToken);

        return new ProcessResult(
            process.ExitCode,
            stdout.ToString(),
            stderr.ToString(),
            sourceAsset);
    }

    public ProcessResult Run(
        string executable,
        IEnumerable<string> arguments,
        string? workingDirectory = null,
        string? sourceAsset = null,
        int timeoutMs = 30000)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = executable,
            WorkingDirectory = workingDirectory ?? Environment.CurrentDirectory,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        foreach (var arg in arguments)
        {
            startInfo.ArgumentList.Add(arg);
        }

        using var process = new System.Diagnostics.Process { StartInfo = startInfo };
        process.Start();

        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();

        var exited = process.WaitForExit(timeoutMs);
        if (!exited)
        {
            process.Kill(entireProcessTree: true);
            return new ProcessResult(-1, stdout, stderr + "\nProcess timed out.", sourceAsset);
        }

        return new ProcessResult(process.ExitCode, stdout, stderr, sourceAsset);
    }
}
