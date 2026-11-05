/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Window and event handling for the platforms that use SDL
//	(Linux, macOS, iOS, Android); the counterpart of WINUTIL. SDL picks the
//	window system at run time (Wayland or X11 on Linux, Cocoa, UIKit).
*****************************************************************************/
#ifndef _USG_CORE_SDL_SDLUTIL_H_
#define _USG_CORE_SDL_SDLUTIL_H_

#include "Engine/Graphics/Device/Display.h"

namespace SDLUTIL
{
	// Initialises SDL's video, event and gamepad subsystems. Call before
	// creating a window
	bool Init();
	void Shutdown();

	WindHndl		GetWindow();
	void			SetWindow(WindHndl hndl);
	void			SetInFocus(bool bInFocus);
	bool			GetInFocus();

	WindHndl CreateDisplayWindow(const char* szName, const usg::DisplayMode* pDisplaySettings, bool bHidden);
	void UpdateWindow(const usg::DisplayMode* pDisplaySettings);

	// The size of the window's drawable area in pixels
	void GetWindowSize(WindHndl hndl, uint32& uWidthOut, uint32& uHeightOut);

	// Dispatches pending window and input events to the engine (Input_ps
	// and usg::GameMessage), as the Win32 WindowProc does on Windows
	void PumpEvents();
};

#endif
