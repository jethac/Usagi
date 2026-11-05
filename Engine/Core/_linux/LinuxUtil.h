/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Linux window and event handling, the counterpart of
//	WINUTIL. Windows are SDL windows; SDL picks Wayland or X11 at run time.
*****************************************************************************/
#ifndef _USG_CORE_LINUX_LINUXUTIL_H_
#define _USG_CORE_LINUX_LINUXUTIL_H_

#include "Engine/Graphics/Device/Display.h"

namespace LINUXUTIL
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
