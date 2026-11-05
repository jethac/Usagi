/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A small software mixer on an SDL3 audio stream. Voices play
//	16-bit PCM with per-output-channel gains, volume, a state-variable
//	filter (XAudio2's parameterisation), pitch by resampling and looping;
//	streaming voices play buffers the game submits. SDL calls Mix on its
//	audio thread; the game thread changes voices under the mixer lock.
*****************************************************************************/
#ifndef __USG_AUDIO_SDL_MIXER_H__
#define __USG_AUDIO_SDL_MIXER_H__

#include "Engine/Core/stl/vector.h"
#include "Engine/Core/stl/memory.h"
#include "Engine/Audio/AudioBank.pb.h"

struct SDL_AudioStream;
struct SDL_Mutex;

namespace usg
{
	class SoundCallbacks;

	// XAudio2-style filter: Frequency is 2*sin(pi*cutoff/sampleRate), where
	// 1 (or more) means fully open; OneOverQ is the reciprocal Q
	struct MixerFilter
	{
		AudioFilterType	eType = AUDIO_FILTER_LOW_PASS;
		float			fFrequency = 1.0f;
		float			fOneOverQ = 1.0f;
	};

	struct MixerVoice
	{
		enum
		{
			MAX_SOURCE_CHANNELS = 2,
			MAX_OUTPUT_CHANNELS = 2,
			MAX_STREAM_BUFFERS = 16
		};

		struct StreamBuffer
		{
			const sint16*	pData;
			uint32			uFrames;
		};

		// Source description, set before the voice starts
		const sint16*	pData = nullptr;	// Interleaved PCM
		uint32			uFrames = 0;
		uint32			uChannels = 1;
		uint32			uSampleRate = 48000;
		bool			bLoop = false;
		uint32			uLoopStart = 0;		// In frames
		uint32			uLoopEnd = 0;		// Exclusive; 0 means the end of the data
		bool			bStreaming = false;
		usg::weak_ptr<SoundCallbacks>	pCallbacks;

		// Parameters (game thread, under the mixer lock)
		float			fGain[MAX_OUTPUT_CHANNELS][MAX_SOURCE_CHANNELS] = {};
		float			fFrequencyRatio = 1.0f;
		MixerFilter		filter;
		bool			bFilterEnabled = false;
		bool			bPaused = false;

		// Mixer state (audio thread)
		double			fPosition = 0.0;
		uint64			uFramesPlayed = 0;
		bool			bFinished = false;
		bool			bActive = false;
		bool			bRampInit = false;
		float			fCurrentGain[MAX_OUTPUT_CHANNELS][MAX_SOURCE_CHANNELS] = {};
		float			fFilterLow[MAX_SOURCE_CHANNELS] = {};
		float			fFilterBand[MAX_SOURCE_CHANNELS] = {};
		StreamBuffer	stream[MAX_STREAM_BUFFERS] = {};
		uint32			uStreamHead = 0;
		uint32			uStreamCount = 0;
		bool			bStreamBufferStarted = false;
	};

	class SDLMixer
	{
	public:
		SDLMixer();
		~SDLMixer();

		bool Init(uint32 uSampleRate);
		void Shutdown();
		bool IsInitialised() const { return m_pStream != nullptr; }
		uint32 GetSampleRate() const { return m_uSampleRate; }

		// Locks out the audio thread while voices are changed
		void Lock();
		void Unlock();

		// Call with the lock held
		void AddVoice(MixerVoice* pVoice);
		void RemoveVoice(MixerVoice* pVoice);
		bool QueueStreamBuffer(MixerVoice* pVoice, const sint16* pData, uint32 uFrames);

		// Mix statistics since the last call: peak output level and the most
		// voices mixed at once
		void ReadAndResetStats(float& fPeakOut, uint32& uMaxVoicesOut);

		// Audio thread: fills uFrames interleaved stereo frames
		void Mix(float* pOut, uint32 uFrames);
		float* GetMixBuffer() { return m_mixBuffer; }
		enum { MIX_CHUNK_FRAMES = 512, OUTPUT_CHANNELS = MixerVoice::MAX_OUTPUT_CHANNELS };

	private:
		void MixVoice(MixerVoice& voice, float* pOut, uint32 uFrames);
		bool ReadFrame(const MixerVoice& voice, uint32 uFrame, float* pSamples) const;
		bool AdvancePosition(MixerVoice& voice);

		SDL_AudioStream*			m_pStream;
		SDL_Mutex*					m_pMutex;
		uint32						m_uSampleRate;
		usg::vector<MixerVoice*>	m_voices;
		float						m_fPeak;
		uint32						m_uMaxVoices;
		float						m_mixBuffer[MIX_CHUNK_FRAMES * OUTPUT_CHANNELS];
	};
}

#endif
