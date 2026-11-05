/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Maths/MathUtil.h"
#include "Engine/Audio/SoundCallbacks.h"
#include "SDLMixer.h"
#include <SDL3/SDL.h>

namespace usg
{
	static const float g_fInv32768 = 1.0f / 32768.0f;
	static const float g_fMaxFrequencyRatio = 4.0f;		// Pitch 2.0 times doppler
	static const float g_fKnee = 0.8f;					// Output limiter knee (-1.9 dBFS)

	static void SDLCALL MixerStreamCallback(void* pUserData, SDL_AudioStream* pStream, int iAdditional, int /*iTotal*/)
	{
		SDLMixer* pMixer = (SDLMixer*)pUserData;
		const int iFrameBytes = (int)(sizeof(float) * SDLMixer::OUTPUT_CHANNELS);
		int iFrames = iAdditional / iFrameBytes;
		while (iFrames > 0)
		{
			uint32 uChunk = (uint32)Math::Min(iFrames, (int)SDLMixer::MIX_CHUNK_FRAMES);
			pMixer->Mix(pMixer->GetMixBuffer(), uChunk);
			SDL_PutAudioStreamData(pStream, pMixer->GetMixBuffer(), (int)(uChunk * iFrameBytes));
			iFrames -= (int)uChunk;
		}
	}

	SDLMixer::SDLMixer()
		: m_pStream(nullptr)
		, m_pMutex(nullptr)
		, m_uSampleRate(48000)
		, m_fPeak(0.0f)
		, m_uMaxVoices(0)
	{
	}

	SDLMixer::~SDLMixer()
	{
		Shutdown();
	}

	bool SDLMixer::Init(uint32 uSampleRate)
	{
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
		{
			DEBUG_PRINT("SDL audio init failed: %s\n", SDL_GetError());
			return false;
		}

		m_uSampleRate = uSampleRate;
		m_voices.reserve(256);
		m_pMutex = SDL_CreateMutex();

		SDL_AudioSpec spec;
		spec.format = SDL_AUDIO_F32;
		spec.channels = OUTPUT_CHANNELS;
		spec.freq = (int)uSampleRate;
		m_pStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, MixerStreamCallback, this);
		if (!m_pStream)
		{
			DEBUG_PRINT("SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
			SDL_DestroyMutex(m_pMutex);
			m_pMutex = nullptr;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return false;
		}

		SDL_ResumeAudioStreamDevice(m_pStream);
		return true;
	}

	void SDLMixer::Shutdown()
	{
		if (m_pStream)
		{
			SDL_DestroyAudioStream(m_pStream);
			m_pStream = nullptr;
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		}
		if (m_pMutex)
		{
			SDL_DestroyMutex(m_pMutex);
			m_pMutex = nullptr;
		}
		m_voices.clear();
	}

	void SDLMixer::Lock()
	{
		if (m_pMutex)
		{
			SDL_LockMutex(m_pMutex);
		}
	}

	void SDLMixer::Unlock()
	{
		if (m_pMutex)
		{
			SDL_UnlockMutex(m_pMutex);
		}
	}

	void SDLMixer::AddVoice(MixerVoice* pVoice)
	{
		if (pVoice->bActive)
		{
			return;
		}
		pVoice->bActive = true;
		pVoice->bRampInit = false;
		m_voices.push_back(pVoice);
	}

	void SDLMixer::RemoveVoice(MixerVoice* pVoice)
	{
		if (!pVoice->bActive)
		{
			return;
		}
		for (memsize i = 0; i < m_voices.size(); i++)
		{
			if (m_voices[i] == pVoice)
			{
				m_voices[i] = m_voices.back();
				m_voices.pop_back();
				break;
			}
		}
		pVoice->bActive = false;
	}

	void SDLMixer::ReadAndResetStats(float& fPeakOut, uint32& uMaxVoicesOut)
	{
		Lock();
		fPeakOut = m_fPeak;
		uMaxVoicesOut = m_uMaxVoices;
		m_fPeak = 0.0f;
		m_uMaxVoices = 0;
		Unlock();
	}

	bool SDLMixer::QueueStreamBuffer(MixerVoice* pVoice, const sint16* pData, uint32 uFrames)
	{
		if (pVoice->uStreamCount >= MixerVoice::MAX_STREAM_BUFFERS)
		{
			return false;
		}
		uint32 uSlot = (pVoice->uStreamHead + pVoice->uStreamCount) % MixerVoice::MAX_STREAM_BUFFERS;
		pVoice->stream[uSlot].pData = pData;
		pVoice->stream[uSlot].uFrames = uFrames;
		pVoice->uStreamCount++;
		return true;
	}

	void SDLMixer::Mix(float* pOut, uint32 uFrames)
	{
		MemClear(pOut, sizeof(float) * uFrames * OUTPUT_CHANNELS);

		Lock();
		uint32 uMixed = 0;
		for (MixerVoice* pVoice : m_voices)
		{
			if (!pVoice->bPaused && !pVoice->bFinished)
			{
				MixVoice(*pVoice, pOut, uFrames);
				uMixed++;
			}
		}

		// Keep the mix inside [-1, 1] with a soft knee rather than hard clipping
		float fPeak = m_fPeak;
		for (uint32 i = 0; i < uFrames * OUTPUT_CHANNELS; i++)
		{
			const float fAbs = Math::Abs(pOut[i]);
			fPeak = Math::Max(fPeak, fAbs);
			if (fAbs > g_fKnee)
			{
				const float fOver = (fAbs - g_fKnee) / (1.0f - g_fKnee);
				const float fLimited = g_fKnee + (1.0f - g_fKnee) * tanhf(fOver);
				pOut[i] = pOut[i] < 0.0f ? -fLimited : fLimited;
			}
		}
		m_fPeak = fPeak;
		m_uMaxVoices = Math::Max(m_uMaxVoices, uMixed);
		Unlock();
	}

	// Reads one source frame, following loops and queued stream buffers.
	// Returns false past the end of the data.
	bool SDLMixer::ReadFrame(const MixerVoice& voice, uint32 uFrame, float* pSamples) const
	{
		const uint32 uChannels = voice.uChannels;
		const sint16* pFrame = nullptr;

		if (voice.bStreaming)
		{
			// uFrame is relative to the head buffer
			for (uint32 i = 0; i < voice.uStreamCount; i++)
			{
				const MixerVoice::StreamBuffer& buffer = voice.stream[(voice.uStreamHead + i) % MixerVoice::MAX_STREAM_BUFFERS];
				if (uFrame < buffer.uFrames)
				{
					pFrame = buffer.pData + uFrame * uChannels;
					break;
				}
				uFrame -= buffer.uFrames;
			}
		}
		else
		{
			const uint32 uEnd = (voice.bLoop && voice.uLoopEnd > voice.uLoopStart) ? voice.uLoopEnd : voice.uFrames;
			if (uFrame >= uEnd)
			{
				if (!voice.bLoop || uEnd <= voice.uLoopStart)
				{
					return false;
				}
				uFrame = voice.uLoopStart + (uFrame - uEnd) % (uEnd - voice.uLoopStart);
			}
			pFrame = voice.pData + uFrame * uChannels;
		}

		if (!pFrame)
		{
			return false;
		}
		for (uint32 c = 0; c < uChannels; c++)
		{
			pSamples[c] = pFrame[c] * g_fInv32768;
		}
		return true;
	}

	// Wraps the position at loop ends and retires finished stream buffers.
	// Returns false when a one-shot voice has ended.
	bool SDLMixer::AdvancePosition(MixerVoice& voice)
	{
		if (voice.bStreaming)
		{
			while (voice.uStreamCount > 0)
			{
				MixerVoice::StreamBuffer& head = voice.stream[voice.uStreamHead];
				if (!voice.bStreamBufferStarted)
				{
					voice.bStreamBufferStarted = true;
					if (auto spCallbacks = voice.pCallbacks.lock()) spCallbacks->BufferStart();
				}
				if (voice.fPosition < (double)head.uFrames)
				{
					return true;
				}
				voice.fPosition -= (double)head.uFrames;
				voice.uStreamHead = (voice.uStreamHead + 1) % MixerVoice::MAX_STREAM_BUFFERS;
				voice.uStreamCount--;
				voice.bStreamBufferStarted = false;
				if (auto spCallbacks = voice.pCallbacks.lock()) spCallbacks->BufferEnd();
			}
			// Starved: wait for more data
			voice.fPosition = 0.0;
			return true;
		}

		const uint32 uEnd = (voice.bLoop && voice.uLoopEnd > voice.uLoopStart) ? voice.uLoopEnd : voice.uFrames;
		if (voice.fPosition < (double)uEnd)
		{
			return true;
		}
		if (voice.bLoop && uEnd > voice.uLoopStart)
		{
			const double fLoopLength = (double)(uEnd - voice.uLoopStart);
			while (voice.fPosition >= (double)uEnd)
			{
				voice.fPosition -= fLoopLength;
			}
			if (auto spCallbacks = voice.pCallbacks.lock()) spCallbacks->LoopEnd();
			return true;
		}
		voice.bFinished = true;
		return false;
	}

	void SDLMixer::MixVoice(MixerVoice& voice, float* pOut, uint32 uFrames)
	{
		const uint32 uChannels = Math::Min(voice.uChannels, (uint32)MixerVoice::MAX_SOURCE_CHANNELS);
		if (uChannels == 0 || (!voice.bStreaming && (!voice.pData || voice.uFrames == 0)))
		{
			voice.bFinished = true;
			return;
		}
		if (voice.bStreaming && voice.uStreamCount == 0)
		{
			return;
		}

		const double fStep = (double)Math::Clamp(voice.fFrequencyRatio, 0.001f, g_fMaxFrequencyRatio)
			* (double)voice.uSampleRate / (double)m_uSampleRate;

		// Ramp gain changes over the chunk so they don't click
		if (!voice.bRampInit)
		{
			MemCpy(voice.fCurrentGain, voice.fGain, sizeof(voice.fGain));
			voice.bRampInit = true;
		}
		float fGainStep[OUTPUT_CHANNELS][MixerVoice::MAX_SOURCE_CHANNELS];
		float fGain[OUTPUT_CHANNELS][MixerVoice::MAX_SOURCE_CHANNELS];
		for (uint32 o = 0; o < OUTPUT_CHANNELS; o++)
		{
			for (uint32 c = 0; c < uChannels; c++)
			{
				fGain[o][c] = voice.fCurrentGain[o][c];
				fGainStep[o][c] = (voice.fGain[o][c] - voice.fCurrentGain[o][c]) / (float)uFrames;
			}
		}

		const bool bFilter = voice.bFilterEnabled &&
			!(voice.filter.eType == AUDIO_FILTER_LOW_PASS && voice.filter.fFrequency >= 1.0f);
		const float fF = Math::Clamp(voice.filter.fFrequency, 0.0f, 1.0f);
		const float fQ = Math::Clamp(voice.filter.fOneOverQ, 0.0f, 1.5f);

		float a[MixerVoice::MAX_SOURCE_CHANNELS];
		float b[MixerVoice::MAX_SOURCE_CHANNELS];
		for (uint32 i = 0; i < uFrames; i++)
		{
			if (!AdvancePosition(voice))
			{
				break;
			}
			if (voice.bStreaming && voice.uStreamCount == 0)
			{
				break;
			}

			const uint32 uIndex = (uint32)voice.fPosition;
			const float fFrac = (float)(voice.fPosition - (double)uIndex);
			if (!ReadFrame(voice, uIndex, a))
			{
				voice.bFinished = !voice.bStreaming;
				break;
			}
			if (!ReadFrame(voice, uIndex + 1, b))
			{
				MemCpy(b, a, sizeof(a));
			}

			for (uint32 c = 0; c < uChannels; c++)
			{
				float fSample = a[c] + (b[c] - a[c]) * fFrac;
				if (bFilter)
				{
					// Chamberlin state-variable filter, as XAudio2 uses
					float& fLow = voice.fFilterLow[c];
					float& fBand = voice.fFilterBand[c];
					fLow += fF * fBand;
					const float fHigh = fSample - fLow - fQ * fBand;
					fBand += fF * fHigh;
					switch (voice.filter.eType)
					{
					case AUDIO_FILTER_HIGH_PASS:
					case AUDIO_FILTER_HIGH_PASS_ONE_POLE:
						fSample = fHigh;
						break;
					case AUDIO_FILTER_BAND_PASS:
						fSample = fBand;
						break;
					case AUDIO_FILTER_NOTCH:
						fSample = fHigh + fLow;
						break;
					default:
						fSample = fLow;
						break;
					}
				}
				for (uint32 o = 0; o < OUTPUT_CHANNELS; o++)
				{
					pOut[i * OUTPUT_CHANNELS + o] += fSample * fGain[o][c];
					fGain[o][c] += fGainStep[o][c];
				}
			}

			voice.fPosition += fStep;
			voice.uFramesPlayed++;
		}

		MemCpy(voice.fCurrentGain, voice.fGain, sizeof(voice.fGain));
	}
}
