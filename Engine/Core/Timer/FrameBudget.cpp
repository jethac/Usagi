/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/OS.h"
#include "Engine/Core/String/String_Util.h"
#include "Engine/Maths/MathUtil.h"
#include OS_HEADER(Engine/Core/Timer, TimeTracker.h)
#include "FrameBudget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace usg {

const float FrameBudget::BUDGET_MS = 1000.0f / 60.0f;

namespace
{
	const float LOG_INTERVAL_MS = 5000.0f;
	const float SMOOTHING = 0.05f;		// Per frame weight of the overlay's moving average

	struct GPUScope
	{
		char	name[FrameBudget::MAX_SCOPE_NAME];
		uint32	uDepth;
		float	fFrameMs;		// Latest frame with timestamps
		float	fSmoothMs;		// Moving average, for the overlay
		double	fIntervalMs;	// Sum over the log interval
	};

	struct BudgetState
	{
		int			iLevel = -1;
		float		cpuFrame[FrameBudget::CPU_STAGE_COUNT] = {};
		float		cpuSmooth[FrameBudget::CPU_STAGE_COUNT] = {};
		double		cpuInterval[FrameBudget::CPU_STAGE_COUNT] = {};
		float		fDrawMs = 0.0f;
		float		fGPUFrameMs = 0.0f;
		float		fGPUSmoothMs = 0.0f;
		double		fGPUIntervalMs = 0.0;
		float		fGPUIntervalMaxMs = 0.0f;
		float		fFrameSmoothMs = 0.0f;
		double		fFrameIntervalMs = 0.0;
		float		fFrameIntervalMaxMs = 0.0f;
		uint32		uIntervalFrames = 0;
		uint32		uIntervalOver = 0;
		GPUScope	gpu[FrameBudget::MAX_GPU_SCOPES];
		uint32		uGPUScopes = 0;
		bool		bFirst = true;
	};

	BudgetState g_budget;

	const char* g_szStageNames[FrameBudget::CPU_STAGE_COUNT] =
	{
		"update",
		"record",
		"gpu-wait",
		"present",
		"submit"
	};

	void Smooth(float& fSmooth, float fValue, bool bFirst)
	{
		fSmooth = bFirst ? fValue : fSmooth + (fValue - fSmooth) * SMOOTHING;
	}
}

uint32 FrameBudget::GetLevel()
{
	if (g_budget.iLevel < 0)
	{
		char szEnv[8] = {};
		g_budget.iLevel = 0;
		if (OS::GetEnvVar("USAGI_BUDGET", szEnv, sizeof(szEnv)) && szEnv[0] != '\0' && szEnv[0] != '0')
		{
			g_budget.iLevel = Math::Clamp(atoi(szEnv), 1, 2);
		}
	}
	return (uint32)g_budget.iLevel;
}

void FrameBudget::AddCPUTime(CPUStage eStage, float fMs)
{
	g_budget.cpuFrame[eStage] += fMs;
}

void FrameBudget::SetDrawTime(float fMs)
{
	g_budget.fDrawMs = fMs;
}

void FrameBudget::SetGPUFrame(float fTotalMs)
{
	g_budget.fGPUFrameMs = fTotalMs;
	for (uint32 i = 0; i < g_budget.uGPUScopes; i++)
	{
		g_budget.gpu[i].fFrameMs = 0.0f;
	}
}

void FrameBudget::SetGPUScope(const char* szName, uint32 uDepth, float fMs)
{
	for (uint32 i = 0; i < g_budget.uGPUScopes; i++)
	{
		GPUScope& scope = g_budget.gpu[i];
		if (scope.uDepth == uDepth && strncmp(scope.name, szName, MAX_SCOPE_NAME - 1) == 0)
		{
			scope.fFrameMs += fMs;
			return;
		}
	}
	if (g_budget.uGPUScopes < MAX_GPU_SCOPES)
	{
		GPUScope& scope = g_budget.gpu[g_budget.uGPUScopes++];
		str::Copy(scope.name, szName, sizeof(scope.name));
		scope.uDepth = uDepth;
		scope.fFrameMs = fMs;
		scope.fSmoothMs = fMs;
		scope.fIntervalMs = 0.0;
	}
}

float FrameBudget::TicksToMs(uint64 uStart, uint64 uEnd)
{
	return (float)((double)(uEnd - uStart) * 1000.0 / (double)TimeTracker::GetTickFrequency());
}

void FrameBudget::EndFrame(float fFrameMs)
{
	if (!IsEnabled())
	{
		return;
	}

	BudgetState& b = g_budget;

	// Recording is what's left of the draw once the waits are taken out
	float fDrawOther = b.cpuFrame[CPU_GPU_WAIT] + b.cpuFrame[CPU_PRESENT] + b.cpuFrame[CPU_SUBMIT];
	b.cpuFrame[CPU_RECORD] += Math::Max(0.0f, b.fDrawMs - fDrawOther);

	for (uint32 i = 0; i < CPU_STAGE_COUNT; i++)
	{
		Smooth(b.cpuSmooth[i], b.cpuFrame[i], b.bFirst);
		b.cpuInterval[i] += b.cpuFrame[i];
		b.cpuFrame[i] = 0.0f;
	}
	b.fDrawMs = 0.0f;

	Smooth(b.fGPUSmoothMs, b.fGPUFrameMs, b.bFirst);
	b.fGPUIntervalMs += b.fGPUFrameMs;
	b.fGPUIntervalMaxMs = Math::Max(b.fGPUIntervalMaxMs, b.fGPUFrameMs);
	for (uint32 i = 0; i < b.uGPUScopes; i++)
	{
		Smooth(b.gpu[i].fSmoothMs, b.gpu[i].fFrameMs, false);
		b.gpu[i].fIntervalMs += b.gpu[i].fFrameMs;
	}

	Smooth(b.fFrameSmoothMs, fFrameMs, b.bFirst);
	b.fFrameIntervalMs += fFrameMs;
	b.fFrameIntervalMaxMs = Math::Max(b.fFrameIntervalMaxMs, fFrameMs);
	b.uIntervalOver += fFrameMs > BUDGET_MS ? 1 : 0;
	b.uIntervalFrames++;
	b.bFirst = false;

	if (b.fFrameIntervalMs < LOG_INTERVAL_MS)
	{
		return;
	}

	const double fInv = 1.0 / (double)b.uIntervalFrames;
	const float fFrame = (float)(b.fFrameIntervalMs * fInv);
	const float fGPU = (float)(b.fGPUIntervalMs * fInv);
	float fCPUBusy = 0.0f;
	for (uint32 i = 0; i < CPU_STAGE_COUNT; i++)
	{
		if (i != CPU_GPU_WAIT)
		{
			fCPUBusy += (float)(b.cpuInterval[i] * fInv);
		}
	}

	char szLine[1024];
	int iLen = snprintf(szLine, sizeof(szLine), "Budget: frame %.2f ms (%.0f%% of 16.67, worst %.2f, %u over) | CPU busy %.2f ms (%.0f%%):",
		fFrame, 100.0f * fFrame / BUDGET_MS, b.fFrameIntervalMaxMs, b.uIntervalOver, fCPUBusy, 100.0f * fCPUBusy / BUDGET_MS);
	for (uint32 i = 0; i < CPU_STAGE_COUNT && iLen > 0 && iLen < (int)sizeof(szLine); i++)
	{
		iLen += snprintf(szLine + iLen, sizeof(szLine) - iLen, " %s %.2f", g_szStageNames[i], (float)(b.cpuInterval[i] * fInv));
	}
	if (iLen > 0 && iLen < (int)sizeof(szLine))
	{
		iLen += snprintf(szLine + iLen, sizeof(szLine) - iLen, " | GPU %.2f ms (%.0f%%, worst %.2f):", fGPU, 100.0f * fGPU / BUDGET_MS, b.fGPUIntervalMaxMs);
	}
	for (uint32 i = 0; i < b.uGPUScopes && iLen > 0 && iLen < (int)sizeof(szLine); i++)
	{
		const float fMs = (float)(b.gpu[i].fIntervalMs * fInv);
		if (fMs >= 0.02f)
		{
			iLen += snprintf(szLine + iLen, sizeof(szLine) - iLen, b.gpu[i].uDepth == 0 ? ", %s %.2f" : " [%s %.2f]", b.gpu[i].name, fMs);
		}
	}
	RELEASE_WARNING("%s\n", szLine);

	for (uint32 i = 0; i < CPU_STAGE_COUNT; i++)
	{
		b.cpuInterval[i] = 0.0;
	}
	for (uint32 i = 0; i < b.uGPUScopes; i++)
	{
		b.gpu[i].fIntervalMs = 0.0;
	}
	b.fGPUIntervalMs = 0.0;
	b.fGPUIntervalMaxMs = 0.0f;
	b.fFrameIntervalMs = 0.0;
	b.fFrameIntervalMaxMs = 0.0f;
	b.uIntervalFrames = 0;
	b.uIntervalOver = 0;
}

float FrameBudget::GetFrameMs()
{
	return g_budget.fFrameSmoothMs;
}

float FrameBudget::GetCPUStageMs(CPUStage eStage)
{
	return g_budget.cpuSmooth[eStage];
}

const char* FrameBudget::GetCPUStageName(CPUStage eStage)
{
	return g_szStageNames[eStage];
}

float FrameBudget::GetGPUMs()
{
	return g_budget.fGPUSmoothMs;
}

uint32 FrameBudget::GetGPUScopeCount()
{
	return g_budget.uGPUScopes;
}

void FrameBudget::GetGPUScope(uint32 uIndex, const char*& szNameOut, uint32& uDepthOut, float& fMsOut)
{
	const GPUScope& scope = g_budget.gpu[uIndex];
	szNameOut = scope.name;
	uDepthOut = scope.uDepth;
	fMsOut = scope.fSmoothMs;
}

}
