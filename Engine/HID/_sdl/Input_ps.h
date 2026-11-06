/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Raw user input on the SDL platforms. Keyboard, mouse and
//	gamepads come from SDL; SDLUtil feeds events in through the Register*
//	functions.
*****************************************************************************/
#ifndef __USG_HID_SDL_INPUT_PS_H__
#define __USG_HID_SDL_INPUT_PS_H__

#include "Engine/HID/InputDefines.h"
#include "Engine/Core/stl/vector.h"
#include "Engine/HID/Microphone.h"
#include HID_HEADER(Engine/HID, VirtualGamepad.h)
#include HID_HEADER(Engine/HID, Keyboard_ps.h)
#include HID_HEADER(Engine/HID, Mouse_ps.h)
#include HID_HEADER(Engine/HID, SDLGamepad.h)
#include HID_HEADER(Engine/HID, TouchGamepad.h)

namespace usg{

class Gamepad;
class GFXDevice;

class Input_ps
{
public:
	Input_ps();
	~Input_ps();

	void Init();
	void Cleanup();
	void Update(GFXDevice* pDevice);

	void		GetActiveGamepads(usg::vector<IGamepad*>& gamepads);
	Gamepad*	GetAccerometer() { return NULL; }
	Microphone* GetMicrophone(uint32 uMic) { return NULL; }
	Keyboard*	GetKeyboard() { return &m_keyboard; }
	Mouse*		GetMouse() { return &m_mouse; }

	// Called from the SDL event pump
	void		RegisterMouseButtonChange(uint32 uButton, bool bValue);
	void		RegisterMouseWheel(long move) { m_mouseWheel += move; }
	void		RegisterInputChar(char16 uChar);
	void		RegisterDeviceChange();
	void		RegisterGamepad(IGamepad* pGamepad);
	TouchGamepad& GetTouchGamepad() { return m_touchGamepad; }

	bool		GetMouseButtonDown(uint32 uButton) const;
	long		GetMouseWheel() const { return m_mouseWheel; }
	uint32		GetInputChars() const { return m_uInputChars; }
	char16		GetInputChar(uint32 uIndex) const { return m_inputChars[uIndex]; }

	void		RegisterHwnd(uint32 uDisplay, WindHndl hndl);

	// Text input (SDL_StartTextInput / SDL_StopTextInput, which show and
	// hide a phone's on-screen keyboard) and IME composition.
	void		SetTextInput(bool bActive, const struct TextInputArea* pArea);
	void		RegisterComposition(const char* szText, sint32 iCursor);
	const char*	GetComposition() const { return m_composition; }
	sint32		GetCompositionCursor() const { return m_iCompositionCursor; }
private:
	PRIVATIZE_COPY(Input_ps);

	enum
	{
		MAX_EXTERNAL_PADS = 4,
		MAX_SDL_PADS = 4
	};

	VirtualGamepad	m_virtualGamepad;
	Mouse_ps		m_mouse;
	Keyboard_ps		m_keyboard;

	SDLGamepad		m_sdlPads[MAX_SDL_PADS];
	TouchGamepad	m_touchGamepad;
	IGamepad*		m_pExternalPads[MAX_EXTERNAL_PADS];
	uint32			m_uExternalPads;

	long		m_mouseWheel;
	char16		m_inputChars[Keyboard::MAX_INPUT_CHARS];
	uint32		m_uInputChars;
	WindHndl	m_window;
	bool		m_bTextInput;
	char		m_composition[Keyboard::MAX_COMPOSITION];
	sint32		m_iCompositionCursor;
	bool		m_mouseButtonsDown[MOUSE_BUTTON_NONE];
	bool		m_mouseButtonsThisFrame[MOUSE_BUTTON_NONE];
};

inline bool	Input_ps::GetMouseButtonDown(uint32 uButton) const
{
	ASSERT(uButton< MOUSE_BUTTON_NONE);
	return m_mouseButtonsDown[uButton] | m_mouseButtonsThisFrame[uButton];
}

}

#endif
