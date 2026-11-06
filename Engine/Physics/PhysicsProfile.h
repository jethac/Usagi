/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Optional physics step timing (USAGI_PHYSICS_PROFILE=1).
//	Every 300 simulated frames a summary line (average, p50, p95 and max of
//	the time the main thread spends in the physics step, plus the frame
//	interval) is appended to _physics_profile.log in the working directory.
*****************************************************************************/
#pragma once

namespace usg
{
	namespace physics
	{
		namespace profile
		{
#ifndef FINAL_BUILD
			bool IsEnabled();

			// Bracket the main-thread work of one step. With an asynchronous
			// backend call Begin/End around the kick and again around the wait;
			// the times add up and the span from the first Begin to the last End
			// is recorded separately.
			void BeginStep();
			void EndStep();

			// Call once per simulated frame, after the last EndStep.
			void EndFrame(uint32 uBodyCount, uint32 uAwakeBodyCount, uint32 uContactCount);
#else
			inline bool IsEnabled() { return false; }
			inline void BeginStep() {}
			inline void EndStep() {}
			inline void EndFrame(uint32, uint32, uint32) {}
#endif
		}
	}
}
