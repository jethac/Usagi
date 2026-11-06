/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A model for collisions
*****************************************************************************/
#ifndef _USG_PHYSICS_PHYSICS_COMPONENTS_H_
#define _USG_PHYSICS_PHYSICS_COMPONENTS_H_

#include "Engine/Resource/ResourceDecl.h"
#include "Engine/Physics/Physics.h"
#include "Engine/Framework/ComponentEntity.h"
#include "Engine/Physics/PhysicsComponents.pb.h"
#include <utility>

namespace usg
{
	namespace physics
	{
		struct PhysicsSceneData;
	}

	// Runtime data shared by every collider type. The Box3D shape's user data points here.
	struct ShapeRuntimeData
	{
		b3ShapeId shapeId;
		Entity shapeAggregateEntity;
		Entity entity;
		uint32 uMaterialFlags = 0;	// PhysicMaterial::uMaterialFlags of the collider
		uint32 uGroup = 0;			// CollisionMasks::uGroup
		uint32 uFilter = 0;			// CollisionMasks::uFilter
		uint32 uReportMask = 0;		// OnCollision mask of the shape aggregate entity
		uint32 uBitmask = 0;		// ShapeBitmask
		void release();
	};

	struct ActorRuntimeData
	{
		enum class Bitmask
		{
			InScene = 1
		};
		uint32 uBitmask;
		b3BodyId bodyId;
	};

	template<>
	struct RuntimeData<usg::Components::PhysicsScene>
	{
		physics::PhysicsSceneData* pSceneData;
		physics::PhysicsSceneData& GetData()
		{
			return *pSceneData;
		}
		const physics::PhysicsSceneData& GetData() const
		{
			return *pSceneData;
		}
	};

	template<>
	struct RuntimeData<usg::Components::RigidBody> : public ActorRuntimeData
	{
		// Box3D has no per-body speed caps; SetMaxLinearVelocity / SetMaxAngularVelocity
		// are applied after each step. Zero means no cap.
		float fMaxLinearSpeed;
		float fMaxAngularSpeed;
		sint32 iAggregateGroup;		// Box3D filter group shared by a PhysicsAggregate's bodies
	};

	template<>
	struct RuntimeData<usg::Components::SphereCollider> : public ShapeRuntimeData
	{

	};

	template<>
	struct RuntimeData<usg::Components::BoxCollider> : public ShapeRuntimeData
	{

	};

	template<>
	struct RuntimeData<usg::Components::ConeCollider> : public ShapeRuntimeData
	{

	};

	template<>
	struct RuntimeData<usg::Components::CylinderCollider> : public ShapeRuntimeData
	{

	};

	template<>
	struct RuntimeData<usg::Components::MeshCollider> : public ShapeRuntimeData
	{
		// Exactly one is set. Convex hulls are owned by the mesh cache, triangle
		// meshes by the mesh cache or (when szCollisionModel is empty) by whoever
		// filled pTriangleMesh between OnActivate and OnLoaded.
		const b3HullData* pConvexMesh;
		const b3MeshData* pTriangleMesh;
	};

	template<>
	struct RuntimeData<usg::Components::HeightFieldCollider> : public ShapeRuntimeData
	{
		b3HeightFieldData* pHeightfield;
	};

	template<>
	struct RuntimeData<usg::Components::PrismaticJoint>
	{
		b3JointId jointId;
	};

	template<>
	struct RuntimeData<usg::Components::RevoluteJoint>
	{
		b3JointId jointId;
	};

	template<>
	struct RuntimeData<usg::Components::FixedJoint>
	{
		b3JointId jointId;
	};

	template<>
	struct RuntimeData<usg::Components::PhysicsAggregate>
	{
		// Box3D has no aggregates. Bodies attached to one share the aggregate entity in
		// collision reports and, unless bEnableSelfCollisions, a negative filter group so
		// they never collide with each other.
		sint32 iGroup;
		bool bLoaded;
	};

	namespace physics
	{
		namespace details
		{
			inline Vector3f GetVelocityAtPosition(const RuntimeData<Components::RigidBody>& rbRtd, const Vector3f& vWorldPosition)
			{
				return b3Body_GetType(rbRtd.bodyId) != b3_staticBody ? ToUsgVec3(b3Body_GetWorldPointVelocity(rbRtd.bodyId, ToB3Vec3(vWorldPosition))) : Vector3f::ZERO;
			}

			inline ShapeRuntimeData* GetUserDataFromShape(b3ShapeId shapeId)
			{
				ASSERT(B3_IS_NON_NULL(shapeId));
				return (ShapeRuntimeData*)b3Shape_GetUserData(shapeId);
			}

			inline Vector3f GetLinearVelocity(const RuntimeData<Components::RigidBody>& rbRtd)
			{
				return ToUsgVec3(b3Body_GetLinearVelocity(rbRtd.bodyId));
			}

			inline Vector3f GetAngularVelocity(const RuntimeData<Components::RigidBody>& rbRtd)
			{
				return ToUsgVec3(b3Body_GetAngularVelocity(rbRtd.bodyId));
			}

			// Move a body to a pose this frame, optionally zeroing its velocity. For teleports
			// (respawns, checkpoint restores); dynamic bodies should otherwise be pushed with
			// forces or velocity events.
			void SetGlobalPose(const RuntimeData<Components::RigidBody>& rbRtd, const Vector3f& vPosition, const Quaternionf& qRotation, bool bResetVelocity);
		}

		template<typename T>
		Vector3f GetLinearVelocity(Optional<RigidBody, T> rigidBody)
		{
			ASSERT(rigidBody.Exists());
			if (rigidBody.Force()->bDynamic)
			{
				return details::GetLinearVelocity(rigidBody.Force().GetRuntimeData());
			}
			return Vector3f::ZERO;
		}

		template<typename T>
		Vector3f GetLinearVelocity(Required<RigidBody, T> rigidBody)
		{
			if (rigidBody->bDynamic)
			{
				return details::GetLinearVelocity(rigidBody.GetRuntimeData());
			}
			return Vector3f::ZERO;
		}

		template<typename T>
		float GetLinearDamping(Required<RigidBody, T> rigidBody)
		{
			if (rigidBody->bDynamic)
			{
				return b3Body_GetLinearDamping(rigidBody.GetRuntimeData().bodyId);
			}
			return 0.0f;
		}

		template<typename T>
		float GetAngularDampning(Required<RigidBody, T> rigidBody)
		{
			if (rigidBody->bDynamic)
			{
				return b3Body_GetAngularDamping(rigidBody.GetRuntimeData().bodyId);
			}
			return 0.0f;
		}

		template<typename T>
		Vector3f GetAngularVelocity(Required<RigidBody, T> rigidBody)
		{
			if (rigidBody->bDynamic)
			{
				return details::GetAngularVelocity(rigidBody.GetRuntimeData());
			}
			return usg::Vector3f::ZERO;
		}

		template<typename T>
		Vector3f GetVelocityAtPosition(Optional<RigidBody, T> rigidBody, const Vector3f& vWorldPosition)
		{
			ASSERT(rigidBody.Exists());
			return details::GetVelocityAtPosition(rigidBody.Force().GetRuntimeData(), vWorldPosition);
		}

		template<typename T>
		Vector3f GetVelocityAtPosition(Required<RigidBody, T> rigidBody, const Vector3f& vWorldPosition)
		{
			return details::GetVelocityAtPosition(rigidBody.GetRuntimeData(), vWorldPosition);
		}

		template<typename T>
		void SetGlobalPose(Required<RigidBody, T> rigidBody, const Vector3f& vPosition, const Quaternionf& qRotation, bool bResetVelocity = true)
		{
			details::SetGlobalPose(rigidBody.GetRuntimeData(), vPosition, qRotation, bResetVelocity);
		}

	}
}


#endif
