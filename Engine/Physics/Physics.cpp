/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Physics backend (Box3D) setup, the per-scene world and the
//	translation of Box3D step events into Usagi signals.
*****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Physics/Physics.h"
#include "Engine/Physics/PhysicsComponents.h"
#include "Engine/Physics/PhysicsMeshCache.h"
#include "Engine/Physics/PhysicsSceneData.h"
#include "Engine/Framework/FrameworkComponents.pb.h"
#include "Engine/Framework/SystemCoordinator.h"
#include "Engine/Framework/EventManager.h"
#include "Engine/Debug/DebugStats.h"
#include "Engine/Physics/CollisionData.pb.h"
#include "Engine/Physics/Signals/OnTrigger.h"
#include "Engine/Physics/Signals/OnCollision.h"
#include "Engine/Physics/CollisionMask.pb.h"
#include "Engine/Physics/PhysicsEvents.pb.h"

namespace usg
{
#ifndef FINAL_BUILD
	namespace physics
	{
		namespace details
		{
			float g_fOnCollisionClosureCallTimes = 0;
			float g_fFoundColliderInputsTimes = 0;
		}
	}
#endif

	namespace physics
	{
		static float32 s_fTimeStep = DefaultTimeStep;

		void SetTimeStep(float32 fStep)
		{
			s_fTimeStep = fStep;
		}

		float32 GetTimeStep()
		{
			return s_fTimeStep;
		}

		Components::TransformComponent ToUsgTransform(const b3Transform& t)
		{
			TransformComponent r;
			r.bInheritFromParent = true;
			r.position = ToUsgVec3(t.p);
			r.rotation = ToUsgQuaternionf(t.q);
			return r;
		}

		// Box3D allocates through Usagi's physics heap
		static void* Box3DAlloc(size_t uSize, int32_t iAlignment)
		{
			return mem::Alloc(MEMTYPE_STANDARD, ALLOC_PHYSICS, uSize, Math::Max((memsize)iAlignment, (memsize)16), false);
		}

		static void Box3DFree(void* pMem, size_t uSize)
		{
			mem::Free(pMem);
		}

		static int Box3DAssert(const char* szCondition, const char* szFileName, int iLineNumber)
		{
			DEBUG_PRINT("Box3D assert: %s (%s:%d)\n", szCondition, szFileName, iLineNumber);
			return 1;	// break into the debugger
		}

		static void Box3DLog(const char* szMessage)
		{
			DEBUG_PRINT("Box3D: %s\n", szMessage);
		}

		static bool s_bInitialized = false;
		static PhysicsMeshCache* s_pMeshCache = nullptr;

		void init()
		{
			ASSERT(!s_bInitialized);
			b3SetAllocator(Box3DAlloc, Box3DFree);
			b3SetAssertFcn(Box3DAssert);
			b3SetLogFcn(Box3DLog);
			s_pMeshCache = vnew(ALLOC_PHYSICS) PhysicsMeshCache();
			s_bInitialized = true;
		}

		void deinit()
		{
			ASSERT(s_bInitialized);
			vdelete s_pMeshCache;
			s_pMeshCache = nullptr;
			s_bInitialized = false;
		}
	}

	usg::Matrix4x4 ToUsgMatrix(const b3Transform& trans)
	{
		usg::Matrix4x4 mRet = ToUsgQuaternionf(trans.q);
		mRet.SetTranslation(ToUsgVec3(trans.p));
		return mRet;
	}

	b3Transform ToB3Transform(const TransformComponent& trans)
	{
		return MakeB3Transform(trans.position, trans.rotation);
	}

	b3Transform ToB3Transform(const MatrixComponent& matrix)
	{
		const Quaternionf qRot = matrix.matrix;
		return MakeB3Transform(matrix.matrix.vPos().v3(), qRot);
	}

	// Surface materials

	uint64 MakeUserMaterialId(const PhysicMaterial& m)
	{
		return (uint64)m.uFlags | ((uint64)(m.eFrictionCombineMode & 0xf) << 32) | ((uint64)(m.eRestitutionCombineMode & 0xf) << 36);
	}

	b3SurfaceMaterial ToB3SurfaceMaterial(const PhysicMaterial& m)
	{
		b3SurfaceMaterial r = b3DefaultSurfaceMaterial();
		// Box3D has one friction coefficient. The dynamic one is what acts while cars shove and
		// slide against each other and the props, which is what the game's tuning is about.
		r.friction = m.fDynamicFriction;
		r.restitution = m.fBounciness;
		r.userMaterialId = MakeUserMaterialId(m);
		return r;
	}

	// PhysicMaterial combine modes: when two materials disagree the higher mode wins
	// (AVERAGE < MIN < MULTIPLY < MAX), as before.
	static float CombineMaterialValues(float fA, uint64 uMaterialA, float fB, uint64 uMaterialB, uint32 uShift)
	{
		const uint32 uModeA = (uint32)((uMaterialA >> uShift) & 0xf);
		const uint32 uModeB = (uint32)((uMaterialB >> uShift) & 0xf);
		switch ((CombineMode)Math::Max(uModeA, uModeB))
		{
		case CombineMode_MIN:		return Math::Min(fA, fB);
		case CombineMode_MULTIPLY:	return fA * fB;
		case CombineMode_MAX:		return Math::Max(fA, fB);
		default:		return (fA + fB) * 0.5f;
		}
	}

	static float CombineFriction(float fFrictionA, uint64_t uMaterialA, float fFrictionB, uint64_t uMaterialB)
	{
		return CombineMaterialValues(fFrictionA, uMaterialA, fFrictionB, uMaterialB, 32);
	}

	static float CombineRestitution(float fRestitutionA, uint64_t uMaterialA, float fRestitutionB, uint64_t uMaterialB)
	{
		return CombineMaterialValues(fRestitutionA, uMaterialA, fRestitutionB, uMaterialB, 36);
	}

	// Bodies with bDisableImpulses detect and report contacts but are not pushed by them
	static bool PreSolve(b3ShapeId shapeIdA, b3ShapeId shapeIdB, b3Pos point, b3Vec3 normal, void* context)
	{
		const ShapeRuntimeData* pA = physics::details::GetUserDataFromShape(shapeIdA);
		const ShapeRuntimeData* pB = physics::details::GetUserDataFromShape(shapeIdB);
		const uint32 uDisable = static_cast<uint32>(ShapeBitmask::DisableImpulses);
		return !((pA && (pA->uBitmask & uDisable)) || (pB && (pB->uBitmask & uDisable)));
	}

	static usg::pair<Entity,Entity> GenerateCollisionPair(Entity e1, Entity e2)
	{
		const uint64 uId1 = reinterpret_cast<uint64>(e1);
		const uint64 uId2 = reinterpret_cast<uint64>(e2);
		return uId1 < uId2 ? usg::pair<Entity, Entity>(e1, e2) : usg::pair<Entity, Entity>(e2,e1);
	}

	// Does a collision listener on 'listener' want to hear about touching 'other'?
	static bool WantsCollisionReport(const ShapeRuntimeData& listener, const ShapeRuntimeData& other)
	{
		return listener.shapeAggregateEntity != nullptr && listener.shapeAggregateEntity->IsCollisionListener()
			&& (listener.uFilter & other.uGroup) != 0
			&& (other.uGroup & listener.shapeAggregateEntity->GetOnCollisionMask()) != 0;
	}

	void InitRaycasting(physics::PhysicsSceneData& sceneRtd);

	void InitPhysicsScene(Component<usg::Components::PhysicsScene>& p)
	{
		p.GetRuntimeData().pSceneData = vnew(ALLOC_PHYSICS)physics::PhysicsSceneData();
		auto& rtd = p.GetRuntimeData().GetData();

		rtd.uTick = 0;
		rtd.pMeshCache = physics::s_pMeshCache;
		rtd.groundBodyId = b3_nullBodyId;
		rtd.bSimulationRunning = false;
		rtd.iNextAggregateGroup = -1;

		b3WorldDef worldDef = b3DefaultWorldDef();
		worldDef.gravity = ToB3Vec3(p->vGravity);
		// PhysX's default bounce threshold (0.2 * the 10 units/s tolerance speed)
		worldDef.restitutionThreshold = 2.0f;
		worldDef.frictionCallback = CombineFriction;
		worldDef.restitutionCallback = CombineRestitution;
		worldDef.enableSleep = true;
		worldDef.enableContinuous = true;
		// Usagi has no job system; Box3D runs the step on the calling thread. The
		// enqueue/finish task callbacks are where a scheduler would plug in.
		worldDef.workerCount = 1;
		worldDef.userData = &p;
		rtd.worldId = b3CreateWorld(&worldDef);
		b3World_SetPreSolveCallback(rtd.worldId, PreSolve, nullptr);

		InitRaycasting(rtd);
		rtd.touchingContacts.reserve(64);
		rtd.triggerSignals.reserve(16);

#ifndef FINAL_BUILD
		rtd.debugStats.Init(rtd.worldId);
		DebugStats::Inst()->RegisterGroup(&rtd.debugStats);
#endif
	}

	void DeinitPhysicsScene(Component<usg::Components::PhysicsScene>& p)
	{
		auto& rtd = p.GetRuntimeData().GetData();

#ifndef FINAL_BUILD
		DebugStats::Inst()->DeregisterGroup(&rtd.debugStats);
#endif

		rtd.dirtyShapeList.clear();
		rtd.raycastData.pendingRequests.clear();
		rtd.raycastData.pendingSweepRequests.clear();

		b3DestroyWorld(rtd.worldId);
		rtd.worldId = b3_nullWorldId;

		vdelete(p.GetRuntimeData().pSceneData);
		p.GetRuntimeData().pSceneData = nullptr;
	}

	void UpdateSimulationFilter(ShapeRuntimeData* pShape, const CollisionMasks* pMasks, const RigidBody* pBody, Entity aggregateEntity)
	{
		ASSERT(pMasks && "Shapes must have CollisionMasks component.");

		pShape->uGroup = pMasks->uGroup;
		pShape->uFilter = pMasks->uFilter;
		pShape->uReportMask = aggregateEntity->GetOnCollisionMask();
		pShape->uBitmask = 0;
		if (pBody && pBody->bDisableImpulses)
		{
			pShape->uBitmask |= static_cast<uint32>(ShapeBitmask::DisableImpulses);
		}
		if (pBody && pBody->bEnableCCD)
		{
			pShape->uBitmask |= static_cast<uint32>(ShapeBitmask::CCD);
		}
		if (pBody && pBody->fMaxImpulse > Math::EPSILON)
		{
			DEBUG_PRINT("Physics: RigidBody.fMaxImpulse is not supported by the Box3D backend and is ignored.\n");
		}

		if (!b3Shape_IsValid(pShape->shapeId))
		{
			return;
		}

		b3Filter filter = b3Shape_GetFilter(pShape->shapeId);
		filter.categoryBits = pMasks->uGroup;
		filter.maskBits = pMasks->uFilter;
		b3Shape_SetFilter(pShape->shapeId, filter, true);

		// Box3D raises begin/end touch events when either shape asks for them
		const bool bListener = aggregateEntity->IsCollisionListener() && pShape->uReportMask != 0;
		b3Shape_EnableContactEvents(pShape->shapeId, bListener);
		b3Shape_EnablePreSolveEvents(pShape->shapeId, (pShape->uBitmask & static_cast<uint32>(ShapeBitmask::DisableImpulses)) != 0);
	}

	namespace physics
	{
		static void ProcessBodyEvents(PhysicsSceneData& rtd)
		{
			// Box3D reports every body that moved, and whether it fell asleep. SleepTag
			// lets the transform systems skip sleeping bodies.
			struct Getter : public UnsafeComponentGetter {} getter;
			const b3BodyEvents events = b3World_GetBodyEvents(rtd.worldId);
			for (int i = 0; i < events.moveCount; i++)
			{
				const b3BodyMoveEvent& move = events.moveEvents[i];
				Component<RigidBody>* pRigidBody = (Component<RigidBody>*)move.userData;
				if (pRigidBody == nullptr)
				{
					continue;
				}
				Entity entity = pRigidBody->GetEntity();
				ASSERT(entity != nullptr);
				Optional<SleepTag> sleepTag;
				getter.GetComponent(entity, sleepTag);
				if (move.fellAsleep && !sleepTag.Exists())
				{
					GameComponents<SleepTag>::Create(entity);
					entity->SetChanged();
				}
				else if (!move.fellAsleep && sleepTag.Exists())
				{
					GameComponents<SleepTag>::RequestFree(entity);
					entity->SetChanged();
				}
			}
		}

		static void ProcessSensorEvents(PhysicsSceneData& rtd)
		{
			const b3SensorEvents events = b3World_GetSensorEvents(rtd.worldId);
			for (int i = 0; i < events.beginCount + events.endCount; i++)
			{
				const bool bEntered = i < events.beginCount;
				const b3ShapeId sensorId = bEntered ? events.beginEvents[i].sensorShapeId : events.endEvents[i - events.beginCount].sensorShapeId;
				const b3ShapeId visitorId = bEntered ? events.beginEvents[i].visitorShapeId : events.endEvents[i - events.beginCount].visitorShapeId;
				if (!b3Shape_IsValid(sensorId) || !b3Shape_IsValid(visitorId))
				{
					continue;	// a shape went away
				}
				const ShapeRuntimeData* pTrigger = details::GetUserDataFromShape(sensorId);
				const ShapeRuntimeData* pTriggerer = details::GetUserDataFromShape(visitorId);
				if (pTrigger == nullptr || pTriggerer == nullptr)
				{
					continue;
				}

				rtd.triggerSignals.push_back();
				auto& td = rtd.triggerSignals.back();
				td.triggerEntity = pTrigger->entity; // Note here that unlike for collisions, trigger signals are sent to the entities which have the shape, not the shape aggregate entity (entity with the rigidbody)
				td.triggererEntity = pTriggerer->entity;
				td.bEntered = bEntered;
			}
		}

		static void ProcessContactBeginEndEvents(PhysicsSceneData& rtd)
		{
			const b3ContactEvents events = b3World_GetContactEvents(rtd.worldId);
			for (int i = 0; i < events.endCount; i++)
			{
				const b3ContactId contactId = events.endEvents[i].contactId;
				for (memsize j = 0; j < rtd.touchingContacts.size(); j++)
				{
					if (B3_ID_EQUALS(rtd.touchingContacts[j].contactId, contactId))
					{
						rtd.touchingContacts[j] = rtd.touchingContacts.back();
						rtd.touchingContacts.pop_back();
						break;
					}
				}
			}

			for (int i = 0; i < events.beginCount; i++)
			{
				const b3ContactBeginTouchEvent& begin = events.beginEvents[i];
				const ShapeRuntimeData* pA = details::GetUserDataFromShape(begin.shapeIdA);
				const ShapeRuntimeData* pB = details::GetUserDataFromShape(begin.shapeIdB);
				if (pA == nullptr || pB == nullptr)
				{
					continue;
				}
				if (!WantsCollisionReport(*pA, *pB) && !WantsCollisionReport(*pB, *pA))
				{
					continue;
				}
				PhysicsSceneData::TouchingContact tc;
				tc.contactId = begin.contactId;
				tc.shapeIdA = begin.shapeIdA;
				tc.shapeIdB = begin.shapeIdB;
				tc.bFirstPending = true;
				rtd.touchingContacts.push_back(tc);
			}
		}

		// Rebuild this step's collision data (as the old PhysX contact callback did) from
		// every touching contact someone listens to.
		static void CollectCollisions(PhysicsSceneData& sceneRtd)
		{
			for (memsize i = 0; i < sceneRtd.touchingContacts.size(); )
			{
				PhysicsSceneData::TouchingContact& tc = sceneRtd.touchingContacts[i];
				if (!b3Contact_IsValid(tc.contactId) || !b3Shape_IsValid(tc.shapeIdA) || !b3Shape_IsValid(tc.shapeIdB))
				{
					tc = sceneRtd.touchingContacts.back();
					sceneRtd.touchingContacts.pop_back();
					continue;
				}
				i++;

				const ShapeRuntimeData* pA = details::GetUserDataFromShape(tc.shapeIdA);
				const ShapeRuntimeData* pB = details::GetUserDataFromShape(tc.shapeIdB);
				if (pA == nullptr || pB == nullptr || pA->shapeAggregateEntity == nullptr || pB->shapeAggregateEntity == nullptr)
				{
					continue;
				}

				const b3ContactData data = b3Contact_GetData(tc.contactId);
				uint32 uPointCount = 0;
				float fTotalImpulse = 0.0f;
				float fMinSeparation = FLT_MAX;
				for (int m = 0; m < data.manifoldCount; m++)
				{
					uPointCount += data.manifolds[m].pointCount;
					for (int k = 0; k < data.manifolds[m].pointCount; k++)
					{
						fTotalImpulse += data.manifolds[m].points[k].totalNormalImpulse;
						fMinSeparation = Math::Min(fMinSeparation, data.manifolds[m].points[k].separation);
					}
				}
				if (uPointCount == 0)
				{
					continue;
				}

				const usg::pair<Entity, Entity> collisionPair = GenerateCollisionPair(pA->shapeAggregateEntity, pB->shapeAggregateEntity);
				auto& collisionDataForPair = sceneRtd.collisions[collisionPair];

				// Box3D's speculative contacts begin touching a little before impact. The first
				// contact is the first step the pair actually pushes on or overlaps each other.
				bool bFirstContactForPair = false;
				if (tc.bFirstPending && (fTotalImpulse > 0.0f || fMinSeparation < 0.0f))
				{
					tc.bFirstPending = false;
					const uint32 uSinceLastCollision = sceneRtd.uTick - collisionDataForPair.uTick;
					bFirstContactForPair = uSinceLastCollision >= 2;
				}
				collisionDataForPair.bFirstContact = bFirstContactForPair;
				collisionDataForPair.uTick = sceneRtd.uTick;

				// Stored normals point from the pair's second entity towards the first, as
				// PhysX reported them. Box3D's manifold normal points from shape A to B.
				const bool bSwap = collisionPair.first != pA->shapeAggregateEntity;
				const float fNormalMul = bSwap ? 1.0f : -1.0f;
				const ShapeRuntimeData* pFirst = bSwap ? pB : pA;
				const ShapeRuntimeData* pSecond = bSwap ? pA : pB;
				if (WantsCollisionReport(*pFirst, *pSecond))
				{
					collisionDataForPair.uNotifyFirstTick = sceneRtd.uTick;
				}
				if (WantsCollisionReport(*pSecond, *pFirst))
				{
					collisionDataForPair.uNotifySecondTick = sceneRtd.uTick;
				}

				const uint64 uMaterialA = b3Shape_GetSurfaceMaterial(tc.shapeIdA).userMaterialId;
				const uint64 uMaterialB = b3Shape_GetSurfaceMaterial(tc.shapeIdB).userMaterialId;
				const b3Vec3 vCenterA = b3Body_GetWorldCenter(b3Shape_GetBody(tc.shapeIdA));
				for (int m = 0; m < data.manifoldCount; m++)
				{
					const b3Manifold& manifold = data.manifolds[m];
					const Vector3f vNormal = ToUsgVec3(manifold.normal) * fNormalMul;
					for (int k = 0; k < manifold.pointCount; k++)
					{
						const b3ManifoldPoint& mp = manifold.points[k];
						collisionDataForPair.contacts.push_back();
						auto& newCollision = collisionDataForPair.contacts.back();
						newCollision.fDepth = mp.separation;
						newCollision.vNormal = vNormal;
						newCollision.vIntersectionPoint = ToUsgVec3(vCenterA) + ToUsgVec3(mp.anchorA);
						newCollision.vImpulse = vNormal * mp.totalNormalImpulse;
						newCollision.uMaterialFlags[bSwap ? 1 : 0] = GetMaterialFlags(uMaterialA) | pA->uMaterialFlags;
						newCollision.uMaterialFlags[bSwap ? 0 : 1] = GetMaterialFlags(uMaterialB) | pB->uMaterialFlags;
						newCollision.uBitmask = 0;
					}
				}
			}
		}

		static void ProcessJointEvents(PhysicsSceneData& rtd, EventManager* pEventManager)
		{
			// Box3D reports joints whose force or torque threshold was exceeded; Usagi
			// breaks them, as PhysX did.
			const b3JointEvents events = b3World_GetJointEvents(rtd.worldId);
			for (int i = 0; i < events.count; i++)
			{
				const b3JointId jointId = events.jointEvents[i].jointId;
				Entity e = (Entity)events.jointEvents[i].userData;
				if (!b3Joint_IsValid(jointId) || e == nullptr)
				{
					continue;
				}
				b3DestroyJoint(jointId, true);
				if (pEventManager)
				{
					OnJointBroken evt;
					OnJointBroken_init(&evt);
					pEventManager->RegisterEventWithEntity(e, evt, ON_ENTITY);
				}
			}
		}

		static void ApplySpeedCaps()
		{
			// SetMaxLinearVelocity / SetMaxAngularVelocity (Box3D has no per-body cap)
			for (auto it = GameComponents<RigidBody>::GetIterator(); !it.IsEnd(); ++it)
			{
				const RuntimeData<RigidBody>& rbRtd = (*it)->GetRuntimeData();
				if ((rbRtd.fMaxLinearSpeed <= 0.0f && rbRtd.fMaxAngularSpeed <= 0.0f) || B3_IS_NULL(rbRtd.bodyId) || !b3Body_IsValid(rbRtd.bodyId))
				{
					continue;
				}
				if (rbRtd.fMaxLinearSpeed > 0.0f)
				{
					const b3Vec3 v = b3Body_GetLinearVelocity(rbRtd.bodyId);
					const float fSpeed = b3Length(v);
					if (fSpeed > rbRtd.fMaxLinearSpeed)
					{
						b3Body_SetLinearVelocity(rbRtd.bodyId, b3MulSV(rbRtd.fMaxLinearSpeed / fSpeed, v));
					}
				}
				if (rbRtd.fMaxAngularSpeed > 0.0f)
				{
					const b3Vec3 w = b3Body_GetAngularVelocity(rbRtd.bodyId);
					const float fSpeed = b3Length(w);
					if (fSpeed > rbRtd.fMaxAngularSpeed)
					{
						b3Body_SetAngularVelocity(rbRtd.bodyId, b3MulSV(rbRtd.fMaxAngularSpeed / fSpeed, w));
					}
				}
			}
		}

		void ProcessStepEvents(PhysicsSceneData& rtd, EventManager* pEventManager)
		{
			ProcessBodyEvents(rtd);
			ProcessSensorEvents(rtd);
			ProcessContactBeginEndEvents(rtd);
			CollectCollisions(rtd);
			ProcessJointEvents(rtd, pEventManager);
			ApplySpeedCaps();
		}
	}

	void GenerateOnTriggerSignals(SystemCoordinator& systemCoordinator, Required<usg::PhysicsScene> scene)
	{
		auto& rtd = scene.GetRuntimeData().GetData();
		for (const auto& trig : rtd.triggerSignals)
		{
			const TriggerEventType eventType = trig.bEntered ? TriggerEventType::Enter : TriggerEventType::Exit;
			OnTriggerSignal onTriggerSignal(trig.triggererEntity, eventType);
			systemCoordinator.Trigger(trig.triggerEntity, onTriggerSignal, ON_ENTITY);
		}
		rtd.triggerSignals.clear();
	}

	void GenerateOnCollisionSignals(SystemCoordinator& systemCoordinator, Required<usg::PhysicsScene> scene)
	{
		auto& rtd = scene.GetRuntimeData().GetData();
		if (rtd.collisions.size() == 0)
		{
			return;
		}
		for (const auto& c : rtd.collisions)
		{
			const auto& pair = c.first;
			const auto& data = c.second;
			for (const auto& contact : data.contacts)
			{
				Collision usgCol;
				usgCol.fDepth = contact.fDepth;
				usgCol.vIntersectionPoint = contact.vIntersectionPoint;
				usgCol.vImpulse = contact.vImpulse;
				usgCol.bFirstContact = data.bFirstContact;
				if (data.uNotifyFirstTick == rtd.uTick - 1)
				{
					usgCol.vNormal = -contact.vNormal;
					usgCol.uMaterialFlags = contact.uMaterialFlags[1];
					CollisionMasks* pMasks = GameComponents<CollisionMasks>::GetComponentData(pair.second);
					if(pMasks)
					{
						OnCollisionSignal onCollisionSignal(pair.second, usgCol, pMasks->uGroup);
						systemCoordinator.Trigger(pair.first, onCollisionSignal, ON_ENTITY);
					}
				}

				if (data.uNotifySecondTick == rtd.uTick - 1)
				{
					usgCol.vNormal = contact.vNormal;
					usgCol.uMaterialFlags = contact.uMaterialFlags[0];
					CollisionMasks* pMasks = GameComponents<CollisionMasks>::GetComponentData(pair.first);
					if(pMasks)
					{
						OnCollisionSignal onCollisionSignal(pair.first, usgCol, pMasks->uGroup);
						systemCoordinator.Trigger(pair.second, onCollisionSignal, ON_ENTITY);
					}
				}
			}
		}
	}
}
