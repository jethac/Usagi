/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: The window system services the Vulkan backend needs. Each
//	OS implements these in its own VulkanWindow_ps.cpp
*****************************************************************************/
#ifndef USAGI_ENGINE_GRAPHICS_VULKAN_WINDOW_H
#define USAGI_ENGINE_GRAPHICS_VULKAN_WINDOW_H

#include OS_HEADER(Engine/Graphics/Device, VulkanIncludes.h)

namespace usg {

struct DisplaySettings;

namespace VulkanWindow
{
	enum
	{
		MAX_SURFACE_EXTENSIONS = 4
	};

	// Writes the instance extensions needed to present to a window and
	// returns how many there are (at most MAX_SURFACE_EXTENSIONS)
	uint32 GetSurfaceExtensions(const char** ppExtensionsOut);

	// Returns VK_NULL_HANDLE on failure
	VkSurfaceKHR CreateSurface(VkInstance instance, WindHndl hndl);

	// The size of the window's drawable area in pixels; zero while minimised
	void GetClientSize(WindHndl hndl, uint32& uWidthOut, uint32& uHeightOut);

	// Lets the window system run while the backend waits on the window,
	// e.g. to be restored from minimised
	void ProcessEvents();

	// True if the output the window is on is set up for HDR
	bool IsHDROutputEnabled(WindHndl hndl);

	// Copies a 24-bit bottom-up DIB (BITMAPINFOHEADER followed by pixels)
	// to the clipboard, where the OS supports it
	void CopyBitmapToClipboard(const void* pInfoHeader, memsize uHeaderSize, const void* pPixels, memsize uPixelSize);

	// Fills in up to uMaxDisplays entries; returns the number found
	uint32 EnumerateDisplays(DisplaySettings* pDisplaysOut, uint32 uMaxDisplays);
}

}

#endif
