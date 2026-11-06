/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2016
//	Description: Helper functions related to physics shapes
*****************************************************************************/

#include "Engine/Common/Common.h"
#include "Engine/Physics/PhysicsShapes.h"
#include "Engine/Physics/PhysicsComponents.h"

namespace usg
{
	namespace physics
	{
		namespace details
		{
			AABB GetAABB(const RuntimeData<Components::MeshCollider>& meshColliderRtd)
			{
				ASSERT(meshColliderRtd.pConvexMesh != nullptr || meshColliderRtd.pTriangleMesh != nullptr);
				AABB r;
				const b3AABB bounds = meshColliderRtd.pConvexMesh ? meshColliderRtd.pConvexMesh->aabb : meshColliderRtd.pTriangleMesh->bounds;
				r.SetMinMax(ToUsgVec3(bounds.lowerBound), ToUsgVec3(bounds.upperBound));
				return r;
			}
		}
	}
}