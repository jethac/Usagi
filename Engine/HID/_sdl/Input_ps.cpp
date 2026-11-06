/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Memory/MemUtil.h"
#include "Input_ps.h"
#include "Engine/HID/Input.h"
#include "Engine/Core/String/String_Util.h"
#include <SDL3/SDL.h>

namespace usg{

Input_ps::Input_ps()
{
	MemClear(m_pExternalPads, sizeof(m_pExternalPads));
	MemClear(m_mouseButtonsDown, sizeof(m_mouseButtonsDown));
	MemClear(m_mouseButtonsThisFrame, sizeof(m_mouseButtonsThisFrame));
	m_mouseWheel = 0;
	m_uInputChars = 0;
	m_uExternalPads = 0;
	m_window = nullptr;
	m_bTextInput = false;
	m_composition[0] = '\0';
	m_iCompositionCursor = 0;
}

Input_ps::~Input_ps()
{

}

void Input_ps::GetActiveGamepads(usg::vector<IGamepad*>& gamepads)
{
	// First add any external pads, giving them priority
	for (uint32 i = 0; i < m_uExternalPads; i++)
	{
		if (m_pExternalPads[i]->IsConnected())
		{
			gamepads.push_back(m_pExternalPads[i]);
		}
	}

	for (auto& pad : m_sdlPads)
	{
		if (pad.IsConnected())
		{
			gamepads.push_back(&pad);
		}
	}

	// After real controllers, which take over when one is connected
	if (m_touchGamepad.IsConnected())
	{
		gamepads.push_back(&m_touchGamepad);
	}

	if (m_virtualGamepad.IsConnected())
	{
		gamepads.push_back(&m_virtualGamepad);
	}
}

void Input_ps::Init()
{
	m_mouse.Init(this);
	m_keyboard.Init(this);
	m_virtualGamepad.Init(&m_keyboard);

	// Phones and tablets are played on their touch screens. Elsewhere a
	// touch screen is usually something else (the Livingroom table is
	// driven by phones), so the on-screen pad is opt-in: USAGI_TOUCH_PAD=1
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
	m_touchGamepad.SetEnabled(true);
#else
	const char* szTouch = SDL_getenv("USAGI_TOUCH_PAD");
	m_touchGamepad.SetEnabled(szTouch && szTouch[0] == '1');
#endif
	RegisterDeviceChange();
}

void Input_ps::Cleanup()
{
	for (auto& pad : m_sdlPads)
	{
		pad.Close();
	}
}

void Input_ps::RegisterDeviceChange()
{
	// Drop pads that have gone away, then give any new ones a slot
	for (auto& pad : m_sdlPads)
	{
		if (pad.IsOpen() && !pad.IsConnected())
		{
			pad.Close();
		}
	}

	int iCount = 0;
	SDL_JoystickID* pIDs = SDL_GetGamepads(&iCount);
	for (int i = 0; pIDs && i < iCount; i++)
	{
		bool bOpen = false;
		for (auto& pad : m_sdlPads)
		{
			if (pad.IsOpen() && pad.GetID() == pIDs[i])
			{
				bOpen = true;
				break;
			}
		}

		if (bOpen)
		{
			continue;
		}

		for (auto& pad : m_sdlPads)
		{
			if (!pad.IsOpen())
			{
				pad.Open(pIDs[i]);
				break;
			}
		}
	}
	SDL_free(pIDs);
}

void Input_ps::RegisterGamepad(IGamepad* pGamepad)
{
	if (m_uExternalPads < MAX_EXTERNAL_PADS)
	{
		m_pExternalPads[m_uExternalPads++] = pGamepad;
	}
}

void Input_ps::Update(GFXDevice* pDevice)
{
	m_keyboard.Update();
	m_mouse.Update();
	m_mouseWheel = 0;
	MemClear(m_mouseButtonsThisFrame, sizeof(m_mouseButtonsThisFrame));
	m_uInputChars = 0;
}

void Input_ps::RegisterMouseButtonChange(uint32 uButton, bool bValue)
{
	if (uButton >= MOUSE_BUTTON_NONE)
	{
		return;
	}
	m_mouseButtonsDown[uButton] = bValue;
	m_mouseButtonsThisFrame[uButton] |= bValue;
}

void Input_ps::RegisterInputChar(char16 uChar)
{
	if(m_uInputChars < Keyboard::MAX_INPUT_CHARS)
	{
		m_inputChars[m_uInputChars] = uChar;
		m_uInputChars++;
	}
}

void Input_ps::RegisterHwnd(uint32 uDisplay, WindHndl hndl)
{
	m_mouse.SetWindow(hndl);
	if (uDisplay == 0) m_window = hndl;
}

void Input_ps::SetTextInput(bool bActive, const TextInputArea* pArea)
{
	if (!m_window) return;
	if (pArea)
	{
		SDL_Rect rect = { pArea->x, pArea->y, pArea->w, pArea->h };
		SDL_SetTextInputArea(m_window, &rect, pArea->cursor);
	}
	if (bActive == m_bTextInput) return;
	m_bTextInput = bActive;
	// Desktops start text input with the window (SDLUtil) and keep it; where
	// the keyboard is on screen it comes up only while text is asked for.
	if (!SDL_HasScreenKeyboardSupport()) return;
	if (bActive) SDL_StartTextInput(m_window);
	else
	{
		SDL_StopTextInput(m_window);
		RegisterComposition("", 0);
	}
}

void Input_ps::RegisterComposition(const char* szText, sint32 iCursor)
{
	str::Copy(m_composition, szText ? szText : "", sizeof(m_composition));
	m_iCompositionCursor = iCursor;
}

}
