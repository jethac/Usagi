/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Linux window and event handling through SDL
*****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/_linux/LinuxUtil.h"
#include "Engine/Graphics/GFX.h"
#include "Engine/HID/Input.h"
#include "Engine/HID/_linux/Input_ps.h"
#include <SDL3/SDL.h>

namespace usg
{
	bool GameExit();
	void GameMessage(const uint32 messageID, const void* const pParameters);
}

namespace LINUXUTIL
{
	static WindHndl g_wndHndl = nullptr;
	static bool g_bInFocus = true;

	bool Init()
	{
		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
		{
			fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
			return false;
		}
		return true;
	}

	void Shutdown()
	{
		if (g_wndHndl)
		{
			SDL_DestroyWindow(g_wndHndl);
			g_wndHndl = nullptr;
		}
		SDL_Quit();
	}

	WindHndl GetWindow()
	{
		return g_wndHndl;
	}

	void SetWindow(WindHndl hndl)
	{
		g_wndHndl = hndl;
	}

	void SetInFocus(bool bInFocus)
	{
		g_bInFocus = bInFocus;
	}

	bool GetInFocus()
	{
		return g_bInFocus;
	}

	// Sizes are given in pixels; SDL window sizes are in window coordinates,
	// which differ under display scaling
	static void PixelsToWindowSize(const usg::DisplayMode* pDisplaySettings, float fDensity, int& iWidth, int& iHeight)
	{
		iWidth = (int)(pDisplaySettings->screenDim.width / fDensity);
		iHeight = (int)(pDisplaySettings->screenDim.height / fDensity);
	}

	WindHndl CreateDisplayWindow(const char* szName, const usg::DisplayMode* pDisplaySettings, bool bHidden)
	{
		SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
		if (bHidden)
		{
			flags |= SDL_WINDOW_HIDDEN;
		}
		if (!pDisplaySettings->bWindowed)
		{
			flags |= SDL_WINDOW_FULLSCREEN;
		}

		SDL_DisplayID display = SDL_GetPrimaryDisplay();
		float fScale = display ? SDL_GetDisplayContentScale(display) : 1.0f;
		if (fScale <= 0.0f)
		{
			fScale = 1.0f;
		}

		int iWidth, iHeight;
		PixelsToWindowSize(pDisplaySettings, fScale, iWidth, iHeight);

		WindHndl hndl = SDL_CreateWindow(szName, iWidth, iHeight, flags);
		if (!hndl)
		{
			fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			return nullptr;
		}

		// Text input events are the equivalent of WM_CHAR
		SDL_StartTextInput(hndl);

		SetWindow(hndl);
		return hndl;
	}

	void UpdateWindow(const usg::DisplayMode* pDisplaySettings)
	{
		WindHndl hndl = GetWindow();
		if (!hndl)
		{
			return;
		}

		usg::GameMessage('ONSZ', nullptr);

		if (!pDisplaySettings->bWindowed)
		{
			// Borderless desktop-resolution fullscreen: Wayland doesn't allow
			// applications to change the display mode
			SDL_SetWindowFullscreenMode(hndl, nullptr);
			SDL_SetWindowFullscreen(hndl, true);
		}
		else
		{
			SDL_SetWindowFullscreen(hndl, false);
			int iWidth, iHeight;
			PixelsToWindowSize(pDisplaySettings, SDL_GetWindowPixelDensity(hndl), iWidth, iHeight);
			SDL_SetWindowSize(hndl, iWidth, iHeight);
		}
		SDL_SyncWindow(hndl);

		usg::GameMessage('WSZE', nullptr);
	}

	void GetWindowSize(WindHndl hndl, uint32& uWidthOut, uint32& uHeightOut)
	{
		int iWidth = 0, iHeight = 0;
		if (hndl)
		{
			SDL_GetWindowSizeInPixels(hndl, &iWidth, &iHeight);
		}
		uWidthOut = (uint32)iWidth;
		uHeightOut = (uint32)iHeight;
	}

	static uint32 GetMouseButton(uint8 uSDLButton)
	{
		switch (uSDLButton)
		{
		case SDL_BUTTON_LEFT:	return usg::MOUSE_BUTTON_LEFT;
		case SDL_BUTTON_RIGHT:	return usg::MOUSE_BUTTON_RIGHT;
		case SDL_BUTTON_MIDDLE:	return usg::MOUSE_BUTTON_MIDDLE;
		case SDL_BUTTON_X1:		return usg::MOUSE_BUTTON_X1;
		case SDL_BUTTON_X2:		return usg::MOUSE_BUTTON_X2;
		default:				return usg::MOUSE_BUTTON_NONE;
		}
	}

	// Text input arrives as UTF-8; the engine takes UTF-16 code units
	static void RegisterText(const char* szText)
	{
		const unsigned char* p = (const unsigned char*)szText;
		while (*p)
		{
			uint32 uCode = 0;
			int iExtra = 0;
			if (*p < 0x80)			{ uCode = *p; }
			else if (*p < 0xE0)		{ uCode = *p & 0x1F; iExtra = 1; }
			else if (*p < 0xF0)		{ uCode = *p & 0x0F; iExtra = 2; }
			else					{ uCode = *p & 0x07; iExtra = 3; }
			p++;
			for (int i = 0; i < iExtra && (*p & 0xC0) == 0x80; i++, p++)
			{
				uCode = (uCode << 6) | (*p & 0x3F);
			}

			if (uCode >= 0x10000)
			{
				uCode -= 0x10000;
				usg::Input::GetPlatform().RegisterInputChar((char16)(0xD800 + (uCode >> 10)));
				usg::Input::GetPlatform().RegisterInputChar((char16)(0xDC00 + (uCode & 0x3FF)));
			}
			else
			{
				usg::Input::GetPlatform().RegisterInputChar((char16)uCode);
			}
		}
	}

	void PumpEvents()
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				usg::GameExit();
				break;

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				SetInFocus(true);
				break;

			case SDL_EVENT_WINDOW_FOCUS_LOST:
				SetInFocus(false);
				break;

			case SDL_EVENT_WINDOW_MINIMIZED:
				usg::GameMessage('WMIN', nullptr);
				break;

			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				usg::GameMessage('ONSZ', nullptr);
				usg::GameMessage('WSZE', nullptr);
				break;

			case SDL_EVENT_TEXT_INPUT:
				RegisterText(event.text.text);
				break;

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				usg::Input::GetPlatform().RegisterMouseButtonChange(GetMouseButton(event.button.button), event.button.down);
				break;

			case SDL_EVENT_MOUSE_WHEEL:
				// Scaled to Windows' WHEEL_DELTA units
				usg::Input::GetPlatform().RegisterMouseWheel((long)(event.wheel.y * 120.0f));
				break;

			case SDL_EVENT_GAMEPAD_ADDED:
			case SDL_EVENT_GAMEPAD_REMOVED:
				usg::Input::DeviceChange();
				usg::GameMessage('DCNG', nullptr);
				break;

			default:
				break;
			}
		}
	}

};
