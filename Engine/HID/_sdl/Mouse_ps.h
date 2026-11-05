/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Mouse read from SDL, relative to the game window
*****************************************************************************/
#ifndef __USG_HID_SDL_MOUSE_PS_H__
#define __USG_HID_SDL_MOUSE_PS_H__

#include "Engine/HID/Gamepad.h"
#include "Engine/HID/Mouse.h"

namespace usg{

class Input_ps;

class Mouse_ps : public Mouse
{
public:
	Mouse_ps() : m_pOwner(nullptr), m_window(nullptr) {}
	~Mouse_ps() {}

	void Init(Input_ps* pOwner);
	void Update();
	void SetWindow(WindHndl window) { m_window = window; }

private:
	const Input_ps*	m_pOwner;
	WindHndl		m_window;
};

}

#endif
