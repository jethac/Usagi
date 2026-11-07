/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/Timer/FrameBudget.h"
#include "Engine/Debug/Rendering/DebugRender.h"
#include "FrameBudgetOverlay.h"
#include <stdio.h>

namespace usg {

namespace
{
	const float BAR_LEFT = 0.01f;
	const float BAR_WIDTH = 0.32f;	// The width of one 16.67 ms frame

	const Color g_stageColors[FrameBudget::CPU_STAGE_COUNT] =
	{
		Color(0.30f, 0.75f, 1.00f, 0.85f),	// update
		Color(0.25f, 1.00f, 0.45f, 0.85f),	// record
		Color(0.35f, 0.35f, 0.35f, 0.60f),	// gpu-wait
		Color(0.90f, 0.60f, 0.20f, 0.85f),	// present
		Color(0.90f, 0.30f, 0.90f, 0.85f),	// submit
	};

	const Color g_gpuColors[] =
	{
		Color(1.00f, 0.45f, 0.35f, 0.85f),
		Color(1.00f, 0.80f, 0.30f, 0.85f),
		Color(0.55f, 0.90f, 0.35f, 0.85f),
		Color(0.35f, 0.80f, 0.95f, 0.85f),
		Color(0.65f, 0.50f, 1.00f, 0.85f),
		Color(1.00f, 0.50f, 0.80f, 0.85f),
	};

	float MsToWidth(float fMs)
	{
		return BAR_WIDTH * fMs / FrameBudget::BUDGET_MS;
	}
}

void DrawFrameBudgetOverlay(DebugRender* pRender)
{
	if (!pRender || FrameBudget::GetLevel() < 2)
	{
		return;
	}

	char szText[128];
	const Color white(1.0f, 1.0f, 1.0f, 1.0f);
	const Color frameBack(0.0f, 0.0f, 0.0f, 0.55f);
	const Color budgetMark(1.0f, 0.2f, 0.2f, 0.9f);

	const float fFrameMs = FrameBudget::GetFrameMs();
	snprintf(szText, sizeof(szText), "FRAME %5.2f MS %3.0f%%  GPU %5.2f MS", fFrameMs, 100.0f * fFrameMs / FrameBudget::BUDGET_MS, FrameBudget::GetGPUMs());
	pRender->AddString(szText, BAR_LEFT, 1.0f, white);

	// CPU, stacked by stage
	float fLine = 2.0f;
	pRender->AddBar(fLine, BAR_LEFT, BAR_WIDTH, frameBack);
	float fX = BAR_LEFT;
	for (uint32 i = 0; i < FrameBudget::CPU_STAGE_COUNT; i++)
	{
		const float fWidth = MsToWidth(FrameBudget::GetCPUStageMs((FrameBudget::CPUStage)i));
		if (fWidth > 0.0005f)
		{
			pRender->AddBar(fLine, fX, fWidth, g_stageColors[i]);
			fX += fWidth;
		}
	}
	pRender->AddBar(fLine, BAR_LEFT + BAR_WIDTH, 0.002f, budgetMark);
	pRender->AddString("CPU", BAR_LEFT + BAR_WIDTH + 0.006f, fLine, white);

	// GPU, stacked by pass
	fLine += 1.0f;
	pRender->AddBar(fLine, BAR_LEFT, BAR_WIDTH, frameBack);
	fX = BAR_LEFT;
	uint32 uColor = 0;
	const uint32 uScopes = FrameBudget::GetGPUScopeCount();
	for (uint32 i = 0; i < uScopes; i++)
	{
		const char* szName; uint32 uDepth; float fMs;
		FrameBudget::GetGPUScope(i, szName, uDepth, fMs);
		const float fWidth = MsToWidth(fMs);
		if (uDepth == 0 && fMs >= 0.02f)
		{
			pRender->AddBar(fLine, fX, fWidth, g_gpuColors[uColor++ % ARRAY_SIZE(g_gpuColors)]);
			fX += fWidth;
		}
	}
	pRender->AddBar(fLine, BAR_LEFT + BAR_WIDTH, 0.002f, budgetMark);
	pRender->AddString("GPU", BAR_LEFT + BAR_WIDTH + 0.006f, fLine, white);

	// CPU stage legend
	fLine += 1.0f;
	for (uint32 i = 0; i < FrameBudget::CPU_STAGE_COUNT; i++)
	{
		const float fMs = FrameBudget::GetCPUStageMs((FrameBudget::CPUStage)i);
		snprintf(szText, sizeof(szText), "%-9s %5.2f %3.0f%%", FrameBudget::GetCPUStageName((FrameBudget::CPUStage)i), fMs, 100.0f * fMs / FrameBudget::BUDGET_MS);
		pRender->AddString(szText, BAR_LEFT, fLine, g_stageColors[i]);
		fLine += 1.0f;
	}

	// GPU passes, with their sub-passes indented
	uColor = 0;
	for (uint32 i = 0; i < uScopes; i++)
	{
		const char* szName; uint32 uDepth; float fMs;
		FrameBudget::GetGPUScope(i, szName, uDepth, fMs);
		if (fMs < 0.02f)
		{
			continue;
		}
		snprintf(szText, sizeof(szText), "%s%-18s %5.2f %3.0f%%", uDepth == 0 ? "" : "  ", szName, fMs, 100.0f * fMs / FrameBudget::BUDGET_MS);
		pRender->AddString(szText, BAR_LEFT, fLine, uDepth == 0 ? g_gpuColors[uColor++ % ARRAY_SIZE(g_gpuColors)] : white);
		fLine += 1.0f;
	}
}

}
