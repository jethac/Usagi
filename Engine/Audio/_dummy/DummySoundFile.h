/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A sound file which plays nothing, for platforms without an
//	audio backend
*****************************************************************************/
#ifndef __USG_AUDIO_DUMMYSOUNDFILE_H__
#define __USG_AUDIO_DUMMYSOUNDFILE_H__
#include "Engine/Common/Common.h"
#include "Engine/Audio/SoundFile.h"

namespace usg{


	class DummySoundFile  : public SoundFile
	{
	public:
		DummySoundFile() {}
		virtual ~DummySoundFile() {}

		virtual void Init(const SoundFileDef* pSoundFile, const PakFileRaw* pPak, Audio* pAudio, const char* pszLocalizedSubdir = NULL) override { }
		virtual void InitRaw(const SoundFileDef* pSoundFile, const void* pData, size_t rawDataSize, Audio* pAudio) override { }
		virtual void Cleanup(Audio* pAudio) override { }
		virtual void BindToSoundObject(class SoundObject* pSoundObject, bool bPositional) override {}

	private:

	};

}

#endif
