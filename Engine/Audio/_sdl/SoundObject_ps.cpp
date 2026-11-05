/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Audio/SoundCallbacks.h"
#include "AudioEffects_ps.h"
#include "SoundObject_ps.h"

namespace usg{

	// Which panning matrix entry feeds each output channel, per channel
	// configuration (the mixer outputs stereo)
	static const uint32 g_suChannelMapping[][MixerVoice::MAX_OUTPUT_CHANNELS] =
	{
		{ SOUND_CHANNEL_FRONT_LEFT, SOUND_CHANNEL_FRONT_RIGHT },	// CHANNEL_CONFIG_2_0
		{ SOUND_CHANNEL_FRONT_LEFT, SOUND_CHANNEL_FRONT_RIGHT },	// CHANNEL_CONFIG_HEADPHONES
		{ SOUND_CHANNEL_FRONT_LEFT, SOUND_CHANNEL_FRONT_RIGHT },	// CHANNEL_CONFIG_2_1
		{ SOUND_CHANNEL_FRONT_LEFT, SOUND_CHANNEL_FRONT_RIGHT },	// CHANNEL_CONFIG_5_1
		{ SOUND_CHANNEL_FRONT_LEFT, SOUND_CHANNEL_FRONT_RIGHT },	// CHANNEL_CONFIG_7_1
	};
	static_assert(ARRAY_SIZE(g_suChannelMapping) == CHANNEL_CONFIG_COUNT, "Mismatched channel mapping");

	static SDLMixer& GetMixer()
	{
		return Audio::Inst()->GetPlatform().GetMixer();
	}

SoundObject_ps::SoundObject_ps()
	: m_pSoundFile(nullptr)
	, m_bLooping(false)
	, m_bPositional(false)
	, m_bPaused(false)
	, m_bValid(false)
	, m_bCustomData(false)
{
}

SoundObject_ps::~SoundObject_ps()
{
	Reset();
}

void SoundObject_ps::Init(Audio* pAudio)
{

}

void SoundObject_ps::Reset()
{
	if (m_voice.bActive && Audio::Inst())
	{
		SDLMixer& mixer = GetMixer();
		mixer.Lock();
		mixer.RemoveVoice(&m_voice);
		mixer.Unlock();
	}
	m_voice = MixerVoice();
	m_baseFilter = MixerFilter();
	m_pSoundFile = nullptr;
	m_bValid = false;
	m_bPaused = false;
	m_bPositional = false;
	m_bCustomData = false;
}

void SoundObject_ps::SetSoundFile(WaveFile* pWaveFile, bool bPositional, bool bLoop)
{
	m_pSoundFile = pWaveFile;
	m_bPositional = bPositional;
	m_bLooping = bLoop;
}

void SoundObject_ps::BindWaveFile()
{
	const WaveFile& waveFile = *m_pSoundFile;
	if (!waveFile.GetData())
	{
		return;
	}

	m_voice.pData = waveFile.GetData();
	m_voice.uFrames = waveFile.GetFrameCount();
	m_voice.uChannels = waveFile.GetChannelCount();
	m_voice.uSampleRate = waveFile.GetSampleRate();
	m_voice.bLoop = m_bLooping;
	m_voice.uLoopStart = 0;
	m_voice.uLoopEnd = 0;
	if (m_bLooping && waveFile.GetLoopLength() > 0)
	{
		m_voice.uLoopStart = waveFile.GetLoopStart();
		m_voice.uLoopEnd = waveFile.GetLoopStart() + waveFile.GetLoopLength();
	}
	m_voice.bStreaming = false;
	m_voice.fPosition = 0.0;
	m_voice.uFramesPlayed = 0;
	m_voice.bFinished = false;

	// The sound's own filter, opened up or closed down by the low-pass
	// amount Audio computes (e.g. for distance)
	m_baseFilter = MixerFilter();
	m_voice.bFilterEnabled = true;
	if (waveFile.GetFilter())
	{
		m_baseFilter = ((const AudioFilter_ps*)waveFile.GetFilter())->GetFilter();
	}

	m_bCustomData = false;
	m_bValid = true;
}

void SoundObject_ps::SetCustomData(const StreamingSoundDef& def)
{
	// Streams are 16-bit PCM, as Audio::PrepareCustomStream describes them
	ASSERT(def.uBitsPerSample == 16);

	SDLMixer& mixer = GetMixer();
	mixer.Lock();
	m_voice.pData = nullptr;
	m_voice.uFrames = 0;
	m_voice.uChannels = def.uChannels;
	m_voice.uSampleRate = def.uSampleRate;
	m_voice.bLoop = false;
	m_voice.bStreaming = true;
	m_voice.pCallbacks = def.pCallbacks;
	m_voice.uStreamHead = 0;
	m_voice.uStreamCount = 0;
	m_voice.fPosition = 0.0;
	m_voice.bFinished = false;
	m_voice.bFilterEnabled = false;
	mixer.Unlock();

	m_bValid = true;
	m_bCustomData = true;
	m_pSoundFile = nullptr;
}

void SoundObject_ps::SubmitData(void* pData, memsize size)
{
	if (!m_bCustomData)
	{
		// Trying to add data to a sound loaded from file
		ASSERT(false);
		return;
	}

	SDLMixer& mixer = GetMixer();
	mixer.Lock();
	const uint32 uFrameBytes = (uint32)(sizeof(sint16) * Math::Max(m_voice.uChannels, 1U));
	bool bQueued = mixer.QueueStreamBuffer(&m_voice, (const sint16*)pData, (uint32)(size / uFrameBytes));
	mixer.Unlock();
	ASSERT_MSG(bQueued, "Too many audio stream buffers queued\n");
}

void SoundObject_ps::Start()
{
	if (m_bValid)
	{
		m_voice.bPaused = false;
		GetMixer().AddVoice(&m_voice);
		m_bPaused = false;
	}
}

void SoundObject_ps::Stop()
{
	if (m_bValid)
	{
		GetMixer().RemoveVoice(&m_voice);
		m_voice.fPosition = 0.0;
		m_voice.uStreamCount = 0;
		m_bPaused = false;
		// Rebind on the next play, as the XAudio2 backend does
		m_bValid = false;

		if (auto spCallbacks = m_voice.pCallbacks.lock())
		{
			spCallbacks->Stopped();
		}
	}
}

void SoundObject_ps::Pause()
{
	if (m_bValid)
	{
		m_voice.bPaused = true;
		m_bPaused = true;
	}
}

void SoundObject_ps::Update(const SoundObject* pParent)
{
	if (pParent->GetRequestedPlayState() == PLAY_STATE_PLAYING && !m_bValid && m_pSoundFile != nullptr)
	{
		BindWaveFile();
	}

	if (!m_bValid)
	{
		return;
	}

	SDLMixer& mixer = GetMixer();
	mixer.Lock();

	const float fVolume = pParent->GetAdjVolume() * pParent->GetAttenMul();
	const uint32 uChannels = Math::Min(m_voice.uChannels, (uint32)MixerVoice::MAX_SOURCE_CHANNELS);
	if (m_bPositional)
	{
		// Every source channel goes to each output at that output's panning
		// gain, as the XAudio2 output matrix is set up
		const PanningData& panning = pParent->GetPanningData();
		for (uint32 o = 0; o < MixerVoice::MAX_OUTPUT_CHANNELS; o++)
		{
			const float fGain = panning.fMatrix[g_suChannelMapping[panning.eConfig][o]] * fVolume;
			for (uint32 c = 0; c < uChannels; c++)
			{
				m_voice.fGain[o][c] = fGain;
			}
		}
	}
	else
	{
		// Mono plays on both outputs; stereo maps straight through
		for (uint32 o = 0; o < MixerVoice::MAX_OUTPUT_CHANNELS; o++)
		{
			for (uint32 c = 0; c < uChannels; c++)
			{
				m_voice.fGain[o][c] = (uChannels == 1 || c == o) ? fVolume : 0.0f;
			}
		}
	}

	m_voice.filter = m_baseFilter;
	m_voice.filter.fFrequency *= pParent->GetLowPassFrequency();
	m_voice.fFrequencyRatio = pParent->GetPitch() * pParent->GetDopplerFactor();

	switch (pParent->GetRequestedPlayState())
	{
	case PLAY_STATE_PLAYING:
		Start();
		break;
	case PLAY_STATE_STOPPED:
		Stop();
		break;
	case PLAY_STATE_PAUSED:
		Pause();
		break;
	case PLAY_STATE_NONE:
	default:
		break;
	}

	mixer.Unlock();
}

bool SoundObject_ps::IsPlaying() const
{
	if (!m_bValid)
	{
		return false;
	}

	SDLMixer& mixer = GetMixer();
	mixer.Lock();
	// As with XAudio2's queued buffers: a bound voice counts as playing until
	// its data runs out, even before it is started
	bool bPlaying = m_bCustomData ? m_voice.uStreamCount > 0 : !m_voice.bFinished;
	mixer.Unlock();
	return bPlaying;
}

uint64 SoundObject_ps::GetSamplesPlayed() const
{
	if (!m_bValid)
	{
		return 0;
	}

	SDLMixer& mixer = GetMixer();
	mixer.Lock();
	uint64 uPlayed = m_voice.uFramesPlayed;
	mixer.Unlock();
	return uPlayed;
}

}
