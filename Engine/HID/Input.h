/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Raw user input (mouse/keyboard/touchscreen/pointer etc)
*****************************************************************************/
#ifndef __USG_HID_HID_H__
#define __USG_HID_HID_H__

#include "Engine/HID/InputDefines.h"
#include "Gamepad.h"
#include "Microphone.h"

namespace usg{

class Input_ps;
class Keyboard;
class Mouse;
class IGamepad;

typedef int (*EvaluatePad) (const IGamepad* pDad);

// Where text is being typed, in window pixels: an IME places its candidate
// window by it, and a phone keeps it clear of its on-screen keyboard.
struct TextInputArea
{
	sint32	x, y, w, h;
	sint32	cursor;		// offset of the caret from x, pixels
};

class Input
{
public:	
	static void Init();
	static void Cleanup();
	static void RenumberGamepads(EvaluatePad fnEvaluatePad = DefaultEvaluate);
	static void Update(usg::GFXDevice* pDevice, float fDelta);
	static void RegisterGamepad(IGamepad* pGamepad);

	static Gamepad*	GetGamepad(uint32 uGamepad);
	static uint32 GetGamepadCount();
	static Gamepad* GetAccerometer();
	static Keyboard* GetKeyboard();
	static Mouse* GetMouse();

	static Microphone* GetMicrophone(uint32 uMicrophone);

	static Input_ps&	GetPlatform();

	static void DeviceChange();

	// Asks for typed text (a text field has focus): on a phone this brings
	// up the on-screen keyboard, and false puts it away. Text arrives as the
	// keyboard's input characters; an IME's unfinished composition as
	// Keyboard::GetComposition. Desktop keyboards type regardless.
	static void SetTextInput(bool bActive, const TextInputArea* pArea = nullptr);

	static int DefaultEvaluate(const IGamepad* pPad) { return 1; }

private:
};

}

#endif
