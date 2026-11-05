/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: 3D and streaming audio interface - a silent implementation
//	for platforms without an audio backend
*****************************************************************************/
#ifndef __USG_AUDIO_DUMMY_AUDIO_PS_H__
#define __USG_AUDIO_DUMMY_AUDIO_PS_H__
#include "Engine/Common/Common.h"
#include "Engine/Audio/SoundActorHandle.h"
#include "Engine/Audio/AudioDefs.h"
#include "Engine/Maths/Vector4f.h"
#include "Engine/Audio/AudioBank.pb.h"
#include "Engine/Audio/AudioFilter.h"
#include "Engine/Audio/AudioEffect.h"
#include "Engine/Audio/AudioRoom.h"
#include "Engine/Memory/Mem.h"
#include "DummySoundFile.h"

namespace usg{

class SoundHandle;
class SoundHandle3D;
class AudioListener;
class SoundFile;
class SoundObject;

class DummyAudioFilter : public AudioFilter
{
public:
	virtual void Init(const AudioFilterDef* pDef, Audio* pAudio) override { InitInt(pDef); }
};

class DummyAudioEffect : public AudioEffect
{
public:
	virtual void Init(const AudioEffectDef* pDef, Audio* pAudio) override { InitInt(pDef); }
};

class DummyAudioRoom : public AudioRoom
{
public:
	virtual void Init(const AudioRoomDef* pDef, Audio* pAudio) override { m_uCRC = pDef->crc; }
};

class Audio_ps
{
public:
	Audio_ps();
	~Audio_ps();

	void Init();

	SoundFile* CreateSoundFile(const SoundFileDef* pDef) { return vnew(ALLOC_AUDIO) DummySoundFile(); }
	AudioFilter* CreateAudioFilter(const AudioFilterDef* pDef) { return vnew(ALLOC_AUDIO) DummyAudioFilter(); }
	AudioEffect* CreateAudioEffect(const AudioEffectDef* pDef) { return vnew(ALLOC_AUDIO) DummyAudioEffect(); }
	AudioRoom* CreateAudioRoom(const AudioRoomDef* pDef) { return vnew(ALLOC_AUDIO) DummyAudioRoom(); }
	void LoadSound(uint32 uSoundId) {}
	void AddListener(AudioListener* pListener) {}
	void RemoveListener(AudioListener* pListener) {}
	void SetAudioDevice(char16* deviceName) {}
	void SetOutputChannelConfig(ChannelConfig eChannelConfig) {}
	void EnableEffect(AudioType eType, AudioEffect* pEffect) {}
	void DisableEffect(AudioType eType, AudioEffect* pEffect) {}

	void Update(float fElapsed);

private:
	PRIVATIZE_COPY(Audio_ps)


};

}


#endif
