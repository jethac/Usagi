/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Win32 window system services for the Vulkan backend
*****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Graphics/Device/Display.h"
#include "Engine/Graphics/GFX.h"
#include "Engine/Core/String/String_Util.h"
#include "Engine/Core/_win/WinUtil.h"
#include "Engine/Maths/MathUtil.h"
#include API_HEADER(Engine/Graphics/Device, VulkanWindow.h)
#include <dxgi.h>
#include <dxgi1_6.h>

#pragma comment(lib, "dxgi.lib")

namespace usg {

static inline LONG InterSection(const RECT rect1, const RECT rect2)
{
	LONG xInterSect = Math::Max(0L, Math::Min(rect1.right, rect2.right) - Math::Max(rect1.left, rect2.left));
	LONG yIntersect = Math::Max(0L, Math::Min(rect1.bottom, rect2.bottom) - Math::Max(rect1.top, rect2.top));
	LONG intersection = xInterSect * yIntersect;
	return intersection;
}

uint32 VulkanWindow::GetSurfaceExtensions(const char** ppExtensionsOut)
{
	ppExtensionsOut[0] = VK_KHR_SURFACE_EXTENSION_NAME;
	ppExtensionsOut[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	return 2;
}

VkSurfaceKHR VulkanWindow::CreateSurface(VkInstance instance, WindHndl hndl)
{
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.hinstance = WINUTIL::GetInstanceHndl();
	createInfo.hwnd = hndl;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
	{
		return VK_NULL_HANDLE;
	}
	return surface;
}

void VulkanWindow::GetClientSize(WindHndl hndl, uint32& uWidthOut, uint32& uHeightOut)
{
	RECT dim;
	GetClientRect(hndl, &dim);
	uWidthOut = dim.right - dim.left;
	uHeightOut = dim.bottom - dim.top;
}

void VulkanWindow::ProcessEvents()
{
	// Window messages are pumped by GFX::PostUpdate on Windows; nothing
	// extra is needed while waiting on the window
}

bool VulkanWindow::IsHDROutputEnabled(WindHndl hndl)
{
	bool bAllowHDR = false;

	RECT windowBounds = {};
	GetWindowRect(hndl, &windowBounds);
	IDXGIAdapter1* dxgiAdapter = NULL;
	IDXGIFactory6* factory = nullptr;

	if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory))))
	{
		HRCHECK( factory->EnumAdapters1(0, &dxgiAdapter));
		UINT         i = 0;
		IDXGIOutput* bestOutput = nullptr;
		IDXGIOutput* output = nullptr;
		LONG         bestAdapter = -1;
		while (dxgiAdapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_OUTPUT_DESC desc;
			output->GetDesc(&desc);
			LONG intersection = InterSection(windowBounds, desc.DesktopCoordinates);
			if (intersection > bestAdapter)
			{
				bestAdapter = intersection;
				bestOutput = output;
			}
			++i;
		}
		IDXGIOutput6* output6;
		HRCHECK(bestOutput->QueryInterface(IID_PPV_ARGS(&output6)));
		DXGI_OUTPUT_DESC1 desc1;
		HRCHECK(output6->GetDesc1(&desc1));

		// This will be true if the user has use HDR set to true in windows, false otherwise
		bAllowHDR = desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;

		factory->Release();
	}

	return bAllowHDR;
}

void VulkanWindow::CopyBitmapToClipboard(const void* pInfoHeader, memsize uHeaderSize, const void* pPixels, memsize uPixelSize)
{
	HANDLE handle = (HANDLE)::GlobalAlloc(GHND, uHeaderSize + uPixelSize);
	if (handle != nullptr)
	{
		char* pHndlData = (char*) ::GlobalLock((HGLOBAL)handle);

		BOOL RETB = OpenClipboard(nullptr);
		RETB = EmptyClipboard();

		memcpy(pHndlData, pInfoHeader, uHeaderSize);

		memcpy(pHndlData + uHeaderSize, pPixels, uPixelSize);

		::GlobalUnlock((HGLOBAL)handle);

		SetClipboardData(CF_DIB, handle);
		CloseClipboard();
	}
}

uint32 VulkanWindow::EnumerateDisplays(DisplaySettings* pDisplaysOut, uint32 uMaxDisplays)
{
	uint32 uDisplayCount = 0;

	DISPLAY_DEVICE DispDev;
	ZeroMemory(&DispDev, sizeof(DISPLAY_DEVICE));
	DispDev.cb = sizeof(DISPLAY_DEVICE);

	// Pass in a value over than zero to the second parameter to enumerate graphics cards
	for (DWORD uDeviceIndex = 0; uDisplayCount < uMaxDisplays && EnumDisplayDevices(NULL, uDeviceIndex, &DispDev, 0); uDeviceIndex++)
	{
		if (!(DispDev.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
		{
			DisplaySettings* pSettings = &pDisplaysOut[uDisplayCount];
			str::Copy(pSettings->name, DispDev.DeviceString, USG_IDENTIFIER_LEN);
			DEVMODE dm;
			ZeroMemory(&dm, sizeof(dm));
			dm.dmSize = sizeof(dm);
			if (EnumDisplaySettingsEx(DispDev.DeviceName, ENUM_CURRENT_SETTINGS, &dm, 0) == FALSE)
			{
				BOOL bReturn = EnumDisplaySettingsEx(DispDev.DeviceName, ENUM_REGISTRY_SETTINGS, &dm, 0);
				ASSERT(bReturn == TRUE);
			}

			HMONITOR hm = 0;
			MONITORINFO mi;
			ZeroMemory(&mi, sizeof(mi));
			mi.cbSize = sizeof(mi);
			if (DispDev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
			{
				// display is enabled. only enabled displays have a monitor handle
				POINT pt = { dm.dmPosition.x, dm.dmPosition.y };
				hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
				if (hm)
				{
					GetMonitorInfo(hm, &mi);
					pSettings->screenDim.x = mi.rcMonitor.left;
					pSettings->screenDim.y = mi.rcMonitor.top;
					pSettings->screenDim.width = mi.rcMonitor.right - mi.rcMonitor.left;
					pSettings->screenDim.height = Math::Abs(mi.rcMonitor.top - mi.rcMonitor.bottom);
					uDisplayCount++;
				}
			}

			// Get other supported resolutions
			DWORD iMode = 0;
			while(EnumDisplaySettingsEx(DispDev.DeviceName, iMode, &dm, 0))
			{
				if (DispDev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
				{
					GFXBounds bounds;
					bounds.x = 0;
					bounds.y = 0;
					bounds.width = dm.dmPelsWidth;
					bounds.height = dm.dmPelsHeight;

					bool bFound = false;
					for (auto itr : pSettings->supportedResolutions)
					{
						if (itr.width == bounds.width && itr.height == bounds.height)
						{
							bFound = true;
							break;
						}
					}

					if(!bFound)
					{
						pSettings->supportedResolutions.push_back(bounds);
					}
				}

				iMode++;
			}
		}
	}

	return uDisplayCount;
}

}
