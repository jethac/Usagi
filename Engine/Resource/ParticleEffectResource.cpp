#include "Engine/Common/Common.h"
#include "Engine/Resource/ParticleEffectResource.h"
#include "Engine/Core/ProtocolBuffers/ProtocolBufferFile.h"
#include "Engine/Core/ProtocolBuffers/ProtocolBufferMemory.h"
#include "Engine/Graphics/Device/GFXDevice.h"

namespace usg{

	ParticleEffectResource::ParticleEffectResource() :
		ResourceBase(StaticResType)
    {
    }
    ParticleEffectResource::~ParticleEffectResource()
    {

    }

	bool ParticleEffectResource::Load(const char* szFileName)
	{
		bool bReadSucceeded = LoadCPUData(szFileName);
		if (bReadSucceeded)
		{
			FinalizeCPUData(szFileName);
		}
		else
		{
			SetState(ResourceState::FAILED);
		}
		return bReadSucceeded;
	}

	bool ParticleEffectResource::LoadCPUData(const char* szFileName)
	{
		m_name = szFileName;
		str::RemovePath(m_name);
		str::TruncateExtension(m_name);
		ProtocolBufferFile effectVPB(szFileName);
		bool bReadSucceeded = effectVPB.Read(&m_definition);	
		return bReadSucceeded;
	}

	bool ParticleEffectResource::FinalizeCPUData(const char* szFileName)
	{
		SetupHash(szFileName);
		SetReady(true);
		return true;
	}

}
