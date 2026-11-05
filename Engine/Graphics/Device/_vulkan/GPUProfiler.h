/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: GPU pass timings from Vulkan timestamp queries. Every
//	BeginGPUTag/EndGPUTag scope on a context is timed; per-scope averages are
//	written periodically to a text file. Enabled with the USAGI_GPU_PROFILE
//	environment variable (value = output file, or "1" for gpu_profile.txt).
*****************************************************************************/
#ifndef _USG_GRAPHICS_VULKAN_GPUPROFILER_H_
#define _USG_GRAPHICS_VULKAN_GPUPROFILER_H_

#include OS_HEADER(Engine/Graphics/Device, VulkanIncludes.h)

namespace usg {

class GPUProfiler
{
public:
	GPUProfiler();
	~GPUProfiler();

	void Init(VkDevice device, float fTimestampPeriodNs);
	void Cleanup(VkDevice device);

	bool IsEnabled() const { return m_pool != VK_NULL_HANDLE; }

	// Must be called outside of a render pass
	void BeginFrame(VkCommandBuffer cmd);
	void EndFrame(VkCommandBuffer cmd);
	void Push(VkCommandBuffer cmd, const char* szName);
	void Pop(VkCommandBuffer cmd);

private:
	enum
	{
		MAX_QUERIES = 1024,
		MAX_SCOPES = MAX_QUERIES / 2,
		MAX_STACK = 16,
		MAX_STATS = 128,
		MAX_NAME = 48,
		REPORT_FRAMES = 300
	};

	struct Scope
	{
		char		name[MAX_NAME];	// Copied, the tag string may not outlive the frame
		uint32		uStartQuery;
		uint32		uEndQuery;
		uint32		uDepth;
	};

	struct Stat
	{
		char		name[MAX_NAME];
		uint32		uDepth;
		uint32		uOrder;
		double		fTotalMs;
		double		fFrameMs;
		double		fMaxMs;
	};

	void ReadBack();
	Stat* FindStat(const char* szName, uint32 uDepth);
	void WriteReport();

	VkDevice		m_device;
	VkQueryPool		m_pool;
	float			m_fPeriodNs;
	Scope			m_scopes[MAX_SCOPES];
	uint32			m_uScopeCount;
	uint32			m_uQueryCount;
	uint32			m_stack[MAX_STACK];
	uint32			m_uStackDepth;
	bool			m_bFrameOpen;
	bool			m_bPending;
	Stat			m_stats[MAX_STATS];
	uint32			m_uStatCount;
	uint32			m_uFrames;
	double			m_fTotalFrameMs;
	double			m_fMaxFrameMs;
	char			m_outFile[256];
};

}

#endif
