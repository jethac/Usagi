/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Platform specific implementation of an instance of a sound,
//	a voice in the SDL backend's mixer
*****************************************************************************/
#ifndef __USG_AUDIO_SOUNDOBJECT_PS_H__
#define __USG_AUDIO_SOUNDOBJECT_PS_H__

#include "WaveFile.h"
#include "SDLMixer.h"

namespace usg{

	class Audio;
	class SoundObject;
	struct StreamingSoundDef;

class SoundObject_ps
{
public:
	SoundObject_ps();
	~SoundObject_ps();

	void Init(Audio* pAudio);
	void SetSoundFile(WaveFile* pWaveFile, bool bPositional, bool bLoop);
	void SetCustomData(const StreamingSoundDef& def);
	void Reset();
	void Update(const SoundObject* pParent);
	bool IsPlaying() const;
	bool IsPaused() const { return m_bPaused; }
	bool IsReady() const { return true; }
	void SetActiveTrack(uint32 uTrack, float fLerpTime) {}
	void SubmitData(void* pData, memsize size);
	uint64 GetSamplesPlayed() const;

private:
	PRIVATIZE_COPY(SoundObject_ps)

	void BindWaveFile();
	void Start();
	void Stop();
	void Pause();

	MixerVoice		m_voice;
	MixerFilter		m_baseFilter;
	WaveFile*		m_pSoundFile;
	bool			m_bLooping;
	bool			m_bPositional;
	bool			m_bPaused;
	bool			m_bValid;
	bool			m_bCustomData;
};

}

#endif
