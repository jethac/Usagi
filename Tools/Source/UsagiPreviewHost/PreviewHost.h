/****************************************************************************
//  Usagi Engine - Preview Host for Tool Integration
//  Description: Headless preview server for external tool communication
****************************************************************************/
#pragma once

#ifndef USG_PREVIEW_HOST_H
#define USG_PREVIEW_HOST_H

#include "Engine/Game/GameInterface.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Core/Timer/Timer.h"
#include "Engine/Scene/Camera/Camera.h"
#include "Engine/Graphics/Lights/DirLight.h"
#include "Engine/PostFX/PostFXSys.h"
#include "Engine/Particles/ParticleEffect.h"
#include "Engine/Particles/ParticleEffectHndl.h"
#include "Engine/Particles/Scripted/ScriptEmitter.h"
#include "IpcProtocol.h"

namespace usg
{
    class ViewContext;
    class IMGuiRenderer;
    class ParticleMgr;
}

class PreviewHost : public usg::GameInterface
{
public:
    PreviewHost();
    virtual ~PreviewHost();

    // GameInterface implementation
    virtual void PreGFXInit() override;
    virtual void Init(usg::GFXDevice* pDevice, usg::ResourceMgr* pResMgr) override;
    virtual void Cleanup(usg::GFXDevice* pDevice) override;
    virtual void Update(usg::GFXDevice* pDevice) override;
    virtual void Draw(usg::GFXDevice* pDevice) override;
    virtual void OnMessage(usg::GFXDevice* const pDevice, const uint32 messageID, const void* const pParameters) override;

    // IPC command handlers
    void HandleInit(const IpcInitCommand& cmd);
    void HandleAttachWindow(const IpcAttachWindowCommand& cmd);
    void HandleLoadEntity(const IpcLoadEntityCommand& cmd);
    void HandleLoadParticle(const IpcLoadParticleCommand& cmd);
    void HandleTick(const IpcTickCommand& cmd);
    void HandlePick(const IpcPickCommand& cmd);
    void HandleSetCameraPosition(const IpcSetCameraPositionCommand& cmd);
    void HandleShutdown();

private:
    void ProcessIpcInput();
    void SendResponse(const char* json);
    void SendReady();
    void SendError(const char* message, const char* details = nullptr);
    void SendLoaded(const char* resourceType, const char* path, bool success, const char* error = nullptr);
    void SendDiagnostic(const char* level, const char* message, const char* source = nullptr);

    void SetupDefaultScene(usg::GFXDevice* pDevice);
    void ClearScene(usg::GFXDevice* pDevice);

    usg::Timer          m_timer;
    usg::Scene          m_scene;
    usg::PostFXSys      m_postFX;
    usg::Camera         m_camera;
    usg::DirLight*      m_pDirLight;
    usg::ViewContext*   m_pViewContext;

    // Active particle effect handle
    usg::ParticleEffectHndl m_activeEffect;
    bool                m_bHasActiveEffect;

    WindHndl            m_hwnd;
    bool                m_bInitialized;
    bool                m_bShutdownRequested;

    // IPC state
    char                m_inputBuffer[8192];
    int                 m_inputPos;
};

#endif // USG_PREVIEW_HOST_H
