/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/OS.h"
#include "Engine/Audio/Audio.h"
#include "DummySoundFile.h"
#include "AudioEffects_ps.h"
#include "Audio_ps.h"

namespace usg{

static const uint32 g_suChannelCount[] =
{
	2, // CHANNEL_CONFIG_2_0,
	2, // CHANNEL_CONFIG_HEADPHONES,
	3, // CHANNEL_CONFIG_2_1,
	6, // CHANNEL_CONFIG_5_1,
	8, // CHANNEL_CONFIG_7_1,
};
static_assert(ARRAY_SIZE(g_suChannelCount) == CHANNEL_CONFIG_COUNT, "Mismatched channel count");

// The game's sounds are authored at 48 kHz
static const uint32 g_uOutputSampleRate = 48000;

uint32 Audio_ps::GetChannelCount(ChannelConfig eChannelConfig)
{
	return g_suChannelCount[eChannelConfig];
}

Audio_ps::Audio_ps()
{
}

Audio_ps::~Audio_ps()
{
	m_mixer.Shutdown();
}

void Audio_ps::Init()
{
	// As on Windows, USAGI_NO_AUDIO=1 skips the device; every sound file is
	// then a DummySoundFile
	char szNoAudio[8] = {};
	if (OS::GetEnvVar("USAGI_NO_AUDIO", szNoAudio, sizeof(szNoAudio)) && szNoAudio[0] == '1')
	{
		return;
	}

	m_mixer.Init(g_uOutputSampleRate);
}

void Audio_ps::Update(float fElapsed)
{
	// USAGI_AUDIO_LOG=1 reports the mixer's activity every few seconds, so
	// output can be checked without listening
	static int s_iLog = -1;
	if (s_iLog < 0)
	{
		char szEnv[8] = {};
		s_iLog = OS::GetEnvVar("USAGI_AUDIO_LOG", szEnv, sizeof(szEnv)) ? 1 : 0;
	}
	if (s_iLog != 1 || !m_mixer.IsInitialised())
	{
		return;
	}

	static float s_fElapsed = 0.0f;
	s_fElapsed += fElapsed;
	if (s_fElapsed >= 5.0f)
	{
		float fPeak;
		uint32 uVoices;
		m_mixer.ReadAndResetStats(fPeak, uVoices);
		RELEASE_WARNING("Audio: up to %u voices, peak output %.3f (%.1f dBFS)\n", uVoices, fPeak, fPeak > 0.0f ? 20.0f * log10f(fPeak) : -999.0f);
		s_fElapsed = 0.0f;
	}
}

SoundFile* Audio_ps::CreateSoundFile(const SoundFileDef* pDef)
{
	if (m_mixer.IsInitialised())
	{
		return vnew(ALLOC_AUDIO) WaveFile();
	}
	return vnew(ALLOC_AUDIO) DummySoundFile();
}

AudioFilter* Audio_ps::CreateAudioFilter(const AudioFilterDef* pDef)
{
	return vnew(ALLOC_AUDIO) AudioFilter_ps;
}

AudioEffect* Audio_ps::CreateAudioEffect(const AudioEffectDef* pDef)
{
	return vnew(ALLOC_AUDIO) AudioEffect_ps;
}

AudioRoom* Audio_ps::CreateAudioRoom(const AudioRoomDef* pDef)
{
	return vnew(ALLOC_AUDIO) AudioRoom_ps;
}

}
