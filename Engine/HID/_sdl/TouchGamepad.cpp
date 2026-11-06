/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Maths/MathUtil.h"
#include "Engine/HID/InputStructs.h"
#include "TouchGamepad.h"

namespace usg{

// The stick reaches full deflection this far from where the finger landed,
// as a fraction of the window's height
static const float g_fStickTravel = 0.12f;
static const float g_fStickDeadZone = 0.1f;

// The right half of the screen, in four zones a thumb can slide between:
//   X (top left)    | R (top right)
//   B (bottom left) | A (bottom right)
struct ButtonZone
{
	float	fX0, fY0, fX1, fY1;
	uint32	uButton;
};

static const ButtonZone g_buttonZones[] =
{
	{ 0.75f, 0.45f, 1.00f, 1.00f, GAMEPAD_BUTTON_A },
	{ 0.50f, 0.45f, 0.75f, 1.00f, GAMEPAD_BUTTON_B },
	{ 0.75f, 0.00f, 1.00f, 0.45f, GAMEPAD_BUTTON_R },
	{ 0.50f, 0.00f, 0.75f, 0.45f, GAMEPAD_BUTTON_X },
};

TouchGamepad::TouchGamepad()
	: m_fAspect(16.0f / 9.0f)
	, m_bEnabled(false)
{
	ReleaseAll();
}

void TouchGamepad::ReleaseAll()
{
	for (Finger& finger : m_fingers)
	{
		finger.uRole = ROLE_NONE;
		finger.uID = 0;
	}
}

TouchGamepad::Finger* TouchGamepad::FindFinger(uint64 uFingerID)
{
	for (Finger& finger : m_fingers)
	{
		if (finger.uRole != ROLE_NONE && finger.uID == uFingerID)
		{
			return &finger;
		}
	}
	return nullptr;
}

uint32 TouchGamepad::GetButtonAt(float fX, float fY)
{
	for (const ButtonZone& zone : g_buttonZones)
	{
		if (fX >= zone.fX0 && fX <= zone.fX1 && fY >= zone.fY0 && fY <= zone.fY1)
		{
			return zone.uButton;
		}
	}
	return GAMEPAD_BUTTON_NONE;
}

void TouchGamepad::RegisterFingerDown(uint64 uFingerID, float fX, float fY)
{
	if (FindFinger(uFingerID))
	{
		RegisterFingerMotion(uFingerID, fX, fY);
		return;
	}

	bool bStickTaken = false;
	for (const Finger& finger : m_fingers)
	{
		bStickTaken |= finger.uRole == ROLE_STICK;
	}

	for (Finger& finger : m_fingers)
	{
		if (finger.uRole == ROLE_NONE)
		{
			// One finger steers; any others on the left half are ignored
			const bool bLeft = fX < 0.5f;
			if (bLeft && bStickTaken)
			{
				return;
			}
			finger.uID = uFingerID;
			finger.uRole = bLeft ? ROLE_STICK : ROLE_BUTTONS;
			finger.fStartX = finger.fX = fX;
			finger.fStartY = finger.fY = fY;
			return;
		}
	}
}

void TouchGamepad::RegisterFingerMotion(uint64 uFingerID, float fX, float fY)
{
	Finger* pFinger = FindFinger(uFingerID);
	if (pFinger)
	{
		pFinger->fX = fX;
		pFinger->fY = fY;
	}
}

void TouchGamepad::RegisterFingerUp(uint64 uFingerID)
{
	Finger* pFinger = FindFinger(uFingerID);
	if (pFinger)
	{
		pFinger->uRole = ROLE_NONE;
	}
}

void TouchGamepad::Update(GFXDevice* pDevice, GamepadDeviceState& deviceStateOut)
{
	deviceStateOut.ClearButtons();
	deviceStateOut.fAxisValues[GAMEPAD_AXIS_LEFT_X] = 0.0f;
	deviceStateOut.fAxisValues[GAMEPAD_AXIS_LEFT_Y] = 0.0f;
	deviceStateOut.bScreenTouch = false;

	if (!m_bEnabled)
	{
		return;
	}

	for (const Finger& finger : m_fingers)
	{
		if (finger.uRole == ROLE_NONE)
		{
			continue;
		}

		if (!deviceStateOut.bScreenTouch)
		{
			deviceStateOut.bScreenTouch = true;
			deviceStateOut.vTouchPos = Vector2f(finger.fX, finger.fY);
		}

		if (finger.uRole == ROLE_STICK)
		{
			// Horizontal distances are in window widths; scale them to
			// heights so the travel is the same in both directions
			float fDX = (finger.fX - finger.fStartX) * m_fAspect / g_fStickTravel;
			float fDY = (finger.fY - finger.fStartY) / g_fStickTravel;
			fDX = Math::Clamp(fDX, -1.0f, 1.0f);
			fDY = Math::Clamp(fDY, -1.0f, 1.0f);
			if (fabsf(fDX) < g_fStickDeadZone) fDX = 0.0f;
			if (fabsf(fDY) < g_fStickDeadZone) fDY = 0.0f;
			// The engine's Y axes point up
			deviceStateOut.fAxisValues[GAMEPAD_AXIS_LEFT_X] = fDX;
			deviceStateOut.fAxisValues[GAMEPAD_AXIS_LEFT_Y] = -fDY;
		}
		else
		{
			uint32 uButton = GetButtonAt(finger.fX, finger.fY);
			if (uButton != GAMEPAD_BUTTON_NONE)
			{
				deviceStateOut.SetButton(uButton, true);
			}
		}
	}
}

}
