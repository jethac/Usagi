/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A context for shadow depth rendering
*****************************************************************************/
#pragma once

#include "Engine/Memory/FastPool.h"
#include "Engine/Scene/SceneContext.h"
#include "Engine/Scene/SceneSearchObject.h"
#include "Engine/Core/stl/hash_map.h"
#include "Engine/Core/stl/vector.h"
#include "Engine/Graphics/Device/DescriptorSet.h"

namespace usg{

class Scene;
class Camera;
class PostFXSys;

class ShadowContextBase : public SceneContext
{
public:
	typedef SceneContext Inherited;

	virtual void Cleanup(GFXDevice* pDevice);
	virtual void ClearLists();
	void CacheDirtyInfo();
	bool IsDirty() const;

protected:
	virtual usg::Matrix4x4 GetLightMat() const = 0;

	void ReplaceInstancedNodes(usg::GFXDevice* pDevice);

	struct ComparisonData
	{
		memsize			CmpNode;	// TODO: Create Ids
		usg::Matrix4x4	CmpLoc;
	};

	// Vectors kept across frames: rebuilding them allocates nothing once warm
	vector<RenderNode*>		m_drawList;
	vector<RenderNode*>		m_cmpList;
	vector<RenderNode*>		m_scratchList;
	vector<ComparisonData>	m_prevData;
	hash_map<uint64, InstancedRenderer*> m_instanceRenders;

};

}


