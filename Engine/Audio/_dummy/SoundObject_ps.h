/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Handle to an instance of a sound
*****************************************************************************/
#ifndef __USG_AUDIO_SOUNDHANDLE_PS_H__
#define __USG_AUDIO_SOUNDHANDLE_PS_H__
#include "Engine/Common/Common.h"

namespace usg{

	class Audio;
	class SoundObject;
	struct StreamingSoundDef;

class SoundObject_ps
{
public:
	SoundObject_ps() {}
	~SoundObject_ps() {}

	void Init(Audio* pAudio) {}
	void SetCustomData(const StreamingSoundDef& def) {}
	void Reset() {}
	void Start() {}
	void Stop() {}
	void Pause() {}
	void Update(const SoundObject* pParent) {}
	bool IsPlaying() const { return false;  }
	bool IsPaused() const { return false; }
	bool IsReady() const { return true; }
	void SetActiveTrack(uint32 uTrack, float fLerpTime) {}
	void SubmitData(void* pData, memsize size) {}
	uint64 GetSamplesPlayed() const { return 0; }

private:

};

}

#endif
