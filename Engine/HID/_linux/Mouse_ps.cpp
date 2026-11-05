/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Maths/MathUtil.h"
#include "Input_ps.h"
#include "Mouse_ps.h"
#include <SDL3/SDL.h>

namespace usg
{
	void Mouse_ps::Init(Input_ps* pOwner)
	{
		m_pOwner = pOwner;
		for (uint32 i = 0; i < _MouseButton_count; i++)
		{
			m_prevButtons[i] = false;
			m_buttons[i] = false;
		}
		for (uint32 i = 0; i < _MouseAxis_count; i++)
		{
			m_fAxis[i] = 0.0f;
		}
	}

	void Mouse_ps::Update()
	{
		for(uint32 i=0; i<MOUSE_BUTTON_NONE; i++)
		{
			m_prevButtons[i] = m_buttons[i];
			m_buttons[i] = m_pOwner->GetMouseButtonDown(i);
		}

		if (!m_window)
		{
			return;
		}

		// SDL reports the position in window coordinates; the renderer works
		// in pixels, which differ under display scaling
		float fX = 0.0f, fY = 0.0f;
		SDL_GetMouseState(&fX, &fY);
		float fDensity = SDL_GetWindowPixelDensity(m_window);
		fX *= fDensity;
		fY *= fDensity;

		int iWidth = 1, iHeight = 1;
		SDL_GetWindowSizeInPixels(m_window, &iWidth, &iHeight);

		// As on Windows, release the buttons when the cursor leaves the window
		if (SDL_GetMouseFocus() != m_window)
		{
			m_buttons[MOUSE_BUTTON_LEFT] = false;
			m_buttons[MOUSE_BUTTON_RIGHT] = false;
			m_buttons[MOUSE_BUTTON_MIDDLE] = false;
		}

		m_fAxis[MOUSE_DELTA_X] = m_fAxis[MOUSE_POS_X] - fX;
		m_fAxis[MOUSE_DELTA_Y] = m_fAxis[MOUSE_POS_Y] - fY;
		m_fAxis[MOUSE_POS_X] = fX;
		m_fAxis[MOUSE_POS_Y] = fY;

		float fWidth = (float)Math::Max(iWidth, 1);
		float fHeight = (float)Math::Max(iHeight, 1);
		m_fAxis[MOUSE_NORM_POS_X] = ((m_fAxis[MOUSE_POS_X] / fWidth) * 2.0f) - 1.0f;
		m_fAxis[MOUSE_NORM_ASPECT_POS_X] = usg::Math::Clamp(m_fAxis[MOUSE_NORM_POS_X] * fWidth/fHeight, -1.0f, 1.0f);
		m_fAxis[MOUSE_NORM_POS_Y] = -(((m_fAxis[MOUSE_POS_Y] / fHeight) * 2.0f) - 1.0f);
		m_fAxis[MOUSE_DELTA_X_NORM] = ((m_fAxis[MOUSE_DELTA_X] / fWidth) * 2.0f) - 1.0f;
		m_fAxis[MOUSE_DELTA_X_NORM_ASPECT_X] = usg::Math::Clamp(((m_fAxis[MOUSE_DELTA_X] / fHeight) * 2.0f) - 1.0f, -1.0f, 1.0f);
		m_fAxis[MOUSE_DELTA_Y_NORM] = -(((m_fAxis[MOUSE_DELTA_Y] / fHeight) * 2.0f) - 1.0f);

		long mouseWheel = m_pOwner->GetMouseWheel();
		m_fAxis[MOUSE_DELTA_WHEEL] = mouseWheel != 0 ? Math::Sign((float)mouseWheel) : 0.0f;

		m_fAxis[MOUSE_NORM_CUSTOM_BOX_X] = m_mouseBox.bAspectCorrect ? m_fAxis[MOUSE_NORM_ASPECT_POS_X] : m_fAxis[MOUSE_NORM_POS_X];
		m_fAxis[MOUSE_NORM_CUSTOM_BOX_Y] = m_fAxis[MOUSE_NORM_POS_Y];
		m_fAxis[MOUSE_NORM_CUSTOM_BOX_X] -= m_mouseBox.vCentre.x;
		m_fAxis[MOUSE_NORM_CUSTOM_BOX_Y] -= m_mouseBox.vCentre.y;
		m_fAxis[MOUSE_NORM_CUSTOM_BOX_X] /= m_mouseBox.vBounds.x;
		m_fAxis[MOUSE_NORM_CUSTOM_BOX_Y] /= m_mouseBox.vBounds.y;
		m_fAxis[MOUSE_NORM_CUSTOM_BOX_X] = usg::Math::Clamp(m_fAxis[MOUSE_NORM_CUSTOM_BOX_X], -1.f, 1.0f);
		m_fAxis[MOUSE_NORM_CUSTOM_BOX_Y] = usg::Math::Clamp(m_fAxis[MOUSE_NORM_CUSTOM_BOX_Y], -1.f, 1.0f);
	}
}
