using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;
using Usagi.ToolCore.Preview;
using Usagi.ToolCore.Projects;

namespace Usagi.ToolShell;

public sealed class PreviewPane : UserControl
{
    private readonly TextBlock _statusText;
    private readonly Button _startButton;
    private readonly Button _stopButton;
    private readonly TextBox _logOutput;

    private PreviewHostClient? _client;
    private UsagiProject? _project;
    private DispatcherTimer? _tickTimer;

    public PreviewPane()
    {
        _statusText = new TextBlock
        {
            Text = "Preview: Not connected",
            Margin = new Thickness(8, 4),
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center
        };

        _startButton = new Button { Content = "Start", Margin = new Thickness(4), Width = 60 };
        _startButton.Click += async (_, _) => await StartPreviewAsync();

        _stopButton = new Button { Content = "Stop", Margin = new Thickness(4), Width = 60, IsEnabled = false };
        _stopButton.Click += async (_, _) => await StopPreviewAsync();

        _logOutput = new TextBox
        {
            IsReadOnly = true,
            AcceptsReturn = true,
            FontFamily = FontFamily.Parse("Consolas"),
            FontSize = 11,
            TextWrapping = TextWrapping.NoWrap
        };

        var toolbar = new StackPanel
        {
            Orientation = Avalonia.Layout.Orientation.Horizontal,
            Children = { _statusText, _startButton, _stopButton }
        };

        var previewArea = new Border
        {
            Background = Brushes.DimGray,
            BorderBrush = Brushes.Gray,
            BorderThickness = new Thickness(1),
            Child = new TextBlock
            {
                Text = "Preview Area\n(Native window will be hosted here)",
                HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center,
                VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center,
                TextAlignment = TextAlignment.Center,
                Foreground = Brushes.LightGray
            }
        };

        var logPanel = new Expander
        {
            Header = "Preview Log",
            IsExpanded = false,
            Content = new ScrollViewer
            {
                Height = 100,
                Content = _logOutput
            }
        };

        Content = new DockPanel
        {
            Children =
            {
                SetDock(toolbar, Dock.Top),
                SetDock(logPanel, Dock.Bottom),
                previewArea
            }
        };
    }

    private static Control SetDock(Control control, Dock dock)
    {
        DockPanel.SetDock(control, dock);
        return control;
    }

    public void SetProject(UsagiProject? project)
    {
        _project = project;
        UpdateStatus();
    }

    public async Task LoadEntityAsync(string entityPath)
    {
        if (_client is not { IsReady: true })
        {
            Log("Cannot load entity: preview host not ready");
            return;
        }

        await _client.LoadEntityAsync(entityPath);
    }

    public async Task LoadParticleAsync(string? emitterPath, string? effectPath)
    {
        if (_client is not { IsReady: true })
        {
            Log("Cannot load particle: preview host not ready");
            return;
        }

        if (emitterPath is not null)
        {
            await _client.LoadParticleAsync(emitterPath, effectPath);
        }
    }

    private async Task StartPreviewAsync()
    {
        if (_project is null)
        {
            Log("No project loaded");
            return;
        }

        if (_client is not null)
        {
            await StopPreviewAsync();
        }

        _client = new PreviewHostClient(_project);
        _client.DiagnosticReceived += Log;
        _client.MessageReceived += OnMessageReceived;
        _client.Disconnected += OnDisconnected;

        _startButton.IsEnabled = false;
        _statusText.Text = "Preview: Starting...";

        Log($"Starting preview host: {_client.PreviewHostPath}");

        var success = await _client.StartAsync();

        if (success)
        {
            _statusText.Text = "Preview: Connected";
            _stopButton.IsEnabled = true;

            // Start tick timer
            _tickTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
            _tickTimer.Tick += async (_, _) =>
            {
                if (_client?.IsReady == true)
                {
                    await _client.TickAsync(0.016f);
                }
            };
            _tickTimer.Start();
        }
        else
        {
            _statusText.Text = "Preview: Failed to start";
            _startButton.IsEnabled = true;
            await _client.DisposeAsync();
            _client = null;
        }
    }

    private async Task StopPreviewAsync()
    {
        _tickTimer?.Stop();
        _tickTimer = null;

        if (_client is not null)
        {
            Log("Stopping preview host...");
            await _client.DisposeAsync();
            _client = null;
        }

        UpdateStatus();
    }

    private void OnMessageReceived(IPreviewMessage message)
    {
        Dispatcher.UIThread.Post(() =>
        {
            switch (message)
            {
                case ReadyResponse ready:
                    Log($"Preview host ready (protocol v{ready.ProtocolVersion}, engine: {ready.EngineVersion ?? "unknown"})");
                    break;

                case ErrorResponse error:
                    Log($"ERROR: {error.Message}");
                    if (error.Details is not null)
                    {
                        Log($"  Details: {error.Details}");
                    }
                    break;

                case LoadedResponse loaded:
                    var status = loaded.Success ? "OK" : $"FAILED: {loaded.Error}";
                    Log($"Loaded {loaded.ResourceType} '{loaded.Path}': {status}");
                    break;

                case DiagnosticResponse diag:
                    Log($"[{diag.Level}] {diag.Message}");
                    break;
            }
        });
    }

    private void OnDisconnected()
    {
        Dispatcher.UIThread.Post(() =>
        {
            Log("Preview host disconnected");
            _tickTimer?.Stop();
            _tickTimer = null;
            UpdateStatus();
        });
    }

    private void UpdateStatus()
    {
        if (_client is { IsReady: true })
        {
            _statusText.Text = "Preview: Connected";
            _startButton.IsEnabled = false;
            _stopButton.IsEnabled = true;
        }
        else if (_client is { IsRunning: true })
        {
            _statusText.Text = "Preview: Connecting...";
            _startButton.IsEnabled = false;
            _stopButton.IsEnabled = true;
        }
        else
        {
            _statusText.Text = _project is not null ? "Preview: Not connected" : "Preview: No project";
            _startButton.IsEnabled = _project is not null;
            _stopButton.IsEnabled = false;
        }
    }

    private void Log(string message)
    {
        Dispatcher.UIThread.Post(() =>
        {
            var timestamp = DateTime.Now.ToString("HH:mm:ss.fff");
            _logOutput.Text = $"[{timestamp}] {message}\n{_logOutput.Text}";

            // Trim log if too long
            if (_logOutput.Text?.Length > 50000)
            {
                _logOutput.Text = _logOutput.Text[..40000];
            }
        });
    }
}
