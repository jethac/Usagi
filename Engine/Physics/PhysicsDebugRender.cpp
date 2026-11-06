#include "Engine/Common/Common.h"
#include "Engine/Physics/PhysicsComponents.pb.h"
#include "Engine/Physics/PhysicsComponents.h"
#include "Engine/Scene/Camera/Camera.h"
#include "Engine/Debug/Rendering/DebugRender.h"
#include "Engine/Network/NetTime.h"
#include "Engine/Physics/CollisionData.pb.h"

namespace usg
{
	namespace physics
	{
		static Color GetShapeColor(b3ShapeId shapeId)
		{
			return !b3Shape_IsSensor(shapeId) ? Color(1, 0, 1, 0.5f) : Color(1, 0, 0, 0.25f + 0.20f*sinf(7.0f*NetTime::GetServerTime()));
		}

		static void RenderHull(const b3HullData* pHull, const b3Transform& t, const Color& cColor)
		{
			const b3Vec3* pPoints = b3GetHullPoints(pHull);
			const b3HullHalfEdge* pEdges = b3GetHullEdges(pHull);
			for (int i = 0; i < pHull->edgeCount; i++)
			{
				// Each edge is stored twice (as twins); draw it once
				const b3HullHalfEdge& edge = pEdges[i];
				if (edge.twin < i)
				{
					continue;
				}
				const b3Vec3 p1 = b3TransformPoint(t, pPoints[edge.origin]);
				const b3Vec3 p2 = b3TransformPoint(t, pPoints[pEdges[edge.twin].origin]);
				Debug3D::GetRenderer()->AddLine(ToUsgVec3(p1), ToUsgVec3(p2), cColor, 0.01f);
			}
		}

		static void RenderShape(b3ShapeId shapeId, const b3Transform& bodyTransform)
		{
			const Color cColor = GetShapeColor(shapeId);
			switch (b3Shape_GetType(shapeId))
			{
			case b3_sphereShape:
			{
				const b3Sphere sphere = b3Shape_GetSphere(shapeId);
				Debug3D::GetRenderer()->AddSphere(ToUsgVec3(b3TransformPoint(bodyTransform, sphere.center)), sphere.radius, cColor);
				break;
			}
			case b3_hullShape:
				RenderHull(b3Shape_GetHull(shapeId), bodyTransform, cColor);
				break;
			default:
			{
				// Meshes and height fields: just their bounds
				const b3AABB aabb = b3Shape_GetAABB(shapeId);
				Matrix4x4 cube;
				cube.LoadIdentity();
				cube.MakeScale((ToUsgVec3(aabb.upperBound) - ToUsgVec3(aabb.lowerBound)) * 0.5f);
				cube.SetPos((ToUsgVec3(aabb.upperBound) + ToUsgVec3(aabb.lowerBound)) * 0.5f);
				Debug3D::GetRenderer()->AddCube(cube, cColor);
				break;
			}
			}
		}

		void DebugRender(const Camera& camera)
		{
			constexpr float VisibilityRadius = 60;
			const Vector3f vCamPos = camera.GetPos().v3();
			for (auto it = GameComponents<RigidBody>::GetIterator(); !it.IsEnd(); ++it)
			{
				const b3BodyId bodyId = (*it)->GetRuntimeData().bodyId;
				if (B3_IS_NULL(bodyId) || !b3Body_IsValid(bodyId))
				{
					continue;
				}
				const b3Transform t = b3Body_GetTransform(bodyId);
				const Vector4f vRigidBodyPos(ToUsgVec3(t.p), 1);
				if (!camera.GetFrustum().ArePointsInFrustum(&vRigidBodyPos, 1) || vRigidBodyPos.v3().GetSquaredDistanceFrom(vCamPos) > VisibilityRadius*VisibilityRadius)
				{
					continue;
				}

				b3ShapeId shapes[16];
				const int iShapeCount = b3Body_GetShapes(bodyId, shapes, 16);
				for (int i = 0; i < iShapeCount; i++)
				{
					RenderShape(shapes[i], t);
				}
				Debug3D::GetRenderer()->AddSphere(vRigidBodyPos.v3(), 0.15f, Color(0.0f,1.0f,0.0f,0.33f));
			}
		}
	}
}
