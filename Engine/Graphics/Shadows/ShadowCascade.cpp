/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Graphics/Device/GFXContext.h"
#include "Engine/Resource/ResourceMgr.h"
#include "Engine/Maths/MathUtil.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Graphics/Lights/DirLight.h"
#include "Engine/Scene/ShadowContext.h"
#include "Engine/Scene/SceneConstantSets.h"
#include <limits>
#include <float.h>
#include "ShadowCascade.h"

//#define CLAMP_TO_TEXEL


#define PCF_BLUR_SIZE 4 // Should get by default with linear sampling

const float FADE_BUFFER = 0.96f;

namespace usg {



static const ShaderConstantDecl g_shadowReadConstDecl[] = 
{
    SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, mCascadeMtx[0],      CT_MATRIX_44, ShadowCascade::MAX_CASCADES ),
    SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, mCascadeMtxVInv[0],   CT_MATRIX_44, ShadowCascade::MAX_CASCADES ),

    SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, vSplitDist,           CT_VECTOR_4, 1 ),
	SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, vFadeSplitDist,       CT_VECTOR_4, 1),
	SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, vInvFadeLength,       CT_VECTOR_4, 1),
    SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, vBias,                CT_VECTOR_4, 1 ),
	SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, vSampleRange,         CT_VECTOR_4, 1),
	SHADER_CONSTANT_ELEMENT(ShadowCascade::ShadowReadConstants, vInvShadowDim,        CT_VECTOR_2, 1),
    SHADER_CONSTANT_END()
};


const ShaderConstantDecl* ShadowCascade::GetDecl()
{
	return g_shadowReadConstDecl;
}

ShadowCascade::ShadowCascade()
{
	m_uGroupWidth = 0;
	m_uGroupHeight = 0;
	m_uCascadeStartIndex = 0;
	m_pRenderTarget = nullptr;
    m_fMaxShadowDist = 1000.f;
    m_fResolutionMultiplier = 1.0f;
	m_bFitReceivers = false;
	m_fPCFRadius = 3.0f;
	m_fSplitStart = 0.0f;
	m_fReceiverMinY = 0.0f;
	m_fReceiverMaxY = 0.0f;
	for (int i = 0; i < MAX_CASCADES; i++)
	{
		m_fSplitDist[i] = 0.0f;
	}

	for (int i = 0; i < MAX_CASCADES; i++)
	{
		m_pSceneContext[i] = nullptr;
	}
}


ShadowCascade::~ShadowCascade()
{
	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		ASSERT(m_pSceneContext[i] == nullptr);
	}
}

void ShadowCascade::Cleanup(GFXDevice* pDevice, Scene* pScene)
{
	m_readConstants.Cleanup(pDevice);
	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		if (m_pSceneContext[i])
		{
			pScene->DeleteShadowContext(m_pSceneContext[i]);
			m_pSceneContext[i] = nullptr;
		}
	}
}

void ShadowCascade::AssignRenderTarget(RenderTarget* pTarget, uint32 uStartIndex)
{
	m_pRenderTarget = pTarget;
	m_uGroupWidth = m_pRenderTarget->GetWidth();
	m_uGroupHeight = m_pRenderTarget->GetHeight();

    m_fResolutionMultiplier = (float)m_uGroupWidth / 2048.0f;
	const float32 fPartSize[] = { m_fMaxShadowDist * 0.04f, m_fMaxShadowDist * 0.12f, m_fMaxShadowDist * 0.4f, m_fMaxShadowDist };

	for (uint32 i = 0; i < CASCADE_COUNT; i++)
	{
		m_fPartitions0To1[i] = fPartSize[i];// * fScale;
	}

	m_fCascadePartitionsMax = fPartSize[CASCADE_COUNT - 1];
}


void ShadowCascade::Init(GFXDevice* pDevice, Scene* pScene, const DirLight* pLight)
{
	m_pLight = pLight;

	// TODO: May want an optimized shadow context
	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		m_pSceneContext[i] = pScene->CreateShadowContext(pDevice);
		m_pSceneContext[i]->Init(&m_cascadeCamera[i]);
		m_pSceneContext[i]->SetActive(true);
	}

	m_readConstants.Init(pDevice, g_shadowReadConstDecl);

}


void ShadowCascade::Update(const Camera& sceneCam)
{
    Vector4f vLookAt = sceneCam.GetPos();
    Vector4f vDirection = m_pLight->GetDirection();
    Vector4f vCameraPos = vLookAt - (vDirection*20.f);
    Vector4f vUp(1.0f, 0.0f, 0.0f, 0.0f);

    //m_shadowCamera.LookAt( vCameraPos.v3(), vLookAt.v3(), vUp.v3());
    m_shadowViewMtx.LookAt( vCameraPos.v3(), vLookAt.v3(), vUp.v3());
	// Init the data
	InitFrame(sceneCam);

    ShadowReadConstants* readData = m_readConstants.Lock<ShadowReadConstants>();

	Matrix4x4 texBias = Matrix4x4::TextureBiasMatrix();
	
    Matrix4x4 mInvView;
    sceneCam.GetViewMatrix().GetInverse(mInvView);
	
    for(uint32 i=0; i<CASCADE_COUNT; i++)
    {
        readData->mCascadeMtx[i] = m_cascadeCamera[i].GetViewMatrix() * m_cascadeCamera[i].GetProjection() * texBias;
        readData->mCascadeMtxVInv[i] = mInvView * m_cascadeCamera[i].GetViewMatrix() * m_cascadeCamera[i].GetProjection() * texBias;
    }

	float fBiasMul = 5.0f / m_fResolutionMultiplier;

    readData->vBias.Assign(-0.000002f*fBiasMul, -0.000004f*fBiasMul, -0.000007f*fBiasMul, -0.000009f*fBiasMul);
	// Coarser cascades get a smaller texel radius to keep the world-space penumbra similar
	readData->vSampleRange.Assign(m_fPCFRadius, m_fPCFRadius * 0.85f, m_fPCFRadius * 0.7f, m_fPCFRadius * 0.5f);

	float fadeDistances[MAX_CASCADES];
	float invFadeRange[MAX_CASCADES];
	for (uint32 i = 0; i < MAX_CASCADES; i++)
	{
		// Fade over the last part of each cascade's depth interval
		float fPrev = i > 0 ? m_fSplitDist[i - 1] : m_fSplitStart;
		fadeDistances[i] = m_fSplitDist[i] - (m_fSplitDist[i] - fPrev) * (1.0f - FADE_BUFFER) * 4.0f;
		invFadeRange[i] = 1.f/Math::Max(m_fSplitDist[i] - fadeDistances[i], 0.0001f);
	}
    readData->vSplitDist.Assign(m_fSplitDist[0], m_fSplitDist[1], m_fSplitDist[2], m_fSplitDist[3]);
	readData->vFadeSplitDist.Assign(fadeDistances[0], fadeDistances[1], fadeDistances[2], fadeDistances[3]);
	readData->vInvFadeLength.Assign(invFadeRange[0], invFadeRange[1], invFadeRange[2], invFadeRange[3]);
	readData->vInvShadowDim.Assign(1.f / (float)m_pRenderTarget->GetWidth(), 1.f / (float)m_pRenderTarget->GetHeight());

	m_readConstantsData = *readData;
    m_readConstants.Unlock();
	
}

void ShadowCascade::SetShadowCastFlags(uint32 uFlags)
{
    for (uint32 i = 0; i < MAX_CASCADES; i++)
    {
        if (m_pSceneContext[i])
        {
            m_pSceneContext[i]->SetShadowFlags(uFlags);
        }
    }
}

void ShadowCascade::GPUUpdate(GFXDevice* pDevice)
{
	if (!m_pRenderTarget)
		return;

	m_readConstants.UpdateData(pDevice);
}

void ShadowCascade::ComputeFrustumFromProjection( PointFrustum* pOut, const Matrix4x4* pProjection )
{
    // Frustum corners
    static Vector4f vHomogenous[6];
	vHomogenous[0].Assign( 1.0f,  0.0f, 1.0f, 1.0f );   // right (at far plane)
	vHomogenous[1].Assign( -1.0f, 0.0f, 1.0f, 1.0f );   // left
	vHomogenous[2].Assign( 0.0f,  1.0f, 1.0f, 1.0f );   // top
	vHomogenous[3].Assign( 0.0f, -1.0f, 1.0f, 1.0f );   // bottom

#if Z_RANGE_0_TO_1
    vHomogenous[4].Assign( 0.0f, 0.0f, 0.0f, 1.0f );     // near
#else
	vHomogenous[4].Assign( 0.0f, 0.0f, -1.0f, 1.0f );     // near
#endif
	vHomogenous[5].Assign( 0.0f, 0.0f, 1.0f, 1.0f );      // far

    Matrix4x4 mInverse;
    pProjection->GetInverse(mInverse);

    // Compute the frustum corners in world space.
    Vector4f vPoints[6];
    for(uint32 i = 0; i < 6; i++ )
    {
        // Transform point.
        vPoints[i] = vHomogenous[i] * mInverse;
    }

    pOut->vOrigin = Vector3f( 0.0f, 0.0f, 0.0f );
    pOut->vOrientation = Vector4f( 0.0f, 0.0f, 0.0f, 1.0f );

    const Vector4f vVecOne(1.0f, 1.0f, 1.0f, 1.0f);
    // Compute the slopes.
    vPoints[0] = vPoints[0] * (vVecOne / Vector4f(vPoints[0].z, vPoints[0].z, vPoints[0].z, vPoints[0].z) );
    vPoints[1] = vPoints[1] * (vVecOne / Vector4f(vPoints[1].z, vPoints[1].z, vPoints[1].z, vPoints[1].z) );
    vPoints[2] = vPoints[2] * (vVecOne / Vector4f(vPoints[2].z, vPoints[2].z, vPoints[2].z, vPoints[2].z) );
    vPoints[3] = vPoints[3] * (vVecOne / Vector4f(vPoints[3].z, vPoints[3].z, vPoints[3].z, vPoints[3].z) );

    pOut->fRightSlope	= vPoints[0].x;
    pOut->fLeftSlope	= vPoints[1].x;
    pOut->fTopSlope		= vPoints[2].y;
    pOut->fBottomSlope	= vPoints[3].y;

    // Compute near and far.
	vPoints[4] = vPoints[4] * vVecOne / Vector4f(vPoints[4].w, vPoints[4].w, vPoints[4].w, vPoints[4].w);
    vPoints[5] = vPoints[5] * vVecOne / Vector4f(vPoints[5].w, vPoints[5].w, vPoints[5].w, vPoints[5].w);

    pOut->fNear = vPoints[4].z;
    pOut->fFar = vPoints[5].z;
}

// View space corner of the camera frustum at a given view depth. Uses two
// points on each corner ray through the inverse projection, so it is valid for
// perspective and orthographic projections alike.
static Vector4f FrustumCornerAtDepth(const Matrix4x4& mInvProj, float fX, float fY, float fDepth)
{
	Vector4f vA = Vector4f(fX, fY, 0.0f, 1.0f) * mInvProj;
	Vector4f vB = Vector4f(fX, fY, 1.0f, 1.0f) * mInvProj;
	vA = vA / vA.w;
	vB = vB / vB.w;
	Vector4f vDir = vB - vA;
	float fT = Math::IsEqual(vDir.z, 0.0f) ? 0.0f : (fDepth - vA.z) / vDir.z;
	Vector4f vOut = vA + vDir * fT;
	vOut.w = 1.0f;
	return vOut;
}

void ShadowCascade::CreateFrustumPointsFromInterval( float fIntervalBegin, float fIntervalEnd,const Matrix4x4& mProj, Vector4f* pvPointsWorld ) 
{
	Matrix4x4 mInvProj;
	mProj.GetInverse(mInvProj);

	// Right top, left top, left bottom, right bottom; near then far
	const float fCorners[4][2] = { { 1.0f, 1.0f }, { -1.0f, 1.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f } };
	for (uint32 i = 0; i < 4; i++)
	{
		pvPointsWorld[i] = FrustumCornerAtDepth(mInvProj, fCorners[i][0], fCorners[i][1], fIntervalBegin);
		pvPointsWorld[i + 4] = FrustumCornerAtDepth(mInvProj, fCorners[i][0], fCorners[i][1], fIntervalEnd);
	}
}

void ShadowCascade::SetReceiverHeightRange(bool bEnable, float fMinY, float fMaxY)
{
	m_bFitReceivers = bEnable;
	m_fReceiverMinY = Math::Min(fMinY, fMaxY);
	m_fReceiverMaxY = Math::Max(fMinY, fMaxY);
}

// The view depth range over which the camera frustum overlaps the receiver
// slab (world Y in [min, max]). For a top-down camera this is a thin range
// around the ground, so the cascades cover just the visible table.
void ShadowCascade::GetReceiverDepthRange(const Camera& sceneCam, float& fStartOut, float& fEndOut) const
{
	Matrix4x4 mInvProj, mInvView;
	sceneCam.GetProjection().GetInverse(mInvProj);
	sceneCam.GetViewMatrix().GetInverse(mInvView);

	float fMin = FLT_MAX;
	float fMax = -FLT_MAX;
	const float fCorners[5][2] = { { 1.0f, 1.0f }, { -1.0f, 1.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 0.0f, 0.0f } };
	for (uint32 i = 0; i < 5; i++)
	{
		// Corner ray in world space
		Vector4f vA = FrustumCornerAtDepth(mInvProj, fCorners[i][0], fCorners[i][1], sceneCam.GetNear()) * mInvView;
		Vector4f vB = FrustumCornerAtDepth(mInvProj, fCorners[i][0], fCorners[i][1], m_fMaxShadowDist) * mInvView;
		float fDepthA = sceneCam.GetNear();
		float fDepthB = m_fMaxShadowDist;
		float fDY = vB.y - vA.y;
		float fT0 = 0.0f, fT1 = 1.0f;
		if (Math::IsEqual(fDY, 0.0f))
		{
			if (vA.y < m_fReceiverMinY || vA.y > m_fReceiverMaxY)
				continue;
		}
		else
		{
			float fTa = (m_fReceiverMinY - vA.y) / fDY;
			float fTb = (m_fReceiverMaxY - vA.y) / fDY;
			fT0 = Math::Max(fT0, Math::Min(fTa, fTb));
			fT1 = Math::Min(fT1, Math::Max(fTa, fTb));
			if (fT0 > fT1)
				continue;
		}
		fMin = Math::Min(fMin, fDepthA + (fDepthB - fDepthA) * fT0);
		fMax = Math::Max(fMax, fDepthA + (fDepthB - fDepthA) * fT1);
	}

	if (fMin > fMax)
	{
		// The slab isn't visible; fall back to the full range
		fMin = sceneCam.GetNear();
		fMax = m_fMaxShadowDist;
	}
	fStartOut = Math::Max(fMin, sceneCam.GetNear());
	fEndOut = Math::Max(fMax, fStartOut + 0.01f);
}

void ShadowCascade::InitFrame(const Camera& sceneCam)
{
    const Matrix4x4& mCameraProj = sceneCam.GetProjection();
	const Matrix4x4& mCameraView = sceneCam.GetViewMatrix();

    const Matrix4x4& mLightView = m_shadowViewMtx;//m_shadowCamera.GetViewMatrix();
	Matrix4x4 mInvCameraView;
	mCameraView.GetInverse(mInvCameraView);

	AABB worldBounds = m_pSceneContext[0]->GetScene()->GetWorldBounds();

    Vector3f vSceneCenter = worldBounds.GetPos();
    Vector3f vSceneExtents = worldBounds.GetRadii();

    AABB::BoxCorners corners;
	worldBounds.GetCorners( corners );

    // Transform the world bounds into light space
	Vector4f vSceneAABBLightSpace[8];
    for(uint32 uCorner=0; uCorner<8; uCorner++) 
    {
        vSceneAABBLightSpace[uCorner] = Vector4f( corners.verts[uCorner], 1.0f) *  mLightView;
    }

    
    float fFrustumIntervalBegin, fFrustumIntervalEnd;
    Vector4f vLightCameraOrthoMin;  // light space frustrum aabb 
    Vector4f vLightCameraOrthoMax;
    float fRangeStart = 0.0f;
    float fRangeEnd = m_fMaxShadowDist;
    if (m_bFitReceivers)
    {
        GetReceiverDepthRange(sceneCam, fRangeStart, fRangeEnd);
    }
    float fCameraNearFarRange = fRangeEnd - fRangeStart;
    m_fSplitStart = fRangeStart;

	Vector4f vWorldUnitsPerTexel = Vector4f(0.0f, 0.0f, 0.0f, 0.0f); 

    // Calculate the orthographic projection matrix for each cascade
    for( uint32 uCascade=0; uCascade < CASCADE_COUNT; ++uCascade ) 
    {
		if (uCascade == 0)
		{
			fFrustumIntervalBegin = 0.0f;
		}
		else
		{
			// Overlap the previous cascade's fade band
			fFrustumIntervalBegin = m_fPartitions0To1[uCascade - 1] - (m_fPartitions0To1[uCascade - 1] - (uCascade > 1 ? m_fPartitions0To1[uCascade - 2] : 0.0f)) * (1.0f - FADE_BUFFER) * 4.0f;
		}


        // Scale the intervals between 0 and 1. They are now percentages that we can scale with.
        fFrustumIntervalEnd		= m_fPartitions0To1[ uCascade ] / m_fCascadePartitionsMax;        
        fFrustumIntervalBegin 	= fFrustumIntervalBegin / m_fCascadePartitionsMax;
        fFrustumIntervalBegin	= fRangeStart + fFrustumIntervalBegin * fCameraNearFarRange;
        fFrustumIntervalEnd		= fRangeStart + fFrustumIntervalEnd * fCameraNearFarRange;
        m_fSplitDist[uCascade]	= fFrustumIntervalEnd;
        Vector4f vFrustumPoints[8];

        // Get the 8 points for the frustum of this particular cascade interval
        CreateFrustumPointsFromInterval( fFrustumIntervalBegin, fFrustumIntervalEnd, 
            mCameraProj, vFrustumPoints );

        vLightCameraOrthoMin.Assign(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
        vLightCameraOrthoMax.Assign(FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN);

        Vector4f vTmpCornerPoint;
        // This next section of code calculates the min and max values for the orthographic projection.
        for( int icpIndex=0; icpIndex < 8; ++icpIndex ) 
        {
            // Transform the frustum from camera view space to world space.
            vFrustumPoints[icpIndex] = vFrustumPoints[icpIndex] * mInvCameraView;
            // Transform the point from world space to Light Camera Space.
            vTmpCornerPoint = vFrustumPoints[icpIndex] * mLightView;
            // Find the closest point.
            vLightCameraOrthoMin = Vec4fMin( vTmpCornerPoint, vLightCameraOrthoMin );
            vLightCameraOrthoMax = Vec4fMax( vTmpCornerPoint, vLightCameraOrthoMax );
        }

        
        // Adjust to the blur size
        float fScaleDuetoBlureAMT = ( (float)( PCF_BLUR_SIZE * 2 + 1 ) 
            /(float)m_uGroupWidth );
        Vector4f vScaleDuetoBlureAMT(fScaleDuetoBlureAMT, fScaleDuetoBlureAMT, 0.0f, 0.0f);

        
        float fNormalizeByBufferSize = ( 1.0f / (float)m_uGroupWidth );
        Vector4f vNormalizeByBufferSize( fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f );

        Vector4f vBoarderOffset = vLightCameraOrthoMax - vLightCameraOrthoMin;
      //  vBoarderOffset *= 0.5f;
        vBoarderOffset *= vScaleDuetoBlureAMT;
        vLightCameraOrthoMax += vBoarderOffset;
        vLightCameraOrthoMin -= vBoarderOffset;
       
        // Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the 
        // camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
        vWorldUnitsPerTexel = vLightCameraOrthoMax - vLightCameraOrthoMin;
        vWorldUnitsPerTexel *= vNormalizeByBufferSize;

#ifdef CLAMP_TO_TEXEL
        // We snape the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
        // This is a matter of integer dividing by the world space size of a texel
        vLightCameraOrthoMin /= vWorldUnitsPerTexel;
        vLightCameraOrthoMin = vLightCameraOrthoMin.GetFloor();
        vLightCameraOrthoMin *= vWorldUnitsPerTexel;
            
        vLightCameraOrthoMax /= vWorldUnitsPerTexel;
        vLightCameraOrthoMax = vLightCameraOrthoMax.GetFloor();
        vLightCameraOrthoMax *= vWorldUnitsPerTexel;
#endif
 
     
		float fNearPlane = 0.0f;
		float fFarPlane = 10000.0f;

		// FIXME: Need better bounds calculation
		Vector4f vLightSpaceSceneAABBMin(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
		Vector4f vLightSpaceSceneAABBMax(FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN);
	 
		// Get the min and max values for the AABB in light space
		for(uint32 uCorner=0; uCorner< 8; ++uCorner)
		{
			vLightSpaceSceneAABBMin = Vec4fMin( vSceneAABBLightSpace[uCorner], vLightSpaceSceneAABBMin );
			vLightSpaceSceneAABBMax = Vec4fMax( vSceneAABBLightSpace[uCorner], vLightSpaceSceneAABBMax );
		}

		// Use the min and max Z values as the near and far places
		fNearPlane = vLightSpaceSceneAABBMin.z;
		fFarPlane = vLightSpaceSceneAABBMax.z;


        // Create the orthographic projection for this cascade.
        m_matShadowProj[ uCascade ].Orthographic( vLightCameraOrthoMin.x, vLightCameraOrthoMax.x, vLightCameraOrthoMin.y, vLightCameraOrthoMax.y, fNearPlane, fFarPlane );

        m_fCascadePartitionsFrustum[ uCascade ] = fFrustumIntervalEnd;

        m_cascadeCamera[uCascade].SetUp( m_shadowViewMtx, m_matShadowProj[uCascade] );
    }
 }

void ShadowCascade::CreateShadowTex(GFXContext* pContext)
{
	if (!m_pRenderTarget)
		return;

    pContext->BeginGPUTag("ShadowCascade", Color::Grey);


	for (uint32 i = 0; i < CASCADE_COUNT; ++i)
    {
        pContext->SetRenderTargetLayer(m_pRenderTarget, m_uCascadeStartIndex + i);
		DrawSceneFromLight(pContext, m_pSceneContext[i]);
        pContext->SetRenderTarget(NULL);
	}

    pContext->EndGPUTag();
}


void ShadowCascade::Finished(GFXContext* pContext)
{

}


void ShadowCascade::DrawSceneFromLight(GFXContext* pGFXCtxt, ShadowContext* pShadowCtxt)
{
	pShadowCtxt->DrawScene(pGFXCtxt);
    
}


}
