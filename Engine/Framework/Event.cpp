#pragma once
#include "Engine/Common/Common.h"
#include "Engine/Network/Network.pb.h"
#include "Event.h"
#include "Engine/Framework/SystemCoordinator.h"

namespace usg
{

	struct OnEventSignalBase::OnEventClosure : public SignalClosure
	{
		const void* pEventData;
		void(*pEventFunction)(const GenericInputOutputs&, GenericInputOutputs&, const uint8& evt);
		OnEventClosure(const void* pEventData, void* userData) : pEventData(pEventData), pEventFunction(reinterpret_cast<decltype(pEventFunction)>(userData)) {}
		virtual void operator()(const Entity e, const void* in, void* out)
		{
			pEventFunction(*(const GenericInputOutputs*)in, *(GenericInputOutputs*)out, *(const uint8*)pEventData);
		}
	};

	void OnEventSignalBase::Trigger(Entity e, void* signal, const uint32 uSystemId, uint32 targets, void* userData)
	{
		OnEventSignalBase* pSignalBase = (OnEventSignalBase*)signal;
		OnEventClosure closure(pSignalBase->pEventData, userData);
		TriggerSignalOnEntity(e, closure, uSystemId, targets);
	}

	void OnEventSignalBase::TriggerFromRoot(Entity e, void* signal, const uint32 uSystemId, void* userData)
	{
		OnEventSignalBase* pSignalBase = (OnEventSignalBase*)signal;
		OnEventClosure closure(pSignalBase->pEventData, userData);
		TriggerSignalFromRoot(ComponentSystemInputOutputsSharedBase::GetRootSystem(uSystemId), closure);
	}

	Entity TriggerableEvent::GetEntityFromNetworkUID(sint64 uid)
	{
		for (GameComponents<NetworkUID>::Iterator it = GameComponents<NetworkUID>::GetIterator(); !it.IsEnd(); ++it)
		{
			if ((*it)->GetData().gameUID == uid)
				return (*it)->GetEntity();
		}

		return nullptr;
	}

	void EventOnNetworkEntityBase::Trigger(SystemCoordinator& sc, Entity root)
	{
		Entity e = GetEntityFromNetworkUID(iNUID);
		if (e != nullptr)
		{
			OnEventSignalBase sig(uSignalId, pData);
			sc.Trigger(e, sig, uTargets);
		}
		else
		{
			DEBUG_PRINT("WARNING: Network event for 0x%016llx dropped -- NULL entity\n", iNUID);
		}
	}

	void EventBase::Trigger(SystemCoordinator& sc, Entity root)
	{
		OnEventSignalBase sig(uSignalId, pData);
		sc.TriggerFromRoot(root, sig);
	}

	EventOnEntityBase::EventOnEntityBase(const uint32 uSignalId, const double fTime, const void* pData, Entity e, uint32 uTargets)
		: TriggerableEvent(fTime)
		, pData(pData)
		, uSignalId(uSignalId)
		, e(e)
		, entityHandle(e ? e->GetStableID() : EntityHandle())
		, uTargets(uTargets)
	{
	}

	EventOnEntityBase::EventOnEntityBase(const uint32 uSignalId, const double fTime, const void* pData, EntityHandle e, uint32 uTargets)
		: TriggerableEvent(fTime)
		, pData(pData)
		, uSignalId(uSignalId)
		, e(nullptr)
		, entityHandle(e)
		, uTargets(uTargets)
	{
	}

	Entity EventOnEntityBase::GetEntity()
	{
		if (entityHandle.IsValid())
		{
			return ComponentEntity::GetEntityFromStableID(entityHandle);
		}

		return e;
	}

	bool EventOnEntityBase::TargetsEntity(Entity entity)
	{
		if (!entity)
		{
			return false;
		}

		return e == entity || (entityHandle.IsValid() && entity->GetStableID() == entityHandle);
	}

	void EventOnEntityBase::Trigger(SystemCoordinator& sc, Entity root)
	{
		Entity entity = GetEntity();
		if (!entity)
		{
			DEBUG_PRINT("WARNING: Entity event 0x%08x dropped -- stale entity handle\n", uSignalId);
			return;
		}

		OnEventSignalBase sig(uSignalId, pData);
		sc.Trigger(entity, sig, uTargets);
	}


}
