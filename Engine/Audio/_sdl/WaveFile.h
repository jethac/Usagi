/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A PCM wave file for the SDL backend, held as 16-bit
//	interleaved samples
*****************************************************************************/
#ifndef __USG_AUDIO_SDL_WAVEFILE_H__
#define __USG_AUDIO_SDL_WAVEFILE_H__

#include "Engine/Audio/SoundFile.h"
#include "Engine/Audio/Shared/WaveFileReader.h"
#include "Engine/Resource/PakFile.h"

namespace usg
{

	class Audio;

	class WaveFile  : public SoundFile
	{
	public:
		WaveFile();
		~WaveFile();

		virtual void Init(const SoundFileDef* pSoundFile, const PakFileRaw* pPak, Audio* pAudio, const char* pszLocalizedSubdir = NULL) override;
		virtual void InitRaw(const SoundFileDef* pSoundFile, const void* pData, size_t rawDataSize, Audio* pAudio) override;
		virtual void Cleanup(Audio* pAudio) override {}
		virtual void BindToSoundObject(class SoundObject* pSoundObject, bool bPositional) override;
		virtual float GetDuration() const override;

		const sint16* GetData() const { return m_pData; }
		uint32 GetFrameCount() const { return m_uFrames; }
		uint32 GetChannelCount() const { return m_uChannels; }
		uint32 GetSampleRate() const { return m_uSampleRate; }
		// In frames
		uint32 GetLoopStart() const { return m_uLoopStart; }
		uint32 GetLoopLength() const { return m_uLoopLength; }

	private:
		void ProcessWaveFile(WaveFileReader& reader, const SoundFileDef* pDef, const char* szName, Audio* pAudio);

		sint16*		m_pData;
		uint32		m_uFrames;
		uint32		m_uChannels;
		uint32		m_uSampleRate;
		uint32		m_uLoopStart;
		uint32		m_uLoopLength;
	};

}

#endif
