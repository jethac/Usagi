/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/

#ifndef USAGI_FRAMEWORK_SCRIPTS_LUA_EVENT_TRIGGERS_H_
#define USAGI_FRAMEWORK_SCRIPTS_LUA_EVENT_TRIGGERS_H_

#include "Engine/Framework/EventManager.h"
#include "Engine/Framework/Event.h"
#include <lua.hpp>

namespace usg {

template<>
struct Event<LuaEvent> : public TriggerableEvent
{
	typedef OnEventSignal<LuaEvent> Signal;
	typedef lua_State* ExtraData;
	LuaEvent evt;
	ExtraData L;
	Event(const LuaEvent& _evt, double _time, ExtraData _extra) : TriggerableEvent(_time), evt(_evt), L(_extra) {}

	virtual void Trigger(SystemCoordinator& sc, Entity root)
	{
		OnEventSignal<LuaEvent> sig(evt);
		sc.TriggerFromRoot( root, sig );

		int result = lua_getglobal(L, "EventsQueue");
		ASSERT(result != LUA_TNIL);
		ASSERT(lua_istable(L, -1));
		luaL_unref(L, -1, evt.eventIndex);
		lua_pop(L, 1);
	}
};

template<>
struct EventOnEntity<LuaEvent> : public TriggerableEvent
{
	typedef OnEventSignal<LuaEvent> Signal;
	typedef lua_State* ExtraData;
	Entity e;
	EntityHandle entityHandle;
	LuaEvent evt;
	uint32 targets;
	ExtraData L;
	EventOnEntity(Entity _e, const LuaEvent& _evt, uint32 _targets, double _time, ExtraData _extra) : TriggerableEvent(_time), e(_e), entityHandle(_e ? _e->GetStableID() : EntityHandle()), evt(_evt), targets(_targets), L(_extra) {}
	EventOnEntity(EntityHandle _e, const LuaEvent& _evt, uint32 _targets, double _time, ExtraData _extra) : TriggerableEvent(_time), e(nullptr), entityHandle(_e), evt(_evt), targets(_targets), L(_extra) {}

	Entity GetEntity() override
	{
		return entityHandle.IsValid() ? ComponentEntity::GetEntityFromStableID(entityHandle) : e;
	}

	bool HasEntityTarget() const override
	{
		return true;
	}

	bool TargetsEntity(Entity entity) override
	{
		if (!entity)
		{
			return false;
		}

		return e == entity || (entityHandle.IsValid() && entity->GetStableID() == entityHandle);
	}

	virtual void Trigger(SystemCoordinator& sc, Entity root)
	{
		Entity entity = GetEntity();
		if (!entity)
		{
			DEBUG_PRINT("WARNING: Lua entity event dropped -- stale entity handle\n");
			return;
		}

		OnEventSignal<LuaEvent> sig(evt);
		sc.Trigger(entity, sig, targets);

		int result = lua_getglobal(L, "EventsQueue");
		ASSERT(result != LUA_TNIL);
		ASSERT(lua_istable(L, -1));
		luaL_unref(L, -1, evt.eventIndex);
		lua_pop(L, 1);
	}
};

}

#endif //USAGI_FRAMEWORK_SCRIPTS_LUA_EVENT_TRIGGERS_H_
