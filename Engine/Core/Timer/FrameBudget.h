/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Frame budget meter (opt-in via USAGI_BUDGET). Collects where
//	a frame's time goes on the CPU (update, command recording, waiting for the
//	GPU, presenting, submitting) and on the GPU (per pass, from the GPU
//	profiler's timestamps), as shares of a 60 Hz frame. USAGI_BUDGET=1 logs a
//	line every few seconds; USAGI_BUDGET=2 also draws an overlay.
*****************************************************************************/
#pragma once

#ifndef USG_CORE_TIMER_FRAME_BUDGET_H
#define USG_CORE_TIMER_FRAME_BUDGET_H

namespace usg {

class FrameBudget
{
public:
	enum CPUStage
	{
		CPU_UPDATE = 0,		// Game update (simulation, systems)
		CPU_RECORD,			// Draw minus the stages below: culling, sorting, command recording
		CPU_GPU_WAIT,		// Blocked on a frame fence until the GPU caught up
		CPU_PRESENT,		// Present and swap chain image acquire
		CPU_SUBMIT,			// Queue submission and end of frame bookkeeping
		CPU_STAGE_COUNT
	};

	enum
	{
		MAX_GPU_SCOPES = 24,
		MAX_SCOPE_NAME = 24
	};

	static const float BUDGET_MS;	// 1000 / 60

	// The level from USAGI_BUDGET: 0 off, 1 log, 2 log and overlay
	static uint32 GetLevel();
	static bool IsEnabled() { return GetLevel() > 0; }

	// Accumulated into the current frame; call from anywhere on the main thread
	static void AddCPUTime(CPUStage eStage, float fMs);
	// Draw is measured as a whole; CPU_RECORD is what's left of it after the
	// other draw-time stages
	static void SetDrawTime(float fMs);
	// From the GPU profiler once a frame's timestamps have landed: the
	// command buffer's total and its scopes (passes at depth 0, their
	// sub-passes at depth 1)
	static void SetGPUFrame(float fTotalMs);
	static void SetGPUScope(const char* szName, uint32 uDepth, float fMs);

	// Closes the frame: smooths the values, logs every few seconds
	static void EndFrame(float fFrameMs);

	// Smoothed values, for the overlay (see Engine/Debug/FrameBudgetOverlay.h)
	static float GetFrameMs();
	static float GetCPUStageMs(CPUStage eStage);
	static const char* GetCPUStageName(CPUStage eStage);
	static float GetGPUMs();
	static uint32 GetGPUScopeCount();
	static void GetGPUScope(uint32 uIndex, const char*& szNameOut, uint32& uDepthOut, float& fMsOut);

	// Milliseconds between two TimeTracker::GetSystemTime values
	static float TicksToMs(uint64 uStart, uint64 uEnd);

	// Named CPU details inside the stages (culling, signal traversal, ...),
	// logged in brackets after the stages. Nested details may overlap.
	enum { MAX_DETAILS = 16 };
	static uint32 RegisterDetail(const char* szName);
	static void AddDetail(uint32 uDetail, float fMs);

	// Times its scope into a detail when the meter is on:
	//   static const uint32 s_uCull = FrameBudget::RegisterDetail("cull");
	//   FrameBudget::Scope scope(s_uCull);
	class Scope
	{
	public:
		explicit Scope(uint32 uDetail);
		~Scope();
	private:
		uint64	m_uStart;
		uint32	m_uDetail;
	};
};

}

#endif
