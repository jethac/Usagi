#include "Engine/Common/Common.h"
#include "Engine/Framework/TransformTool.h"
#include "Engine/Physics/Physics.h"

namespace usg
{

	static TransformComponent GetTransform(Entity e, ComponentLoadHandles& handles)
	{
		Optional<TransformComponent> trans;
		handles.GetComponent(e, trans);
		if (trans.Exists())
		{
			return *trans.Force();
		}
		TransformComponent r;
		TransformComponent_init(&r);
		return r;
	}

	TransformComponent TransformTool::GetRelativeTransform(Entity parent, Entity child, ComponentLoadHandles& handles)
	{
		// FIXME: This won't handle any scaled objects
		TransformComponent usgTrans = GetTransform(child, handles);
		if (!usgTrans.bInheritFromParent)
		{
			const b3Transform p = ToB3Transform(usgTrans);
			const b3Transform parentTrans = ToB3Transform(GetTransform(parent, handles));

			return physics::ToUsgTransform(b3InvMulTransforms(parentTrans, p));
		}
		b3Transform trans = ToB3Transform(usgTrans);
		Entity e = child;
		while (e->GetParentEntity() != parent)
		{
			const b3Transform parentTrans = ToB3Transform(GetTransform(e->GetParentEntity(), handles));
			trans = b3MulTransforms(parentTrans, trans);
			e = e->GetParentEntity();
		}
		usgTrans.position = ToUsgVec3(trans.p);
		usgTrans.rotation = ToUsgQuaternionf(trans.q);
		return usgTrans;
	}

	Vector3f TransformTool::TransformVector3(const TransformComponent& trans, const Vector3f& v)
	{
		return v*trans.rotation + trans.position;
	}
}