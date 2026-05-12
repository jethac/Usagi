/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Component.h"
#include <atomic>

using namespace usg;

ComponentType* ComponentType::GetNextComponent() const
{
	return m_pNextComponent;
}

void ComponentType::SetNextComponent(ComponentType* pComp)
{
	ASSERT(pComp != m_pPrevComponent || (pComp == NULL && m_pPrevComponent == NULL));
	m_pNextComponent = pComp;
}

ComponentType* ComponentType::GetPrevComponent() const
{
	return m_pPrevComponent;
}

void ComponentType::SetPrevComponent(ComponentType* pComp)
{
	ASSERT(pComp != m_pNextComponent || (pComp == NULL && m_pNextComponent == NULL));
	m_pPrevComponent = pComp;
}

uint32 ComponentType::GetNextTypeID()
{
	static std::atomic<uint32> s_uNextTypeID(0);
	return s_uNextTypeID.fetch_add(1, std::memory_order_relaxed);
}

void ComponentType::RequestFree()
{
	if (m_uEntity && !m_bFreeRequested.exchange(true, std::memory_order_acq_rel))
	{
		if (ComponentEntity::IsSystemExecutionActive())
		{
			ComponentEntity::QueueDeferredComponentFree(m_uEntity->GetStableID(), GetTypeID());
		}
		else
		{
			m_uEntity->SetComponentPendingDelete();
		}
	}
}
