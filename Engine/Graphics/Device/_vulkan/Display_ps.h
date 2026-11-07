#/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Represents a final destination screen, or window (not a viewport)
*****************************************************************************/
#ifndef _USG_GRAPHICS_DISPLAY_PC_H_
#define _USG_GRAPHICS_DISPLAY_PC_H_

#include "Engine/Graphics/Color.h"
#include "Engine/Graphics/RenderConsts.h"
#include "Engine/Graphics/Viewports/Viewport.h"
#include "Engine/Graphics/Textures/RenderTarget.h"
#include <thread>

namespace usg {

class Display_ps
{
public:
	Display_ps();
	~Display_ps();

	void Initialise(usg::GFXDevice* pDevice, WindHndl hndl);
	void Cleanup(usg::GFXDevice* pDevice);
	void SetAsTarget(VkCommandBuffer& cmd);

	void Present();
	bool GetActualDimensions(uint32 &xOut, uint32 &yOut, bool bOrient);
	bool GetDisplayDimensions(uint32 &xOut, uint32 &yOut, bool bOrient);
	void ScreenShot(usg::GFXDevice* pDevice, const char* szFileName);
	const RenderPassHndl& GetRenderPass() { return m_directRenderPass; }

	void Resize(usg::GFXDevice* pDevice, uint32 uWidth, uint32 uHeight);
    void Resize(usg::GFXDevice* pDevice);	
	void SetVSyncMode(VSyncMode eVsync);
	void Minimized(usg::GFXDevice* pDevice);
	
	// PS
	void Transfer(GFXContext* pContext, RenderTarget* pTarget);
	void TransferRect(GFXContext* pContext, RenderTarget* pTarget, const GFXBounds& srcBounds, const GFXBounds& dstBounds);
	void SwapBuffers(GFXDevice* pDevice);
	// Signalled by this frame's acquire, waited on by its submission
	VkSemaphore& GetImageAcquired() { return m_imageAcquired[m_uAcquireSlot]; }
	// Signalled by the submission, waited on by the image's present
	VkSemaphore& GetRenderComplete() { return m_renderComplete[m_uActiveImage]; }
	VkImage GetActiveImage() const { return m_pSwapchainImages[m_uActiveImage]; }
	bool HasHDRSupport() const { return m_bHDR; }
	bool HasSrgbBlendTarget() const { return m_bSrgbBlendView; }
	const RenderPassHndl& GetSrgbBlendRenderPass() { return m_bSrgbBlendView ? m_srgbBlendRenderPass : m_directRenderPass; }
	void SetSrgbBlendTarget(bool bEnable) { m_bUseSrgbBlend = bEnable; }
	ColorCorrection GetRequiredColorCorrection() const { return m_eColorCorrect; }

private:
	PRIVATIZE_COPY(Display_ps)

	void InitFrameBuffers(GFXDevice* pDevice);
	void DestroySwapChain(GFXDevice* pDevice);
	void RecreateSwapChain(GFXDevice* pDevice);
	void CreateSwapChain(GFXDevice* pDevice);
	void CreateSwapChainImageViews(GFXDevice* pDevice);
	bool GetFallbackPresentMode(VkPresentModeKHR& eModeInOut);

	enum 
	{
		SWAP_BUFFER_COUNT = 2,
		MAX_SWAP_IMAGES = 8
	};

	usg::RenderPassHndl	m_directRenderPass;
	usg::RenderPassHndl	m_postCopyRenderPass;
	usg::RenderPassHndl	m_srgbBlendRenderPass;		// UNORM view, loads the transferred image
	VkFramebuffer*		m_pFramebuffersSrgbBlend;
	VkImageView*		m_pSwapchainBlendViews;		// UNORM views of the sRGB images
	VkFormat			m_blendViewFormat;
	ColorFormat			m_eBlendViewFormat;
	bool				m_bSrgbBlendView;
	bool				m_bUseSrgbBlend;
	VkFramebuffer*		m_pFramebuffers;
	VkFramebuffer*		m_pFramebuffersNoCopy;
	VSyncMode			m_eVsync;
	WindHndl			m_hwnd;
	VkImage*			m_pSwapchainImages;
	VkImageView*		m_pSwapchainImageViews;
	VkSurfaceKHR		m_surface;
	VkSwapchainKHR		m_swapChain;
	VkSemaphore			m_imageAcquired[GFX_NUM_DYN_BUFF];	// one per frame in flight
	VkSemaphore			m_renderComplete[MAX_SWAP_IMAGES];	// one per swap chain image
	uint32				m_uAcquireSlot;
	VkFormat			m_swapChainImageFormat;
	uint32				m_uSwapChainImageCount;
	uint32				m_uID;
	uint32				m_uWidth;
	uint32				m_uHeight;
	uint32				m_uActiveImage;
	ColorFormat			m_eSwapChainFormat;
	VkFormat			m_eVkSwapChainFormat;
	std::thread			m_screenshotThread;
	bool				m_bWindowResized;
	bool				m_bRTShouldLoad;
	bool				m_bHDR;
	ColorCorrection		m_eColorCorrect;
};

inline bool Display_ps::GetActualDimensions(uint32 & xOut, uint32 & yOut, bool bOrient)
{
	xOut = m_uWidth;
	yOut = m_uHeight;

	return true;
}

inline bool Display_ps::GetDisplayDimensions(uint32 & xOut, uint32 & yOut, bool bOrient)
{
	xOut = m_uWidth;
	yOut = m_uHeight;
	return true;
}

}

#endif