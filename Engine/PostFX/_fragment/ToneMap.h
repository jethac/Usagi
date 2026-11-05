/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: HDR to display: exposure, filmic tone curve (ACES fit),
//	saturation/contrast, LUT colour grading and vignette
*****************************************************************************/
#ifndef _USG_POSTFX_TONEMAP_H_
#define _USG_POSTFX_TONEMAP_H_

#include "Engine/Graphics/Effects/Effect.h"
#include "Engine/Graphics/Materials/Material.h"
#include "Engine/PostFX/PostEffect.h"
#include "Engine/PostFX/PostFXSettings.pb.h"

namespace usg {

class PostFXSys;

class ToneMap : public PostEffect
{
public:
	ToneMap();
	~ToneMap();

	virtual void Init(GFXDevice* pDevice, ResourceMgr* pResource, PostFXSys* pSys) override;
	virtual void Cleanup(GFXDevice* pDevice) override;
	virtual void SetDestTarget(GFXDevice* pDevice, RenderTarget* pDst) override;
	virtual void Resize(GFXDevice* pDevice, uint32 uWidth, uint32 uHeight) override;
	virtual bool Draw(GFXContext* pContext, RenderContext& renderContext) override;

	virtual bool RequiresHDR() const override { return true; }
	virtual bool ReadsTexture(Input eInput) const override { return eInput == Input::Color; }
	virtual bool LoadsTexture(Input eInput) const override { return false; }
	virtual void SetTexture(GFXDevice* pDevice, Input eInput, const TextureHndl& texture) override;

	void SetSettings(GFXDevice* pDevice, ResourceMgr* pResource, const ToneMapSettings& settings);

private:
	void UpdateConstants(GFXDevice* pDevice);

	PostFXSys*				m_pSys;
	RenderTarget*			m_pDestTarget;
	SamplerHndl				m_pointSampler;
	SamplerHndl				m_linearSampler;
	TextureHndl				m_lut;
	ConstantSet				m_constantSet;
	Material				m_material;
	PipelineStateDecl		m_decl;
	ToneMapSettings			m_settings;
};

}

#endif
