/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Maths/MathUtil.h"
#include "Engine/HID/InputStructs.h"
#include "SDLGamepad.h"
#include <SDL3/SDL.h>

namespace usg{

// Same dead zones as XInput's defaults
static const sint16 g_sLeftDeadZone = 7849;
static const sint16 g_sRightDeadZone = 8689;
static const sint16 g_sTriggerThreshold = 30 * 128;

struct ButtonMapping
{
	uint32				uAbstractID;
	SDL_GamepadButton	eSDLButton;
};

// Positional mapping matching XInputPad: the engine's A is the east button
static const ButtonMapping g_buttonMapping[] =
{
	{ GAMEPAD_BUTTON_A,			SDL_GAMEPAD_BUTTON_EAST },
	{ GAMEPAD_BUTTON_B,			SDL_GAMEPAD_BUTTON_SOUTH },
	{ GAMEPAD_BUTTON_X,			SDL_GAMEPAD_BUTTON_NORTH },
	{ GAMEPAD_BUTTON_Y,			SDL_GAMEPAD_BUTTON_WEST },
	{ GAMEPAD_BUTTON_L,			SDL_GAMEPAD_BUTTON_LEFT_SHOULDER },
	{ GAMEPAD_BUTTON_R,			SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
	{ GAMEPAD_BUTTON_UP,		SDL_GAMEPAD_BUTTON_DPAD_UP },
	{ GAMEPAD_BUTTON_DOWN,		SDL_GAMEPAD_BUTTON_DPAD_DOWN },
	{ GAMEPAD_BUTTON_LEFT,		SDL_GAMEPAD_BUTTON_DPAD_LEFT },
	{ GAMEPAD_BUTTON_RIGHT,		SDL_GAMEPAD_BUTTON_DPAD_RIGHT },
	{ GAMEPAD_BUTTON_START,		SDL_GAMEPAD_BUTTON_START },
	{ GAMEPAD_BUTTON_SELECT,	SDL_GAMEPAD_BUTTON_BACK },
	{ GAMEPAD_BUTTON_HOME,		SDL_GAMEPAD_BUTTON_GUIDE },
	{ GAMEPAD_BUTTON_THUMB_L,	SDL_GAMEPAD_BUTTON_LEFT_STICK },
	{ GAMEPAD_BUTTON_THUMB_R,	SDL_GAMEPAD_BUTTON_RIGHT_STICK },
	{ GAMEPAD_BUTTON_NONE,		SDL_GAMEPAD_BUTTON_INVALID }
};

static float GetAxisWithDeadZone(sint16 value, sint16 deadzone)
{
	if(value > deadzone || value < -deadzone)
	{
		float fSign = Math::Sign((float)value);
		float fDead = fSign * deadzone;
		return Math::Clamp(((float)value - fDead)/(32767.f - deadzone), -1.0f, 1.0f);
	}
	return 0.0f;
}

SDLGamepad::SDLGamepad()
	: m_pGamepad(nullptr)
	, m_uID(0)
{
}

SDLGamepad::~SDLGamepad()
{
	Close();
}

bool SDLGamepad::Open(uint32 uID)
{
	Close();
	m_pGamepad = SDL_OpenGamepad((SDL_JoystickID)uID);
	m_uID = m_pGamepad ? uID : 0;
	return m_pGamepad != nullptr;
}

void SDLGamepad::Close()
{
	if (m_pGamepad)
	{
		SDL_CloseGamepad(m_pGamepad);
		m_pGamepad = nullptr;
		m_uID = 0;
	}
}

bool SDLGamepad::IsConnected() const
{
	return m_pGamepad && SDL_GamepadConnected(m_pGamepad);
}

const char* SDLGamepad::GetProductName() const
{
	const char* szName = m_pGamepad ? SDL_GetGamepadName(m_pGamepad) : nullptr;
	return szName ? szName : "SDLGamepad";
}

void SDLGamepad::Update(GFXDevice* pDevice, GamepadDeviceState& deviceStateOut)
{
	deviceStateOut.ClearButtons();

	if (!IsConnected())
	{
		return;
	}

	// SDL's Y axes point down; the engine's point up, as XInput's do
	deviceStateOut.fAxisValues[GAMEPAD_AXIS_LEFT_X] = GetAxisWithDeadZone(SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_LEFTX), g_sLeftDeadZone);
	deviceStateOut.fAxisValues[GAMEPAD_AXIS_LEFT_Y] = -GetAxisWithDeadZone(SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_LEFTY), g_sLeftDeadZone);
	deviceStateOut.fAxisValues[GAMEPAD_AXIS_RIGHT_X] = GetAxisWithDeadZone(SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_RIGHTX), g_sRightDeadZone);
	deviceStateOut.fAxisValues[GAMEPAD_AXIS_RIGHT_Y] = -GetAxisWithDeadZone(SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_RIGHTY), g_sRightDeadZone);

	for (const ButtonMapping* pMapping = g_buttonMapping; pMapping->uAbstractID != GAMEPAD_BUTTON_NONE; pMapping++)
	{
		if (SDL_GetGamepadButton(m_pGamepad, pMapping->eSDLButton))
		{
			deviceStateOut.SetButton(pMapping->uAbstractID, true);
		}
	}

	if (SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > g_sTriggerThreshold)
		deviceStateOut.SetButton(GAMEPAD_BUTTON_ZL, true);
	if (SDL_GetGamepadAxis(m_pGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > g_sTriggerThreshold)
		deviceStateOut.SetButton(GAMEPAD_BUTTON_ZR, true);
}

void SDLGamepad::Vibrate(float fLeft, float fRight)
{
	if (m_pGamepad)
	{
		Uint16 uLeft = (Uint16)(Math::Clamp(fLeft, 0.0f, 1.0f) * 65535.f);
		Uint16 uRight = (Uint16)(Math::Clamp(fRight, 0.0f, 1.0f) * 65535.f);
		// Zero duration would stop at once; rumble until told otherwise
		SDL_RumbleGamepad(m_pGamepad, uLeft, uRight, (uLeft || uRight) ? SDL_MAX_UINT32 : 0);
	}
}

}
