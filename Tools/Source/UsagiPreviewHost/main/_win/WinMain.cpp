/****************************************************************************
//  Usagi Engine - Preview Host Entry Point
//  Description: Headless entry point for preview host (no window initially)
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/HID/Input.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include API_HEADER(Engine/Graphics/Device, GFXDevice_ps.h)
#include OS_HEADER(Engine/Core, WinUtil.h)
#include OS_HEADER(Engine/HID, Input_ps.h)
#include <stdlib.h>

bool g_bFullScreen = false;
uint32 g_uWindowWidth = 800;
uint32 g_uWindowHeight = 600;

using namespace usg;

// Forward declarations
namespace usg
{
    bool GameMain(const char** dllModules, uint32 uModuleCount);
    bool GameExit();
    void GameMessage(const uint32 messageID, const void* const pParameters);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
    }
    break;

    case WM_SIZE:
    {
        if (wparam == SIZE_MINIMIZED)
        {
            GameMessage('WMIN', nullptr);
        }
        else
        {
            GameMessage('ONSZ', nullptr);
            GameMessage('WSZE', nullptr);
        }
    }
    break;

    case WM_DESTROY:
        GameExit();
        PostQuitMessage(0);
        break;

    case WM_ERASEBKGND:
        return 0;

    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpcmdline, int ncmdshow)
{
    WINUTIL::Init(hInstance);

    // Set working directory based on USAGI_DIR
    const char* szUsagi = getenv("USAGI_DIR");
    if (szUsagi != NULL)
    {
        char path[512];
        sprintf_s(path, 512, "%s\\..\\_romfiles\\win", szUsagi);
        SetCurrentDirectory(path);
    }

    // Create a minimal hidden window for the graphics context
    usg::DisplayMode settings;
    settings.screenDim.x = 0;
    settings.screenDim.y = 0;
    settings.screenDim.width = g_uWindowWidth;
    settings.screenDim.height = g_uWindowHeight;
    settings.bWindowed = true;
    settings.parentHndl = NULL;
    settings.bMenu = false;

    const char* const szWindowName = "UsagiPreviewHost";
    str::Copy(settings.name, szWindowName, sizeof(settings.name));

    WindHndl hndl = WINUTIL::CreateDisplayWindow(WindowProc, szWindowName, &settings, false);

    // Initially hide the window - it will be shown when AttachWindow is called
    // or when the host decides to show it
    ShowWindow(hndl, SW_HIDE);

    usg::Input::GetPlatform().RegisterHwnd(0, hndl);

    // Run the main game loop
    GameMain(nullptr, 0);

    return 0;
}
