/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Physics backend (Box3D) setup, constants and conversions
//	between Usagi and Box3D types.
*****************************************************************************/
#ifndef _USG_PHYSICS_PHYSICS_H_
#define _USG_PHYSICS_PHYSICS_H_

#include "Engine/Maths/Vector3f.h"
#include "Engine/Maths/Quaternionf.h"
#include "Engine/Maths/Matrix4x4.h"
#include "Engine/Framework/Component.h"
#include <box3d/box3d.h>

namespace usg
{
	class SystemCoordinator;
	struct ShapeRuntimeData;

	namespace Components
	{
		typedef struct _PhysicsScene PhysicsScene;
		typedef struct _TransformComponent TransformComponent;
		typedef struct _MatrixComponent MatrixComponent;
		typedef struct _CollisionMasks CollisionMasks;
		typedef struct _RigidBody RigidBody;
	}

	namespace physics
	{
		constexpr float32 DefaultTimeStep = 1.0f / 60.0f;

		// Box3D sub-steps every step this many times (its soft-step solver
		// replaces per-body position/velocity iteration counts).
		constexpr int SubStepCount = 4;

		void SetTimeStep(float32 fStep);
		float32 GetTimeStep();

		Components::TransformComponent ToUsgTransform(const b3Transform& t);

		void init();
		void deinit();
	}

	typedef struct _PhysicMaterial PhysicMaterial;

	struct PhysicsConstants
	{
		static constexpr uint32 MaxNumAsyncRaycastsPerFrame = 256;
		static constexpr uint32 MaxNumAsyncSweepsPerFrame = 256;
		static constexpr uint32 RaycastWorkBufferSize = 4096;
	};

	// Per-shape behaviour flags, kept in ShapeRuntimeData::uBitmask
	enum class ShapeBitmask : uint32
	{
		DisableImpulses = (uint32)(1 << 30),
		CCD = (uint32)(1 << 29),
	};

	// Conversion functions between usg and Box3D types (single precision build)

	static_assert(sizeof(b3Vec3) == sizeof(Vector3f), "Size mismatch between usg::Vector3f and b3Vec3");
	static_assert(sizeof(b3Quat) == sizeof(Quaternionf), "Size mismatch between usg::Quaternionf and b3Quat");
	static_assert(sizeof(b3Pos) == sizeof(b3Vec3), "Usagi builds Box3D in single precision");

	inline const b3Vec3& ToB3Vec3(const Vector3f& v)
	{
		return *(const b3Vec3*)(&v);
	}

	inline const Vector3f& ToUsgVec3(const b3Vec3& v)
	{
		return *(const Vector3f*)(&v);
	}

	// Both are stored x, y, z, w
	inline const b3Quat& ToB3Quat(const Quaternionf& q)
	{
		return *(const b3Quat*)(&q);
	}

	inline const Quaternionf& ToUsgQuaternionf(const b3Quat& q)
	{
		return *(const Quaternionf*)(&q);
	}

	inline b3Transform MakeB3Transform(const Vector3f& vPos, const Quaternionf& qRot)
	{
		b3Transform t;
		t.p = ToB3Vec3(vPos);
		t.q = ToB3Quat(qRot);
		return t;
	}

	usg::Matrix4x4 ToUsgMatrix(const b3Transform& trans);

	b3Transform ToB3Transform(const Components::TransformComponent& trans);
	b3Transform ToB3Transform(const Components::MatrixComponent& matrix);

	// Physics world initialization / deinitialization

	void InitPhysicsScene(Component<usg::Components::PhysicsScene>& p);
	void DeinitPhysicsScene(Component<usg::Components::PhysicsScene>& p);

	// Surface materials

	// The Box3D user material id carries the PhysicMaterial flags (low 32 bits) and
	// the friction / restitution combine modes, so contacts and ray casts can
	// recover them without a lookup.
	uint64 MakeUserMaterialId(const PhysicMaterial& m);
	inline uint32 GetMaterialFlags(uint64 uUserMaterialId) { return (uint32)(uUserMaterialId & 0xffffffffull); }
	b3SurfaceMaterial ToB3SurfaceMaterial(const PhysicMaterial& m);

	// Misc

	void UpdateSimulationFilter(ShapeRuntimeData* pShape, const usg::Components::CollisionMasks* pMasks, const usg::Components::RigidBody* pBody, Entity aggregateEntity);

	void GenerateOnCollisionSignals(SystemCoordinator& systemCoordinator, Required<usg::Components::PhysicsScene> scene);

}

#endif
