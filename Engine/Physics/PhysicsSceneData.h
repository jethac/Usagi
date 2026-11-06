/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/

#pragma once

#include "Engine/Core/stl/set.h"
#include "Engine/Core/stl/vector.h"
#include "Engine/Core/stl/map.h"
#include "Engine/Maths/MovingAverage.h"
#include "Engine/Physics/PhysicsStructs.pb.h"
#include "Engine/Physics/Physics.h"
#include "Engine/Physics/Raycast.h"
#include "Engine/Physics/RaycastHitBase.h"
#include "Engine/Physics/Debug/PhysicsDebugStats.h"

namespace usg
{
	namespace Components
	{
		typedef struct _PhysicsScene PhysicsScene;
	}

	class PhysicsMeshCache;
	struct ActorRuntimeData;
	struct ShapeRuntimeData;

	namespace physics
	{
		struct PhysicsSceneData
		{
			uint32 uTick;

			b3WorldId worldId;
			PhysicsMeshCache* pMeshCache;

			// Static body that joints without a parent rigid body attach to (created on demand)
			b3BodyId groundBodyId;

			volatile bool bSimulationRunning;

			// Next negative Box3D filter group handed to a PhysicsAggregate
			sint32 iNextAggregateGroup;

			set<ShapeRuntimeData*> dirtyShapeList;

#ifndef FINAL_BUILD
			PhysicsDebugStats debugStats;
#endif

			struct
			{
				vector<pair<AsyncRaycastRequest, pair<uint32, Entity>>> pendingRequests;
				vector<pair<AsyncSweepRequest, pair<uint32, Entity>>> pendingSweepRequests;

				struct
				{
					vector<RaycastHitBase> hits;
					vector<Entity> entities;
					vector<uint8> workBuffer;
				} workData;
			} raycastData;

			// Contacts between shapes that someone listens to, from Box3D begin-touch
			// until end-touch events. Collision signals are rebuilt from them every step.
			struct TouchingContact
			{
				b3ContactId contactId;
				b3ShapeId shapeIdA;
				b3ShapeId shapeIdB;
				bool bFirstPending;	// waiting for the first step with an impulse or overlap
			};
			vector<TouchingContact> touchingContacts;

			struct CollisionData
			{
				uint32 uTick;
				uint32 uNotifyFirstTick;
				uint32 uNotifySecondTick;
				bool bFirstContact;
				uint32 uMaterialFlags;

				struct Contact
				{
					static constexpr uint32 NotifyFirst = 1;
					static constexpr uint32 NotifySecond = 2;
					Vector3f vNormal;
					Vector3f vIntersectionPoint;
					Vector3f vImpulse;
					float fDepth;
					uint32 uMaterialFlags[2];
					uint32 uBitmask;
				};
				vector<Contact> contacts;
			};

			map<pair<Entity, Entity>, CollisionData> collisions;

			struct TriggerData
			{
				Entity triggerEntity;
				Entity triggererEntity;
				bool bEntered;
			};
			vector<TriggerData> triggerSignals;

#ifndef FINAL_BUILD
			struct
			{
				bool bDebugRenderOnNextFrame = false;
				Vector3f vDebugRenderCenter;

				// Profiling data
				MovingAverage<float, 16> fFetchResultsTime;
			} diagnostics;
#endif
		};
	}
}
