/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Filters, effects and rooms for the SDL backend. Filters are
//	applied per voice by the mixer; reverb effects and rooms are accepted
//	but not processed yet (the sound plays dry).
*****************************************************************************/
#ifndef __USG_AUDIO_SDL_AUDIO_EFFECTS_PS_H__
#define __USG_AUDIO_SDL_AUDIO_EFFECTS_PS_H__

#include "Engine/Audio/AudioFilter.h"
#include "Engine/Audio/AudioEffect.h"
#include "Engine/Audio/AudioRoom.h"
#include "SDLMixer.h"

namespace usg{

class AudioFilter_ps : public AudioFilter
{
public:
	virtual void Init(const AudioFilterDef* pDef, Audio* pAudio) override
	{
		AudioFilter::InitInt(pDef);
		m_filter.eType = pDef->eFilter;
		m_filter.fFrequency = pDef->fFrequency;
		m_filter.fOneOverQ = pDef->fOneOverQ;
	}

	const MixerFilter& GetFilter() const { return m_filter; }

private:
	MixerFilter	m_filter;
};

class AudioEffect_ps : public AudioEffect
{
public:
	virtual void Init(const AudioEffectDef* pDef, Audio* pAudio) override { InitInt(pDef); }
};

class AudioRoom_ps : public AudioRoom
{
public:
	virtual void Init(const AudioRoomDef* pDef, Audio* pAudio) override { m_uCRC = pDef->crc; }
};

}

#endif
