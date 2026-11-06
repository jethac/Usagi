/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Framework/GameComponents.h"
#include "Engine/Resource/CollisionModelResource.h"
#include "Engine/Resource/ResourceMgr.h"
#include "Engine/Physics/Physics.h"
#include "Engine/Physics/PhysicsMeshCache.h"
#include "Engine/Core/stl/set.h"
#include "Engine/Core/stl/vector.h"
#include "Engine/Maths/AABB.h"

namespace usg
{
	static const uint32 EmptyHash = utl::CRC32("");

	PhysicsMeshCache::PhysicsMeshCache()
	{

	}

	PhysicsMeshCache::~PhysicsMeshCache()
	{
		for (auto& it : m_triangleMeshCache)
		{
			b3DestroyMesh(it.second);
		}
		for (auto& it : m_convexMeshCache)
		{
			b3DestroyHull(it.second);
		}
	}

	const b3MeshData* PhysicsMeshCache::GetTriangleMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource, bool bFlipNormals)
	{
		const uint32 uHash = utl::CRC32(szCollisionModelResource) ^ (const uint32)bFlipNormals;
		auto it = m_triangleMeshCache.find(uHash);
		if (it != m_triangleMeshCache.end())
		{
			return it->second;
		}

		CollisionModelResHndl handle = handles.pResourceMgr->GetCollisionModel(szCollisionModelResource);
		ASSERT(handle);
		static_assert(sizeof(CollisionModelResource::TriangleIndices) == 3 * sizeof(int32_t), "Box3D meshes take 32 bit indices");

		b3MeshDef meshDef = {};
		meshDef.vertices = const_cast<b3Vec3*>(&ToB3Vec3(handle->GetVertices()[0]));
		meshDef.stride = sizeof(Vector3f);
		meshDef.indices = reinterpret_cast<int32_t*>(const_cast<CollisionModelResource::TriangleIndices*>(handle->GetTriangles()));
		meshDef.vertexCount = (int)handle->GetVertexCount();
		meshDef.triangleCount = (int)handle->GetTriangleCount();
		meshDef.weldVertices = false;
		meshDef.identifyEdges = true;
		// Flipping the normals is the same as reversing the winding
		meshDef.clockWiseWinding = bFlipNormals;

		b3MeshData* pMesh = b3CreateMesh(&meshDef, nullptr, 0);
		ASSERT(pMesh != nullptr && "Failed to generate triangle mesh.");
		m_triangleMeshCache[uHash] = pMesh;
		return pMesh;
	}

	void PhysicsMeshCache::preloadConvexMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource)
	{
		CollisionModelResHndl handle = handles.pResourceMgr->GetCollisionModel(szCollisionModelResource);
		ASSERT(handle);
		if (handle)
		{
			for (auto& bd : handle->GetSubmeshData())
			{
				GetConvexMesh(handles, szCollisionModelResource, bd.uNameHash);
			}
		}
	}

	b3HullData* PhysicsMeshCache::CreateHull(const Vector3f* pPoints, uint32 uCount)
	{
		ASSERT(uCount >= 4);
		b3HullData* pHull = b3CreateHull(&ToB3Vec3(pPoints[0]), (int)uCount, B3_MAX_HULL_VERTICES);
		if (pHull != nullptr)
		{
			return pHull;
		}

		DEBUG_PRINT("Warning: failed to build a convex hull, using its AABB as a fallback\n");
		AABB boundingBox;
		boundingBox.Invalidate();
		for (uint32 i = 0; i < uCount; i++)
		{
			boundingBox.Apply(pPoints[i]);
		}
		Vector3f points[8];
		for (memsize i = 0; i < 8; i++)
		{
			points[i].x = ((i & 1) == 0) ? boundingBox.GetMin().x : boundingBox.GetMax().x;
			points[i].y = ((i & 2) == 0) ? boundingBox.GetMin().y : boundingBox.GetMax().y;
			points[i].z = ((i & 4) == 0) ? boundingBox.GetMin().z : boundingBox.GetMax().z;
		}
		pHull = b3CreateHull(&ToB3Vec3(points[0]), 8, 8);
		ASSERT(pHull != nullptr && "Failed to generate convex mesh.");
		return pHull;
	}

	const b3HullData* PhysicsMeshCache::GetConvexMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource, uint32 uBoneNameHash)
	{
		const bool bFetchSubmesh = uBoneNameHash != EmptyHash;
		const uint32 uNameHash = utl::CRC32(szCollisionModelResource) ^ uBoneNameHash;
		auto it = m_convexMeshCache.find(uNameHash);
		if (it != m_convexMeshCache.end())
		{
			return it->second;
		}

		CollisionModelResHndl handle = handles.pResourceMgr->GetCollisionModel(szCollisionModelResource);
		ASSERT(handle);

		b3HullData* pHull = nullptr;
		if (!bFetchSubmesh)
		{
			pHull = CreateHull(handle->GetVertices(), handle->GetVertexCount());
		}
		else
		{
			set<uint32> boneVertexIndices;
			const auto boneDataSource = handle->GetBoneData(uBoneNameHash);
			for (const auto& smd : boneDataSource)
			{
				for (size_t uTriangleIndex = smd.uTrianglesBegin; uTriangleIndex < smd.uTrianglesEnd; uTriangleIndex++)
				{
					const auto& tri = handle->GetTriangles()[uTriangleIndex];
					boneVertexIndices.insert(tri.i1);
					boneVertexIndices.insert(tri.i2);
					boneVertexIndices.insert(tri.i3);
				}
			}
			vector<Vector3f> boneVertices;
			boneVertices.reserve(boneVertexIndices.size());
			for (auto uVertexIndex : boneVertexIndices)
			{
				boneVertices.push_back(handle->GetVertices()[uVertexIndex]);
			}
			pHull = CreateHull(&boneVertices[0], (uint32)boneVertices.size());
		}
		m_convexMeshCache[uNameHash] = pHull;
		return pHull;
	}

	const b3HullData* PhysicsMeshCache::GetConvexMesh(ComponentLoadHandles& handles, const char* szCollisionModelResource, const char* szMeshName)
	{
		return GetConvexMesh(handles, szCollisionModelResource, utl::CRC32(szMeshName));
	}

	b3HullData* PhysicsMeshCache::CreateCylinderHull(const Vector3f& vCenter, const Vector3f& vDir, float fRadius, float fHeight, uint32 uCircleVertices)
	{
		Vector3f vOtherVec = Vector3f::Z_AXIS;
		if (DotProduct(vDir, Vector3f::Z_AXIS) > 0.99f)
		{
			vOtherVec = Vector3f::X_AXIS;
		}
		const Vector3f vToCircle = CrossProduct(vDir, vOtherVec).GetNormalised()*fRadius;

		uCircleVertices = Math::Clamp(uCircleVertices, 3U, (uint32)(B3_MAX_HULL_VERTICES / 2));
		Vector3f vertices[B3_MAX_HULL_VERTICES];
		uint32 uCount = 0;
		for (uint32 uVertexIndex = 0; uVertexIndex < uCircleVertices; uVertexIndex++)
		{
			const float fAngle = (float)uVertexIndex / (float)uCircleVertices*Math::two_pi;
			const Vector3f vOffset = vToCircle*Quaternionf(vDir, fAngle);
			const Vector3f vTopVertex = vCenter + vDir*fHeight*0.5f + vOffset;
			vertices[uCount++] = vTopVertex;
			vertices[uCount++] = vTopVertex - vDir*fHeight;
		}
		return CreateHull(vertices, uCount);
	}

}
