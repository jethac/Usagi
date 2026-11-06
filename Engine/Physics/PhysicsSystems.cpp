/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Framework/EventManager.h"
#include "Engine/Framework/FrameworkComponents.pb.h"
#include "Engine/Physics/PhysicsComponents.pb.h"
#include "Engine/Debug/Rendering/DebugRender.h"
#include "Engine/Debug/GlobalDebugStats.h"
#include "Engine/Scene/Common/SceneComponents.pb.h"
#include "Engine/Debug/DebugStats.h"
#include "Engine/Physics/Physics.h"
#include <limits>
#include "Engine/Physics/PhysicsEvents.pb.h"
#include "Engine/Scene/Common/SceneEvents.pb.h"
#include "Engine/Physics/PhysicsEventsInternal.pb.h"
#include "Engine/Physics/PhysicsSceneData.h"
#include "Engine/Physics/PhysicsProfile.h"
#include "Engine/Framework/ExclusionCheck.h"
#include "Engine/Physics/CollisionData.pb.h"
#ifndef FINAL_BUILD
#include "Engine/Scene/Common/SceneComponents.pb.h"
#include "Engine/Scene/Common/SceneComponents.h"
#include "Engine/Scene/Scene.h"
#endif
#include "Engine/Core/stl/array.h"

namespace usg
{
#ifndef FINAL_BUILD
	namespace physics
	{
		void DebugRender(const Camera& camera);
	}
#endif

	namespace physics
	{
		void ProcessStepEvents(PhysicsSceneData& rtd, EventManager* pEventManager);
	}

	namespace Systems {

		class AlterCollisionMasks : public System
		{
		public:
			struct Inputs
			{
				Required<EventManagerHandle, FromSelfOrParents> eventManager;
				Required<PhysicsScene, FromSelfOrParents> scene;
			};

			struct Outputs
			{
				Required<CollisionMasks> collisionMasks;
				Optional<SphereCollider> sphereCollider;
				Optional<BoxCollider> boxCollider;
				Optional<MeshCollider> meshCollider;
				Optional<ConeCollider> coneCollider;
				Optional< HeightFieldCollider> heightfieldCollider;
				Optional<CylinderCollider> cylinderCollider;
			};

			DECLARE_SYSTEM(SYSTEM_PHYSICS)

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const UpdateCollisionMasks& evt)
			{
				auto& masks = outputs.collisionMasks.Modify();
				if (evt.has_uFilter)
				{
					masks.uFilter = evt.uFilter;
				}
				if (evt.has_uGroup)
				{
					masks.uGroup = evt.uGroup;
				}
				usg::array<ShapeRuntimeData*, 8> ptrs;
				size_t uPtrCount = 0;
				if (outputs.sphereCollider.Exists())
				{
					ASSERT(uPtrCount < ptrs.size());
					ptrs[uPtrCount++] = static_cast<ShapeRuntimeData*>(&outputs.sphereCollider.Force().GetRuntimeData());
				}
				if (outputs.boxCollider.Exists())
				{
					ASSERT(uPtrCount < ptrs.size());
					ptrs[uPtrCount++] = static_cast<ShapeRuntimeData*>(&outputs.boxCollider.Force().GetRuntimeData());
				}
				if (outputs.meshCollider.Exists())
				{
					ASSERT(uPtrCount < ptrs.size());
					ptrs[uPtrCount++] = static_cast<ShapeRuntimeData*>(&outputs.meshCollider.Force().GetRuntimeData());
				}
				if (outputs.coneCollider.Exists())
				{
					ASSERT(uPtrCount < ptrs.size());
					ptrs[uPtrCount++] = static_cast<ShapeRuntimeData*>(&outputs.coneCollider.Force().GetRuntimeData());
				}
				if (outputs.cylinderCollider.Exists())
				{
					ASSERT(uPtrCount < ptrs.size());
					ptrs[uPtrCount++] = static_cast<ShapeRuntimeData*>(&outputs.cylinderCollider.Force().GetRuntimeData());
				}
				if (outputs.heightfieldCollider.Exists())
				{
					ASSERT(uPtrCount < ptrs.size());
					ptrs[uPtrCount++] = static_cast<ShapeRuntimeData*>(&outputs.heightfieldCollider.Force().GetRuntimeData());
				}
				for (size_t i = 0; i < uPtrCount; i++)
				{
					physics::details::Events::MarkShapeDirty dirtyEvt;
					physics::details::Events::MarkShapeDirty_init(&dirtyEvt);
					static_assert(sizeof(decltype(dirtyEvt.pShape)) >= sizeof(ShapeRuntimeData*), "Unable to store pointer into the integer type.");
					dirtyEvt.pShape = reinterpret_cast<uint64>(ptrs[i]);
					inputs.eventManager->handle->RegisterEventWithEntity(inputs.scene.GetEntity(), dirtyEvt);
				}
			}
		};


		// FIXME: This system can't be run on a thread - we need some way of signalling this
		class BeginPhysicsSimulationSystem : public System, public UnsafeComponentGetter
		{
		public:

			struct Inputs
			{
				Required<usg::Components::SimulationActive, FromSelfOrParents> simactive;
				Required<PhysicsScene, FromSelf> scene;
				Required<EventManagerHandle, FromSelfOrParents> eventManager;
			};

			struct Outputs
			{
				Required<PhysicsScene, FromSelf> scene;
			};

			DECLARE_SYSTEM(SYSTEM_PHYSICS)

			static void ProcessDirtyShapes(const Inputs& inputs, Outputs& outputs)
			{
				const auto& rtd = inputs.scene.GetRuntimeData().GetData();
				if (rtd.dirtyShapeList.size())
				{
					// Update collision filters / callbacks for dirty shapes
					auto& mutableRtd = outputs.scene.GetRuntimeData().GetData();
					for (auto& dirtyShapeData : mutableRtd.dirtyShapeList)
					{
						Required<CollisionMasks, FromSelfOrParents> collisionMasks;
						GetComponentImpl(dirtyShapeData->entity, collisionMasks);

						Optional<RigidBody, FromSelfOrParents> rigidBody;
						GetComponentImpl(dirtyShapeData->entity, rigidBody);

						const CollisionMasks* pMasks = &(*collisionMasks);
						const RigidBody* pBody = rigidBody.Exists() ? &(*rigidBody.Force()) : nullptr;

						UpdateSimulationFilter(dirtyShapeData, pMasks, pBody, dirtyShapeData->shapeAggregateEntity);
					}
					mutableRtd.dirtyShapeList.clear();
				}
			}

			static void ClearCollisions(const Inputs& inputs, Outputs& outputs)
			{
				const uint32 uCurrentTick = inputs.scene.GetRuntimeData().GetData().uTick;

				auto& mutableRtd = outputs.scene.GetRuntimeData().GetData();
				// Erase stale pairs in place; collecting them first would allocate every frame
				for (auto it = mutableRtd.collisions.begin(); it != mutableRtd.collisions.end(); )
				{
					it->second.contacts.clear();
					if (uCurrentTick - it->second.uTick > 5)
					{
						it = mutableRtd.collisions.erase(it);
					}
					else
					{
						++it;
					}
				}
			}

			static void PreSimulation(const Inputs& inputs, Outputs& outputs, float fDelta)
			{
				auto& mutableRtd = outputs.scene.GetRuntimeData().GetData();
				mutableRtd.bSimulationRunning = true;

				ProcessDirtyShapes(inputs, outputs);
				ClearCollisions(inputs, outputs);
			}

			static void Run(const Inputs& inputs, Outputs& outputs, float fDelta)
			{
				if (!inputs.simactive->bActive)
				{
					return;
				}
				physics::profile::BeginStep();
				PreSimulation(inputs, outputs, fDelta);
				// Box3D steps synchronously; FetchPhysicsResultsSystem turns its events into signals
				if (fDelta > 0.0f)
				{
					b3World_Step(inputs.scene.GetRuntimeData().GetData().worldId, fDelta, physics::SubStepCount);
				}
				physics::profile::EndStep();
			}


			static void OnEvent(const Inputs& inputs, Outputs& outputs, const ShiftWorldOrigin& evt)
			{
				PreSimulation(inputs, outputs, 0.0f);

				// Box3D has no origin shift; move every body instead
				const Vector3f vShift = evt.vShift;
				for (auto it = GameComponents<RigidBody>::GetIterator(); !it.IsEnd(); ++it)
				{
					const auto& rbRtd = (*it)->GetRuntimeData();
					if (B3_IS_NULL(rbRtd.bodyId) || !b3Body_IsValid(rbRtd.bodyId))
					{
						continue;
					}
					const b3Transform t = b3Body_GetTransform(rbRtd.bodyId);
					b3Body_SetTransform(rbRtd.bodyId, b3Sub(t.p, ToB3Vec3(vShift)), t.q);
				}

				PhysicsSceneDirty dirtyEvt;
				inputs.eventManager->handle->RegisterEvent(dirtyEvt);
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const physics::details::Events::MarkShapeDirty& evt)
			{
				auto& mutableRtd = outputs.scene.GetRuntimeData().GetData();
				ShapeRuntimeData* pShapeRtd = reinterpret_cast<ShapeRuntimeData*>(evt.pShape);
				mutableRtd.dirtyShapeList.insert(pShapeRtd);
			}
		};

		class FetchPhysicsResultsSystem : public System
		{
		public:

			struct Inputs
			{
				Required<EventManagerHandle, FromSelfOrParents> eventManager;
				Required<usg::Components::SimulationActive, FromSelfOrParents> simactive;
				Required<SceneComponent, FromSelfOrParents> visualScene;
				Required<usg::PhysicsScene, FromSelf> scene;
			};

			struct Outputs
			{
				Required<usg::PhysicsScene, FromSelf> scene;
			};

			DECLARE_SYSTEM(SYSTEM_POST_PHYSICS)

			static void PostSimulation(const Inputs& inputs, Outputs& outputs)
			{
				auto& mutableRtd = outputs.scene.GetRuntimeData().GetData();
				physics::ProcessStepEvents(mutableRtd, inputs.eventManager->handle);
				mutableRtd.uTick++;
				mutableRtd.bSimulationRunning = false;
			}

			static void Run(const Inputs& inputs, Outputs& outputs, float fDelta)
			{
				if (!inputs.simactive->bActive)
				{
					return;
				}
				ProfilingTimer timer;
				timer.Start();
				physics::profile::BeginStep();
				PostSimulation(inputs, outputs);
				physics::profile::EndStep();
				timer.Stop();
				if (physics::profile::IsEnabled())
				{
					const b3WorldId worldId = inputs.scene.GetRuntimeData().GetData().worldId;
					const b3Counters counters = b3World_GetCounters(worldId);
					physics::profile::EndFrame((uint32)counters.bodyCount, (uint32)b3World_GetAwakeBodyCount(worldId), (uint32)counters.contactCount);
				}
#ifndef FINAL_BUILD
				auto& mutableRtd = outputs.scene.GetRuntimeData().GetData();
				mutableRtd.diagnostics.fFetchResultsTime.Add(timer.GetTotalMilliSeconds());
				bool bPhysicsDebug = DebugStats::Inst()->GetPage() == GlobalDebugStats::PAGE_PHYSICS;
				if (mutableRtd.diagnostics.bDebugRenderOnNextFrame || bPhysicsDebug)
				{
					mutableRtd.diagnostics.bDebugRenderOnNextFrame = false;
					physics::DebugRender(*inputs.visualScene.GetRuntimeData().pScene->GetSceneCamera(0));
				}
#endif
			}

		};

		class TransformKinematicBody : public System
		{
		public:

			struct Inputs
			{
				Required<RigidBody> rigidBody;
				Required<TransformComponent> transform;
				Required<KinematicBodyTag> kinematicTag;
				Required<SceneComponent, FromSelfOrParents> visualScene;
			};

			struct Outputs
			{
				Required<TransformComponent> transform;
			};

			DECLARE_SYSTEM(SYSTEM_PRE_PHYSICS)

			// Convenience so reset code can be shared without knowing if kinematic
			static void OnEvent(const Inputs& inputs, Outputs& outputs, const FullPhysicsSync& msg)
			{
				// We set the transform
				outputs.transform.Modify().position = msg.vPosition;
				outputs.transform.Modify().rotation = msg.qRotation;

				ASSERT(inputs.rigidBody->bDynamic);
				Vector3f vPos = msg.vPosition;
				if (msg.bGlobalSpace)
				{
					vPos -= inputs.visualScene->vOriginOffset;
					outputs.transform.Modify().position -= inputs.visualScene->vOriginOffset;
				}
				b3Body_SetTransform(inputs.rigidBody.GetRuntimeData().bodyId, ToB3Vec3(vPos), ToB3Quat(msg.qRotation));
			}


			static void Run(const Inputs& inputs, Outputs& outputs, float fDelta)
			{
				const auto& rtd = inputs.rigidBody.GetRuntimeData();
				ASSERT(inputs.rigidBody->bKinematic);
				if ((rtd.uBitmask & static_cast<decltype(rtd.uBitmask)>(RuntimeData<RigidBody>::Bitmask::InScene)) == 0 || fDelta <= 0.0f)
				{
					return;
				}
				// Box3D drives the body to the target over the coming step
				b3Body_SetTargetTransform(rtd.bodyId, ToB3Transform(*inputs.transform), fDelta, true);
			}
		};


		class HandleSphereEvents : public System
		{
		public:

			struct Outputs
			{
				Required<SphereCollider> sphereCollider;
			};

			DECLARE_SYSTEM(SYSTEM_TRANSFORM_RIGIDBODIES)

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const ResizeSphere& evt)
			{
				outputs.sphereCollider.Modify().fRadius = evt.fNewRadius;
				const b3ShapeId shapeId = outputs.sphereCollider.GetRuntimeData().shapeId;
				b3Sphere sphere = b3Shape_GetSphere(shapeId);
				sphere.radius = evt.fNewRadius;
				b3Shape_SetSphere(shapeId, &sphere);
			}
		};

		class HandleRigidBodyEvents : public System
		{
		public:

			struct Inputs
			{
				Required<RigidBody> rigidBody;
				Required<DynamicBodyTag> dynamicTag;
				Required<SceneComponent, FromSelfOrParents> visualScene;
			};

			struct Outputs
			{
				Required<RigidBody> rigidBody;	// runtime speed caps
			};

			DECLARE_SYSTEM(SYSTEM_TRANSFORM_RIGIDBODIES)
			EXCLUSION(usg::pair<KinematicBodyTag, FromSelf>)

			static bool IsDynamic(const Inputs& inputs)
			{
				return inputs.rigidBody->bDynamic && !inputs.rigidBody->bKinematic;
			}

			static b3BodyId Body(const Inputs& inputs)
			{
				return inputs.rigidBody.GetRuntimeData().bodyId;
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const ApplyForce& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				const b3BodyId bodyId = Body(inputs);
				const b3Vec3 vForce = ToB3Vec3(evt.vForce);
				switch (evt.eForceMode)
				{
				case ForceMode_VELOCITY_CHANGE:
					b3Body_SetLinearVelocity(bodyId, b3Add(b3Body_GetLinearVelocity(bodyId), vForce));
					break;
				case ForceMode_ACCELERATION:
					b3Body_ApplyForceToCenter(bodyId, b3MulSV(b3Body_GetMass(bodyId), vForce), true);
					break;
				case ForceMode_IMPULSE:
					if (evt.has_vWorldPosition)
					{
						b3Body_ApplyLinearImpulse(bodyId, vForce, ToB3Vec3(evt.vWorldPosition), true);
					}
					else
					{
						b3Body_ApplyLinearImpulseToCenter(bodyId, vForce, true);
					}
					break;
				default:
					if (evt.has_vWorldPosition)
					{
						b3Body_ApplyForce(bodyId, vForce, ToB3Vec3(evt.vWorldPosition), true);
					}
					else
					{
						b3Body_ApplyForceToCenter(bodyId, vForce, true);
					}
					break;
				}
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const FullPhysicsSync& msg)
			{
				ASSERT(inputs.rigidBody->bDynamic);
				const b3BodyId bodyId = Body(inputs);

				Vector3f vPos = msg.vPosition;
				if (msg.bGlobalSpace)
				{
					vPos -= inputs.visualScene->vOriginOffset;
				}

				b3Body_SetTransform(bodyId, ToB3Vec3(vPos), ToB3Quat(msg.qRotation));
				b3Body_SetLinearVelocity(bodyId, ToB3Vec3(msg.vVelocity));
				b3Body_SetAngularVelocity(bodyId, ToB3Vec3(msg.vTorque));
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const ApplyTorque& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				const b3BodyId bodyId = Body(inputs);
				const b3Vec3 vTorque = ToB3Vec3(evt.vTorque);
				switch (evt.eForceMode)
				{
				case ForceMode_VELOCITY_CHANGE:
					b3Body_SetAngularVelocity(bodyId, b3Add(b3Body_GetAngularVelocity(bodyId), vTorque));
					break;
				case ForceMode_ACCELERATION:
					// An angular acceleration for the coming step, independent of inertia
					b3Body_SetAngularVelocity(bodyId, b3Add(b3Body_GetAngularVelocity(bodyId), b3MulSV(physics::GetTimeStep(), vTorque)));
					break;
				case ForceMode_IMPULSE:
					b3Body_ApplyAngularImpulse(bodyId, vTorque, true);
					break;
				default:
					b3Body_ApplyTorque(bodyId, vTorque, true);
					break;
				}
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const SetLinearVelocity& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				b3Body_SetLinearVelocity(Body(inputs), ToB3Vec3(evt.vVelocity));
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const OverrideAngularDamping& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				b3Body_SetAngularDamping(Body(inputs), evt.fDamping);
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const RestoreAngularDamping& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				b3Body_SetAngularDamping(Body(inputs), inputs.rigidBody->fAngularDamping);
			}


			static void OnEvent(const Inputs& inputs, Outputs& outputs, const OverrideLinearDamping& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				b3Body_SetLinearDamping(Body(inputs), evt.fDamping);
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const RestoreLinearDamping& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				b3Body_SetLinearDamping(Body(inputs), inputs.rigidBody->fLinearDamping);
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const AddLinearVelocity& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				const b3BodyId bodyId = Body(inputs);
				b3Body_SetLinearVelocity(bodyId, b3Add(ToB3Vec3(evt.vVelocity), b3Body_GetLinearVelocity(bodyId)));
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const SetAngularVelocity& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				b3Body_SetAngularVelocity(Body(inputs), ToB3Vec3(evt.vVelocity));
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const SetMaxAngularVelocity& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				// Applied after each step (Box3D has no per-body speed cap)
				outputs.rigidBody.GetRuntimeData().fMaxAngularSpeed = evt.fMax;
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const SetMaxLinearVelocity& evt)
			{
				if (!IsDynamic(inputs))
				{
					return;
				}
				// Applied after each step (Box3D has no per-body speed cap)
				outputs.rigidBody.GetRuntimeData().fMaxLinearSpeed = evt.fMax;
			}

		};

		class RigidBodyUpdateWorldMatrix : public System
		{
		public:
			struct Inputs
			{
				Optional<MatrixComponent, FromParents>      parentMtx; // Optional parent matrix
				Required<TransformComponent>				tran;
				Required<RigidBodyTransformUpdate>			rigidBodyUpdate;
			};

			struct Outputs
			{
				Required<MatrixComponent>      worldMtx;
			};

			DECLARE_SYSTEM(SYSTEM_TRANSFORM_NESTED_RIGIDBODIES)

			static  void Run(const Inputs& inputs, Outputs& outputs, float fDelta)
			{
				Matrix4x4& mOut = outputs.worldMtx.Modify().matrix;
				mOut = inputs.tran->rotation;
				mOut.Translate(inputs.tran->position.x, inputs.tran->position.y, inputs.tran->position.z);

				if (inputs.parentMtx.Exists() && inputs.tran->bInheritFromParent)
				{
					mOut = mOut * inputs.parentMtx.Force()->matrix;
				}
			}
		};

		// Writes a body's pose into its TransformComponent, relative to the parent's matrix
		// when the transform inherits from one.
		static void WriteBodyTransform(const RuntimeData<RigidBody>& rtd, const TransformComponent& current, const MatrixComponent* pParentMatrix, TransformComponent& transOut)
		{
			const b3Transform globalTransform = b3Body_GetTransform(rtd.bodyId);
			if (!current.bInheritFromParent || pParentMatrix == nullptr)
			{
				transOut.position = ToUsgVec3(globalTransform.p);
				transOut.rotation = ToUsgQuaternionf(globalTransform.q);
				return;
			}

			const b3Transform cumulativeParentTransform = ToB3Transform(*pParentMatrix);
			const b3Transform localTransform = b3InvMulTransforms(cumulativeParentTransform, globalTransform);
			transOut.position = ToUsgVec3(localTransform.p);
			transOut.rotation = ToUsgQuaternionf(localTransform.q);
		}

		class HandlePhysicsDirty : public System
		{
		public:

			struct Inputs
			{
				Required<RigidBody> rigidBody;
				Required<TransformComponent> transform;
				Optional<MatrixComponent, FromParents> parentMatrix;
			};

			struct Outputs
			{
				Required<TransformComponent> transform;
			};

			DECLARE_SYSTEM(SYSTEM_TRANSFORM_RIGIDBODIES)

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const PhysicsSceneDirty& evt)
			{
				WriteBodyTransform(inputs.rigidBody.GetRuntimeData(), *inputs.transform, inputs.parentMatrix.Exists() ? &(*inputs.parentMatrix.Force()) : nullptr, outputs.transform.Modify());
			}
		};

		class TransformRigidBody : public System
		{
		public:

			struct Inputs
			{
				Required<RigidBody> rigidBody;
				Required<DynamicBodyTag> dynamicTag;
			};

			struct Outputs
			{
				Required<TransformComponent> transform;
			};

			DECLARE_SYSTEM(SYSTEM_TRANSFORM_RIGIDBODIES)
			EXCLUSION(IfHas<SleepTag, FromSelf>, IfHas<TransformComponent, FromParents>, IfHas<KinematicBodyTag, FromSelf>)

			static void Run(const Inputs& inputs, Outputs& outputs, float fDelta)
			{
				ASSERT(inputs.rigidBody->bDynamic);
				const b3Transform t = b3Body_GetTransform(inputs.rigidBody.GetRuntimeData().bodyId);
				TransformComponent& transOut = outputs.transform.Modify();
				transOut.position = ToUsgVec3(t.p);
				transOut.rotation = ToUsgQuaternionf(t.q);
			}
		};


		class TransformRigidBodyWithParent : public System
		{
		public:

			struct Inputs
			{
				Required<RigidBody> rigidBody;
				Required<DynamicBodyTag> dynamicTag;
				Required<TransformComponent> transform;
				Required<MatrixComponent, FromParents> parentMatrix;
			};

			struct Outputs
			{
				Required<TransformComponent> transform;
			};

			DECLARE_SYSTEM(SYSTEM_TRANSFORM_RIGIDBODIES)
			EXCLUSION(IfHas<SleepTag, FromSelf>)

			static void Run(const Inputs& inputs, Outputs& outputs, float fDelta)
			{
				// Note that the performance of this solution is very slow. You should generally avoid nested rigid bodies.
				ASSERT(inputs.rigidBody->bDynamic);
				WriteBodyTransform(inputs.rigidBody.GetRuntimeData(), *inputs.transform, &(*inputs.parentMatrix), outputs.transform.Modify());
			}
		};

		namespace joint_details
		{
			template<typename JointType>
			void DestroyJoint(Required<JointType> c)
			{
				const b3JointId jointId = c.GetRuntimeData().jointId;
				if (B3_IS_NON_NULL(jointId))
				{
					if (b3Joint_IsValid(jointId))
					{
						b3DestroyJoint(jointId, true);
					}
					c.GetRuntimeData().jointId = b3_nullJointId;
				}
			}
		}

		class RevoluteJointSystem : public System
		{
		public:

			struct Inputs
			{
				Required<RevoluteJoint> joint;
			};

			struct Outputs
			{
				Required<RevoluteJoint> joint;
			};

			DECLARE_SYSTEM(SYSTEM_DEFAULT_PRIORITY)

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const SetRevoluteJointAngleLimits& evt)
			{
				const b3JointId jointId = inputs.joint.GetRuntimeData().jointId;
				if (B3_IS_NULL(jointId) || !b3Joint_IsValid(jointId))
				{
					return;
				}
				const bool bLimitEnabled = evt.has_fMin || evt.has_fMax;
				if (bLimitEnabled)
				{
					// Box3D limits are within +-0.99 pi
					const float fLimit = 0.99f * Math::pi;
					b3RevoluteJoint_SetLimits(jointId, evt.has_fMin ? Math::Max(evt.fMin, -fLimit) : -fLimit, evt.has_fMax ? Math::Min(evt.fMax, fLimit) : fLimit);
				}
				b3RevoluteJoint_EnableLimit(jointId, bLimitEnabled);
			}

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const DestroyJoint& evt)
			{
				joint_details::DestroyJoint(outputs.joint);
			}
		};

		class FixedJointSystem : public System
		{
		public:

			struct Inputs
			{
				Required<FixedJoint> joint;
			};

			struct Outputs
			{
				Required<FixedJoint> joint;
			};

			DECLARE_SYSTEM(SYSTEM_DEFAULT_PRIORITY)

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const DestroyJoint& evt)
			{
				joint_details::DestroyJoint(outputs.joint);
			}
		};

		class PrismaticJointSystem : public System
		{
		public:

			struct Inputs
			{
				Required<PrismaticJoint> joint;
			};

			struct Outputs
			{
				Required<PrismaticJoint> joint;
			};

			DECLARE_SYSTEM(SYSTEM_DEFAULT_PRIORITY)

			static void OnEvent(const Inputs& inputs, Outputs& outputs, const DestroyJoint& evt)
			{
				joint_details::DestroyJoint(outputs.joint);
			}
		};
	}
}

#include GENERATED_SYSTEM_CODE(Engine/Physics/PhysicsSystems.cpp)
