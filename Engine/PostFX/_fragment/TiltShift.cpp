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
#include "TiltShift.h"

namespace usg {

struct TiltShiftConstants
{
	Vector4f	vFocus;		// centre, half height, 1 / transition, max radius (uv)
	Vector4f	vPixel;		// 1/width, 1/height, aspect, unused
};

static const ShaderConstantDecl g_tiltShiftConstantDef[] =
{
	SHADER_CONSTANT_ELEMENT(TiltShiftConstants, vFocus,	CT_VECTOR_4, 1),
	SHADER_CONSTANT_ELEMENT(TiltShiftConstants, vPixel,	CT_VECTOR_4, 1),
	SHADER_CONSTANT_END()
};

static const DescriptorDeclaration g_tiltShiftDescriptorDecl[] =
{
	DESCRIPTOR_ELEMENT(0,						 DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, SHADER_FLAG_PIXEL),
	DESCRIPTOR_ELEMENT(SHADER_CONSTANT_MATERIAL, DESCRIPTOR_TYPE_CONSTANT_BUFFER, 1, SHADER_FLAG_PIXEL),
	DESCRIPTOR_END()
};


TiltShift::TiltShift() :
	PostEffect()
{
	SetRenderMask(RENDER_MASK_POST_EFFECT);
	SetLayer(LAYER_POST_PROCESS);
	// After bloom (20), before tone mapping (25)
	SetPriority(22);
	m_pSys = nullptr;
	m_pDestTarget = nullptr;
}

TiltShift::~TiltShift()
{
}

void TiltShift::Init(GFXDevice* pDevice, ResourceMgr* pResource, PostFXSys* pSys)
{
	m_pSys = pSys;

	m_decl.inputBindings[0].Init(GetVertexDeclaration(VT_POSITION));
	m_decl.uInputBindingCount = 1;
	m_decl.ePrimType = PT_TRIANGLES;
	m_decl.pEffect = pResource->GetEffect(pDevice, "PostProcess.TiltShift");

	DescriptorSetLayoutHndl matDescriptors = pDevice->GetDescriptorSetLayout(g_tiltShiftDescriptorDecl);
	m_decl.layout.descriptorSets[0] = pDevice->GetDescriptorSetLayout(SceneConsts::g_globalDescriptorDecl);
	m_decl.layout.descriptorSets[1] = matDescriptors;
	m_decl.layout.uDescriptorSetCount = 2;
	m_decl.rasterizerState.eCullFace = CULL_FACE_NONE;
	m_decl.alphaState.SetColor0Only();

	m_pointSampler = pDevice->GetSampler(SamplerDecl(SAMP_FILTER_POINT, SAMP_WRAP_CLAMP));
	m_linearSampler = pDevice->GetSampler(SamplerDecl(SAMP_FILTER_LINEAR, SAMP_WRAP_CLAMP));

	m_material.SetDescriptorLayout(pDevice, matDescriptors);
	m_constantSet.Init(pDevice, g_tiltShiftConstantDef);
	m_material.SetConstantSet(SHADER_CONSTANT_MATERIAL, &m_constantSet);

	TiltShiftSettings_init(&m_settings);
	SetSettings(pDevice, m_settings);
}

void TiltShift::SetSettings(GFXDevice* pDevice, const TiltShiftSettings& settings)
{
	m_settings = settings;
	UpdateConstants(pDevice);
}

void TiltShift::UpdateConstants(GFXDevice* pDevice)
{
	const float fWidth = (float)Math::Max(m_pSys->GetFinalTargetWidth(), 1U);
	const float fHeight = (float)Math::Max(m_pSys->GetFinalTargetHeight(), 1U);
	TiltShiftConstants* pConsts = m_constantSet.Lock<TiltShiftConstants>();
	pConsts->vFocus.Assign(m_settings.fFocusCenter, m_settings.fFocusHalfHeight, 1.0f / Math::Max(m_settings.fTransition, 0.001f),
		m_settings.fMaxRadius / 1080.0f);
	pConsts->vPixel.Assign(1.0f / fWidth, 1.0f / fHeight, fWidth / fHeight, 0.0f);
	m_constantSet.Unlock();
	m_constantSet.UpdateData(pDevice);
}

void TiltShift::Cleanup(GFXDevice* pDevice)
{
	m_constantSet.Cleanup(pDevice);
	m_material.Cleanup(pDevice);
}

void TiltShift::SetDestTarget(GFXDevice* pDevice, RenderTarget* pDst)
{
	if (m_pDestTarget != pDst)
	{
		m_pDestTarget = pDst;
		m_material.SetPipelineState(pDevice->GetPipelineState(pDst->GetRenderPass(), m_decl));
	}
}

void TiltShift::SetTexture(GFXDevice* pDevice, Input eInput, const TextureHndl& texture)
{
	if (eInput == Input::Color)
	{
		m_material.SetTexture(0, texture, m_linearSampler);
		m_material.UpdateDescriptors(pDevice);
	}
}

void TiltShift::Resize(GFXDevice* pDevice, uint32 uWidth, uint32 uHeight)
{
	if (GetEnabled())
	{
		UpdateConstants(pDevice);
		m_material.UpdateDescriptors(pDevice);
	}
}

bool TiltShift::Draw(GFXContext* pContext, RenderContext& renderContext)
{
	if (!GetEnabled())
		return false;

	pContext->BeginGPUTag("TiltShift", Color::Green);
	pContext->SetRenderTarget(m_pDestTarget);
	m_material.Apply(pContext);
	m_pSys->DrawFullScreenQuad(pContext);
	pContext->EndGPUTag();

	return true;
}

}
