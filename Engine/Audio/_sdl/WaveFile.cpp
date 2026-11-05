/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Maths/MathUtil.h"
#include "Engine/Audio/Shared/WaveFileReader.h"
#include "Engine/Audio/SoundObject.h"
#include "WaveFile.h"

namespace usg
{

static const uint16 WAVE_FORMAT_ID_PCM = 1;
static const uint16 WAVE_FORMAT_ID_FLOAT = 3;

WaveFile::WaveFile()
	: m_pData(nullptr)
	, m_uFrames(0)
	, m_uChannels(0)
	, m_uSampleRate(48000)
	, m_uLoopStart(0)
	, m_uLoopLength(0)
{
}

WaveFile::~WaveFile()
{
	if (m_pData)
	{
		vdelete[] m_pData;
		m_pData = nullptr;
	}
}

// Reads the data chunk and converts it to 16-bit PCM, the mixer's format
void WaveFile::ProcessWaveFile(WaveFileReader& reader, const SoundFileDef* pDef, const char* szName, Audio* pAudio)
{
	const WaveFileReader::WaveFormat& format = reader.GetWaveFormat();
	const uint32 uBytesPerSample = format.quantumBits / 8;
	const sint64 dataSize = reader.GetDataSize();

	m_uChannels = format.channel;
	m_uSampleRate = format.sampleRate;
	m_uLoopStart = reader.GetLoopStart();
	m_uLoopLength = reader.GetLoopLength();

	const bool bSupported = m_uChannels > 0 && uBytesPerSample > 0 &&
		((format.fmtId == WAVE_FORMAT_ID_PCM && uBytesPerSample <= 4) || (format.fmtId == WAVE_FORMAT_ID_FLOAT && uBytesPerSample == 4));
	if (!bSupported || dataSize <= 0)
	{
		DEBUG_PRINT("%s: unsupported wave format (id %u, %u bits)\n", szName, format.fmtId, format.quantumBits);
		InitInt(pDef, szName, pAudio);
		return;
	}

	const uint32 uSamples = (uint32)(dataSize / uBytesPerSample);
	m_uFrames = uSamples / m_uChannels;
	m_pData = vnew(ALLOC_AUDIO) sint16[m_uFrames * m_uChannels];

	if (format.fmtId == WAVE_FORMAT_ID_PCM && uBytesPerSample == 2)
	{
		reader.ReadData(m_pData, m_uFrames * m_uChannels * sizeof(sint16));
	}
	else
	{
		uint8* pRaw = vnew(ALLOC_AUDIO) uint8[(size_t)dataSize];
		reader.ReadData(pRaw, dataSize);
		for (uint32 i = 0; i < m_uFrames * m_uChannels; i++)
		{
			const uint8* p = pRaw + i * uBytesPerSample;
			float fSample;
			if (format.fmtId == WAVE_FORMAT_ID_FLOAT)
			{
				float32 f;
				MemCpy(&f, p, sizeof(f));
				fSample = f;
			}
			else if (uBytesPerSample == 1)
			{
				fSample = ((float)p[0] - 128.0f) / 128.0f;
			}
			else if (uBytesPerSample == 3)
			{
				sint32 v = (sint32)((uint32)p[0] << 8 | (uint32)p[1] << 16 | (uint32)p[2] << 24) >> 8;
				fSample = (float)v / 8388608.0f;
			}
			else
			{
				sint32 v;
				MemCpy(&v, p, sizeof(v));
				fSample = (float)v / 2147483648.0f;
			}
			m_pData[i] = (sint16)Math::Clamp(fSample * 32767.0f, -32768.0f, 32767.0f);
		}
		vdelete[] pRaw;
	}

	InitInt(pDef, szName, pAudio);
}

float WaveFile::GetDuration() const
{
	return m_uSampleRate > 0 ? (float)m_uFrames / (float)m_uSampleRate : 0.0f;
}

void WaveFile::InitRaw(const SoundFileDef* pDef, const void* pData, size_t rawDataSize, Audio* pAudio)
{
	WaveFileReader reader;
	reader.Initialize(pData, rawDataSize);
	ProcessWaveFile(reader, pDef, pDef->filename, pAudio);
}

void WaveFile::Init(const SoundFileDef* pDef, const PakFileRaw* pPak, Audio* pAudio, const char* pszLocalizedSubdir)
{
	string name = "Audio/";
	if (pDef->localized && pszLocalizedSubdir != NULL)
	{
		name += pszLocalizedSubdir;
		name += "/";
	}
	name += pDef->filename;
	name += ".wav";

	PakFileRaw::FileRef ref;
	WaveFileReader reader;
	if (pPak && pPak->GetFile(name.c_str(), ref))
	{
		reader.Initialize(ref.pData, (memsize)ref.pFileHeader->uDataSize);
	}
	else
	{
		reader.Initialize(name.c_str());
	}

	ProcessWaveFile(reader, pDef, name.c_str(), pAudio);
}

void WaveFile::BindToSoundObject(class SoundObject* pSoundObject, bool bPositional)
{
	pSoundObject->GetPlatform().SetSoundFile(this, bPositional, m_bLooping);
	pSoundObject->SetSoundFile(this);
}

}
