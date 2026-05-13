using Usagi.ToolCore.Projects;

namespace Usagi.ToolCore.Preview;

public sealed class PreviewHostClient : IAsyncDisposable
{
    private readonly UsagiProject _project;
    private System.Diagnostics.Process? _process;
    private StreamWriter? _stdin;
    private Task? _readTask;
    private CancellationTokenSource? _cts;
    private bool _isReady;

    public event Action<IPreviewMessage>? MessageReceived;
    public event Action<string>? DiagnosticReceived;
    public event Action? Disconnected;

    public bool IsRunning => _process is { HasExited: false };
    public bool IsReady => _isReady && IsRunning;

    public PreviewHostClient(UsagiProject project)
    {
        _project = project;
    }

    public string PreviewHostPath => Path.Combine(_project.ToolsPath, "bin", "UsagiPreviewHost.exe");

    public async Task<bool> StartAsync(CancellationToken cancellationToken = default)
    {
        if (IsRunning)
        {
            return true;
        }

        var hostPath = PreviewHostPath;
        if (!File.Exists(hostPath))
        {
            DiagnosticReceived?.Invoke($"Preview host not found: {hostPath}");
            return false;
        }

        _cts = new CancellationTokenSource();

        var startInfo = new System.Diagnostics.ProcessStartInfo
        {
            FileName = hostPath,
            WorkingDirectory = _project.RomfilesWinPath,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        _process = new System.Diagnostics.Process { StartInfo = startInfo };
        _process.ErrorDataReceived += (_, e) =>
        {
            if (e.Data is not null)
            {
                DiagnosticReceived?.Invoke($"[stderr] {e.Data}");
            }
        };

        try
        {
            _process.Start();
            _process.BeginErrorReadLine();

            _stdin = _process.StandardInput;
            _readTask = ReadOutputAsync(_cts.Token);

            // Send init command
            await SendAsync(new InitCommand
            {
                DataPath = _project.DataPath,
                RomfilesPath = _project.RomfilesWinPath
            });

            // Wait for ready response with timeout
            var readyTimeout = Task.Delay(TimeSpan.FromSeconds(10), cancellationToken);
            while (!_isReady && !readyTimeout.IsCompleted && IsRunning)
            {
                await Task.Delay(50, cancellationToken);
            }

            return _isReady;
        }
        catch (Exception ex)
        {
            DiagnosticReceived?.Invoke($"Failed to start preview host: {ex.Message}");
            await StopAsync();
            return false;
        }
    }

    public async Task StopAsync()
    {
        if (_process is null)
        {
            return;
        }

        try
        {
            if (IsRunning)
            {
                await SendAsync(new ShutdownCommand());

                // Give it a moment to shut down gracefully
                using var exitTimeout = new CancellationTokenSource(TimeSpan.FromSeconds(2));
                try
                {
                    await _process.WaitForExitAsync(exitTimeout.Token);
                }
                catch (OperationCanceledException)
                {
                    _process.Kill(entireProcessTree: true);
                }
            }
        }
        catch
        {
            // Best effort
        }
        finally
        {
            _cts?.Cancel();
            _cts?.Dispose();
            _cts = null;

            _stdin?.Dispose();
            _stdin = null;

            _process?.Dispose();
            _process = null;

            _isReady = false;

            if (_readTask is not null)
            {
                try { await _readTask; }
                catch { /* ignore */ }
                _readTask = null;
            }
        }
    }

    public async Task SendAsync(IPreviewMessage message)
    {
        if (_stdin is null || !IsRunning)
        {
            return;
        }

        var json = PreviewProtocol.Serialize(message);
        DiagnosticReceived?.Invoke($"[send] {json}");

        await _stdin.WriteLineAsync(json);
        await _stdin.FlushAsync();
    }

    public Task AttachWindowAsync(IntPtr hwnd, int width, int height)
    {
        return SendAsync(new AttachWindowCommand
        {
            Hwnd = hwnd.ToInt64(),
            Width = width,
            Height = height
        });
    }

    public Task LoadEntityAsync(string path)
    {
        return SendAsync(new LoadEntityCommand { Path = path });
    }

    public Task LoadParticleAsync(string emitterPath, string? effectPath = null)
    {
        return SendAsync(new LoadParticleCommand
        {
            EmitterPath = emitterPath,
            EffectPath = effectPath
        });
    }

    public Task TickAsync(float deltaTime = 1f / 60f)
    {
        return SendAsync(new TickCommand { DeltaTime = deltaTime });
    }

    public Task PickAsync(int x, int y)
    {
        return SendAsync(new PickCommand { X = x, Y = y });
    }

    public Task SetCameraPositionAsync(float x, float y, float z, float targetX, float targetY, float targetZ)
    {
        return SendAsync(new SetCameraPositionCommand
        {
            X = x, Y = y, Z = z,
            TargetX = targetX, TargetY = targetY, TargetZ = targetZ
        });
    }

    private async Task ReadOutputAsync(CancellationToken cancellationToken)
    {
        if (_process is null)
        {
            return;
        }

        var reader = _process.StandardOutput;

        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                var line = await reader.ReadLineAsync(cancellationToken);
                if (line is null)
                {
                    break;
                }

                DiagnosticReceived?.Invoke($"[recv] {line}");

                try
                {
                    var message = PreviewProtocol.Deserialize(line);
                    if (message is not null)
                    {
                        if (message is ReadyResponse)
                        {
                            _isReady = true;
                        }

                        MessageReceived?.Invoke(message);
                    }
                }
                catch (Exception ex)
                {
                    DiagnosticReceived?.Invoke($"Failed to parse message: {ex.Message}");
                }
            }
        }
        catch (OperationCanceledException)
        {
            // Expected when stopping
        }
        catch (Exception ex)
        {
            DiagnosticReceived?.Invoke($"Read error: {ex.Message}");
        }
        finally
        {
            Disconnected?.Invoke();
        }
    }

    public async ValueTask DisposeAsync()
    {
        await StopAsync();
    }
}
