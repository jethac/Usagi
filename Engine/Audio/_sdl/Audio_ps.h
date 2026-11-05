/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: 3D and streaming audio on SDL3. The engine's Audio class does
//	the panning, attenuation, doppler and low-pass calculations; this backend
//	plays the voices with its own mixer on an SDL audio stream.
*****************************************************************************/
#ifndef __USG_AUDIO_SDL_AUDIO_PS_H__
#define __USG_AUDIO_SDL_AUDIO_PS_H__

#include "Engine/Audio/SoundActorHandle.h"
#include "Engine/Maths/Vector4f.h"
#include "Engine/Audio/AudioBank.pb.h"
#include "Engine/Memory/Mem.h"
#include "Engine/Audio/AudioDefs.h"
#include "SDLMixer.h"
#include "WaveFile.h"

namespace usg{

class SoundHandle;
class SoundHandle3D;
class AudioListener;
class SoundFile;
class AudioFilter;
class AudioEffect;
class AudioRoom;
class SoundObject;

class Audio_ps
{
public:
	Audio_ps();
	~Audio_ps();

	void Init();

	SoundFile* CreateSoundFile(const SoundFileDef* pDef);
	AudioFilter* CreateAudioFilter(const AudioFilterDef* pDef);
	AudioEffect* CreateAudioEffect(const AudioEffectDef* pDef);
	AudioRoom* CreateAudioRoom(const AudioRoomDef* pDef);

	void AddListener(AudioListener* pListener) {}
	void RemoveListener(AudioListener* pListener) {}

	void Update(float fElapsed);
	void SetAudioDevice(char16* deviceName) {}
	void SetOutputChannelConfig(ChannelConfig eChannelConfig) {}
	static uint32 GetChannelCount(ChannelConfig eChannelConfig);

	void EnableEffect(AudioType eType, AudioEffect* pEffect) {}
	void DisableEffect(AudioType eType, AudioEffect* pEffect) {}

	SDLMixer& GetMixer() { return m_mixer; }

private:
	PRIVATIZE_COPY(Audio_ps)

	SDLMixer	m_mixer;
};

}

#endif
