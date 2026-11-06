/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2016
****************************************************************************/

#pragma once

#include "Engine/Maths/Vector3f.h"
#include "Engine/Maths/Matrix4x4.h"
#include "Engine/Framework/Component.h"

namespace usg
{
	namespace Components
	{
		typedef struct _PhysicsScene PhysicsScene;
	}

	class EventManager;
	class SystemCoordinator;

	struct AsyncRaycastRequest
	{
		Vector3f vFrom;
		Vector3f vTo;
		uint32 uFilter = 0xffffffff;
		uint32 uMaxHits = 16;
		uint32 uRaycastId = 0;
	};

	// Shape swept by an AsyncSweepRequest, in the request's mTrans frame
	enum class SweepShape : uint32
	{
		Sphere,		// radius vHalfExtents.x
		Box,		// half extents vHalfExtents
		Capsule		// radius vHalfExtents.x, half height of the segment along local Y vHalfExtents.y
	};

	struct AsyncSweepRequest
	{
		uint32 uFilter = 0xffffffff;
		uint32 uMaxHits = 16;
		uint32 uRaycastId = 0;
		SweepShape eShape = SweepShape::Sphere;
		Vector3f vHalfExtents = Vector3f(0.5f, 0.5f, 0.5f);
		usg::Matrix4x4 mTrans;
		Vector3f vDir = Vector3f(0.0f, 0.0f, 1.0f);
		float fDist = 0.0f;
	};

	void ExecuteRaycasts(Required<usg::PhysicsScene> scene, EventManager& eventManager, SystemCoordinator& systemCoordinator);
}
