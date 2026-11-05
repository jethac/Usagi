/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/PostFX/PostFXSys.h"
#include "Engine/Graphics/GFX.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Graphics/Device/GFXContext.h"
#include "Engine/Resource/ResourceMgr.h"
#include "Engine/Graphics/StandardVertDecl.h"
#include "Engine/Scene/SceneConstantSets.h"
#include "ToneMap.h"

namespace usg {

struct ToneMapConstants
{
	Vector4f	vParams;	// exposure, saturation, contrast, vignette
	Vector4f	vLUT;		// strength, enabled, aspect, unused
};

static const ShaderConstantDecl g_toneMapConstantDef[] =
{
	SHADER_CONSTANT_ELEMENT(ToneMapConstants, vParams,	CT_VECTOR_4, 1),
	SHADER_CONSTANT_ELEMENT(ToneMapConstants, vLUT,		CT_VECTOR_4, 1),
	SHADER_CONSTANT_END()
};

static const DescriptorDeclaration g_toneMapDescriptorDecl[] =
{
	DESCRIPTOR_ELEMENT(0,						 DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, SHADER_FLAG_PIXEL),
	DESCRIPTOR_ELEMENT(1,						 DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, SHADER_FLAG_PIXEL),
	DESCRIPTOR_ELEMENT(SHADER_CONSTANT_MATERIAL, DESCRIPTOR_TYPE_CONSTANT_BUFFER, 1, SHADER_FLAG_PIXEL),
	DESCRIPTOR_END()
};


ToneMap::ToneMap() :
	PostEffect()
{
	SetRenderMask(RENDER_MASK_POST_EFFECT);
	SetLayer(LAYER_POST_PROCESS);
	// After bloom (20) and tilt shift (22), before the LDR anti-aliasing (29)
	SetPriority(25);
	m_pSys = nullptr;
	m_pDestTarget = nullptr;
}

ToneMap::~ToneMap()
{
}

void ToneMap::Init(GFXDevice* pDevice, ResourceMgr* pResource, PostFXSys* pSys)
{
	m_pSys = pSys;

	m_decl.inputBindings[0].Init(GetVertexDeclaration(VT_POSITION));
	m_decl.uInputBindingCount = 1;
	m_decl.ePrimType = PT_TRIANGLES;
	m_decl.pEffect = pResource->GetEffect(pDevice, "PostProcess.ToneMap");

	DescriptorSetLayoutHndl matDescriptors = pDevice->GetDescriptorSetLayout(g_toneMapDescriptorDecl);
	m_decl.layout.descriptorSets[0] = pDevice->GetDescriptorSetLayout(SceneConsts::g_globalDescriptorDecl);
	m_decl.layout.descriptorSets[1] = matDescriptors;
	m_decl.layout.uDescriptorSetCount = 2;
	m_decl.rasterizerState.eCullFace = CULL_FACE_NONE;
	m_decl.alphaState.SetColor0Only();

	m_pointSampler = pDevice->GetSampler(SamplerDecl(SAMP_FILTER_POINT, SAMP_WRAP_CLAMP));
	m_linearSampler = pDevice->GetSampler(SamplerDecl(SAMP_FILTER_LINEAR, SAMP_WRAP_CLAMP));

	m_material.SetDescriptorLayout(pDevice, matDescriptors);
	m_constantSet.Init(pDevice, g_toneMapConstantDef);
	m_material.SetConstantSet(SHADER_CONSTANT_MATERIAL, &m_constantSet);

	ToneMapSettings_init(&m_settings);
	SetSettings(pDevice, pResource, m_settings);
}

void ToneMap::SetSettings(GFXDevice* pDevice, ResourceMgr* pResource, const ToneMapSettings& settings)
{
	m_settings = settings;
	const char* szLUT = settings.lutName[0] != '\0' ? settings.lutName : "lut_neutral";
	m_lut = pResource->GetTexture(pDevice, szLUT);
	m_material.SetTexture(1, m_lut, m_linearSampler);
	UpdateConstants(pDevice);
	if (m_pDestTarget)
	{
		m_material.UpdateDescriptors(pDevice);
	}
}

void ToneMap::UpdateConstants(GFXDevice* pDevice)
{
	ToneMapConstants* pConsts = m_constantSet.Lock<ToneMapConstants>();
	pConsts->vParams.Assign(m_settings.fExposure, m_settings.fSaturation, m_settings.fContrast, m_settings.fVignette);
	const float fAspect = (float)m_pSys->GetFinalTargetWidth() / (float)Math::Max(m_pSys->GetFinalTargetHeight(), 1U);
	pConsts->vLUT.Assign(m_settings.fLUTStrength, m_settings.lutName[0] != '\0' ? 1.0f : 0.0f, fAspect, 0.0f);
	m_constantSet.Unlock();
	m_constantSet.UpdateData(pDevice);
}

void ToneMap::Cleanup(GFXDevice* pDevice)
{
	m_constantSet.Cleanup(pDevice);
	m_material.Cleanup(pDevice);
}

void ToneMap::SetDestTarget(GFXDevice* pDevice, RenderTarget* pDst)
{
	if (m_pDestTarget != pDst)
	{
		m_pDestTarget = pDst;
		m_material.SetPipelineState(pDevice->GetPipelineState(pDst->GetRenderPass(), m_decl));
	}
}

void ToneMap::SetTexture(GFXDevice* pDevice, Input eInput, const TextureHndl& texture)
{
	if (eInput == Input::Color)
	{
		m_material.SetTexture(0, texture, m_pointSampler);
		m_material.UpdateDescriptors(pDevice);
	}
}

void ToneMap::Resize(GFXDevice* pDevice, uint32 uWidth, uint32 uHeight)
{
	if (GetEnabled())
	{
		UpdateConstants(pDevice);
		m_material.UpdateDescriptors(pDevice);
	}
}

bool ToneMap::Draw(GFXContext* pContext, RenderContext& renderContext)
{
	if (!GetEnabled())
		return false;

	pContext->BeginGPUTag("ToneMap", Color::Green);
	pContext->SetRenderTarget(m_pDestTarget);
	m_material.Apply(pContext);
	m_pSys->DrawFullScreenQuad(pContext);
	pContext->EndGPUTag();

	return true;
}

}
