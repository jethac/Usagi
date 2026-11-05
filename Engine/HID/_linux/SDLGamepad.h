/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A gamepad read through SDL's gamepad API, which maps any
//	controller in its database to an Xbox-style layout
*****************************************************************************/
#ifndef __USG_HID_LINUX_SDL_GAMEPAD_H__
#define __USG_HID_LINUX_SDL_GAMEPAD_H__

#include "Engine/HID/IGamepad.h"

struct SDL_Gamepad;

namespace usg{

class SDLGamepad : public IGamepad
{
public:
	SDLGamepad();
	~SDLGamepad();

	// uID is an SDL_JoystickID
	bool Open(uint32 uID);
	void Close();
	bool IsOpen() const { return m_pGamepad != nullptr; }
	uint32 GetID() const { return m_uID; }

	virtual uint32 GetCaps() const override { return CAP_LEFT_STICK | CAP_RIGHT_STICK | CAP_GAMEPAD; }
	virtual bool IsConnected() const override;
	virtual const char* GetProductName() const override;
	virtual void Update(GFXDevice* pDevice, GamepadDeviceState& deviceStateOut) override;
	virtual const char* GetModuleName() const { return "SDLGamepad"; }
	virtual void Vibrate(float fLeft, float fRight) override;

private:
	PRIVATIZE_COPY(SDLGamepad)

	SDL_Gamepad*	m_pGamepad;
	uint32			m_uID;
};

}

#endif
