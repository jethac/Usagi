/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Stores the header for a streamed file so we can kick off
//	playback as quickly as possible
*****************************************************************************/
#ifndef __USG_AUDIO_DUMMYSOUNDFILE_H__
#define __USG_AUDIO_DUMMYSOUNDFILE_H__

#include "Engine/Audio/SoundFile.h"
#include "Engine/Resource/PakFile.h"

namespace usg{


	class DummySoundFile  : public SoundFile
	{
	public:	
		DummySoundFile() {}
		~DummySoundFile() {}

		// No device, so no data, but the definition (type, priority, instance
		// cap, ...) is still needed: Audio indexes per-type volumes with it.
		virtual void Init(const SoundFileDef* pSoundFile, const PakFileRaw* pPak, Audio* pAudio, const char* pszLocalizedSubdir = NULL) { InitInt(pSoundFile, pSoundFile->filename, pAudio); }
		virtual void InitRaw(const SoundFileDef* pSoundFile, const void* pData, size_t rawDataSize, Audio* pAudio) { InitInt(pSoundFile, pSoundFile->filename, pAudio); }
		virtual void Cleanup(Audio* pAudio) { }
		virtual void BindToSoundObject(class SoundObject* pSoundObject, bool bPositional) {}

	private:

	};

}

#endif
