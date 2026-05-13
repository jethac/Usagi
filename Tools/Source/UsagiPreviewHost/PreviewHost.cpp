/****************************************************************************
//  Usagi Engine - Preview Host Implementation
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include API_HEADER(Engine/Graphics/Device, GFXDevice_ps.h)
#include "Engine/Scene/ViewContext.h"
#include "Engine/Scene/SceneContext.h"
#include "Engine/Graphics/Device/GFXContext.h"
#include "Engine/Graphics/Device/Display.h"
#include "Engine/Graphics/Lights/LightMgr.h"
#include "Engine/Resource/ResourceMgr.h"
#include "Engine/Particles/ParticleMgr.h"
#include "Engine/Particles/ParticleEffect.h"
#include "Engine/Particles/Scripted/ScriptEmitter.h"
#include "Engine/Maths/AABB.h"
#include "Engine/Maths/MathUtil.h"
#include "PreviewHost.h"
#include <cstdio>
#include <cstring>

#ifdef PLATFORM_PC
#include <io.h>
#include <fcntl.h>
#endif

static PreviewHost* g_spPreviewHost = nullptr;

usg::GameInterface* usg::CreateGame()
{
    return vnew(usg::ALLOC_OBJECT) PreviewHost();
}

const char* usg::GetGameName()
{
    return "UsagiPreviewHost";
}

PreviewHost::PreviewHost()
    : GameInterface()
    , m_pDirLight(nullptr)
    , m_pViewContext(nullptr)
    , m_bHasActiveEffect(false)
    , m_hwnd(nullptr)
    , m_bInitialized(false)
    , m_bShutdownRequested(false)
    , m_inputPos(0)
{
    g_spPreviewHost = this;
    memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
}

PreviewHost::~PreviewHost()
{
    g_spPreviewHost = nullptr;
}

void PreviewHost::PreGFXInit()
{
#ifdef PLATFORM_PC
    // Set stdin to non-blocking binary mode
    _setmode(_fileno(stdin), _O_BINARY);
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
#endif
}

void PreviewHost::Init(usg::GFXDevice* pDevice, usg::ResourceMgr* pResMgr)
{
    m_timer.Init();

    // Basic scene setup
    SetupDefaultScene(pDevice);

    m_bInitialized = true;

    // Send ready response
    SendReady();

    SendDiagnostic("info", "Preview host initialized");
}

void PreviewHost::SetupDefaultScene(usg::GFXDevice* pDevice)
{
    // Set up world bounds for the scene
    usg::AABB worldBounds;
    worldBounds.SetCentreRadii(usg::Vector3f(0.0f, 0.0f, 0.0f), usg::Vector3f(512.0f, 512.0f, 512.0f));

    // Initialize scene with particle support (NULL for ParticleSet means scripted particles only)
    m_scene.Init(pDevice, usg::ResourceMgr::Inst(), worldBounds, nullptr);

    // Create view context for rendering
    m_pViewContext = m_scene.CreateViewContext(pDevice);

    // Initialize camera with default aspect ratio (will be updated when window is attached)
    m_camera.Init(1.0f);
    m_camera.SetUp(usg::Vector4f(0.0f, 1.0f, 0.0f, 0.0f));
    m_camera.SetPos(usg::Vector4f(0.0f, 5.0f, -20.0f, 1.0f));
    m_camera.SetTarget(usg::Vector4f(0.0f, 0.0f, 0.0f, 1.0f));

    // Initialize PostFX system with basic effects
    uint32 uEffectFlags = usg::PostFXSys::EFFECT_DEFERRED_SHADING | usg::PostFXSys::EFFECT_BLOOM;
    m_postFX.Init(pDevice, usg::ResourceMgr::Inst(), 800, 600, uEffectFlags);
    m_postFX.SetSkyTexture(pDevice, usg::ResourceMgr::Inst()->GetTexture(pDevice, "white_default"));

    // Initialize view context with PostFX
    m_pViewContext->Init(pDevice, usg::ResourceMgr::Inst(), &m_postFX, 0, usg::RenderMask::RENDER_MASK_ALL);
    m_pViewContext->SetCamera(&m_camera);

    // Add directional light
    m_pDirLight = m_scene.GetLightMgr().AddDirectionalLight(pDevice, false);
    m_pDirLight->SetAmbient(usg::Color(0.3f, 0.3f, 0.3f));
    m_pDirLight->SetDiffuse(usg::Color(2.0f, 2.0f, 2.0f));
    m_pDirLight->SetSpecularColor(usg::Color(5.0f, 5.0f, 5.0f));
    m_pDirLight->SetDirection(usg::Vector4f(-1.0f, -1.0f, 0.5f, 0.0f).GetNormalised());
    m_pDirLight->SwitchOn(true);
}

void PreviewHost::ClearScene(usg::GFXDevice* pDevice)
{
    // Kill any active particle effect
    if (m_bHasActiveEffect && m_activeEffect.GetEffect() != nullptr)
    {
        m_activeEffect.Kill(true); // Force immediate cleanup
        m_bHasActiveEffect = false;
    }
}

void PreviewHost::Cleanup(usg::GFXDevice* pDevice)
{
    ClearScene(pDevice);

    // Clean up lighting
    if (m_pDirLight)
    {
        m_scene.GetLightMgr().RemoveDirLight(m_pDirLight);
        m_pDirLight = nullptr;
    }

    // Clean up view context
    if (m_pViewContext)
    {
        m_scene.DeleteViewContext(m_pViewContext);
        m_pViewContext = nullptr;
    }

    // Clean up PostFX and scene
    m_postFX.Cleanup(pDevice);
    m_scene.Cleanup(pDevice);

    m_bInitialized = false;
}

void PreviewHost::Update(usg::GFXDevice* pDevice)
{
    // Process any pending IPC commands
    ProcessIpcInput();

    if (m_bShutdownRequested)
    {
        Quit();
        return;
    }

    // Update timer
    m_timer.Update();
    float fElapsed = m_timer.GetDeltaGameTime();

    // Update scene (including particles)
    m_scene.TransformUpdate(fElapsed);
    m_scene.Update(pDevice);
}

void PreviewHost::Draw(usg::GFXDevice* pDevice)
{
    if (!m_bInitialized || m_pViewContext == nullptr)
    {
        return;
    }

    usg::Display* pDisplay = pDevice->GetDisplay(0);
    if (pDisplay == nullptr)
    {
        return;
    }

    pDevice->Begin();
    usg::GFXContext* pImmContext = pDevice->GetImmediateCtxt();
    pImmContext->Begin(true);

    // Pre-draw phase
    m_scene.PreDraw(pImmContext);
    m_scene.GetLightMgr().ViewShadowRender(pImmContext, &m_scene, m_pViewContext);

    // Begin PostFX scene
    m_postFX.BeginScene(pImmContext, 0);
    m_postFX.SetActiveViewContext(m_pViewContext);

    // Draw the scene
    m_pViewContext->PreDraw(pImmContext, usg::VIEW_CENTRAL);
    m_pViewContext->DrawScene(pImmContext);

    m_postFX.SetActiveViewContext(nullptr);
    m_postFX.EndScene();

    // Present to display
    pImmContext->RenderToDisplay(pDisplay);
    pDisplay->Present();

    pImmContext->End();
    pDevice->End();
}

void PreviewHost::OnMessage(usg::GFXDevice* const pDevice, const uint32 messageID, const void* const pParameters)
{
    // Handle window messages
    switch (messageID)
    {
    case 'WSZE':
        // Window resized
        SendDiagnostic("info", "Window resized");
        break;

    case 'WMIN':
        // Window minimized
        SendDiagnostic("info", "Window minimized");
        break;
    }
}

void PreviewHost::ProcessIpcInput()
{
#ifdef PLATFORM_PC
    // Non-blocking read from stdin
    while (_kbhit() || m_inputPos > 0)
    {
        int ch = _getch();
        if (ch == EOF)
        {
            break;
        }

        if (ch == '\n' || ch == '\r')
        {
            if (m_inputPos > 0)
            {
                m_inputBuffer[m_inputPos] = '\0';

                // Parse and handle the command
                IpcCommandType cmdType = IpcParser::ParseCommandType(m_inputBuffer);

                switch (cmdType)
                {
                case IpcCommandType::Init:
                {
                    IpcInitCommand cmd;
                    if (IpcParser::ParseInit(m_inputBuffer, cmd))
                    {
                        HandleInit(cmd);
                    }
                    break;
                }

                case IpcCommandType::Shutdown:
                    HandleShutdown();
                    break;

                case IpcCommandType::AttachWindow:
                {
                    IpcAttachWindowCommand cmd;
                    if (IpcParser::ParseAttachWindow(m_inputBuffer, cmd))
                    {
                        HandleAttachWindow(cmd);
                    }
                    break;
                }

                case IpcCommandType::LoadEntity:
                {
                    IpcLoadEntityCommand cmd;
                    if (IpcParser::ParseLoadEntity(m_inputBuffer, cmd))
                    {
                        HandleLoadEntity(cmd);
                    }
                    break;
                }

                case IpcCommandType::LoadParticle:
                {
                    IpcLoadParticleCommand cmd;
                    if (IpcParser::ParseLoadParticle(m_inputBuffer, cmd))
                    {
                        HandleLoadParticle(cmd);
                    }
                    break;
                }

                case IpcCommandType::Tick:
                {
                    IpcTickCommand cmd;
                    if (IpcParser::ParseTick(m_inputBuffer, cmd))
                    {
                        HandleTick(cmd);
                    }
                    break;
                }

                case IpcCommandType::Pick:
                {
                    IpcPickCommand cmd;
                    if (IpcParser::ParsePick(m_inputBuffer, cmd))
                    {
                        HandlePick(cmd);
                    }
                    break;
                }

                case IpcCommandType::SetCameraPosition:
                {
                    IpcSetCameraPositionCommand cmd;
                    if (IpcParser::ParseSetCameraPosition(m_inputBuffer, cmd))
                    {
                        HandleSetCameraPosition(cmd);
                    }
                    break;
                }

                default:
                    SendError("Unknown command type", m_inputBuffer);
                    break;
                }

                m_inputPos = 0;
            }
        }
        else if (m_inputPos < (int)sizeof(m_inputBuffer) - 1)
        {
            m_inputBuffer[m_inputPos++] = (char)ch;
        }

        // Only read one character per frame to avoid blocking
        break;
    }
#endif
}

void PreviewHost::HandleInit(const IpcInitCommand& cmd)
{
    if (cmd.protocolVersion != IPC_PROTOCOL_VERSION)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Protocol version mismatch: expected %d, got %d",
            IPC_PROTOCOL_VERSION, cmd.protocolVersion);
        SendError(msg);
        return;
    }

    SendDiagnostic("info", "Init command received");

    // Already initialized in Init(), just acknowledge
    SendReady();
}

void PreviewHost::HandleAttachWindow(const IpcAttachWindowCommand& cmd)
{
    m_hwnd = (WindHndl)(uintptr_t)cmd.hwnd;

    char msg[128];
    snprintf(msg, sizeof(msg), "Window attached: %dx%d", cmd.width, cmd.height);
    SendDiagnostic("info", msg);

    // TODO: Reconfigure display for new window
}

void PreviewHost::HandleLoadEntity(const IpcLoadEntityCommand& cmd)
{
    SendDiagnostic("info", "Loading entity", cmd.path);

    // TODO: Actually load entity hierarchy
    // For now, just report success/failure placeholder
    SendLoaded("entity", cmd.path, false, "Entity loading not yet implemented");
}

void PreviewHost::HandleLoadParticle(const IpcLoadParticleCommand& cmd)
{
    SendDiagnostic("info", "Loading particle", cmd.emitterPath);

    // Clear any existing effect
    ClearScene(nullptr);

    // Determine which file to use - prefer effect (.pfx) over emitter (.pem)
    const char* effectName = nullptr;
    if (cmd.effectPath[0] != '\0')
    {
        effectName = cmd.effectPath;
    }
    else if (cmd.emitterPath[0] != '\0')
    {
        // If only emitter specified, we need an effect that references it
        // For now, try loading it as an effect (some emitters have matching effects)
        effectName = cmd.emitterPath;
    }

    if (effectName == nullptr || effectName[0] == '\0')
    {
        SendLoaded("particle", "", false, "No particle path specified");
        return;
    }

    // Create transform at origin
    usg::Matrix4x4 mTransform;
    mTransform.LoadIdentity();
    mTransform.SetTranslation(usg::Vector3f(0.0f, 0.0f, 0.0f));

    // Try to create the scripted effect
    // Note: This expects the file to exist as Particle/{name}.pfx in the romfiles
    m_scene.CreateScriptedEffect(m_activeEffect, mTransform, effectName, usg::Vector3f(0.0f, 0.0f, 0.0f), 1.0f);

    if (m_activeEffect.GetEffect() != nullptr)
    {
        m_bHasActiveEffect = true;
        SendLoaded("particle", effectName, true);
    }
    else
    {
        m_bHasActiveEffect = false;
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to load particle effect: %s", effectName);
        SendLoaded("particle", effectName, false, errMsg);
    }
}

void PreviewHost::HandleTick(const IpcTickCommand& cmd)
{
    // Update simulation with specified delta time
    // This is handled in Update() using the timer
}

void PreviewHost::HandlePick(const IpcPickCommand& cmd)
{
    // TODO: Implement picking
    char msg[128];
    snprintf(msg, sizeof(msg), "Pick at %d, %d", cmd.x, cmd.y);
    SendDiagnostic("info", msg);

    // For now, no picking support
    char response[256];
    IpcResponse::Picked(response, sizeof(response), nullptr, nullptr);
    SendResponse(response);
}

void PreviewHost::HandleSetCameraPosition(const IpcSetCameraPositionCommand& cmd)
{
    // Update camera position and target
    m_camera.SetPos(usg::Vector4f(cmd.x, cmd.y, cmd.z, 1.0f));
    m_camera.SetTarget(usg::Vector4f(cmd.targetX, cmd.targetY, cmd.targetZ, 1.0f));

    char msg[256];
    snprintf(msg, sizeof(msg), "Camera position: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)",
        cmd.x, cmd.y, cmd.z, cmd.targetX, cmd.targetY, cmd.targetZ);
    SendDiagnostic("info", msg);
}

void PreviewHost::HandleShutdown()
{
    SendDiagnostic("info", "Shutdown requested");
    m_bShutdownRequested = true;
}

void PreviewHost::SendResponse(const char* json)
{
    fprintf(stdout, "%s\n", json);
    fflush(stdout);
}

void PreviewHost::SendReady()
{
    char buffer[256];
    IpcResponse::Ready(buffer, sizeof(buffer), IPC_PROTOCOL_VERSION, "Usagi Preview Host 1.0");
    SendResponse(buffer);
}

void PreviewHost::SendError(const char* message, const char* details)
{
    char buffer[1024];
    IpcResponse::Error(buffer, sizeof(buffer), message, details);
    SendResponse(buffer);
}

void PreviewHost::SendLoaded(const char* resourceType, const char* path, bool success, const char* error)
{
    char buffer[1024];
    IpcResponse::Loaded(buffer, sizeof(buffer), resourceType, path, success, error);
    SendResponse(buffer);
}

void PreviewHost::SendDiagnostic(const char* level, const char* message, const char* source)
{
    char buffer[1024];
    IpcResponse::Diagnostic(buffer, sizeof(buffer), level, message, source);
    SendResponse(buffer);
}
