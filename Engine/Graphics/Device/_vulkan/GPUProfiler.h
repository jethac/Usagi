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
#include "Engine/Graphics/RenderConsts.h"

namespace usg {

class GPUProfiler
{
public:
	GPUProfiler();
	~GPUProfiler();

	void Init(VkDevice device, float fTimestampPeriodNs);
	void Cleanup(VkDevice device);

	bool IsEnabled() const { return m_pool != VK_NULL_HANDLE; }

	// Must be called outside of a render pass. uSlot is the frame-in-flight
	// slot: a slot's queries are read back when it is next begun, after the
	// device has waited on that slot's fence.
	void BeginFrame(VkCommandBuffer cmd, uint32 uSlot);
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

	void ReadBack(uint32 uSlot);
	Stat* FindStat(const char* szName, uint32 uDepth);
	void WriteReport();

	VkDevice		m_device;
	VkQueryPool		m_pool;
	float			m_fPeriodNs;
	// The queries of each frame in flight, in their own range of the pool
	struct FrameQueries
	{
		Scope		scopes[MAX_SCOPES];
		uint32		uScopeCount;
		uint32		uQueryCount;
		bool		bPending;
	};

	uint32 QueryIndex(uint32 uLocal) const { return m_uSlot * MAX_QUERIES + uLocal; }

	FrameQueries	m_frames[GFX_NUM_DYN_BUFF];
	uint32			m_uSlot;
	uint32			m_stack[MAX_STACK];
	uint32			m_uStackDepth;
	bool			m_bFrameOpen;
	Stat			m_stats[MAX_STATS];
	uint32			m_uStatCount;
	uint32			m_uFrames;
	double			m_fTotalFrameMs;
	double			m_fMaxFrameMs;
	char			m_outFile[256];
};

}

#endif
