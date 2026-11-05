/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Linux window system services for the Vulkan backend. SDL
//	chooses the Wayland or X11 surface extension at run time.
*****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Graphics/Device/Display.h"
#include "Engine/Core/String/String_Util.h"
#include "Engine/Maths/MathUtil.h"
#include "Engine/Core/_linux/LinuxUtil.h"
#include API_HEADER(Engine/Graphics/Device, VulkanWindow.h)
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace usg {

uint32 VulkanWindow::GetSurfaceExtensions(const char** ppExtensionsOut)
{
	Uint32 uCount = 0;
	const char* const* ppExtensions = SDL_Vulkan_GetInstanceExtensions(&uCount);
	if (!ppExtensions)
	{
		FATAL_RELEASE(false, "SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
		return 0;
	}

	ASSERT(uCount <= MAX_SURFACE_EXTENSIONS);
	uCount = Math::Min(uCount, (Uint32)MAX_SURFACE_EXTENSIONS);
	for (Uint32 i = 0; i < uCount; i++)
	{
		ppExtensionsOut[i] = ppExtensions[i];
	}
	return uCount;
}

VkSurfaceKHR VulkanWindow::CreateSurface(VkInstance instance, WindHndl hndl)
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(hndl, instance, nullptr, &surface))
	{
		DEBUG_PRINT("SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
		return VK_NULL_HANDLE;
	}
	return surface;
}

void VulkanWindow::GetClientSize(WindHndl hndl, uint32& uWidthOut, uint32& uHeightOut)
{
	if (SDL_GetWindowFlags(hndl) & SDL_WINDOW_MINIMIZED)
	{
		uWidthOut = uHeightOut = 0;
		return;
	}
	LINUXUTIL::GetWindowSize(hndl, uWidthOut, uHeightOut);
}

void VulkanWindow::ProcessEvents()
{
	SDL_PumpEvents();
}

bool VulkanWindow::IsHDROutputEnabled(WindHndl hndl)
{
	// SDL reports whether the display the window is on is in HDR mode
	SDL_PropertiesID props = SDL_GetDisplayProperties(SDL_GetDisplayForWindow(hndl));
	return props && SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);
}

void VulkanWindow::CopyBitmapToClipboard(const void* pInfoHeader, memsize uHeaderSize, const void* pPixels, memsize uPixelSize)
{
	// Not supported yet; the screenshot is still written to disk
}

uint32 VulkanWindow::EnumerateDisplays(DisplaySettings* pDisplaysOut, uint32 uMaxDisplays)
{
	int iDisplays = 0;
	SDL_DisplayID* pDisplayIDs = SDL_GetDisplays(&iDisplays);
	uint32 uDisplayCount = 0;

	for (int i = 0; pDisplayIDs && i < iDisplays && uDisplayCount < uMaxDisplays; i++)
	{
		SDL_DisplayID id = pDisplayIDs[i];
		SDL_Rect bounds;
		if (!SDL_GetDisplayBounds(id, &bounds))
		{
			continue;
		}

		DisplaySettings* pSettings = &pDisplaysOut[uDisplayCount];
		const char* szName = SDL_GetDisplayName(id);
		str::Copy(pSettings->name, szName ? szName : "Display", USG_IDENTIFIER_LEN);

		// Report pixels, as Windows does, rather than scaled desktop units
		const SDL_DisplayMode* pDesktop = SDL_GetDesktopDisplayMode(id);
		float fDensity = pDesktop ? pDesktop->pixel_density : 1.0f;
		pSettings->screenDim.x = (sint32)(bounds.x * fDensity);
		pSettings->screenDim.y = (sint32)(bounds.y * fDensity);
		pSettings->screenDim.width = (sint32)(bounds.w * fDensity);
		pSettings->screenDim.height = (sint32)(bounds.h * fDensity);
		pSettings->hardwareHndl = nullptr;
		pSettings->supportedResolutions.clear();

		int iModes = 0;
		SDL_DisplayMode** ppModes = SDL_GetFullscreenDisplayModes(id, &iModes);
		for (int m = 0; ppModes && m < iModes; m++)
		{
			GFXBounds res;
			res.x = 0;
			res.y = 0;
			res.width = (sint32)(ppModes[m]->w * ppModes[m]->pixel_density);
			res.height = (sint32)(ppModes[m]->h * ppModes[m]->pixel_density);

			bool bFound = false;
			for (auto itr : pSettings->supportedResolutions)
			{
				if (itr.width == res.width && itr.height == res.height)
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				pSettings->supportedResolutions.push_back(res);
			}
		}
		SDL_free(ppModes);

		uDisplayCount++;
	}

	SDL_free(pDisplayIDs);
	return uDisplayCount;
}

}
