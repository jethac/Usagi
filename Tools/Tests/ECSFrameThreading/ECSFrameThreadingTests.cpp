#include "Engine/Common/Common.h"
#include "Engine/Framework/SystemCoordinator.h"
#include "Engine/Framework/EventManager.h"
#include "Engine/Framework/ComponentEntity.h"
#include "Engine/Framework/ComponentStats.h"
#include "Engine/Graphics/GPUUpdate.h"

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

	void EventManager::RegisterEventWithEntityAtTime(Entity, const uint32, const void*, const memsize, uint32, float64)
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
	struct FrameInput
	{
		uint32 value;
	};

	struct FrameOutput
	{
		uint32 runSum;
		uint32 parentSum;
		uint32 eventSum;
		uint32 lateSum;
		uint32 gpuSum;
	};

	struct DeleteMarker
	{
		uint32 value;
	};

	struct TestEvent
	{
		uint32 add;
	};
}

namespace usg
{
	template<> struct ComponentInitializer<FrameInput> { static void Init(FrameInput* p) { p->value = 0; } };
	template<> struct ComponentInitializer<FrameOutput> { static void Init(FrameOutput* p) { usg::MemSet(p, 0, sizeof(*p)); } };
	template<> struct ComponentInitializer<DeleteMarker> { static void Init(DeleteMarker* p) { p->value = 0; } };
}

namespace
{
	static constexpr uint32 TEST_EVENT_SIGNAL_ID = 0x9e570001;

	struct TestEventSignal : public usg::Signal
	{
		static constexpr uint32 ID = TEST_EVENT_SIGNAL_ID;

		TestEventSignal(const TestEvent& eventData)
			: Signal(ID)
			, event(eventData)
		{
		}

		const TestEvent& event;
	};

	struct RunValueSystem : public usg::System
	{
		struct Inputs { usg::Required<FrameInput> input; };
		struct Outputs { usg::Required<FrameOutput> output; };
		static const usg::SystemCategory CATEGORY = usg::SYSTEM_DEFAULT_PRIORITY;
		static const char* Name() { return "RunValueSystem"; }
		static bool GetInputOutputs(usg::ComponentGetter& getter, Inputs& inputs, Outputs& outputs) { return getter(inputs.input) && getter(outputs.output); }
	};

	struct ParentValueSystem : public usg::System
	{
		struct Inputs { usg::Required<FrameInput> input; usg::Required<FrameInput, usg::FromParents> parentInput; };
		struct Outputs { usg::Required<FrameOutput> output; };
		static const usg::SystemCategory CATEGORY = usg::SYSTEM_DEFAULT_PRIORITY;
		static const char* Name() { return "ParentValueSystem"; }
		static bool GetInputOutputs(usg::ComponentGetter& getter, Inputs& inputs, Outputs& outputs)
		{
			return getter(inputs.input) && getter(outputs.output) && getter(inputs.parentInput);
		}
	};

	struct EventValueSystem : public usg::System
	{
		struct Inputs { usg::Required<FrameInput> input; };
		struct Outputs { usg::Required<FrameOutput> output; };
		static const usg::SystemCategory CATEGORY = usg::SYSTEM_POST_GAMEPLAY;
		static const char* Name() { return "EventValueSystem"; }
		static bool GetInputOutputs(usg::ComponentGetter& getter, Inputs& inputs, Outputs& outputs) { return getter(inputs.input) && getter(outputs.output); }
	};

	struct LateValueSystem : public usg::System
	{
		struct Inputs { usg::Required<FrameInput> input; };
		struct Outputs { usg::Required<FrameOutput> output; };
		static const usg::SystemCategory CATEGORY = usg::SYSTEM_POST_GAMEPLAY;
		static const char* Name() { return "LateValueSystem"; }
		static bool GetInputOutputs(usg::ComponentGetter& getter, Inputs& inputs, Outputs& outputs) { return getter(inputs.input) && getter(outputs.output); }
	};

	struct GPUValueSystem : public usg::System
	{
		struct Inputs { usg::Required<FrameInput> input; };
		struct Outputs { usg::Required<FrameOutput> output; };
		static const usg::SystemCategory CATEGORY = usg::SYSTEM_SCENE;
		static const char* Name() { return "GPUValueSystem"; }
		static bool GetInputOutputs(usg::ComponentGetter& getter, Inputs& inputs, Outputs& outputs) { return getter(inputs.input) && getter(outputs.output); }
	};

	template<typename System>
	uint32 RequiredKey(uint32 uOffset)
	{
		const uint32 ids[] = { usg::Component<FrameInput>::GetTypeID(), usg::Component<FrameOutput>::GetTypeID() };
		uint32 key = 0;
		for (uint32 i = 0; i < ARRAY_SIZE(ids); ++i)
		{
			if (ids[i] / usg::BITFIELD_LENGTH == uOffset)
			{
				key |= 1 << (ids[i] % usg::BITFIELD_LENGTH);
			}
		}
		return key;
	}

	uint32 SingleComponentKey(uint32 uComponentId, uint32 uOffset)
	{
		return uComponentId / usg::BITFIELD_LENGTH == uOffset ? 1 << (uComponentId % usg::BITFIELD_LENGTH) : 0;
	}
}

namespace usg
{
	template<> uint32 GetSystemKey<RunValueSystem>(uint32 uOffset) { return RequiredKey<RunValueSystem>(uOffset); }
	template<> uint32 GetSystemReadKey<RunValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameInput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemWriteKey<RunValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameOutput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemParentReadKey<RunValueSystem>(uint32) { return 0; }

	template<> uint32 GetSystemKey<ParentValueSystem>(uint32 uOffset) { return RequiredKey<ParentValueSystem>(uOffset); }
	template<> uint32 GetSystemReadKey<ParentValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameInput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemWriteKey<ParentValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameOutput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemParentReadKey<ParentValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameInput>::GetTypeID(), uOffset); }

	template<> uint32 GetSystemKey<EventValueSystem>(uint32 uOffset) { return RequiredKey<EventValueSystem>(uOffset); }
	template<> uint32 GetSystemReadKey<EventValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameInput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemWriteKey<EventValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameOutput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemParentReadKey<EventValueSystem>(uint32) { return 0; }

	template<> uint32 GetSystemKey<LateValueSystem>(uint32 uOffset) { return RequiredKey<LateValueSystem>(uOffset); }
	template<> uint32 GetSystemReadKey<LateValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameInput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemWriteKey<LateValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameOutput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemParentReadKey<LateValueSystem>(uint32) { return 0; }

	template<> uint32 GetSystemKey<GPUValueSystem>(uint32 uOffset) { return RequiredKey<GPUValueSystem>(uOffset); }
	template<> uint32 GetSystemReadKey<GPUValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameInput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemWriteKey<GPUValueSystem>(uint32 uOffset) { return SingleComponentKey(Component<FrameOutput>::GetTypeID(), uOffset); }
	template<> uint32 GetSystemParentReadKey<GPUValueSystem>(uint32) { return 0; }
}

namespace
{
	template<typename System, typename Func>
	void TraverseSystem(usg::GenericInputOutputs* pIO, Func func)
	{
		if (pIO == nullptr)
		{
			return;
		}

		typedef typename usg::ComponentSystemInputOutputs<System>::InputOutputs IO;
		IO* pTyped = static_cast<IO*>(pIO);
		func(*pTyped);

		for (usg::GenericInputOutputs* pChild = pIO->GetChildEntity(); pChild != nullptr; pChild = pChild->GetNextSibling())
		{
			TraverseSystem<System>(pChild, func);
		}
	}

	void TriggerRunBranch(usg::GenericInputOutputs* pBranchRoot, void*, void*)
	{
		TraverseSystem<RunValueSystem>(pBranchRoot, [](auto& io) {
			io.outputs.output.Modify().runSum += io.inputs.input->value;
			usg::Component<DeleteMarker>* pDelete = usg::GameComponents<DeleteMarker>::GetComponent(io.outputs.output.GetEntity());
			if (pDelete != nullptr && pDelete->GetData().value != 0)
			{
				pDelete->RequestFree();
			}
		});
	}

	void TriggerParentBranch(usg::GenericInputOutputs* pBranchRoot, void*, void*)
	{
		TraverseSystem<ParentValueSystem>(pBranchRoot, [](auto& io) {
			io.outputs.output.Modify().parentSum += io.inputs.input->value + io.inputs.parentInput->value;
		});
	}

	void TriggerEventBranch(usg::GenericInputOutputs* pBranchRoot, void* pSignal, void*)
	{
		const TestEvent& eventData = ((TestEventSignal*)pSignal)->event;
		TraverseSystem<EventValueSystem>(pBranchRoot, [&eventData](auto& io) {
			io.outputs.output.Modify().eventSum += io.inputs.input->value + eventData.add;
		});
	}

	void TriggerLateBranch(usg::GenericInputOutputs* pBranchRoot, void* pSignal, void*)
	{
		const usg::LateUpdateSignal* pLate = (const usg::LateUpdateSignal*)pSignal;
		TraverseSystem<LateValueSystem>(pBranchRoot, [pLate](auto& io) {
			io.outputs.output.Modify().lateSum += io.inputs.input->value + (uint32)(pLate->dt * 1000.0f);
		});
	}

	void TriggerGPUBranch(usg::GenericInputOutputs* pBranchRoot, void*, void*)
	{
		TraverseSystem<GPUValueSystem>(pBranchRoot, [](auto& io) {
			io.outputs.output.Modify().gpuSum += io.inputs.input->value + 3;
		});
	}

	template<typename System>
	void RegisterTestSystem(usg::SystemCoordinator& coordinator, uint32 uSignalId, void(*pTriggerRootBranch)(usg::GenericInputOutputs*, void*, void*))
	{
		usg::InitSystem<System>();
		coordinator.RegisterSystem<System>();
		usg::SignalRunner runner;
		runner.systemID = usg::GetSystemId<System>();
		runner.priority = (sint32)System::CATEGORY;
		runner.TriggerRootBranch = pTriggerRootBranch;
		coordinator.RegisterSystemWithSignal(runner.systemID, uSignalId, runner);
	}

	void RegisterTestComponents(usg::SystemCoordinator& coordinator)
	{
		coordinator.RegisterComponent<usg::Components::EntityID>();
		coordinator.RegisterComponent<FrameInput>();
		coordinator.RegisterComponent<FrameOutput>();
		coordinator.RegisterComponent<DeleteMarker>();
	}

	void RegisterTestSystems(usg::SystemCoordinator& coordinator)
	{
		coordinator.RegisterSignal<TestEventSignal>();
		RegisterTestSystem<RunValueSystem>(coordinator, usg::RunSignal::ID, TriggerRunBranch);
		RegisterTestSystem<ParentValueSystem>(coordinator, usg::RunSignal::ID, TriggerParentBranch);
		RegisterTestSystem<EventValueSystem>(coordinator, TEST_EVENT_SIGNAL_ID, TriggerEventBranch);
		RegisterTestSystem<LateValueSystem>(coordinator, usg::LateUpdateSignal::ID, TriggerLateBranch);
		RegisterTestSystem<GPUValueSystem>(coordinator, usg::GPUUpdateSignal::ID, TriggerGPUBranch);
	}

	bool Expect(bool condition, const char* szMessage)
	{
		if (!condition)
		{
			fprintf(stderr, "FAILED: %s\n", szMessage);
			return false;
		}
		return true;
	}

	struct ScenarioResult
	{
		uint32 runSum = 0;
		uint32 parentSum = 0;
		uint32 eventSum = 0;
		uint32 lateSum = 0;
		uint32 gpuSum = 0;
		uint32 pendingDeletes = 0;
		uint32 signalTasks = 0;
		uint32 rootBranchTasks = 0;
	};

	void AddEntity(usg::SystemCoordinator& coordinator, usg::vector<usg::Entity>& entities, usg::Entity parent, uint32 value, bool bDeleteMarker)
	{
		usg::Entity entity = usg::CreateEntity(parent);
		entity->SetParent(parent);
		usg::GameComponents<FrameInput>::Create(entity)->value = value;
		usg::GameComponents<FrameOutput>::Create(entity);
		if (bDeleteMarker)
		{
			usg::GameComponents<DeleteMarker>::Create(entity)->value = 1;
		}
		coordinator.UpdateEntityIO(entity);
		entity->ClearChanged();
		entities.push_back(entity);
	}

	ScenarioResult RunScenario(uint32 uWorkerCount)
	{
		static bool s_bInitialized = false;
		static usg::SystemCoordinator* s_pCoordinator = nullptr;
		static usg::vector<usg::Entity> s_entities;
		static usg::Entity s_root = nullptr;

		if (!s_bInitialized)
		{
			usg::ComponentStats::Reset();
			usg::ComponentEntity::InitPool(256);

			s_pCoordinator = vnew(usg::ALLOC_OBJECT) usg::SystemCoordinator();
			s_pCoordinator->Init(nullptr, nullptr, nullptr);
			RegisterTestComponents(*s_pCoordinator);
			RegisterTestSystems(*s_pCoordinator);
			s_pCoordinator->LockRegistration();

			s_root = usg::ComponentEntity::GetRoot();
			usg::GameComponents<FrameInput>::Create(s_root)->value = 100;
			usg::GameComponents<FrameOutput>::Create(s_root);

			for (uint32 branch = 0; branch < 8; ++branch)
			{
				usg::Entity parent = s_root;
				for (uint32 depth = 0; depth < 8; ++depth)
				{
					const uint32 value = branch * 10 + depth + 1;
					const bool bDeleteMarker = branch == 3 && depth == 4;
					AddEntity(*s_pCoordinator, s_entities, parent, value, bDeleteMarker);
					parent = s_entities.back();
				}
			}

			s_bInitialized = true;
		}

		for (usg::Entity entity : s_entities)
		{
			FrameOutput* pOutput = usg::GameComponents<FrameOutput>::GetComponentData(entity);
			usg::MemSet(pOutput, 0, sizeof(*pOutput));
		}

		usg::SystemCoordinator* pCoordinator = s_pCoordinator;
		usg::vector<usg::Entity>& entities = s_entities;
		usg::Entity root = s_root;
		pCoordinator->ConfigureSystemScheduler(uWorkerCount, 128);
		const usg::SystemCoordinator::SystemSchedulerStats startStats = pCoordinator->GetSystemSchedulerStats();

		usg::RunSignal runSignal(0.016f);
		pCoordinator->TriggerFromRoot(root, runSignal);
		TestEvent eventData = { 7 };
		TestEventSignal eventSignal(eventData);
		pCoordinator->TriggerFromRoot(root, eventSignal);
		usg::LateUpdateSignal lateSignal(0.025f);
		pCoordinator->TriggerFromRoot(root, lateSignal);
		usg::GPUHandles gpuHandles = { nullptr };
		usg::GPUUpdateSignal gpuSignal(&gpuHandles);
		pCoordinator->TriggerFromRoot(root, gpuSignal);

		ScenarioResult result;
		for (usg::Entity entity : entities)
		{
			FrameOutput* pOutput = usg::GameComponents<FrameOutput>::GetComponentData(entity);
			result.runSum += pOutput->runSum;
			result.parentSum += pOutput->parentSum;
			result.eventSum += pOutput->eventSum;
			result.lateSum += pOutput->lateSum;
			result.gpuSum += pOutput->gpuSum;
			result.pendingDeletes += entity->HasPendingDeletions() ? 1 : 0;
		}

		const usg::SystemCoordinator::SystemSchedulerStats stats = pCoordinator->GetSystemSchedulerStats();
		result.signalTasks = stats.uTotalSignalTaskCount - startStats.uTotalSignalTaskCount;
		result.rootBranchTasks = stats.uTotalRootBranchTaskCount - startStats.uTotalRootBranchTaskCount;

		return result;
	}

	bool TestSerialAndWorkerFrameResultsMatch()
	{
		const ScenarioResult serial = RunScenario(0);
		const ScenarioResult worker = RunScenario(3);

		bool ok = true;
		ok &= Expect(worker.runSum == serial.runSum, "worker run sum matches serial");
		ok &= Expect(worker.parentSum == serial.parentSum, "worker parent-read sum matches serial");
		ok &= Expect(worker.eventSum == serial.eventSum, "worker event sum matches serial");
		ok &= Expect(worker.lateSum == serial.lateSum, "worker late update sum matches serial");
		ok &= Expect(worker.gpuSum == serial.gpuSum, "worker gpu update sum matches serial");
		ok &= Expect(serial.pendingDeletes == 1, "serial frame marks one pending delete");
		ok &= Expect(worker.signalTasks == serial.signalTasks, "worker signal task count matches serial");
		ok &= Expect(worker.rootBranchTasks == serial.rootBranchTasks, "worker root branch task count matches serial");
		ok &= Expect(worker.rootBranchTasks > 0, "frame harness records root branch tasks");
		return ok;
	}
}

int main()
{
	bool ok = true;
	ok &= TestSerialAndWorkerFrameResultsMatch();

	if (ok)
	{
		printf("ECSFrameThreading tests passed\n");
		fflush(stdout);
		_exit(0);
	}

	fflush(stdout);
	_exit(1);
}
