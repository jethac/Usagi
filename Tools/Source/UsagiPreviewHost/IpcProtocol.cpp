/****************************************************************************
//  Usagi Engine - Preview Host IPC Protocol Implementation
****************************************************************************/
#include "Engine/Common/Common.h"
#include "IpcProtocol.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Simple JSON parsing helpers (no external dependencies)

static const char* SkipWhitespace(const char* p)
{
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
    {
        p++;
    }
    return p;
}

static const char* FindKey(const char* json, const char* key)
{
    char searchPattern[128];
    snprintf(searchPattern, sizeof(searchPattern), "\"%s\"", key);

    const char* found = strstr(json, searchPattern);
    if (!found) return nullptr;

    // Skip past the key and colon
    found += strlen(searchPattern);
    found = SkipWhitespace(found);
    if (*found != ':') return nullptr;
    found++;
    found = SkipWhitespace(found);

    return found;
}

const char* IpcParser::FindString(const char* json, const char* key, char* buffer, int bufferSize)
{
    const char* value = FindKey(json, key);
    if (!value) return nullptr;

    if (*value == 'n' && strncmp(value, "null", 4) == 0)
    {
        buffer[0] = '\0';
        return buffer;
    }

    if (*value != '"') return nullptr;
    value++;

    int i = 0;
    while (*value && *value != '"' && i < bufferSize - 1)
    {
        if (*value == '\\' && *(value + 1))
        {
            value++;
            switch (*value)
            {
            case 'n': buffer[i++] = '\n'; break;
            case 't': buffer[i++] = '\t'; break;
            case 'r': buffer[i++] = '\r'; break;
            case '\\': buffer[i++] = '\\'; break;
            case '"': buffer[i++] = '"'; break;
            default: buffer[i++] = *value; break;
            }
        }
        else
        {
            buffer[i++] = *value;
        }
        value++;
    }
    buffer[i] = '\0';

    return buffer;
}

bool IpcParser::FindInt(const char* json, const char* key, int& out)
{
    const char* value = FindKey(json, key);
    if (!value) return false;

    char* end;
    long result = strtol(value, &end, 10);
    if (end == value) return false;

    out = (int)result;
    return true;
}

bool IpcParser::FindInt64(const char* json, const char* key, int64_t& out)
{
    const char* value = FindKey(json, key);
    if (!value) return false;

    char* end;
    int64_t result = strtoll(value, &end, 10);
    if (end == value) return false;

    out = result;
    return true;
}

bool IpcParser::FindFloat(const char* json, const char* key, float& out)
{
    const char* value = FindKey(json, key);
    if (!value) return false;

    char* end;
    float result = strtof(value, &end);
    if (end == value) return false;

    out = result;
    return true;
}

IpcCommandType IpcParser::ParseCommandType(const char* json)
{
    char type[64];
    if (!FindString(json, "type", type, sizeof(type)))
    {
        return IpcCommandType::Unknown;
    }

    if (strcmp(type, "init") == 0) return IpcCommandType::Init;
    if (strcmp(type, "shutdown") == 0) return IpcCommandType::Shutdown;
    if (strcmp(type, "attachWindow") == 0) return IpcCommandType::AttachWindow;
    if (strcmp(type, "loadEntity") == 0) return IpcCommandType::LoadEntity;
    if (strcmp(type, "loadParticle") == 0) return IpcCommandType::LoadParticle;
    if (strcmp(type, "tick") == 0) return IpcCommandType::Tick;
    if (strcmp(type, "pick") == 0) return IpcCommandType::Pick;
    if (strcmp(type, "setCameraPosition") == 0) return IpcCommandType::SetCameraPosition;

    return IpcCommandType::Unknown;
}

bool IpcParser::ParseInit(const char* json, IpcInitCommand& out)
{
    memset(&out, 0, sizeof(out));
    FindInt(json, "protocolVersion", out.protocolVersion);
    FindString(json, "dataPath", out.dataPath, sizeof(out.dataPath));
    FindString(json, "romfilesPath", out.romfilesPath, sizeof(out.romfilesPath));
    return true;
}

bool IpcParser::ParseAttachWindow(const char* json, IpcAttachWindowCommand& out)
{
    memset(&out, 0, sizeof(out));
    if (!FindInt64(json, "hwnd", out.hwnd)) return false;
    FindInt(json, "width", out.width);
    FindInt(json, "height", out.height);
    return true;
}

bool IpcParser::ParseLoadEntity(const char* json, IpcLoadEntityCommand& out)
{
    memset(&out, 0, sizeof(out));
    return FindString(json, "path", out.path, sizeof(out.path)) != nullptr;
}

bool IpcParser::ParseLoadParticle(const char* json, IpcLoadParticleCommand& out)
{
    memset(&out, 0, sizeof(out));
    if (!FindString(json, "emitterPath", out.emitterPath, sizeof(out.emitterPath)))
    {
        return false;
    }
    FindString(json, "effectPath", out.effectPath, sizeof(out.effectPath));
    return true;
}

bool IpcParser::ParseTick(const char* json, IpcTickCommand& out)
{
    memset(&out, 0, sizeof(out));
    out.deltaTime = 1.0f / 60.0f;  // Default
    FindFloat(json, "deltaTime", out.deltaTime);
    return true;
}

bool IpcParser::ParsePick(const char* json, IpcPickCommand& out)
{
    memset(&out, 0, sizeof(out));
    if (!FindInt(json, "x", out.x)) return false;
    if (!FindInt(json, "y", out.y)) return false;
    return true;
}

bool IpcParser::ParseSetCameraPosition(const char* json, IpcSetCameraPositionCommand& out)
{
    memset(&out, 0, sizeof(out));
    if (!FindFloat(json, "x", out.x)) return false;
    if (!FindFloat(json, "y", out.y)) return false;
    if (!FindFloat(json, "z", out.z)) return false;
    if (!FindFloat(json, "targetX", out.targetX)) return false;
    if (!FindFloat(json, "targetY", out.targetY)) return false;
    if (!FindFloat(json, "targetZ", out.targetZ)) return false;
    return true;
}

// Response builders

static void EscapeJsonString(const char* input, char* output, int maxLen)
{
    int j = 0;
    for (int i = 0; input[i] && j < maxLen - 2; i++)
    {
        char c = input[i];
        if (c == '"' || c == '\\')
        {
            output[j++] = '\\';
        }
        else if (c == '\n')
        {
            output[j++] = '\\';
            c = 'n';
        }
        else if (c == '\r')
        {
            output[j++] = '\\';
            c = 'r';
        }
        else if (c == '\t')
        {
            output[j++] = '\\';
            c = 't';
        }

        if (j < maxLen - 1)
        {
            output[j++] = c;
        }
    }
    output[j] = '\0';
}

void IpcResponse::Ready(char* buffer, int bufferSize, int protocolVersion, const char* engineVersion)
{
    char escapedVersion[128] = "";
    if (engineVersion)
    {
        EscapeJsonString(engineVersion, escapedVersion, sizeof(escapedVersion));
    }

    snprintf(buffer, bufferSize,
        "{\"type\":\"ready\",\"protocolVersion\":%d,\"engineVersion\":\"%s\"}",
        protocolVersion, escapedVersion);
}

void IpcResponse::Error(char* buffer, int bufferSize, const char* message, const char* details)
{
    char escapedMessage[512];
    EscapeJsonString(message, escapedMessage, sizeof(escapedMessage));

    if (details)
    {
        char escapedDetails[512];
        EscapeJsonString(details, escapedDetails, sizeof(escapedDetails));
        snprintf(buffer, bufferSize,
            "{\"type\":\"error\",\"message\":\"%s\",\"details\":\"%s\"}",
            escapedMessage, escapedDetails);
    }
    else
    {
        snprintf(buffer, bufferSize,
            "{\"type\":\"error\",\"message\":\"%s\"}",
            escapedMessage);
    }
}

void IpcResponse::Loaded(char* buffer, int bufferSize, const char* resourceType, const char* path, bool success, const char* error)
{
    char escapedType[64], escapedPath[512];
    EscapeJsonString(resourceType, escapedType, sizeof(escapedType));
    EscapeJsonString(path, escapedPath, sizeof(escapedPath));

    if (error)
    {
        char escapedError[512];
        EscapeJsonString(error, escapedError, sizeof(escapedError));
        snprintf(buffer, bufferSize,
            "{\"type\":\"loaded\",\"resourceType\":\"%s\",\"path\":\"%s\",\"success\":%s,\"error\":\"%s\"}",
            escapedType, escapedPath, success ? "true" : "false", escapedError);
    }
    else
    {
        snprintf(buffer, bufferSize,
            "{\"type\":\"loaded\",\"resourceType\":\"%s\",\"path\":\"%s\",\"success\":%s}",
            escapedType, escapedPath, success ? "true" : "false");
    }
}

void IpcResponse::Picked(char* buffer, int bufferSize, const char* entityId, const char* componentName)
{
    if (entityId)
    {
        char escapedId[256], escapedComp[256] = "";
        EscapeJsonString(entityId, escapedId, sizeof(escapedId));
        if (componentName)
        {
            EscapeJsonString(componentName, escapedComp, sizeof(escapedComp));
        }

        snprintf(buffer, bufferSize,
            "{\"type\":\"picked\",\"entityId\":\"%s\",\"componentName\":%s}",
            escapedId, componentName ? escapedComp : "null");
    }
    else
    {
        snprintf(buffer, bufferSize, "{\"type\":\"picked\",\"entityId\":null}");
    }
}

void IpcResponse::Diagnostic(char* buffer, int bufferSize, const char* level, const char* message, const char* source)
{
    char escapedLevel[32], escapedMessage[1024];
    EscapeJsonString(level, escapedLevel, sizeof(escapedLevel));
    EscapeJsonString(message, escapedMessage, sizeof(escapedMessage));

    if (source)
    {
        char escapedSource[256];
        EscapeJsonString(source, escapedSource, sizeof(escapedSource));
        snprintf(buffer, bufferSize,
            "{\"type\":\"diagnostic\",\"level\":\"%s\",\"message\":\"%s\",\"source\":\"%s\"}",
            escapedLevel, escapedMessage, escapedSource);
    }
    else
    {
        snprintf(buffer, bufferSize,
            "{\"type\":\"diagnostic\",\"level\":\"%s\",\"message\":\"%s\"}",
            escapedLevel, escapedMessage);
    }
}
