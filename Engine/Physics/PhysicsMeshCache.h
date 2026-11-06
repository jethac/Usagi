/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Box3D collision geometry built from collision models, shared
//	by every scene and freed at physics::deinit().
*****************************************************************************/
#pragma once

#include "Engine/Core/stl/hash_map.h"
#include "Engine/Maths/Vector3f.h"

typedef struct b3HullData b3HullData;
typedef struct b3MeshData b3MeshData;

namespace usg {

	struct ComponentLoadHandles;

	class PhysicsMeshCache
	{
	public:
		PhysicsMeshCache();
		~PhysicsMeshCache();

		const b3MeshData* GetTriangleMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource, bool bFlipNormals = false);
		const b3HullData* GetConvexMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource, const char* szMeshName = "");

		void preloadConvexMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource);

		// Convex hull of a point cloud (cylinders, cones). The caller owns the result
		// and frees it with b3DestroyHull; Box3D shapes copy hulls into their world.
		static b3HullData* CreateHull(const Vector3f* pPoints, uint32 uCount);
		static b3HullData* CreateCylinderHull(const Vector3f& vCenter, const Vector3f& vDir, float fRadius, float fHeight, uint32 uCircleVertices);

	private:
		const b3HullData* GetConvexMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource, uint32 uBoneNameHash);

		hash_map<uint32, b3MeshData*> m_triangleMeshCache;
		hash_map<uint32, b3HullData*> m_convexMeshCache;
	};

}
