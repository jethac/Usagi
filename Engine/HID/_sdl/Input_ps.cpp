/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Memory/MemUtil.h"
#include "Input_ps.h"
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
}

}
