/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Stats which should always be available regardless of the mode
*****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Graphics/Color.h"
#include "Engine/Debug/Rendering/DebugRender.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Core/Timer/ProfilingTimer.h"
#include "Engine/Scene/SceneContext.h"
#include "Engine/Physics/Physics.h"
#include "Engine/Scene/Scene.h"
#include "PhysicsDebugStats.h"

#ifndef FINAL_BUILD

namespace usg {

	PhysicsDebugStats::PhysicsDebugStats() :
		IDebugStatGroup(),
		m_worldId(b3_nullWorldId)
	{

	}

	PhysicsDebugStats::~PhysicsDebugStats()
	{

	}

	void PhysicsDebugStats::Init(b3WorldId worldId)
	{
		m_worldId = worldId;
	}

	void PhysicsDebugStats::Cleanup(GFXDevice* pDevice)
	{
	
	}

	void PhysicsDebugStats::Update(float fElapsed)
	{

	}

	void PhysicsDebugStats::Draw(DebugRender* pRender)
	{
		switch (m_uActivePage)
		{
		case PAGE_COLLISION:
		case PAGE_JOINTS:
			DrawPhysicsPage(pRender);
			break;
		default:
			ASSERT(false);
		}
	}

	void PhysicsDebugStats::PreDraw(GFXDevice* pDevice)
	{

	}

	void PhysicsDebugStats::PostDraw(GFXDevice* pDevice)
	{

	}

	void PhysicsDebugStats::SetActive(bool bActive)
	{
		IDebugStatGroup::SetActive(bActive);
	}


	void PhysicsDebugStats::SetPage(uint32 uPage)
	{
		IDebugStatGroup::SetPage(uPage);
	}

	static Color ToColor(b3HexColor color, float fAlpha = 1.0f)
	{
		const uint32 c = (uint32)color;
		return Color(((c >> 16) & 0xff) / 255.0f, ((c >> 8) & 0xff) / 255.0f, (c & 0xff) / 255.0f, fAlpha);
	}

	static void DrawSegment(b3Pos p1, b3Pos p2, b3HexColor color, void* context)
	{
		Debug3D::GetRenderer()->AddLine(ToUsgVec3(p1), ToUsgVec3(p2), ToColor(color), 0.01f);
	}

	static void DrawPoint(b3Pos p, float size, b3HexColor color, void* context)
	{
		Debug3D::GetRenderer()->AddSphere(ToUsgVec3(p), 0.02f, ToColor(color));
	}

	static void DrawSphere(b3Pos p, float radius, b3HexColor color, float alpha, void* context)
	{
		Debug3D::GetRenderer()->AddSphere(ToUsgVec3(p), radius, ToColor(color, alpha));
	}

	static void DrawBox(b3Vec3 extents, b3WorldTransform transform, b3HexColor color, void* context)
	{
		Matrix4x4 cube;
		cube.LoadIdentity();
		cube.MakeScale(ToUsgVec3(extents));
		Matrix4x4 rot = ToUsgQuaternionf(transform.q);
		cube = cube * rot;
		cube.SetPos(ToUsgVec3(transform.p));
		Debug3D::GetRenderer()->AddCube(cube, ToColor(color, 0.5f));
	}

	void PhysicsDebugStats::DrawPhysicsPage(DebugRender* pRender)
	{
		Color cStatCol(0.0f, 1.0f, 1.0f, 1.0f);

		if (B3_IS_NULL(m_worldId) || !b3World_IsValid(m_worldId))
		{
			pRender->AddString("Physics system not set", 0.0f, 2.0f, cStatCol);
			return;
		}

		const b3Counters counters = b3World_GetCounters(m_worldId);
		const b3Profile profile = b3World_GetProfile(m_worldId);
		char szText[128];
		snprintf(szText, sizeof(szText), "Box3D: %d bodies (%d awake), %d contacts, %d joints, step %.3f ms",
			counters.bodyCount, b3World_GetAwakeBodyCount(m_worldId), counters.contactCount, counters.jointCount, profile.step);
		pRender->AddString(szText, 0.0f, 2.0f, cStatCol);

		// Shapes are drawn by PhysicsDebugRender; this page adds contacts or joints
		b3DebugDraw draw = b3DefaultDebugDraw();
		draw.DrawSegmentFcn = DrawSegment;
		draw.DrawPointFcn = DrawPoint;
		draw.DrawSphereFcn = DrawSphere;
		draw.DrawBoxFcn = DrawBox;
		draw.drawingBounds = b3World_GetBounds(m_worldId);
		draw.drawShapes = false;
		draw.drawContacts = m_uActivePage == PAGE_COLLISION;
		draw.drawContactNormals = m_uActivePage == PAGE_COLLISION;
		draw.drawJoints = m_uActivePage == PAGE_JOINTS;
		draw.drawJointExtras = m_uActivePage == PAGE_JOINTS;
		b3World_Draw(m_worldId, &draw, B3_DEFAULT_MASK_BITS);
	}

}

#endif
