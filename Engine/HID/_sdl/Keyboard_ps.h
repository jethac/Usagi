/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Keyboard read from SDL's key state, using the same key
//	codes as DirectInputKeyboard on Windows
*****************************************************************************/
#ifndef __USG_HID_SDL_KEYBOARD_PS_H__
#define __USG_HID_SDL_KEYBOARD_PS_H__

#include "Engine/HID/Gamepad.h"
#include "Engine/HID/Keyboard.h"

namespace usg{

class Input_ps;

class Keyboard_ps : public Keyboard
{
public:
	Keyboard_ps() {}
	~Keyboard_ps() {}

	void Init(Input_ps* pOwner);
	void Update();

private:
	const Input_ps*	m_pOwner;
};

}

#endif
