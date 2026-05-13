/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#ifndef _COMPONENT_ENTITY_H
#define _COMPONENT_ENTITY_H

#include "Engine/Core/ProtocolBuffers/ProtocolBufferFields.h"
#include "Engine/Core/String/StringCRC.h"
#include "Engine/Core/Containers/StringPointerHash.h"
#include "Engine/Framework/ComponentLoadHandles.h"
#include "Engine/Memory/FastPool.h"
#include "HierarchyNode.h"
#include "SystemKey.h"
#include <atomic>

namespace usg
{

	struct GenericInputOutputs;
	class ComponentEntity;
	class NewEntities;
	struct ComponentLoadHandles;
	struct UnsafeComponentGetter;

	class ComponentType;
	typedef ComponentEntity* Entity;

	struct EntityHandle
	{
		EntityHandle() : uIndex(0), uGeneration(0) {}
		EntityHandle(uint32 uIndexIn, uint32 uGenerationIn) : uIndex(uIndexIn), uGeneration(uGenerationIn) {}

		bool IsValid() const { return uIndex != 0 && uGeneration != 0; }
		bool operator==(const EntityHandle& rhs) const { return uIndex == rhs.uIndex && uGeneration == rhs.uGeneration; }
		bool operator!=(const EntityHandle& rhs) const { return !(*this == rhs); }

		uint32 uIndex;
		uint32 uGeneration;
	};

	class ComponentEntity : public HierearchyNode<ComponentEntity>
	{
		friend class ComponentManager;
	public:
		ComponentEntity();
		~ComponentEntity();

		void SetComponent(uint32 uCompIndex, ComponentType *pComp);

		ComponentType* GetComponent(uint32 uCompIndex) const
		{
			return (m_uComponentBitfield[uCompIndex / BITFIELD_LENGTH] & (1 << (uCompIndex % BITFIELD_LENGTH))) ? m_pComponents.Get(GetComponentIDFromIndex(uCompIndex)) : NULL;
		}

		bool HasComponent(uint32 uCompIndex) const
		{
			return (m_uComponentBitfield[uCompIndex / BITFIELD_LENGTH] & (1 << (uCompIndex % BITFIELD_LENGTH))) != 0;
		}

		// Creates an entity. It will not be added to global hierarchy until next frame begins. You can still send events to the new entity using the pointer returned.
		static ComponentEntity* Create(ComponentEntity* parent);

		static void Free(ComponentEntity* entity, ComponentLoadHandles& handles);
		void Deactivate();
		void Activate();

		static uint32 NextSystemType();
		void SetParent(ComponentEntity* pParent);
		void SetSystem(uint32 uSysIndex, GenericInputOutputs *pSys);
		GenericInputOutputs* GetSystem(uint32 uSysIndex) const;
		bool HasSystem(uint32 uSysIndex) const;

		static void BeginSystemExecution();
		static void EndSystemExecution();
		static bool IsSystemExecutionActive();
		static void AssertStructureMutationAllowed();
		static void QueueDeferredComponentFree(EntityHandle entity, uint32 uComponentTypeId);
		static void QueueDeferredChanged(EntityHandle entity);
		static void FlushDeferredStructureChanges();

		class SystemExecutionScope
		{
		public:
			SystemExecutionScope(bool bActive = true)
				: m_bActive(bActive)
			{
				if (m_bActive)
				{
					BeginSystemExecution();
				}
			}

			~SystemExecutionScope()
			{
				if (m_bActive)
				{
					EndSystemExecution();
				}
			}

		private:
			bool m_bActive;
		};

		bool IsActive() const { return m_bActive; }

		bool HasChanged() const { return m_bChanged; }
		void ClearChanged() { m_bChanged = false; }
		void SetChanged();
		void SetComponentPendingDelete();
		bool HasPendingDeletions() { return m_bPendingDeletions; }
		void HandlePendingDeletes(ComponentLoadHandles& handles);

		bool HaveChildrenChanged() { return m_bChildrenChanged; }
		void ClearChildrenChanged() { m_bChildrenChanged = false; }
		void SetChildrenChanged();

		static ComponentEntity* GetRoot() { return g_hierarchy; }

		float GetCatchupTime() const { return m_fCatchupTime; }
		void SetCatchupTime(float t) { m_fCatchupTime = t; }

		uint32 GetComponentBitfield(uint32 offset) { return m_uComponentBitfield[offset]; }
		uint32* GetRawComponentBitfield() { return &m_uComponentBitfield[0]; }
		static uint32 NumEntities();

		ComponentEntity* GetChildEntityByName(const UnsafeComponentGetter& getter, const char* szName, bool bRecursive = true);
		ComponentEntity* GetChildEntityByName(const UnsafeComponentGetter& getter, uint32 uNameHash, bool bRecursive = true);

		template <typename UnaryFunction>
		void ProcessEntityRecursively(UnaryFunction function, ComponentLoadHandles& handles) {
			ComponentEntity* pChild = GetChildEntity();
			function(this, handles);

			if (pChild == NULL) { return; }

			ComponentEntity* pChildSibling = pChild->GetNextSibling();

			while (pChildSibling) {
				ComponentEntity* pTmp = pChildSibling->GetNextSibling();
				pChildSibling->ProcessEntityRecursively(function, handles);
				pChildSibling = pTmp;
			}

			pChild->ProcessEntityRecursively(function, handles);
		}

		template <typename UnaryFunction>
		void ProcessEntityRecursively(UnaryFunction function) {
			ComponentEntity* pChild = GetChildEntity();
			function(this);

			if (pChild == NULL) { return; }

			ComponentEntity* pChildSibling = pChild->GetNextSibling();

			while (pChildSibling) {
				ComponentEntity* pTmp = pChildSibling->GetNextSibling();
				pChildSibling->ProcessEntityRecursively(function);
				pChildSibling = pTmp;
			}

			pChild->ProcessEntityRecursively(function);
		}

		static void InitPool(uint32 uPoolSize=100);
		static void Reset();

		StringPointerHash<GenericInputOutputs*>& GetSystems();
		static const uint32 MAX_COMPONENT_TYPES = 320;
		static const uint32 BITFIELD_SIZE = (MAX_COMPONENT_TYPES / BITFIELD_LENGTH) + 1;

		bool IsCollisionListener() const
		{
			return m_uOnCollisionMask != 0;
		}

		uint32 GetOnCollisionMask() const { return m_uOnCollisionMask; }
		void SetOnCollisionMask(const uint32 uOnCollisionMask) { m_uOnCollisionMask = uOnCollisionMask; }

		uint32 GetSpawnFrame() const
		{
			return m_uSpawnFrame;
		}

		EntityHandle GetStableID() const { return EntityHandle(m_uIndex, m_uGeneration); }
		static Entity GetEntityFromStableID(EntityHandle id);
		static bool IsStableIDValid(EntityHandle id) { return GetEntityFromStableID(id) != nullptr; }
	private:
		static NewEntities& GetNewEntities();

		uint32			 m_uSpawnFrame = 0;
		uint32           m_uIndex;
		uint32			 m_uGeneration;
		uint32			 m_uOnCollisionMask;
		float            m_fCatchupTime;
		bool             m_bChanged;
		bool             m_bChildrenChanged;
		bool			 m_bPendingDeletions;
		StringPointerHash<ComponentType*> m_pComponents;
		ComponentType*   m_pFirstComponent;
		StringPointerHash<GenericInputOutputs*> m_pSystems;
		bool             m_bActive;
		uint32           m_uComponentBitfield[BITFIELD_SIZE];
		static const uint32 MAX_SYSTEM_TYPES = 512;
		static const uint32 SYSTEM_BITFIELD_SIZE = (MAX_SYSTEM_TYPES / BITFIELD_LENGTH) + 1;
		uint32           m_uSystemBitfield[SYSTEM_BITFIELD_SIZE];

		// Statics
		static FastPool<ComponentEntity>*		g_pPool;
		static ComponentEntity *				g_hierarchy;
		static uint32							g_uNumSystemTypes;

		static void RegisterStableEntity(ComponentEntity* entity);
		static void UnregisterStableEntity(ComponentEntity* entity);
		static std::atomic<uint32>& SystemExecutionDepth();
		static void ResetSystemExecutionDepth();
		static void ClearDeferredStructureChanges();
		void SetComponentBit(uint32 uBitfieldOffset, uint32 uBitfieldIndex, bool bValue);

		uint32 GetSystemIDFromIndex(uint32 uSysIndex) const { return uSysIndex + 1; }
		uint32 GetComponentIDFromIndex(uint32 uCompIndex) const { return uCompIndex + 1; }

		void LinkComponent(ComponentType *pComp);
		void UnlinkComponent(ComponentType *pComp);
		void SetSystemBit(uint32 uSysIndex, bool bValue);
	};

	inline std::atomic<uint32>& ComponentEntity::SystemExecutionDepth()
	{
		static std::atomic<uint32> s_uSystemExecutionDepth(0);
		return s_uSystemExecutionDepth;
	}

	inline void ComponentEntity::BeginSystemExecution()
	{
		SystemExecutionDepth().fetch_add(1, std::memory_order_acq_rel);
	}

	inline void ComponentEntity::EndSystemExecution()
	{
		std::atomic<uint32>& depth = SystemExecutionDepth();
		const uint32 uPreviousDepth = depth.fetch_sub(1, std::memory_order_acq_rel);
		ASSERT(uPreviousDepth > 0);
		if (uPreviousDepth == 0)
		{
			depth.fetch_add(1, std::memory_order_acq_rel);
		}
	}

	inline bool ComponentEntity::IsSystemExecutionActive()
	{
		return SystemExecutionDepth().load(std::memory_order_acquire) > 0;
	}

	inline void ComponentEntity::AssertStructureMutationAllowed()
	{
		ASSERT(!IsSystemExecutionActive());
	}

	inline void ComponentEntity::ResetSystemExecutionDepth()
	{
		SystemExecutionDepth().store(0, std::memory_order_release);
	}

	inline Entity CreateEntity(Entity parent)
	{
		return ComponentEntity::Create(parent);
	}

	inline void FreeEntity(Entity e, ComponentLoadHandles& handles)
	{
		ComponentEntity::Free(e, handles);
	}

	namespace Components
	{
		struct EntityID
		{
			Entity id;
			EntityHandle stableId;
		};
	}

	template<>
	inline constexpr char * NamePB<Components::EntityID>()
	{
		return "EntityID";
	}

}

#endif //_COMPONENT_ENTITY_H
