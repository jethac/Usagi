/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: An on-screen gamepad driven by touch, for phones and
//	tablets. The left half of the screen is an analogue stick that centres
//	wherever the finger lands; the right half holds four face buttons.
*****************************************************************************/
#ifndef __USG_HID_SDL_TOUCH_GAMEPAD_H__
#define __USG_HID_SDL_TOUCH_GAMEPAD_H__

#include "Engine/HID/IGamepad.h"

namespace usg{

class TouchGamepad : public IGamepad
{
public:
	TouchGamepad();
	~TouchGamepad() {}

	// The pad only reports itself connected when enabled; desktops with a
	// touch screen (the Livingroom table) leave it off
	void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

	// Finger positions are normalised to the window (0..1, y down). The
	// window's aspect ratio keeps the stick's travel circular.
	void RegisterFingerDown(uint64 uFingerID, float fX, float fY);
	void RegisterFingerMotion(uint64 uFingerID, float fX, float fY);
	void RegisterFingerUp(uint64 uFingerID);
	void SetAspectRatio(float fWidthOverHeight) { m_fAspect = fWidthOverHeight; }
	void ReleaseAll();

	virtual uint32 GetCaps() const override { return CAP_LEFT_STICK | CAP_GAMEPAD | CAP_POINTER | CAP_VIRTUAL_PAD; }
	virtual bool IsConnected() const override { return m_bEnabled; }
	virtual const char* GetProductName() const override { return "TouchGamepad"; }
	virtual const char* GetModuleName() const { return "TouchGamepad"; }
	virtual void Update(GFXDevice* pDevice, GamepadDeviceState& deviceStateOut) override;

private:
	PRIVATIZE_COPY(TouchGamepad)

	enum
	{
		MAX_FINGERS = 10,
		ROLE_NONE = 0,
		ROLE_STICK,
		ROLE_BUTTONS
	};

	struct Finger
	{
		uint64	uID;
		uint32	uRole;
		float	fStartX, fStartY;
		float	fX, fY;
	};

	Finger* FindFinger(uint64 uFingerID);
	static uint32 GetButtonAt(float fX, float fY);

	Finger	m_fingers[MAX_FINGERS];
	float	m_fAspect;
	bool	m_bEnabled;
};

}

#endif
