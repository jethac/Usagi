using System.Text.Json;
using System.Text.Json.Serialization;

namespace Usagi.ToolCore.Preview;

/// <summary>
/// JSON-lines IPC protocol for UsagiPreviewHost communication.
/// Protocol version: 1
/// </summary>
public static class PreviewProtocol
{
    public const int Version = 1;

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = false,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public static string Serialize<T>(T message) where T : IPreviewMessage
    {
        return JsonSerializer.Serialize(message, message.GetType(), JsonOptions);
    }

    public static IPreviewMessage? Deserialize(string json)
    {
        using var doc = JsonDocument.Parse(json);
        var type = doc.RootElement.GetProperty("type").GetString();

        return type switch
        {
            "init" => JsonSerializer.Deserialize<InitCommand>(json, JsonOptions),
            "shutdown" => JsonSerializer.Deserialize<ShutdownCommand>(json, JsonOptions),
            "attachWindow" => JsonSerializer.Deserialize<AttachWindowCommand>(json, JsonOptions),
            "loadEntity" => JsonSerializer.Deserialize<LoadEntityCommand>(json, JsonOptions),
            "loadParticle" => JsonSerializer.Deserialize<LoadParticleCommand>(json, JsonOptions),
            "tick" => JsonSerializer.Deserialize<TickCommand>(json, JsonOptions),
            "pick" => JsonSerializer.Deserialize<PickCommand>(json, JsonOptions),
            "setCameraPosition" => JsonSerializer.Deserialize<SetCameraPositionCommand>(json, JsonOptions),
            "ready" => JsonSerializer.Deserialize<ReadyResponse>(json, JsonOptions),
            "error" => JsonSerializer.Deserialize<ErrorResponse>(json, JsonOptions),
            "loaded" => JsonSerializer.Deserialize<LoadedResponse>(json, JsonOptions),
            "picked" => JsonSerializer.Deserialize<PickedResponse>(json, JsonOptions),
            "diagnostic" => JsonSerializer.Deserialize<DiagnosticResponse>(json, JsonOptions),
            _ => null
        };
    }
}

public interface IPreviewMessage
{
    string Type { get; }
}

// Commands (sent from C# to C++)

public sealed record InitCommand : IPreviewMessage
{
    public string Type => "init";
    public int ProtocolVersion { get; init; } = PreviewProtocol.Version;
    public string? DataPath { get; init; }
    public string? RomfilesPath { get; init; }
}

public sealed record ShutdownCommand : IPreviewMessage
{
    public string Type => "shutdown";
}

public sealed record AttachWindowCommand : IPreviewMessage
{
    public string Type => "attachWindow";
    public long Hwnd { get; init; }
    public int Width { get; init; }
    public int Height { get; init; }
}

public sealed record LoadEntityCommand : IPreviewMessage
{
    public string Type => "loadEntity";
    public required string Path { get; init; }
}

public sealed record LoadParticleCommand : IPreviewMessage
{
    public string Type => "loadParticle";
    public required string EmitterPath { get; init; }
    public string? EffectPath { get; init; }
}

public sealed record TickCommand : IPreviewMessage
{
    public string Type => "tick";
    public float DeltaTime { get; init; }
}

public sealed record PickCommand : IPreviewMessage
{
    public string Type => "pick";
    public int X { get; init; }
    public int Y { get; init; }
}

public sealed record SetCameraPositionCommand : IPreviewMessage
{
    public string Type => "setCameraPosition";
    public float X { get; init; }
    public float Y { get; init; }
    public float Z { get; init; }
    public float TargetX { get; init; }
    public float TargetY { get; init; }
    public float TargetZ { get; init; }
}

// Responses (sent from C++ to C#)

public sealed record ReadyResponse : IPreviewMessage
{
    public string Type => "ready";
    public int ProtocolVersion { get; init; }
    public string? EngineVersion { get; init; }
}

public sealed record ErrorResponse : IPreviewMessage
{
    public string Type => "error";
    public required string Message { get; init; }
    public string? Details { get; init; }
}

public sealed record LoadedResponse : IPreviewMessage
{
    public string Type => "loaded";
    public required string ResourceType { get; init; }
    public required string Path { get; init; }
    public bool Success { get; init; }
    public string? Error { get; init; }
}

public sealed record PickedResponse : IPreviewMessage
{
    public string Type => "picked";
    public string? EntityId { get; init; }
    public string? ComponentName { get; init; }
}

public sealed record DiagnosticResponse : IPreviewMessage
{
    public string Type => "diagnostic";
    public required string Level { get; init; } // "info", "warning", "error"
    public required string Message { get; init; }
    public string? Source { get; init; }
}
