/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Tilt-shift "miniature" blur, a sharp horizontal band with
//	blur growing above and below it. Runs on the HDR image before tone mapping.
*****************************************************************************/
#ifndef _USG_POSTFX_TILTSHIFT_H_
#define _USG_POSTFX_TILTSHIFT_H_

#include "Engine/Graphics/Effects/Effect.h"
#include "Engine/Graphics/Materials/Material.h"
#include "Engine/PostFX/PostEffect.h"
#include "Engine/PostFX/PostFXSettings.pb.h"

namespace usg {

class PostFXSys;

class TiltShift : public PostEffect
{
public:
	TiltShift();
	~TiltShift();

	virtual void Init(GFXDevice* pDevice, ResourceMgr* pResource, PostFXSys* pSys) override;
	virtual void Cleanup(GFXDevice* pDevice) override;
	virtual void SetDestTarget(GFXDevice* pDevice, RenderTarget* pDst) override;
	virtual void Resize(GFXDevice* pDevice, uint32 uWidth, uint32 uHeight) override;
	virtual bool Draw(GFXContext* pContext, RenderContext& renderContext) override;

	virtual bool RequiresHDR() const override { return true; }
	virtual bool ReadsTexture(Input eInput) const override { return eInput == Input::Color; }
	virtual bool LoadsTexture(Input eInput) const override { return false; }
	virtual void SetTexture(GFXDevice* pDevice, Input eInput, const TextureHndl& texture) override;

	void SetSettings(GFXDevice* pDevice, const TiltShiftSettings& settings);

private:
	void UpdateConstants(GFXDevice* pDevice);

	PostFXSys*				m_pSys;
	RenderTarget*			m_pDestTarget;
	SamplerHndl				m_pointSampler;
	SamplerHndl				m_linearSampler;
	ConstantSet				m_constantSet;
	Material				m_material;
	PipelineStateDecl		m_decl;
	TiltShiftSettings		m_settings;
};

}

#endif
