/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A cascade shadow system, the scene is split into various layers
*****************************************************************************/
#ifndef _USG_POSTFX_SHADOWS_SHADOW_CASCADE_H_
#define _USG_POSTFX_SHADOWS_SHADOW_CASCADE_H_

#include "Engine/Scene/Camera/StandardCamera.h"
#include "Engine/Scene/SceneContext.h"
#include "Engine/Graphics/Shadows/DirectionalShadow.h"

namespace usg
{

class ShadowContext;
class GFXContext;
class DirLight;
class RenderTarget;

#define RENDER_TO_LAYER 1

class ShadowCascade : public DirectionalShadow
{
public:
	ShadowCascade();
	virtual ~ShadowCascade();

	virtual void Init(GFXDevice* pDevice, Scene* pScene, const DirLight* pLight);
	void AssignRenderTarget(RenderTarget* pTarget, uint32 uStartIndex);
	void Cleanup(GFXDevice* pDevice, Scene* pScene);
	virtual void Update(const Camera &sceneCam);
	void GPUUpdate(GFXDevice* pDevice);
	virtual void CreateShadowTex(GFXContext* pContext);
	virtual void Finished(GFXContext* pContext);

	void SetShadowCastFlags(uint32 uFlags);
	// Fit the cascades to where the camera sees world Y in [fMinY, fMaxY]
	// (the ground and what stands on it) rather than to a fixed view distance.
	// Keeps top-down and orthographic views at full shadow resolution.
	void SetReceiverHeightRange(bool bEnable, float fMinY, float fMaxY);
	void SetMaxShadowDistance(float fDist) { m_fMaxShadowDist = fDist; }
	void SetPCFRadius(float fTexels) { m_fPCFRadius = fTexels; }

	const ConstantSet* GetShadowReadConstants() const { return &m_readConstants; }
	ShadowContext* GetContext(uint32 uContext) { return m_pSceneContext[uContext]; }

	enum
	{
		CASCADE_COUNT = 4,	// MAX is 4
		MAX_CASCADES = 4
	};

	struct ShadowReadConstants
	{
		Matrix4x4   mCascadeMtx[ShadowCascade::MAX_CASCADES];
		Matrix4x4   mCascadeMtxVInv[ShadowCascade::MAX_CASCADES];
		Vector4f    vSplitDist;
		Vector4f	vFadeSplitDist;	// We give the next cascade a little extra to allow us to fade between
		Vector4f	vInvFadeLength;
		Vector4f    vBias;
		Vector4f    vSampleRange;
		Vector2f    vInvShadowDim;
	};

	const ShadowReadConstants& GetShadowReadConstantData() const { return m_readConstantsData; }
	static const ShaderConstantDecl* GetDecl();

private:

	struct PointFrustum
	{
		Vector3f vOrigin;
	    Vector4f vOrientation;

	    float fRightSlope;
	    float fLeftSlope;
	    float fTopSlope;
	    float fBottomSlope;
	    float fNear, fFar;
	};

	void ComputeFrustumFromProjection(PointFrustum* pOut, const Matrix4x4* pProjection );
	void CreateFrustumPointsFromInterval(float fIntervalBegin, float fIntervalEnd,const Matrix4x4& mViewProj, Vector4f* pvPointsWorld);
	void DrawSceneFromLight(GFXContext* pGFXCtxt, ShadowContext* pShadowCtxt);
	void InitFrame(const Camera& sceneCam);
	void GetReceiverDepthRange(const Camera& sceneCam, float& fStartOut, float& fEndOut) const;

	uint32					m_uGroupWidth;
	uint32					m_uGroupHeight;

	Matrix4x4				m_lightView;
	Matrix4x4				m_lightProj;

	float					m_fPartitions0To1[MAX_CASCADES];
	float					m_fCascadePartitionsFrustum[MAX_CASCADES];
	float					m_fCascadePartitionsMax;
	float					m_fMaxShadowDist;
	float					m_fResolutionMultiplier;
	float					m_fSplitDist[MAX_CASCADES];	// View depth where each cascade ends, this frame
	float					m_fSplitStart;		// View depth where the first cascade starts
	bool					m_bFitReceivers;
	float					m_fPCFRadius;
	float					m_fReceiverMinY;
	float					m_fReceiverMaxY;

	// One frustum for each cascade
	PointFrustum			m_frustum[MAX_CASCADES];
	Matrix4x4				m_matShadowProj[MAX_CASCADES];

	const DirLight*			m_pLight;
	ConstantSet				m_readConstants;
	ShadowReadConstants		m_readConstantsData;

	Matrix4x4				m_shadowViewMtx;
	StandardCamera			m_cascadeCamera[MAX_CASCADES];
	ShadowContext*			m_pSceneContext[MAX_CASCADES];

	class RenderTarget*		m_pRenderTarget;
	uint32					m_uCascadeStartIndex;
};

}


#endif
