#include "Engine/Common/Common.h"
#include "Engine/Framework/GameComponents.h"
#include "Engine/Framework/ComponentEntity.h"
#include "Engine/Framework/ComponentStats.h"
#include "Engine/Framework/SystemCoordinator.h"

#include <cstdio>
#include <cstdlib>
#include <process.h>

namespace usg
{
	MemType g_uDefaultMemType = MEMTYPE_STANDARD;

	namespace mem
	{
		void* Alloc(MemType, MemAllocType, memsize uSize, uint32 uAlign, bool)
		{
			UNUSED_VAR(uAlign);
			return new char[uSize];
		}

		void Free(MemType, void* pData, bool)
		{
			delete[] (char*)pData;
		}

		void Free(void* pData)
		{
			delete[] (char*)pData;
		}
	}

	void SystemCoordinator::RegisterComponent(uint32, uint32, const ComponentHelper&)
	{
	}
}

namespace eastl
{
	void AssertionFailure(const char* pExpression)
	{
		printf("EASTL ASSERT FAILED: %s\n", pExpression);
		abort();
	}
}

namespace
{
	struct TestComponent
	{
		uint32 value;
	};
}

namespace usg
{
	template<>
	struct ComponentInitializer<TestComponent>
	{
		static void Init(TestComponent* pComponent)
		{
			pComponent->value = 0;
		}
	};
}

namespace
{
	bool Expect(bool condition, const char* szMessage)
	{
		if (!condition)
		{
			printf("FAILED: %s\n", szMessage);
			return false;
		}
		return true;
	}

	void InitECS()
	{
		usg::ComponentStats::Reset();
		usg::ComponentEntity::InitPool(32);
		usg::GameComponents<usg::Components::EntityID>::Init();
		usg::GameComponents<TestComponent>::Init();
	}

	bool TestDeferredChanged()
	{
		usg::Entity e = usg::CreateEntity(usg::ComponentEntity::GetRoot());
		e->ClearChanged();

		usg::ComponentEntity::BeginSystemExecution();
		e->SetChanged();
		const bool bChangedDuringExecution = e->HasChanged();
		usg::ComponentEntity::EndSystemExecution();

		bool ok = true;
		ok &= Expect(!bChangedDuringExecution, "changed request is deferred during system execution");
		usg::ComponentEntity::FlushDeferredStructureChanges();
		ok &= Expect(e->HasChanged(), "deferred changed request is flushed");
		return ok;
	}

	bool TestDeferredRequestFree()
	{
		usg::Entity e = usg::CreateEntity(usg::ComponentEntity::GetRoot());
		TestComponent* pData = usg::GameComponents<TestComponent>::Create(e);
		UNUSED_VAR(pData);
		usg::Component<TestComponent>* pComponent = usg::GameComponents<TestComponent>::GetComponent(e);
		e->ClearChanged();

		usg::ComponentEntity::BeginSystemExecution();
		pComponent->RequestFree();
		const bool bPendingDuringExecution = e->HasPendingDeletions();
		usg::ComponentEntity::EndSystemExecution();

		bool ok = true;
		ok &= Expect(!bPendingDuringExecution, "free request is deferred during system execution");
		ok &= Expect(pComponent->WaitingOnFree(), "deferred free request marks component waiting");
		usg::ComponentEntity::FlushDeferredStructureChanges();
		ok &= Expect(e->HasPendingDeletions(), "deferred free request marks pending delete");
		ok &= Expect(e->HasChanged(), "deferred free request marks entity changed");
		return ok;
	}
}

int main()
{
	InitECS();

	bool ok = true;
	ok &= TestDeferredChanged();
	ok &= TestDeferredRequestFree();

	if (ok)
	{
		printf("ComponentMutation tests passed\n");
		fflush(stdout);
		_exit(0);
	}

	fflush(stdout);
	_exit(1);
}
