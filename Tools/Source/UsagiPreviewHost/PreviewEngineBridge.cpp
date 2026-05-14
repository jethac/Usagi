/****************************************************************************
//  Usagi engine bridge for the native preview host.
****************************************************************************/
#include "PreviewEngineBridge.h"

#include "Engine/Common/Common.h"
#include "Engine/Core/_win/WinUtil.h"
#include "Engine/Core/Modules/ModuleManager.h"
#include "Engine/Game/GameInterface.h"
#include "Engine/Graphics/Device/Display.h"
#include "Engine/Graphics/Device/GFXContext.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Graphics/Textures/RenderTarget.h"
#include "Engine/HID/Input.h"
#include OS_HEADER(Engine/HID, Input_ps.h)

#include <cstdio>

namespace usg
{
bool InitEngine(const char** dllModules, uint32 uModuleCount);
bool GameInit();
void GameLoop();
void GameCleanup();
void GameMessage(const uint32 messageID, const void* const pParameters);
GFXDevice* GetGFXDevice();
}

namespace
{
class PreviewGame final : public usg::GameInterface
{
public:
    void Init(usg::GFXDevice* pDevice, usg::ResourceMgr* pResMgr) override
    {
        UNREFERENCED_PARAMETER(pDevice);
        UNREFERENCED_PARAMETER(pResMgr);

        usg::Input::Init();
        m_bIsRunning = true;
    }

    void Cleanup(usg::GFXDevice* pDevice) override
    {
        if (pDevice != nullptr)
        {
            pDevice->WaitIdle();
        }
    }

    void Update(usg::GFXDevice* pDevice) override
    {
        UNREFERENCED_PARAMETER(pDevice);
    }

    void Draw(usg::GFXDevice* pDevice) override
    {
        if (pDevice == nullptr)
        {
            return;
        }

        usg::Display* display = pDevice->GetDisplay(0);
        if (display == nullptr)
        {
            return;
        }

        pDevice->Begin();
        usg::GFXContext* context = pDevice->GetImmediateCtxt();
        context->Begin(true);
        context->RenderToDisplay(display, usg::RenderTarget::RT_FLAG_COLOR_0 | usg::RenderTarget::RT_FLAG_DEPTH);
        display->Present();
        context->End();
        pDevice->End();
    }

    void OnMessage(usg::GFXDevice* const pDevice, const uint32 messageID, const void* const pParameters) override
    {
        UNREFERENCED_PARAMETER(pParameters);

        if (pDevice == nullptr)
        {
            return;
        }

        switch (messageID)
        {
        case 'ONSZ':
            pDevice->WaitIdle();
            break;
        case 'WSZE':
            if (usg::Display* display = pDevice->GetDisplay(0))
            {
                display->Resize(pDevice);
            }
            break;
        case 'WMIN':
            if (usg::Display* display = pDevice->GetDisplay(0))
            {
                display->Minimized(pDevice);
            }
            break;
        default:
            break;
        }
    }
};
}

usg::GameInterface* usg::CreateGame()
{
    return vnew(usg::ALLOC_OBJECT) PreviewGame();
}

const char* usg::GetGameName()
{
    return "Usagi Preview Host";
}

PreviewEngineBridge::PreviewEngineBridge()
    : m_initialized(false)
{
}

PreviewEngineBridge::~PreviewEngineBridge()
{
    Shutdown();
}

bool PreviewEngineBridge::Initialize(HINSTANCE instance, HWND hwnd, const char* romfilesPath, char* error, int errorSize)
{
    if (m_initialized)
    {
        return true;
    }

    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        std::snprintf(error, errorSize, "Preview engine received an invalid HWND.");
        return false;
    }

    WINUTIL::Init(instance);
    WINUTIL::SetWindow(hwnd);
    usg::Input::GetPlatform().RegisterHwnd(0, hwnd);

    if (romfilesPath != nullptr && romfilesPath[0] != '\0' && !SetCurrentDirectoryA(romfilesPath))
    {
        std::snprintf(error, errorSize, "Failed to set preview working directory: %s", romfilesPath);
        return false;
    }

    if (!usg::InitEngine(nullptr, 0))
    {
        std::snprintf(error, errorSize, "Usagi engine initialization failed.");
        return false;
    }

    if (!usg::GameInit())
    {
        std::snprintf(error, errorSize, "Usagi preview game initialization failed.");
        usg::GameCleanup();
        return false;
    }

    usg::ModuleManager::Inst()->PostInit(usg::GetGFXDevice());
    m_initialized = true;
    return true;
}

void PreviewEngineBridge::Tick()
{
    if (m_initialized)
    {
        usg::GameLoop();
    }
}

void PreviewEngineBridge::Resize()
{
    if (m_initialized)
    {
        usg::GameMessage('ONSZ', nullptr);
        usg::GameMessage('WSZE', nullptr);
    }
}

void PreviewEngineBridge::Shutdown()
{
    if (m_initialized)
    {
        usg::GameCleanup();
        m_initialized = false;
    }
}
