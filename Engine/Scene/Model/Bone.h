/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: An bone used within model skeletons
*****************************************************************************/
#pragma once

#ifndef USG_GRAPHICS_SCENE_BONE_H
#define USG_GRAPHICS_SCENE_BONE_H

#include "Engine/Maths/Matrix4x4.h"
#include "Engine/Scene/TransformNode.h"
#include "Engine/Graphics/Effects/ConstantSet.h"
#include "Engine/Resource/SkeletonResource.h"
#include "Engine/Scene/SceneConstantSets.h"

namespace usg {

class Model;
class ModelResource;

class Bone
{
public:
	Bone();
	~Bone();

	void Init(GFXDevice* pDevice, Bone* pParent, const SkeletonResource::Bone* pBone);
	void Cleanup(GFXDevice* pDevice);
	void SetTransform(const Matrix4x4& mat, bool bUpdateConsts = true)
	{
		m_pTransformNode->SetMatrix(mat);
		if (bUpdateConsts /*&& m_pRenderGroup*/)
		{
			// Static props hand in the same matrix every frame: only re-upload
			// the constant when it changed
			const SceneConsts::ModelTransform* pCurrent = (const SceneConsts::ModelTransform*)m_boneConstants.GetCPUData();
			const Matrix4x3 mNew = mat;
			if (!m_bConstsSet || memcmp(&pCurrent->mModelMat, &mNew, sizeof(mNew)) != 0)
			{
				// FIXME: Only do for referenced base nodes
				SceneConsts::ModelTransform* pTransform = m_boneConstants.Lock<SceneConsts::ModelTransform>();
				pTransform->mModelMat = mNew;
				m_boneConstants.Unlock();
				m_bConstsSet = true;
			}
		}
	}

	void UpdateScaleBounds(float fScale);

	// Set when a per-bone entity (BoneComponent) drives this bone's transform, so
	// the owning Model knows not to overwrite the bone constant from its own world
	// matrix. Rigid ModelComponent entities without per-bone child entities leave
	// this false, letting the Model fill the mModelMat constant directly.
	void SetExternallyDriven(bool bDriven) { m_bExternallyDriven = bDriven; }
	bool IsExternallyDriven() const { return m_bExternallyDriven; }

	void UpdateConstants(GFXDevice* pDevice)
	{
		// TODO: Only perform if we are visible
		m_boneConstants.UpdateData(pDevice);
	}

	// Used by a fully skinned animation
	const ConstantSet* GetConstantSet() const { return &m_boneConstants;  }
	const Matrix4x4& GetLocalTransform() const { return m_pTransformNode->GetLocalMatrix(); }

	// Note, may currently be a frame behind, may need a post transform update for objects dependent on this
	// frames matrix
	const Matrix4x4& GetWorldMatrix() const { return m_pTransformNode->GetMatrix(); }
	const Matrix4x4& GetInverseBindMatrix() const { return m_pResource->mInvBindMatrix;  }
	
	Matrix4x4 GetDefaultMatrix(bool bIncludeParents) const;

	const usg::Sphere& GetLocalColSphere() const { return m_pResource->cColSphere;  }
	void AttachRenderNode(GFXDevice* pDevice, Scene* pScene, RenderNode* pNode, uint8 uLod = 0, bool bDynamic = false);
	void RemoveRenderNode(Scene* pScene, RenderNode* pNode);
	void RemoveRenderNodes(Scene* pScene);

	void CreateTransformNode(Scene* pScene, TransformNode* pModelNode);
	void RemoveTransformNode(Scene* pScene);
	TransformNode* GetTransformNode() { return m_pTransformNode; }
	RenderGroup* GetRenderGroup() { return m_pRenderGroup; }

	bool IsValid() { return (m_pResource != NULL); }
	const SkeletonResource::Bone* GetResource() const { return m_pResource; }

private:
	PRIVATIZE_COPY(Bone)


	const SkeletonResource::Bone*	m_pResource;
	Bone*							m_pParent;
	// TODO: Really need one of these per view if billboarded
	TransformNode*					m_pTransformNode;
	ConstantSet						m_boneConstants;
	RenderGroup*					m_pRenderGroup;
	bool							m_bExternallyDriven = false;
	bool							m_bConstsSet = false;	// m_boneConstants holds a matrix
};

} // namespace usg

#endif	// USG_GRAPHICS_SCENE_BONE_H
