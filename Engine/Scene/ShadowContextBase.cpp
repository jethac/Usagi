/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Scene/Camera/Camera.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/InstancedRenderer.h"
#include "Engine/Scene/SceneConstantSets.h"
#include "Engine/Scene/ShadowContext.h"
#include "Engine/Graphics/Device/GFXContext.h"

namespace usg {

	void ShadowContextBase::Cleanup(GFXDevice* pDevice)
	{
		for (auto itr : m_instanceRenders)
		{
			itr.second->Cleanup(pDevice);
		}
	}

	void ShadowContextBase::ClearLists()
	{
		m_drawList.clear();
		Inherited::ClearLists();

		for (auto itr : m_instanceRenders)
		{
			itr.second->DrawFinished();
		}
	}

	void ShadowContextBase::CacheDirtyInfo()
	{
		usg::Matrix4x4 mLightTransform = GetLightMat();

		m_prevData.clear();
		for (auto itr : m_cmpList)
		{
			ComparisonData Cmp;
			Cmp.CmpNode = (memsize)itr;
			if (itr->GetParent()->GetTransform())
			{
				Cmp.CmpLoc = itr->GetParent()->GetTransform()->GetMatrix() * mLightTransform;
			}
			else
			{
				Cmp.CmpLoc = mLightTransform;
			}
			
			m_prevData.push_back(Cmp);
		}
	}
	 
	bool ShadowContextBase::IsDirty() const
	{
		usg::Matrix4x4 mLightTransform = GetLightMat();

		if (m_prevData.size() != m_cmpList.size())
		{
			return true;
		}

		auto prev = m_prevData.begin();
		for(auto itr : m_cmpList)
		{
			if ((memsize)itr != prev->CmpNode)
			{
				return true;
			}

			// Disabling for now due to false positives, need to exclude external items
			/*if (itr->IsAnimated())
			{
				return true;
			}*/

			usg::Matrix4x4 CmpMat = itr->GetParent()->GetTransform()->GetMatrix() * mLightTransform;
			if (!CmpMat.IsEqual(prev->CmpLoc, FLT_EPSILON * 10.f))
			{
				return true;

			}
			++prev;
		}
		return false;
	}



	void ShadowContextBase::ReplaceInstancedNodes(usg::GFXDevice* pDevice)
	{
		// Now look for instances and build a new list: nodes that don't instance
		// stay, instanced ones go to their renderer (one batch per instance id)
		InstancedRenderer* pCurrentInstanceRenderer = nullptr;
		uint64 uCurrentInstance = USG_INVALID_ID64;

		m_cmpList = m_drawList;
		m_scratchList.clear();

		for (RenderNode* pNode : m_drawList)
		{
			uint64 uInstanceId = pNode->GetInstanceId();
			if (uInstanceId == USG_INVALID_ID64)
			{
				m_scratchList.push_back(pNode);
				continue;
			}

			if (uInstanceId != uCurrentInstance)
			{
				auto itr = m_instanceRenders.find(uInstanceId);
				if (itr == m_instanceRenders.end())
				{
					pCurrentInstanceRenderer = pNode->CreateInstanceRenderer(pDevice, GetScene());
					m_instanceRenders[uInstanceId] = pCurrentInstanceRenderer;
				}
				else
				{
					pCurrentInstanceRenderer = itr->second;
				}
				uCurrentInstance = uInstanceId;
			}
			pCurrentInstanceRenderer->AddNode(pNode);
		}
		m_drawList.swap(m_scratchList);


		for (auto itr : m_instanceRenders)
		{
			RenderNode* pNode = itr.second->EndBatch();
			if (pNode)
			{
				m_drawList.push_back(pNode);
			}
		}

		for (auto itr : m_instanceRenders)
		{
			itr.second->PreDraw(pDevice);
		}
	}


}

