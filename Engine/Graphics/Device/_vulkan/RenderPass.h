/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _USG_GRAPHICS_DEVICE_PC_RENDERPASS_H_
#define _USG_GRAPHICS_DEVICE_PC_RENDERPASS_H_

#include "Engine/Graphics/Device/RenderState.h"
#include "Engine/Core/stl/vector.h"
#include OS_HEADER(Engine/Graphics/Device, VulkanIncludes.h)

namespace usg {

class RenderPass
{
public:
	RenderPass() {};
	~RenderPass();
	
	void Init(GFXDevice* pDevice, const class RenderPassInitData &decl, uint32 uId);
	void Cleanup(GFXDevice* pDevice);
	const VkRenderPass& GetPass() const { return m_renderPass; }
	uint32 GetCRC() const { return m_uCRCForPass; }

	// Attachment formats of the first subpass, for external renderers (e.g. middleware)
	// which need to build pipelines compatible with this pass
	uint32 GetColorAttachmentCount() const { return m_uColorAttachments; }
	VkFormat GetColorAttachmentFormat(uint32 uIndex) const { return uIndex < m_uColorAttachments ? m_colorFormats[uIndex] : VK_FORMAT_UNDEFINED; }
	VkFormat GetDepthAttachmentFormat() const { return m_depthFormat; }
	VkSampleCountFlagBits GetSampleCount() const { return m_sampleCount; }

private:

	
	VkRenderPass m_renderPass = VK_NULL_HANDLE;
	uint32		 m_uCRCForPass = 0;
	VkFormat	 m_colorFormats[MAX_COLOR_TARGETS] = {};
	uint32		 m_uColorAttachments = 0;
	VkFormat	 m_depthFormat = VK_FORMAT_UNDEFINED;
	VkSampleCountFlagBits m_sampleCount = VK_SAMPLE_COUNT_1_BIT;
};

}


#endif