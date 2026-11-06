#include "Engine/Common/Common.h"
#include "Engine/Physics/PhysicsComponents.pb.h"
#include "Engine/Physics/PhysicsComponents.h"
#include "Engine/Physics/Raycast.h"
#include "Engine/Framework/EventManager.h"
#include "Engine/Debug/Rendering/DebugRender.h"
#include "Engine/Physics/Signals/OnRaycastHit.h"
#include "Engine/Physics/PhysicsSceneData.h"

namespace usg
{
	namespace physics
	{
		namespace details
		{
			static physics::PhysicsSceneData& GetSceneData(Entity callbackEntity)
			{
				// FIXME: THREADING ISSUE
				// We should make access to the raycasts queue publically accessible in a threadsafe way
				struct Getter : public UnsafeComponentGetter {		}getter;
				Required<usg::PhysicsScene, FromSelfOrParents> sceneFromSelfOrParents;
				getter.GetComponent(callbackEntity, sceneFromSelfOrParents);
				Required<usg::PhysicsScene> scene;
				getter.GetComponent(sceneFromSelfOrParents.GetEntity(), scene);
				// FIXME: THREADING ISSUE
				return scene.GetRuntimeData().GetData();
			}

			void RaycastAsync(Entity callbackEntity, uint32 uSystemId, const AsyncRaycastRequest& request)
			{
				ASSERT((request.vTo - request.vFrom).MagnitudeSquared() > Math::EPSILON);
				ASSERT(request.uMaxHits > 0);

				auto& sceneRtd = GetSceneData(callbackEntity);
				ASSERT(sceneRtd.raycastData.pendingRequests.size() < PhysicsConstants::MaxNumAsyncRaycastsPerFrame);
				sceneRtd.raycastData.pendingRequests.emplace_back(request, usg::pair<uint32, Entity>(uSystemId, callbackEntity));
			}

			void SweepAsync(Entity callbackEntity, uint32 uSystemId, const AsyncSweepRequest& request)
			{
				ASSERT(request.fDist > Math::EPSILON);
				ASSERT(Math::IsEqual(request.vDir.MagnitudeSquared(), 1.0f, 0.01f));
				ASSERT(request.uMaxHits > 0);

				auto& sceneRtd = GetSceneData(callbackEntity);
				ASSERT(sceneRtd.raycastData.pendingSweepRequests.size() < PhysicsConstants::MaxNumAsyncSweepsPerFrame);
				sceneRtd.raycastData.pendingSweepRequests.emplace_back(request, usg::pair<uint32, Entity>(uSystemId, callbackEntity));
			}
		}
	}

	// Collects the hits of one Box3D ray or shape cast
	struct CastCollector
	{
		vector<RaycastHitBase>* pHits;
		vector<Entity>* pEntities;
		uint32 uMaxHits;
		float fLength;
	};

	static float CollectCastHit(b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId, int triangleIndex, int childIndex, void* context)
	{
		CastCollector& collector = *(CastCollector*)context;
		const ShapeRuntimeData* pShapeRtd = physics::details::GetUserDataFromShape(shapeId);
		if (pShapeRtd == nullptr || pShapeRtd->entity == nullptr)
		{
			return -1.0f;	// ignore and continue
		}

		RaycastHitBase hit;
		hit.vPosition = ToUsgVec3(point);
		hit.vNormal = ToUsgVec3(normal);
		hit.fDistance = fraction * collector.fLength;
		hit.uMaterialFlags = GetMaterialFlags(userMaterialId);
		hit.uGroup = pShapeRtd->uGroup;

		if (collector.uMaxHits == 1)
		{
			// Closest hit only: keep the best so far and clip the cast to it
			if (collector.pHits->empty())
			{
				collector.pHits->push_back(hit);
				collector.pEntities->push_back(pShapeRtd->entity);
			}
			else
			{
				(*collector.pHits)[0] = hit;
				(*collector.pEntities)[0] = pShapeRtd->entity;
			}
			return fraction;
		}

		collector.pHits->push_back(hit);
		collector.pEntities->push_back(pShapeRtd->entity);
		return collector.pHits->size() < collector.uMaxHits ? 1.0f : 0.0f;
	}

	static b3QueryFilter MakeQueryFilter(uint32 uFilter)
	{
		// A shape is hit when its collision group is in the request's filter
		b3QueryFilter filter = b3DefaultQueryFilter();
		filter.categoryBits = B3_DEFAULT_CATEGORY_BITS;
		filter.maskBits = uFilter;
		return filter;
	}

	static void SortHitsByDistance(vector<RaycastHitBase>& hits, vector<Entity>& entities)
	{
		// Insertion sort: hit counts are small and this keeps entities paired with hits
		for (memsize i = 1; i < hits.size(); i++)
		{
			for (memsize j = i; j > 0 && hits[j].fDistance < hits[j - 1].fDistance; j--)
			{
				eastl::swap(hits[j], hits[j - 1]);
				eastl::swap(entities[j], entities[j - 1]);
			}
		}
	}

	static void SendHits(SystemCoordinator& systemCoordinator, physics::PhysicsSceneData& sceneRtd, Entity callbackEntity, uint32 uSystemId, uint32 uRaycastId)
	{
		auto& work = sceneRtd.raycastData.workData;
		ASSERT(work.entities.size() == work.hits.size());
		SortHitsByDistance(work.hits, work.entities);
		const uint32 uHitCount = (uint32)work.hits.size();
		RaycastHitBase* pHits = uHitCount > 0 ? &work.hits[0] : nullptr;
		Entity* pEntities = uHitCount > 0 ? &work.entities[0] : nullptr;

		OnRaycastHitSignal onRaycastHitSignal(uSystemId, uRaycastId, pHits, pEntities, uHitCount, &work.workBuffer[0], work.workBuffer.size());
		systemCoordinator.Trigger(callbackEntity, onRaycastHitSignal, ON_ENTITY);
	}

	static void ExecuteSweeps(physics::PhysicsSceneData& sceneRtd, SystemCoordinator& systemCoordinator)
	{
		auto& work = sceneRtd.raycastData.workData;
		for (const auto& pending : sceneRtd.raycastData.pendingSweepRequests)
		{
			const AsyncSweepRequest& request = pending.first;

			// Box3D casts a convex point cloud with a radius
			b3Vec3 points[8];
			b3ShapeProxy proxy;
			proxy.points = points;
			const Vector3f& e = request.vHalfExtents;
			switch (request.eShape)
			{
			case SweepShape::Box:
				for (uint32 i = 0; i < 8; i++)
				{
					points[i] = b3Vec3{ (i & 1) ? e.x : -e.x, (i & 2) ? e.y : -e.y, (i & 4) ? e.z : -e.z };
				}
				proxy.count = 8;
				proxy.radius = 0.0f;
				break;
			case SweepShape::Capsule:
				points[0] = b3Vec3{ 0.0f, -e.y, 0.0f };
				points[1] = b3Vec3{ 0.0f, e.y, 0.0f };
				proxy.count = 2;
				proxy.radius = e.x;
				break;
			default:
				points[0] = b3Vec3_zero;
				proxy.count = 1;
				proxy.radius = e.x;
				break;
			}

			// The proxy points are relative to the cast origin; apply the request's rotation
			const Quaternionf qRot = request.mTrans;
			const b3Quat q = ToB3Quat(qRot);
			for (int i = 0; i < proxy.count; i++)
			{
				points[i] = b3RotateVector(q, points[i]);
			}

			work.hits.clear();
			work.entities.clear();
			CastCollector collector{ &work.hits, &work.entities, request.uMaxHits, request.fDist };
			const b3Vec3 vTranslation = ToB3Vec3(request.vDir * request.fDist);
			b3World_CastShape(sceneRtd.worldId, ToB3Vec3(request.mTrans.vPos().v3()), &proxy, vTranslation, MakeQueryFilter(request.uFilter), CollectCastHit, &collector);

			SendHits(systemCoordinator, sceneRtd, pending.second.second, pending.second.first, request.uRaycastId);
		}
		sceneRtd.raycastData.pendingSweepRequests.clear();
	}

	void ExecuteRaycasts(Required<usg::PhysicsScene> scene, EventManager& eventManager, SystemCoordinator& systemCoordinator)
	{
		auto& sceneRtd = scene.GetRuntimeData().GetData();
		auto& work = sceneRtd.raycastData.workData;

		for (const auto& pending : sceneRtd.raycastData.pendingRequests)
		{
			const AsyncRaycastRequest& request = pending.first;
			const Vector3f vTranslation = request.vTo - request.vFrom;

			work.hits.clear();
			work.entities.clear();
			CastCollector collector{ &work.hits, &work.entities, request.uMaxHits, vTranslation.Magnitude() };
			b3World_CastRay(sceneRtd.worldId, ToB3Vec3(request.vFrom), ToB3Vec3(vTranslation), MakeQueryFilter(request.uFilter), CollectCastHit, &collector);

			SendHits(systemCoordinator, sceneRtd, pending.second.second, pending.second.first, request.uRaycastId);
		}
		sceneRtd.raycastData.pendingRequests.clear();

		ExecuteSweeps(sceneRtd, systemCoordinator);
	}

	void InitRaycasting(physics::PhysicsSceneData& sceneRtd)
	{
		// Reserve up front so queueing and collecting never allocate per frame
		sceneRtd.raycastData.pendingRequests.reserve(PhysicsConstants::MaxNumAsyncRaycastsPerFrame);
		sceneRtd.raycastData.pendingSweepRequests.reserve(PhysicsConstants::MaxNumAsyncSweepsPerFrame);
		sceneRtd.raycastData.workData.hits.reserve(64);
		sceneRtd.raycastData.workData.entities.reserve(64);
		sceneRtd.raycastData.workData.workBuffer.resize(PhysicsConstants::RaycastWorkBufferSize);
	}

}
