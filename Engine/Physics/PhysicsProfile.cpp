/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Optional physics step timing (USAGI_PHYSICS_PROFILE=1)
*****************************************************************************/
#include "Engine/Common/Common.h"
#include OS_HEADER(Engine/Core/Timer, TimeTracker.h)
#include "Engine/Core/OS.h"
#include "Engine/Physics/PhysicsProfile.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef FINAL_BUILD

#ifdef _MSC_VER
#pragma warning(disable : 4996)	// fopen is fine for a dev log
#endif

namespace usg
{
	namespace physics
	{
		namespace profile
		{
			static constexpr uint32 WindowSize = 300;

			static struct
			{
				sint32 iEnabled = -1;
				uint64 uStepStart = 0;
				uint64 uSpanStart = 0;
				uint64 uSpanEnd = 0;
				uint64 uFrameAccum = 0;
				uint64 uLastFrame = 0;
				uint32 uCount = 0;
				uint32 uWindow = 0;
				float fStep[WindowSize];
				float fSpan[WindowSize];
				float fFrame[WindowSize];
				uint32 uMaxBodies = 0;
				uint32 uMaxAwake = 0;
				uint32 uMaxContacts = 0;
			} s_profile;

			static float TicksToMs(uint64 uTicks)
			{
				return (float)((double)uTicks * 1000.0 / (double)TimeTracker::GetTickFrequency());
			}

			bool IsEnabled()
			{
				if (s_profile.iEnabled < 0)
				{
					char szEnv[8] = {};
					s_profile.iEnabled = (OS::GetEnvVar("USAGI_PHYSICS_PROFILE", szEnv, sizeof(szEnv)) && szEnv[0] == '1') ? 1 : 0;
				}
				return s_profile.iEnabled == 1;
			}

			void BeginStep()
			{
				if (!IsEnabled())
				{
					return;
				}
				s_profile.uStepStart = TimeTracker::GetSystemTime();
				if (s_profile.uSpanStart == 0)
				{
					s_profile.uSpanStart = s_profile.uStepStart;
				}
			}

			void EndStep()
			{
				if (!IsEnabled() || s_profile.uStepStart == 0)
				{
					return;
				}
				s_profile.uSpanEnd = TimeTracker::GetSystemTime();
				s_profile.uFrameAccum += s_profile.uSpanEnd - s_profile.uStepStart;
				s_profile.uStepStart = 0;
			}

			static int CompareFloat(const void* a, const void* b)
			{
				const float fA = *(const float*)a;
				const float fB = *(const float*)b;
				return (fA > fB) - (fA < fB);
			}

			static void Summarise(float* pValues, uint32 uCount, float& fAvg, float& fP50, float& fP95, float& fMax)
			{
				qsort(pValues, uCount, sizeof(float), CompareFloat);
				double fSum = 0.0;
				for (uint32 i = 0; i < uCount; i++)
				{
					fSum += pValues[i];
				}
				fAvg = (float)(fSum / (double)uCount);
				fP50 = pValues[uCount / 2];
				fP95 = pValues[Math::Min(uCount - 1, (uCount * 95) / 100)];
				fMax = pValues[uCount - 1];
			}

			void EndFrame(uint32 uBodyCount, uint32 uAwakeBodyCount, uint32 uContactCount)
			{
				if (!IsEnabled() || s_profile.uSpanStart == 0)
				{
					return;
				}

				const uint64 uNow = TimeTracker::GetSystemTime();
				const uint32 i = s_profile.uCount;
				s_profile.fStep[i] = TicksToMs(s_profile.uFrameAccum);
				s_profile.fSpan[i] = TicksToMs(s_profile.uSpanEnd - s_profile.uSpanStart);
				s_profile.fFrame[i] = s_profile.uLastFrame != 0 ? TicksToMs(uNow - s_profile.uLastFrame) : 0.0f;
				s_profile.uLastFrame = uNow;
				s_profile.uFrameAccum = 0;
				s_profile.uSpanStart = 0;
				s_profile.uMaxBodies = Math::Max(s_profile.uMaxBodies, uBodyCount);
				s_profile.uMaxAwake = Math::Max(s_profile.uMaxAwake, uAwakeBodyCount);
				s_profile.uMaxContacts = Math::Max(s_profile.uMaxContacts, uContactCount);

				if (++s_profile.uCount < WindowSize)
				{
					return;
				}

				float fStepAvg, fStepP50, fStepP95, fStepMax;
				float fSpanAvg, fSpanP50, fSpanP95, fSpanMax;
				float fFrameAvg, fFrameP50, fFrameP95, fFrameMax;
				Summarise(s_profile.fStep, WindowSize, fStepAvg, fStepP50, fStepP95, fStepMax);
				Summarise(s_profile.fSpan, WindowSize, fSpanAvg, fSpanP50, fSpanP95, fSpanMax);
				Summarise(s_profile.fFrame, WindowSize, fFrameAvg, fFrameP50, fFrameP95, fFrameMax);

				if (FILE* f = fopen("_physics_profile.log", "a"))
				{
					fprintf(f, "window=%u frames=%u step_ms avg=%.4f p50=%.4f p95=%.4f max=%.4f | span_ms avg=%.4f p95=%.4f max=%.4f | frame_ms avg=%.3f p50=%.3f p95=%.3f max=%.3f | bodies=%u awake=%u contacts=%u\n",
						s_profile.uWindow, WindowSize, fStepAvg, fStepP50, fStepP95, fStepMax, fSpanAvg, fSpanP95, fSpanMax,
						fFrameAvg, fFrameP50, fFrameP95, fFrameMax, s_profile.uMaxBodies, s_profile.uMaxAwake, s_profile.uMaxContacts);
					fclose(f);
				}
				s_profile.uWindow++;
				s_profile.uCount = 0;
				s_profile.uMaxBodies = 0;
				s_profile.uMaxAwake = 0;
				s_profile.uMaxContacts = 0;
			}
		}
	}
}

#endif
