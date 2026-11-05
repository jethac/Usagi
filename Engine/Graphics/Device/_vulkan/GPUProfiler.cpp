/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/String/String_Util.h"
#include "GPUProfiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace usg {

GPUProfiler::GPUProfiler()
	: m_device(VK_NULL_HANDLE)
	, m_pool(VK_NULL_HANDLE)
	, m_fPeriodNs(1.0f)
	, m_uScopeCount(0)
	, m_uQueryCount(0)
	, m_uStackDepth(0)
	, m_bFrameOpen(false)
	, m_bPending(false)
	, m_uStatCount(0)
	, m_uFrames(0)
	, m_fTotalFrameMs(0.0)
	, m_fMaxFrameMs(0.0)
{
	m_outFile[0] = '\0';
}

GPUProfiler::~GPUProfiler()
{
	ASSERT(m_pool == VK_NULL_HANDLE);
}

void GPUProfiler::Init(VkDevice device, float fTimestampPeriodNs)
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
	const char* szEnv = getenv("USAGI_GPU_PROFILE");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
	if (!szEnv || szEnv[0] == '\0' || strcmp(szEnv, "0") == 0)
	{
		return;
	}
	str::Copy(m_outFile, strcmp(szEnv, "1") == 0 ? "gpu_profile.txt" : szEnv, sizeof(m_outFile));

	VkQueryPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = MAX_QUERIES;
	if (vkCreateQueryPool(device, &info, nullptr, &m_pool) != VK_SUCCESS)
	{
		m_pool = VK_NULL_HANDLE;
		return;
	}
	m_device = device;
	m_fPeriodNs = fTimestampPeriodNs;
}

void GPUProfiler::Cleanup(VkDevice device)
{
	if (m_pool != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(device, m_pool, nullptr);
		m_pool = VK_NULL_HANDLE;
	}
}

void GPUProfiler::BeginFrame(VkCommandBuffer cmd)
{
	if (!IsEnabled())
		return;

	// The device waits on the previous frame's fence before recording again,
	// so its queries have landed.
	if (m_bPending)
	{
		ReadBack();
	}

	vkCmdResetQueryPool(cmd, m_pool, 0, MAX_QUERIES);
	m_uScopeCount = 0;
	m_uQueryCount = 0;
	m_uStackDepth = 0;
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_pool, m_uQueryCount++);
	m_bFrameOpen = true;
}

void GPUProfiler::EndFrame(VkCommandBuffer cmd)
{
	if (!IsEnabled() || !m_bFrameOpen)
		return;

	while (m_uStackDepth > 0)
	{
		Pop(cmd);
	}
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_pool, m_uQueryCount++);
	m_bFrameOpen = false;
	m_bPending = true;
}

void GPUProfiler::Push(VkCommandBuffer cmd, const char* szName)
{
	if (!IsEnabled() || !m_bFrameOpen)
		return;

	if (m_uQueryCount + 3 > MAX_QUERIES || m_uScopeCount >= MAX_SCOPES || m_uStackDepth >= MAX_STACK)
	{
		// Keep the stack balanced even when we run out of queries
		if (m_uStackDepth < MAX_STACK)
		{
			m_stack[m_uStackDepth++] = USG_INVALID_ID;
		}
		return;
	}

	Scope& scope = m_scopes[m_uScopeCount];
	str::Copy(scope.name, szName ? szName : "?", sizeof(scope.name));
	scope.uDepth = m_uStackDepth;
	scope.uStartQuery = m_uQueryCount++;
	scope.uEndQuery = USG_INVALID_ID;
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_pool, scope.uStartQuery);
	m_stack[m_uStackDepth++] = m_uScopeCount++;
}

void GPUProfiler::Pop(VkCommandBuffer cmd)
{
	if (!IsEnabled() || !m_bFrameOpen || m_uStackDepth == 0)
		return;

	uint32 uScope = m_stack[--m_uStackDepth];
	if (uScope == USG_INVALID_ID)
		return;

	Scope& scope = m_scopes[uScope];
	scope.uEndQuery = m_uQueryCount++;
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_pool, scope.uEndQuery);
}

GPUProfiler::Stat* GPUProfiler::FindStat(const char* szName, uint32 uDepth)
{
	for (uint32 i = 0; i < m_uStatCount; i++)
	{
		if (m_stats[i].uDepth == uDepth && strcmp(m_stats[i].name, szName) == 0)
		{
			return &m_stats[i];
		}
	}
	if (m_uStatCount >= MAX_STATS)
	{
		return nullptr;
	}
	Stat& stat = m_stats[m_uStatCount];
	str::Copy(stat.name, szName, sizeof(stat.name));
	stat.uDepth = uDepth;
	stat.uOrder = m_uStatCount;
	stat.fTotalMs = 0.0;
	stat.fFrameMs = 0.0;
	stat.fMaxMs = 0.0;
	m_uStatCount++;
	return &stat;
}

void GPUProfiler::ReadBack()
{
	m_bPending = false;

	uint64 results[MAX_QUERIES];
	VkResult res = vkGetQueryPoolResults(m_device, m_pool, 0, m_uQueryCount, sizeof(uint64) * m_uQueryCount, results, sizeof(uint64),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	if (res != VK_SUCCESS)
	{
		return;
	}

	const double fToMs = (double)m_fPeriodNs * 1e-6;

	for (uint32 i = 0; i < m_uStatCount; i++)
	{
		m_stats[i].fFrameMs = 0.0;
	}

	for (uint32 i = 0; i < m_uScopeCount; i++)
	{
		const Scope& scope = m_scopes[i];
		if (scope.uEndQuery == USG_INVALID_ID)
			continue;
		Stat* pStat = FindStat(scope.name, scope.uDepth);
		if (pStat && results[scope.uEndQuery] >= results[scope.uStartQuery])
		{
			pStat->fFrameMs += (double)(results[scope.uEndQuery] - results[scope.uStartQuery]) * fToMs;
		}
	}

	for (uint32 i = 0; i < m_uStatCount; i++)
	{
		m_stats[i].fTotalMs += m_stats[i].fFrameMs;
		m_stats[i].fMaxMs = Math::Max(m_stats[i].fMaxMs, m_stats[i].fFrameMs);
	}

	const double fFrameMs = (double)(results[m_uQueryCount - 1] - results[0]) * fToMs;
	m_fTotalFrameMs += fFrameMs;
	m_fMaxFrameMs = Math::Max(m_fMaxFrameMs, fFrameMs);
	m_uFrames++;

	if (m_uFrames >= REPORT_FRAMES)
	{
		WriteReport();
		for (uint32 i = 0; i < m_uStatCount; i++)
		{
			m_stats[i].fTotalMs = 0.0;
			m_stats[i].fMaxMs = 0.0;
		}
		m_uFrames = 0;
		m_fTotalFrameMs = 0.0;
		m_fMaxFrameMs = 0.0;
	}
}

void GPUProfiler::WriteReport()
{
	FILE* pFile = nullptr;
#ifdef _MSC_VER
	fopen_s(&pFile, m_outFile, "w");
#else
	pFile = fopen(m_outFile, "w");
#endif
	if (!pFile)
		return;

	const double fInvFrames = 1.0 / (double)m_uFrames;
	fprintf(pFile, "GPU frame (command buffer) avg %.3f ms, max %.3f ms over %u frames\n", m_fTotalFrameMs * fInvFrames, m_fMaxFrameMs, m_uFrames);
	fprintf(pFile, "%-48s %9s %9s\n", "scope", "avg ms", "max ms");
	for (uint32 i = 0; i < m_uStatCount; i++)
	{
		const Stat& stat = m_stats[i];
		char label[MAX_NAME + MAX_STACK * 2 + 1];
		uint32 uIndent = Math::Min((uint32)MAX_STACK, stat.uDepth) * 2;
		memset(label, ' ', uIndent);
		str::Copy(label + uIndent, stat.name, sizeof(label) - uIndent);
		fprintf(pFile, "%-48s %9.3f %9.3f\n", label, stat.fTotalMs * fInvFrames, stat.fMaxMs);
	}
	fclose(pFile);
}

}
