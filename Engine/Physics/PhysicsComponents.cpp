/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Resource/ResourceMgr.h"
#include "PhysicsComponents.h"
#include "Engine/Physics/Physics.h"
#include "Engine/Framework/FrameworkComponents.pb.h"
#include "Engine/Framework/ComponentLoadHandles.h"
#include "Engine/Resource/CollisionModelResource.h"
#include "Engine/Resource/HeightFieldResource.h"
#include "Engine/Physics/PhysicsMeshCache.h"
#include "Engine/Physics/PhysicsSceneData.h"
#include "Engine/Framework/TransformTool.h"
#include <EASTL/string.h>

namespace usg
{
	static_assert(sizeof(CollisionModelResource::IndexType)==sizeof(uint32),"Box3D and Usagi CollisionModelResource are using different index type.");

	template<typename ShapeType>
	static void OnActivateShape(Component<ShapeType>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.shapeAggregateEntity = nullptr;
		rtd.entity = nullptr;
		rtd.shapeId = b3_nullShapeId;
		rtd.uMaterialFlags = 0;
		rtd.uGroup = 0;
		rtd.uFilter = 0;
		rtd.uReportMask = 0;
		rtd.uBitmask = 0;
	}

	template<typename ShapeType>
	static void OnDeactivateShape(Component<ShapeType>& c, ComponentLoadHandles& handles)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.release();

		if (handles.pPhysicsScene->dirtyShapeList.count(&rtd))
		{
			handles.pPhysicsScene->dirtyShapeList.erase(&rtd);
		}

		rtd.shapeAggregateEntity = nullptr;
		rtd.entity = nullptr;
	}

	void ShapeRuntimeData::release()
	{
		// The shape is already gone if its body was destroyed first
		if (B3_IS_NON_NULL(shapeId) && b3Shape_IsValid(shapeId))
		{
			b3Shape_SetUserData(shapeId, nullptr);
			b3DestroyShape(shapeId, true);
		}
		shapeId = b3_nullShapeId;
	}

	namespace physics
	{
		namespace details
		{
			void SetGlobalPose(const RuntimeData<Components::RigidBody>& rbRtd, const Vector3f& vPosition, const Quaternionf& qRotation, bool bResetVelocity)
			{
				if (B3_IS_NULL(rbRtd.bodyId) || !b3Body_IsValid(rbRtd.bodyId))
				{
					return;
				}
				b3Body_SetTransform(rbRtd.bodyId, ToB3Vec3(vPosition), ToB3Quat(qRotation));
				if (bResetVelocity && b3Body_GetType(rbRtd.bodyId) == b3_dynamicBody)
				{
					b3Body_SetLinearVelocity(rbRtd.bodyId, b3Vec3_zero);
					b3Body_SetAngularVelocity(rbRtd.bodyId, b3Vec3_zero);
				}
			}
		}
	}

	TransformComponent GetTransform(Entity e, ComponentLoadHandles& handles)
	{
		Optional<TransformComponent> trans;
		handles.GetComponent(e, trans);
		if (trans.Exists())
		{
			return *trans.Force();
		}
		TransformComponent r;
		TransformComponent_init(&r);
		return r;
	}

	// Get an entity's transform relative to some not necessarily direct parent (pass nullptr as parent to get global pose)
	static b3Transform GetRelativeTransform(Entity parent, Entity child, ComponentLoadHandles& handles)
	{
		if (parent == nullptr)
		{
			// Use the matrix component to initialize static bodies with no transformcomponent
			Optional<TransformComponent, FromSelfOrParents> transFromSelfOrParents;
			handles.GetComponent(child, transFromSelfOrParents);
			if (!transFromSelfOrParents.Exists())
			{
				Optional<MatrixComponent, FromSelfOrParents> mtxFromSelfOrParents;
				handles.GetComponent(child, mtxFromSelfOrParents);
				if (mtxFromSelfOrParents.Exists())
				{
					b3Transform t = ToB3Transform(*mtxFromSelfOrParents.Force());
					ASSERT(b3IsNormalizedQuat(t.q));
					return t;
				}
			}
		}

		const TransformComponent usgTrans = GetTransform(child, handles);
		if (!usgTrans.bInheritFromParent)
		{
			return ToB3Transform(usgTrans);
		}
		b3Transform trans = ToB3Transform(usgTrans);
		Entity e = child;
		while (e->GetParentEntity() != parent)
		{
			const b3Transform parentTrans = ToB3Transform(GetTransform(e->GetParentEntity(), handles));
			trans = b3MulTransforms(parentTrans, trans);
			e = e->GetParentEntity();
		}
		// Some propagated error
		trans.q = b3NormalizeQuat(trans.q);
		return trans;
	}

	static b3ShapeDef MakeShapeDef(const PhysicMaterial& m, bool bIsTrigger, float fDensity)
	{
		b3ShapeDef def = b3DefaultShapeDef();
		def.baseMaterial = ToB3SurfaceMaterial(m);
		def.density = fDensity;
		def.isSensor = bIsTrigger;
		// Triggers fire for any shape that overlaps them
		def.enableSensorEvents = true;
		// Mass is set once all of a body's shapes are attached (see UpdateMass)
		def.updateBodyMass = false;
		return def;
	}

	// The Scene

	template<>
	void OnActivate<PhysicsScene>(Component<PhysicsScene>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.pSceneData = nullptr;
	}

	template<>
	void OnDeactivate<PhysicsScene>(Component<PhysicsScene>& c, ComponentLoadHandles& handles)
	{
		DeinitPhysicsScene(c);
	}

	template <>
	void OnLoaded<PhysicsScene>(Component<PhysicsScene>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}
		InitPhysicsScene(c);
	}

	// Rigid Body

	static void OnRigidBodyLoaded(Required<RigidBody>& c, ComponentLoadHandles& handles)
	{
		auto& rtd = c.GetRuntimeData();

		if (B3_IS_NON_NULL(rtd.bodyId))
		{
			return;
		}

		auto& sceneRuntimeData = *handles.pPhysicsScene;

		ASSERT(!c->bDynamic || GameComponents<TransformComponent>::GetComponentData(c.GetEntity()) != nullptr && "Entities with dynamic rigid bodies must have TransformComponent");
		const b3Transform initialTransform = GetRelativeTransform(nullptr, c.GetEntity(), handles);

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.position = initialTransform.p;
		bodyDef.rotation = initialTransform.q;
		bodyDef.userData = GameComponents<RigidBody>::GetComponent(c.GetEntity());

		Optional<Identifier> id;
		handles.GetComponent(c.GetEntity(), id);
		if (id.Exists())
		{
			bodyDef.name = id.Force()->name;
		}

		if (c->bDynamic)
		{
			bodyDef.type = c->bKinematic ? b3_kinematicBody : b3_dynamicBody;
			bodyDef.angularDamping = c->fAngularDamping;
			bodyDef.linearDamping = c->fLinearDamping;
			bodyDef.motionLocks.angularX = c->bLockRotationX;
			bodyDef.motionLocks.angularY = c->bLockRotationY;
			bodyDef.motionLocks.angularZ = c->bLockRotationZ;
			bodyDef.enableSleep = !c->bDisableSleep;
			// fSleepThreshold is a mass-normalised kinetic energy (PhysX); Box3D wants a speed
			bodyDef.sleepThreshold = sqrtf(2.0f * Math::Max(c->fSleepThreshold, 0.0f));
			bodyDef.gravityScale = c->bDisableGravity ? 0.0f : 1.0f;
			if (c->bEnableCCD)
			{
				ASSERT_MSG(!c->bKinematic, "CCD not supported on kinematic bodies.");
				// Box3D always sweeps dynamic bodies against static geometry; bullets are
				// also swept against other dynamic bodies.
				bodyDef.isBullet = true;
			}
			GameComponents<DynamicBodyTag>::Create(c.GetEntity());
			if (c->bKinematic)
			{
				GameComponents<KinematicBodyTag>::Create(c.GetEntity());
			}
		}
		else
		{
			ASSERT_MSG(!c->bKinematic, "Kinematic bodies must be dynamic.");
			ASSERT_MSG(!c->bEnableCCD, "CCD is applied to dynamic bodies.");
			ASSERT_MSG(!c->bDisableGravity, "Gravity is applied to dynamic bodies.");
			bodyDef.type = b3_staticBody;
		}

		rtd.bodyId = b3CreateBody(sceneRuntimeData.worldId, &bodyDef);
		ASSERT(B3_IS_NON_NULL(rtd.bodyId));
		rtd.uBitmask |= static_cast<uint32>(ActorRuntimeData::Bitmask::InScene);

		Optional<PhysicsAggregate, FromSelfOrParents> aggregate;
		handles.GetComponent(c.GetEntity(), aggregate);
		if (aggregate.Exists() && !c->bDoNotAttachToParentAggregate)
		{
			auto& aggregateComponent = *GameComponents<PhysicsAggregate>::GetComponent(aggregate.Force().GetEntity());
			if (!aggregateComponent.GetRuntimeData().bLoaded)
			{
				OnLoaded(aggregateComponent, handles, false);
			}
			rtd.iAggregateGroup = aggregateComponent.GetRuntimeData().iGroup;
		}

		// Go through the parents and inform them physics needs the transform updated
		// FIXME: This should be done in data by the hierarchy processor rather than at runtime
		Entity parent = c.GetEntity();
		while (parent = parent->GetParentEntity())
		{
			Optional<TransformComponent> transform;
			Optional<RigidBodyTransformUpdate> parentMarker;
			handles.GetComponent(parent, transform);
			handles.GetComponent(parent, parentMarker);
			if (transform.Exists() && !parentMarker.Exists())
			{
				GameComponents<RigidBodyTransformUpdate>::Create(parent);
			}
		}
	}

	template <>
	void OnLoaded<RigidBody>(Component<RigidBody>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		Required<RigidBody, FromSelf> rb;
		handles.GetComponent(c.GetEntity(), rb);
		OnRigidBodyLoaded(rb, handles);
	}

	template<>
	void OnActivate<RigidBody>(Component<RigidBody>& c)
	{
		c.Modify().vInertiaTensor = Vector3f(1, 1, 1);

		auto& rtd = c.GetRuntimeData();
		rtd.bodyId = b3_nullBodyId;
		rtd.uBitmask = 0;
		rtd.fMaxLinearSpeed = 0.0f;
		rtd.fMaxAngularSpeed = 0.0f;
		rtd.iAggregateGroup = 0;
	}

	template<>
	void OnDeactivate<RigidBody>(Component<RigidBody>& c, ComponentLoadHandles& handles)
	{
		auto& rtd = c.GetRuntimeData();
		if (B3_IS_NON_NULL(rtd.bodyId))
		{
			if (b3Body_IsValid(rtd.bodyId))
			{
				// Destroying the body destroys its shapes and joints too. Clear the user data
				// so nothing reads the components while they are torn down.
				b3ShapeId shapes[16];
				const int iShapeCount = b3Body_GetShapes(rtd.bodyId, shapes, 16);
				for (int i = 0; i < iShapeCount; i++)
				{
					b3Shape_SetUserData(shapes[i], nullptr);
				}
				b3Body_SetUserData(rtd.bodyId, nullptr);
				b3DestroyBody(rtd.bodyId);
			}
			rtd.bodyId = b3_nullBodyId;
		}
		rtd.uBitmask = 0;
	}

	// Generic Shape Functions

	// Mass and inertia from the attached shapes. With fDensity set, the shapes' mass is used
	// (and written back to fMass). Otherwise the body gets fMass, with the inertia of the same
	// shapes at that mass (a uniform density of fMass / volume). vInertiaTensor scales the
	// result per axis.
	//
	// The PhysX backend set fMass but left the inertia at its density 1 value, which for a
	// 100 kg toy car was 600 times too small. Box3D's per-point friction then let two cars
	// that touched side by side pin each other's yaw and lock together; the consistent
	// inertia removes that (see lanes/physics/BOX3D_REPORT.md).
	template<typename T>
	static void UpdateMass(Required<RigidBody, T> rigidBody)
	{
		const b3BodyId bodyId = rigidBody.GetRuntimeData().bodyId;
		if (!rigidBody->bDynamic || b3Body_GetType(bodyId) != b3_dynamicBody)
		{
			return;
		}

		b3Body_ApplyMassFromShapes(bodyId);
		b3MassData massData = b3Body_GetMassData(bodyId);
		if (rigidBody->fDensity > Math::EPSILON)
		{
			rigidBody.Modify().fMass = massData.mass;
		}
		else if (massData.mass > 0.0f)
		{
			const float fScale = rigidBody->fMass / massData.mass;
			massData.inertia.cx = b3MulSV(fScale, massData.inertia.cx);
			massData.inertia.cy = b3MulSV(fScale, massData.inertia.cy);
			massData.inertia.cz = b3MulSV(fScale, massData.inertia.cz);
			massData.mass = rigidBody->fMass;
		}
		else
		{
			massData.mass = rigidBody->fMass;
		}

		// D * I * D with D = sqrt(scale) keeps the tensor symmetric
		const Vector3f& vScale = rigidBody->vInertiaTensor;
		const float d[3] = { sqrtf(vScale.x), sqrtf(vScale.y), sqrtf(vScale.z) };
		b3Vec3* cols[3] = { &massData.inertia.cx, &massData.inertia.cy, &massData.inertia.cz };
		for (int c = 0; c < 3; c++)
		{
			cols[c]->x *= d[0] * d[c];
			cols[c]->y *= d[1] * d[c];
			cols[c]->z *= d[2] * d[c];
		}
		b3Body_SetMassData(bodyId, massData);
	}

	template<typename SearchMask>
	static void EnsureRigidBodyLoaded(Optional<RigidBody, SearchMask> rigidBody, ComponentLoadHandles& handles)
	{
		if (rigidBody.Exists() && B3_IS_NULL(rigidBody.Force().GetRuntimeData().bodyId))
		{
			Required<RigidBody, FromSelf> rigidBodyFromSelf;
			handles.GetComponent(rigidBody.Force().GetEntity(), rigidBodyFromSelf);
			ASSERT(rigidBodyFromSelf.IsValid());
			OnRigidBodyLoaded(rigidBodyFromSelf, handles);
		}
	}

	template<typename SearchMask>
	static void EnsureRigidBodyLoaded(Required<RigidBody, SearchMask> rigidBody, ComponentLoadHandles& handles)
	{
		if (B3_IS_NULL(rigidBody.GetRuntimeData().bodyId))
		{
			Required<RigidBody, FromSelf> rigidBodyFromSelf;
			handles.GetComponent(rigidBody.GetEntity(), rigidBodyFromSelf);
			ASSERT(rigidBodyFromSelf.IsValid());
			OnRigidBodyLoaded(rigidBodyFromSelf, handles);
		}
	}

	// The geometry callback receives the body, the shape definition and the shape's pose
	// relative to the body, and creates the Box3D shape.
	template<typename ShapeType, typename CreateFn>
	static void OnShapeLoaded(Component<ShapeType>&c, ComponentLoadHandles& handles, CreateFn createShape)
	{
		ShapeRuntimeData& rtd = c.GetRuntimeData();
		rtd.uMaterialFlags = c->material.uMaterialFlags;

		Entity shapeAggregateEntity = nullptr;
		Entity actorEntity = nullptr;
		Optional<RigidBody, FromSelfOrParents> rigidBody;
		handles.GetComponent(c.GetEntity(), rigidBody);
		ASSERT((rigidBody.Exists() || c->bIsTrigger) && "A shape must be attached to a rigid body or be a trigger.");
		if (rigidBody.Exists())
		{
			Optional<PhysicsAggregate, FromSelfOrParents> aggregate;
			handles.GetComponent(c.GetEntity(), aggregate);
			if (aggregate.Exists() && !rigidBody.Force()->bDoNotAttachToParentAggregate)
			{
				shapeAggregateEntity = aggregate.Force().GetEntity();
			}
			else
			{
				shapeAggregateEntity = rigidBody.Force().GetEntity();
			}
			actorEntity = rigidBody.Force().GetEntity();
		}
		else if (c->bIsTrigger)
		{
			shapeAggregateEntity = c.GetEntity();
			actorEntity = shapeAggregateEntity;
		}
		ASSERT(shapeAggregateEntity != nullptr);
		rtd.shapeAggregateEntity = shapeAggregateEntity;
		rtd.entity = c.GetEntity();

		EnsureRigidBodyLoaded(rigidBody, handles);

		Required<RigidBody> rigidBodyFromSelf;
		if (rigidBody.Exists())
		{
			handles.GetComponent(actorEntity, rigidBodyFromSelf);
		}
		else
		{
			// No rigid body found. In this case, the shape must be a trigger.
			ASSERT(c->bIsTrigger);
			auto pRigidBody = GameComponents<RigidBody>::Create(shapeAggregateEntity);
			RigidBody_init(pRigidBody);
			pRigidBody->bDynamic = false;

			handles.GetComponent(shapeAggregateEntity, rigidBodyFromSelf);
			OnRigidBodyLoaded(rigidBodyFromSelf, handles);
		}

		b3Transform localPose = b3Transform_identity;
		localPose.p = ToB3Vec3(c->vCenter);
		if (c.GetEntity() != actorEntity)
		{
			const b3Transform transformRelativeToActor = GetRelativeTransform(actorEntity, c.GetEntity(), handles);
			localPose = b3MulTransforms(localPose, transformRelativeToActor);
		}

		const auto& rbRtd = rigidBodyFromSelf.GetRuntimeData();
		b3ShapeDef shapeDef = MakeShapeDef(c->material, c->bIsTrigger, rigidBodyFromSelf->fDensity > Math::EPSILON ? rigidBodyFromSelf->fDensity : 1.0f);
		shapeDef.userData = &rtd;
		shapeDef.filter.groupIndex = rbRtd.iAggregateGroup;
		// Collision masks, contact events and the impulse switch are set by UpdateSimulationFilter
		// before the next step (dirtyShapeList); start with nothing colliding.
		shapeDef.filter.categoryBits = 0;
		shapeDef.filter.maskBits = 0;

		Optional<Identifier> id;
		handles.GetComponent(c.GetEntity(), id);
		if (id.Exists())
		{
			shapeDef.name = id.Force()->name;
		}

		rtd.shapeId = createShape(rbRtd.bodyId, shapeDef, localPose);
		ASSERT(B3_IS_NON_NULL(rtd.shapeId) && "Box3D could not create the shape");

		UpdateMass(rigidBodyFromSelf);
		handles.pPhysicsScene->dirtyShapeList.insert(&rtd);
	}

	// Sphere Collider

	template <>
	void OnLoaded<SphereCollider>(Component<SphereCollider>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		const float fRadius = c->fRadius;
		OnShapeLoaded(c, handles, [fRadius](b3BodyId bodyId, const b3ShapeDef& def, const b3Transform& pose)
		{
			b3Sphere sphere;
			sphere.center = pose.p;
			sphere.radius = fRadius;
			return b3CreateSphereShape(bodyId, &def, &sphere);
		});
	}

	template<>
	void OnActivate<SphereCollider>(Component<SphereCollider>& c)
	{
		OnActivateShape(c);
	}

	template<>
	void OnDeactivate<SphereCollider>(Component<SphereCollider>& c, ComponentLoadHandles& handles)
	{
		OnDeactivateShape(c, handles);
	}

	// Box Collider

	template <>
	void OnLoaded<BoxCollider>(Component<BoxCollider>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		const Vector3f vExtents = c->vExtents;
		OnShapeLoaded(c, handles, [vExtents](b3BodyId bodyId, const b3ShapeDef& def, const b3Transform& pose)
		{
			const b3BoxHull box = b3MakeTransformedBoxHull(vExtents.x, vExtents.y, vExtents.z, pose);
			return b3CreateHullShape(bodyId, &def, &box.base);
		});
	}

	template<>
	void OnActivate<BoxCollider>(Component<BoxCollider>& c)
	{
		OnActivateShape(c);
	}

	template<>
	void OnDeactivate<BoxCollider>(Component<BoxCollider>& c, ComponentLoadHandles& handles)
	{
		OnDeactivateShape(c, handles);
	}

	// Convex hull shapes. The hull is copied into the world (and shared between identical
	// hulls), so the caller keeps ownership of pHull.
	static b3ShapeId CreateHullShape(b3BodyId bodyId, const b3ShapeDef& def, const b3HullData* pHull, const b3Transform& pose, float fScale)
	{
		const bool bIdentity = pose.p.x == 0.0f && pose.p.y == 0.0f && pose.p.z == 0.0f && pose.q.s == 1.0f && fScale == 1.0f;
		if (bIdentity)
		{
			return b3CreateHullShape(bodyId, &def, pHull);
		}
		return b3CreateTransformedHullShape(bodyId, &def, pHull, pose, b3Vec3{ fScale, fScale, fScale });
	}

	// Cone Collider

	template <>
	void OnLoaded<ConeCollider>(Component<ConeCollider>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		Vector3f vOtherVec = Vector3f::Z_AXIS;
		if (DotProduct(c->vDirection, Vector3f::Z_AXIS) > 0.99f)
		{
			vOtherVec = Vector3f::X_AXIS;
		}
		const Vector3f vToCircle = CrossProduct(c->vDirection, vOtherVec).GetNormalised()*c->fRadius;

		// The cone's apex is at vCenter (which the shape pose also offsets by, as before)
		Vector3f vertices[B3_MAX_HULL_VERTICES];
		const uint32 uCircleVertexCount = Math::Clamp(1u + (uint32)(c->fRadius * 3), 3u, (uint32)B3_MAX_HULL_VERTICES - 1);
		uint32 uCount = 0;
		vertices[uCount++] = c->vCenter;
		for (uint32 uVertexIndex = 0; uVertexIndex < uCircleVertexCount; uVertexIndex++)
		{
			const float fAngle = (float)uVertexIndex / (float)uCircleVertexCount*Math::two_pi;
			const Vector3f vOffset = vToCircle*Quaternionf(c->vDirection, fAngle);
			vertices[uCount++] = c->vCenter + c->vDirection*c->fLength + vOffset;
		}

		b3HullData* pHull = PhysicsMeshCache::CreateHull(vertices, uCount);
		if (pHull == nullptr)
		{
			ASSERT(false && "Failed to generate convex mesh from cone geometry.");
			return;
		}
		OnShapeLoaded(c, handles, [pHull](b3BodyId bodyId, const b3ShapeDef& def, const b3Transform& pose)
		{
			return CreateHullShape(bodyId, def, pHull, pose, 1.0f);
		});
		b3DestroyHull(pHull);
	}

	template<>
	void OnActivate<ConeCollider>(Component<ConeCollider>& c)
	{
		OnActivateShape(c);
	}

	template<>
	void OnDeactivate<ConeCollider>(Component<ConeCollider>& c, ComponentLoadHandles& handles)
	{
		OnDeactivateShape(c, handles);
	}

	// Cylinder Collider

	template <>
	void OnLoaded<CylinderCollider>(Component<CylinderCollider>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		const memsize uCircleVertexCount = c->has_uCircleVertices ? c->uCircleVertices : Math::Min((memsize)16, 1 + (memsize)(c->fRadius * 5));
		b3HullData* pHull = PhysicsMeshCache::CreateCylinderHull(c->vCenter, c->vDirection, c->fRadius, c->fHeight, (uint32)uCircleVertexCount);
		ASSERT(pHull != nullptr);
		if (pHull != nullptr)
		{
			OnShapeLoaded(c, handles, [pHull](b3BodyId bodyId, const b3ShapeDef& def, const b3Transform& pose)
			{
				return CreateHullShape(bodyId, def, pHull, pose, 1.0f);
			});
			b3DestroyHull(pHull);
		}
	}

	template<>
	void OnActivate<CylinderCollider>(Component<CylinderCollider>& c)
	{
		OnActivateShape(c);
	}

	template<>
	void OnDeactivate<CylinderCollider>(Component<CylinderCollider>& c, ComponentLoadHandles& handles)
	{
		OnDeactivateShape(c, handles);
	}

	// Heightfield Collider
	template <>
	void OnLoaded<HeightFieldCollider>(Component<HeightFieldCollider>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		// FIXME: Once working this should be cached like the CollisionMesh as we may well re-use terrain sections
		auto& rtd = c.GetRuntimeData();

		if (c->szPakName[0] != '\0')
		{
			usg::string fullName = "Terrain/";
			fullName += c->szPakName;
			handles.pResourceMgr->LoadPackage(handles.pDevice, fullName.c_str());
		}

		HeightFieldResHndl heightField = handles.pResourceMgr->GetHeightfieldResource(c->szAsset);

		// The resource stores rows along X and columns along Z (sample = row * columns + column);
		// Box3D indexes heights as z * countX + x.
		const uint32 uRows = heightField->GetRows();
		const uint32 uColumns = heightField->GetColumns();
		const HeightFieldSample* pSamples = heightField->GetSamples();
		vector<float> heights(uRows * uColumns);
		vector<uint8> materials((uRows - 1) * (uColumns - 1));
		float fMin = FLT_MAX, fMax = -FLT_MAX;
		for (uint32 x = 0; x < uRows; x++)
		{
			for (uint32 z = 0; z < uColumns; z++)
			{
				const HeightFieldSample& s = pSamples[x * uColumns + z];
				heights[z * uRows + x] = (float)s.height;
				fMin = Math::Min(fMin, (float)s.height);
				fMax = Math::Max(fMax, (float)s.height);
				if (x + 1 < uRows && z + 1 < uColumns)
				{
					materials[z * (uRows - 1) + x] = (s.materialIndex0 & 0x7f) == HeightFieldSample::HoleMaterial ? B3_HEIGHT_FIELD_HOLE : 0;
				}
			}
		}

		b3HeightFieldDef hfDef = {};
		hfDef.heights = &heights[0];
		hfDef.materialIndices = &materials[0];
		hfDef.scale = b3Vec3{ c->fRowScale, c->fHeightScale, c->fColumnScale };
		hfDef.countX = (int)uRows;
		hfDef.countZ = (int)uColumns;
		hfDef.globalMinimumHeight = fMin;
		hfDef.globalMaximumHeight = fMax;
		rtd.pHeightfield = b3CreateHeightField(&hfDef);

		ASSERT(c->vCenter.MagnitudeSquared() == 0.0f && "Box3D height fields cannot be offset from their body");
		b3HeightFieldData* pHeightfield = rtd.pHeightfield;
		OnShapeLoaded(c, handles, [pHeightfield](b3BodyId bodyId, const b3ShapeDef& def, const b3Transform& pose)
		{
			return b3CreateHeightFieldShape(bodyId, &def, pHeightfield);
		});
	}

	template<>
	void OnActivate<HeightFieldCollider>(Component<HeightFieldCollider>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.pHeightfield = nullptr;
		OnActivateShape(c);
	}

	template<>
	void OnDeactivate<HeightFieldCollider>(Component<HeightFieldCollider>& c, ComponentLoadHandles& handles)
	{
		auto& rtd = c.GetRuntimeData();
		OnDeactivateShape(c, handles);
		if (rtd.pHeightfield)
		{
			// The shape held a reference to it, so it goes after the shape
			b3DestroyHeightField(rtd.pHeightfield);
			rtd.pHeightfield = nullptr;
		}
	}

	// Joint Shared functions

	static b3BodyId GetJointParentBody(Optional<usg::RigidBody, FromParents>& parentRigidBody, ComponentLoadHandles& handles)
	{
		if (parentRigidBody.Exists())
		{
			return parentRigidBody.Force().GetRuntimeData().bodyId;
		}
		// Box3D joints always connect two bodies; joints to the world use a static ground body
		auto& scene = *handles.pPhysicsScene;
		if (B3_IS_NULL(scene.groundBodyId))
		{
			b3BodyDef def = b3DefaultBodyDef();
			def.type = b3_staticBody;
			def.name = "Ground";
			scene.groundBodyId = b3CreateBody(scene.worldId, &def);
		}
		return scene.groundBodyId;
	}

	template<typename JointType>
	static void SetupJointDef(b3JointDef& def, Component<JointType>& c, b3BodyId bodyA, b3BodyId bodyB, const b3Transform& frameA, const b3Transform& frameB)
	{
		def.bodyIdA = bodyA;
		def.bodyIdB = bodyB;
		def.localFrameA = frameA;
		def.localFrameB = frameB;
		def.userData = c.GetEntity();
		if (c->bCanBreak)
		{
			// Box3D reports joints over these thresholds and the physics systems break them
			def.forceThreshold = c->breakForce.fLinear;
			def.torqueThreshold = c->breakForce.fAngular;
		}
		// bEnableProjection has no Box3D equivalent; its soft step solver keeps joints together
	}

	// Revolute Joint

	template<>
	void OnActivate<RevoluteJoint>(Component<RevoluteJoint>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.jointId = b3_nullJointId;
	}

	template<>
	void OnDeactivate<RevoluteJoint>(Component<RevoluteJoint>& c, ComponentLoadHandles& handles)
	{

	}

	template <>
	void OnLoaded<RevoluteJoint>(Component<RevoluteJoint>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		// Note: currently only parent-child joints supported. Consider adding support for joints between sibling rigid bodies.

		Required<usg::RigidBody, FromSelf> myRigidBody;
		Required<usg::RigidBody, FromParents> myParentsRigidBody;
		handles.GetComponent(c.GetEntity(), myRigidBody);
		handles.GetComponent(c.GetEntity(), myParentsRigidBody);
		EnsureRigidBodyLoaded(myRigidBody, handles);
		EnsureRigidBodyLoaded(myParentsRigidBody, handles);

		const b3BodyId body1 = myParentsRigidBody.GetRuntimeData().bodyId;
		const b3BodyId body2 = myRigidBody.GetRuntimeData().bodyId;
		ASSERT(B3_IS_NON_NULL(body1) && B3_IS_NON_NULL(body2));

		usg::Matrix4x4 mMat;
		mMat.SetRight(usg::Vector4f(c->vAxis, 0.0f));
		mMat.SetUp( usg::Vector4f(c->vFace, 0.0f) );
		usg::Vector4f vCross = usg::CrossProduct(mMat.vRight(), mMat.vUp());
		mMat.SetFace(vCross);

		Quaternionf qAxis;
		qAxis = mMat;
		qAxis.Normalise();

		// Usagi's hinge axis is the frame's X axis; Box3D's revolute joint turns about Z.
		// A quarter turn about Y takes Z onto X.
		const b3Quat qZtoX = { { 0.0f, 0.70710678f, 0.0f }, 0.70710678f };
		const b3Quat qFrame = b3NormalizeQuat(b3MulQuat(ToB3Quat(qAxis), qZtoX));

		const TransformComponent trans = TransformTool::GetRelativeTransform(myParentsRigidBody.GetEntity(), myRigidBody.GetEntity(), handles);
		b3Transform t1;
		t1.p = ToB3Vec3(trans.position);
		t1.q = b3NormalizeQuat(b3MulQuat(ToB3Quat(trans.rotation), qFrame));

		Required<TransformComponent> myTrans;
		handles.GetComponent(c.GetEntity(), myTrans);

		// FIXME: The code to handle inverting the transforms doesn't handle nested
		// entities. Although there's no good reason to inherit
		myTrans.Modify().bInheritFromParent = false;
		b3Transform t2;
		t2.p = b3Vec3_zero;
		t2.q = qFrame;

		b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
		SetupJointDef(def.base, c, body1, body2, t1, t2);
		def.enableMotor = c->bEnableMotor;
		if (c->fMinAngleDegrees > -180.f || c->fMaxAngleDegrees < 180.f)
		{
			def.enableLimit = true;
			def.lowerAngle = Math::Max(Math::DegToRad(c->fMinAngleDegrees), -0.99f * Math::pi);
			def.upperAngle = Math::Min(Math::DegToRad(c->fMaxAngleDegrees), 0.99f * Math::pi);
		}
		c.GetRuntimeData().jointId = b3CreateRevoluteJoint(handles.pPhysicsScene->worldId, &def);
	}

	// Fixed Joint

	template<>
	void OnActivate<FixedJoint>(Component<FixedJoint>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.jointId = b3_nullJointId;
	}

	template<>
	void OnDeactivate<FixedJoint>(Component<FixedJoint>& c, ComponentLoadHandles& handles)
	{

	}

	template <>
	void OnLoaded<FixedJoint>(Component<FixedJoint>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		// Note: currently only parent-child joints supported. Consider adding support for joints between sibling rigid bodies.

		Required<usg::RigidBody, FromSelf> myRigidBody;
		Optional<usg::RigidBody, FromParents> myParentsRigidBody;
		handles.GetComponent(c.GetEntity(), myRigidBody);
		handles.GetComponent(c.GetEntity(), myParentsRigidBody);
		EnsureRigidBodyLoaded(myRigidBody, handles);
		if (myParentsRigidBody.Exists())
		{
			EnsureRigidBodyLoaded(myParentsRigidBody, handles);
		}

		const b3BodyId body1 = GetJointParentBody(myParentsRigidBody, handles);
		const b3BodyId body2 = myRigidBody.GetRuntimeData().bodyId;
		ASSERT(B3_IS_NON_NULL(body2));

		const TransformComponent trans = TransformTool::GetRelativeTransform(myParentsRigidBody.Exists() ? myParentsRigidBody.Force().GetEntity() : nullptr, myRigidBody.GetEntity(), handles);
		const b3Transform t1 = MakeB3Transform(trans.position, trans.rotation);

		b3WeldJointDef def = b3DefaultWeldJointDef();
		SetupJointDef(def.base, c, body1, body2, t1, b3Transform_identity);
		c.GetRuntimeData().jointId = b3CreateWeldJoint(handles.pPhysicsScene->worldId, &def);
	}

	// Prismatic Joint

	template<>
	void OnActivate<PrismaticJoint>(Component<PrismaticJoint>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.jointId = b3_nullJointId;
	}

	template<>
	void OnDeactivate<PrismaticJoint>(Component<PrismaticJoint>& c, ComponentLoadHandles& handles)
	{

	}

	template <>
	void OnLoaded<PrismaticJoint>(Component<PrismaticJoint>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		// Note: currently only parent-child joints supported. Consider adding support for joints between sibling rigid bodies.

		Required<usg::RigidBody, FromSelf> myRigidBody;
		Optional<usg::RigidBody, FromParents> myParentsRigidBody;
		handles.GetComponent(c.GetEntity(), myRigidBody);
		handles.GetComponent(c.GetEntity(), myParentsRigidBody);
		EnsureRigidBodyLoaded(myRigidBody, handles);
		if (myParentsRigidBody.Exists())
		{
			EnsureRigidBodyLoaded(myParentsRigidBody, handles);
		}

		const b3BodyId body1 = GetJointParentBody(myParentsRigidBody, handles);
		const b3BodyId body2 = myRigidBody.GetRuntimeData().bodyId;
		ASSERT(B3_IS_NON_NULL(body2));

		const TransformComponent myTransformRelativeToConnectedParent = TransformTool::GetRelativeTransform(myParentsRigidBody.Exists() ? myParentsRigidBody.Force().GetEntity() : nullptr, myRigidBody.GetEntity(), handles);
		// Both PhysX and Box3D slide along the joint frame's X axis
		Quaternionf q1;
		q1.MakeVectorRotation(Vector3f::X_AXIS, c->vAxis);
		b3Transform t1;
		t1.p = ToB3Vec3(myTransformRelativeToConnectedParent.position);
		t1.q = ToB3Quat(q1);
		b3Transform t2;
		t2.p = b3Vec3_zero;
		t2.q = ToB3Quat(q1);

		b3PrismaticJointDef def = b3DefaultPrismaticJointDef();
		SetupJointDef(def.base, c, body1, body2, t1, t2);
		c.GetRuntimeData().jointId = b3CreatePrismaticJoint(handles.pPhysicsScene->worldId, &def);
	}

	// Aggregate

	template<>
	void OnActivate<PhysicsAggregate>(Component<PhysicsAggregate>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.iGroup = 0;
		rtd.bLoaded = false;
	}

	template<>
	void OnDeactivate<PhysicsAggregate>(Component<PhysicsAggregate>& c, ComponentLoadHandles& handles)
	{
		c.GetRuntimeData().iGroup = 0;
		c.GetRuntimeData().bLoaded = false;
	}

	template <>
	void OnLoaded<PhysicsAggregate>(Component<PhysicsAggregate>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		auto& rtd = c.GetRuntimeData();
		if (bWasPreviouslyCalled || rtd.bLoaded)
		{
			return;
		}
		// Shapes sharing a negative group never collide with each other. Group 0 leaves
		// self collision to the collision masks.
		rtd.iGroup = c->bEnableSelfCollisions ? 0 : handles.pPhysicsScene->iNextAggregateGroup--;
		rtd.bLoaded = true;
	}

	// Mesh Collider

	template <>
	void OnLoaded<MeshCollider>(Component<MeshCollider>& c, ComponentLoadHandles& handles, bool bWasPreviouslyCalled)
	{
		if (bWasPreviouslyCalled)
		{
			return;
		}

		auto& rtd = c.GetRuntimeData();

		Required<RigidBody, FromSelfOrParents> rigidBody;
		handles.GetComponent(c.GetEntity(), rigidBody);
		if (rigidBody->bDynamic && !c->bConvex)
		{
			// Box3D only collides triangle meshes on static bodies. Split the mesh into convex
			// submeshes if you REALLY need such a complex collision model in a dynamic entity.
			ASSERT(false);
			c.Modify().bConvex = true;
		}

		if (c->szPakName[0] != '\0')
		{
			usg::string fullName = "Models/";
			fullName += c->szPakName;
			handles.pResourceMgr->LoadPackage(handles.pDevice, fullName.c_str());
		}

		PhysicsMeshCache* pMeshCache = handles.pPhysicsScene->pMeshCache;
		const float fMeshScale = c->fMeshScale;
		if (c->bConvex)
		{
			const b3HullData* pConvexMesh = pMeshCache->GetConvexMesh(handles, c->szCollisionModel, c->szMeshName);
			rtd.pConvexMesh = pConvexMesh;
			OnShapeLoaded(c, handles, [pConvexMesh, fMeshScale](b3BodyId bodyId, const b3ShapeDef& def, const b3Transform& pose)
			{
				return CreateHullShape(bodyId, def, pConvexMesh, pose, fMeshScale);
			});
		}
		else
		{
			ASSERT(c->szMeshName[0]==0 && "Non-convex collision models with bones not supported as of now");
			if (c->szCollisionModel[0])
			{
				ASSERT(rtd.pTriangleMesh == nullptr);
				rtd.pTriangleMesh = pMeshCache->GetTriangleMesh(handles, c->szCollisionModel, c->bFlipNormals);
			}
			else
			{
				ASSERT(rtd.pTriangleMesh != nullptr && "If you do not specify collision mesh filename, you must programmatically create the b3MeshData between OnActivate and OnLoaded.");
			}
			const b3MeshData* pTriangleMesh = rtd.pTriangleMesh;
			OnShapeLoaded(c, handles, [pTriangleMesh, fMeshScale](b3BodyId bodyId, const b3ShapeDef& def, const b3Transform& pose)
			{
				// Box3D meshes take a scale but no local transform
				ASSERT(pose.p.x == 0.0f && pose.p.y == 0.0f && pose.p.z == 0.0f && pose.q.s == 1.0f && "Triangle mesh colliders must sit at their rigid body's origin");
				return b3CreateMeshShape(bodyId, &def, pTriangleMesh, b3Vec3{ fMeshScale, fMeshScale, fMeshScale });
			});
		}
	}

	template<>
	void OnActivate<MeshCollider>(Component<MeshCollider>& c)
	{
		auto& rtd = c.GetRuntimeData();
		rtd.pConvexMesh = nullptr;
		rtd.pTriangleMesh = nullptr;
		OnActivateShape(c);
	}

	template<>
	void OnDeactivate<MeshCollider>(Component<MeshCollider>& c, ComponentLoadHandles& handles)
	{
		auto& rtd = c.GetRuntimeData();
		OnDeactivateShape(c, handles);
		rtd.pConvexMesh = nullptr;
		rtd.pTriangleMesh = nullptr;
	}

}
