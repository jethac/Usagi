/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Debug/DebugStats.h"
#include "SystemCoordinator.h"
#include "Engine/Core/stl/hash_map.h"
#include "Engine/Core/stl/vector.h"
#include "Engine/Core/stl/memory.h"
#include "Engine/Graphics/GPUUpdate.h"
#include "Engine/Physics/Signals/OnCollision.h"
#include "Engine/Physics/Signals/OnTrigger.h"
#include "Engine/Physics/Signals/OnRaycastHit.h"
#include "Engine/Core/stl/algorithm.h"
#include "Engine/Framework/EventManager.h"
#include "Engine/Framework/SystemScheduler.h"

using namespace usg;

namespace usg
{
	struct ProtocolBufferReaderData
	{
		void(*pPtrToReader)(ProtocolBufferFile& file, void* data);
		memsize uDataSize;
	};

	struct SystemCoordinator::InternalData
	{
		struct SignalExecutionBatch
		{
			SignalExecutionBatch()
				: priority(0)
			{
			}

			sint32 priority;
			usg::vector<uint32> runnerIndices;
		};

		usg::vector<SystemHelper> systemHelpers;
		usg::vector<SystemDependencyInfo> systemDependencies;
		SystemScheduler scheduler;
		usg::vector<ComponentHelper> componentHelpers;
		usg::hash_map<uint32, uint32> componentHashLookUp; // component-hash => id mapping
		usg::hash_map<uint32, usg::vector<SignalRunner>> signalRunners;
		usg::hash_map<uint32, usg::vector<SignalExecutionBatch>> signalExecutionBatches;
		usg::hash_map<uint32, ProtocolBufferReaderData> protocolBufferReaders;
		uint32 uSignalDispatchDepth = 0;

		void Clear()
		{
			systemHelpers.clear();
			systemDependencies.clear();
			componentHelpers.clear();
			componentHashLookUp.clear();
			signalRunners.clear();
			signalExecutionBatches.clear();
			uSignalDispatchDepth = 0;
		}
	};

	struct SignalDispatchScope
	{
		SignalDispatchScope(uint32& uDepth)
			: m_uDepth(uDepth)
		{
			m_uDepth++;
		}

		~SignalDispatchScope()
		{
			ASSERT(m_uDepth > 0);
			m_uDepth--;
		}

		uint32& m_uDepth;
	};

	SystemCoordinator::SystemDependencyInfo::SystemDependencyInfo()
		: systemName(nullptr)
		, systemTypeID(0xffffffff)
		, priority(0)
		, uRequiredComponentKeyCount(0)
		, uReadComponentKeyCount(0)
		, uWriteComponentKeyCount(0)
		, uParentReadComponentKeyCount(0)
		, bIsCollisionListener(false)
		, uOnCollisionMask(0)
	{
		usg::MemSet(uRequiredComponentMask, 0, sizeof(uRequiredComponentMask));
		usg::MemSet(uReadComponentMask, 0, sizeof(uReadComponentMask));
		usg::MemSet(uWriteComponentMask, 0, sizeof(uWriteComponentMask));
		usg::MemSet(uParentReadComponentMask, 0, sizeof(uParentReadComponentMask));
	}

	static uint32 CopySystemDependencyMask(uint32(*pGetKey)(uint32), uint32* pDst, uint32 uDstCount)
	{
		uint32 uKeyCount = 0;
		if (pGetKey == nullptr)
		{
			return uKeyCount;
		}

		for (uint32 i = 0; i < uDstCount; ++i)
		{
			pDst[i] = pGetKey(i);
			if (pDst[i] != 0)
			{
				uKeyCount++;
			}
		}

		return uKeyCount;
	}

	SystemCoordinator::SystemSchedulerStats::SystemSchedulerStats()
		: uLastSignalId(0)
		, uLastSignalTaskCount(0)
		, uTotalSignalTaskCount(0)
		, uLastRootBranchRunnerCount(0)
		, uTotalRootBranchRunnerCount(0)
		, uLastRootBranchTaskCount(0)
		, uTotalRootBranchTaskCount(0)
		, uWorkerCount(0)
	{
	}

	SystemCoordinator::SignalExecutionBatchInfo::SignalExecutionBatchInfo()
		: uSignalId(0)
		, uBatchIndex(0)
		, uRunnerCount(0)
		, priority(0)
	{
	}

	void SystemCoordinator::RegisterProtocolBufferTypeInt(const uint32 uTypeId, memsize uDataSize, void(*pPtrToReader)(ProtocolBufferFile& file, void* data))
	{
		auto& readerData = m_pInternalData->protocolBufferReaders[uTypeId];
		readerData.pPtrToReader = pPtrToReader;
		readerData.uDataSize = uDataSize;
	}

	void SystemCoordinator::LoadEntityInitializerEvents(ProtocolBufferFile& file, Entity e)
	{
		usg::vector<uint8> buf;
		buf.resize(64);

		InitializerEventHeader eventHeader;
		while (file.Read(&eventHeader))
		{
			ASSERT(m_pInternalData->protocolBufferReaders.count(eventHeader.id) > 0);
			auto& reader = m_pInternalData->protocolBufferReaders[eventHeader.id];
			if (reader.uDataSize > buf.size())
			{
				buf.resize(reader.uDataSize);
			}
			reader.pPtrToReader(file, &buf[0]);
			m_pEventManager->RegisterEventWithEntityAtTime(e, eventHeader.id, &buf[0], reader.uDataSize, ON_ENTITY, 0);
		}
	}

	void SystemCoordinator::RegisterSignalInt(const uint32 uSignalId, void(*pPtrToInitializer)(SystemCoordinator& sc))
	{
		auto& runners = m_pInternalData->signalRunners;
		auto data = runners.find(uSignalId);
		if (data == runners.end())
		{
			runners[uSignalId].reserve(16);
			pPtrToInitializer(*this);
		}
	}

	void SystemCoordinator::RegisterSystemWithSignal(uint32 uSystemId, uint32 uSignalId, const SignalRunner& runner)
	{
		ASSERT(m_pInternalData->signalRunners.count(uSignalId) > 0);
		auto& runners = m_pInternalData->signalRunners[uSignalId];
		runners.push_back(runner);
	}

	void SystemCoordinator::Trigger(Entity e, Signal& sig, uint32 targets)
	{
		auto& runners = m_pInternalData->signalRunners[sig.uId];
		{
			m_pInternalData->scheduler.BeginSignal(sig.uId);
			SignalDispatchScope dispatchScope(m_pInternalData->uSignalDispatchDepth);
#ifdef ENABLE_SYSTEM_PROFILE_TIMERS
			for (auto& runner : runners)
			{
				TriggerRunner(e, sig, runner, targets);
			}
#else
			auto batchIt = m_pInternalData->signalExecutionBatches.find(sig.uId);
			if (batchIt != m_pInternalData->signalExecutionBatches.end())
			{
				for (auto& batch : batchIt->second)
				{
					m_pInternalData->scheduler.RunSignalTasks(e, sig, &runners[0], &batch.runnerIndices[0], (uint32)batch.runnerIndices.size(), targets);
				}
			}
			else
			{
				for (auto& runner : runners)
				{
					TriggerRunner(e, sig, runner, targets);
				}
			}
#endif
		}
	}

	void SystemCoordinator::TriggerFromRoot(Entity e, Signal& sig)
	{
		auto& runners = m_pInternalData->signalRunners[sig.uId];
		{
			m_pInternalData->scheduler.BeginSignal(sig.uId);
			SignalDispatchScope dispatchScope(m_pInternalData->uSignalDispatchDepth);
#ifdef ENABLE_SYSTEM_PROFILE_TIMERS
			for (auto& runner : runners)
			{
				TriggerRunnerFromRoot(e, sig, runner);
			}
#else
			auto batchIt = m_pInternalData->signalExecutionBatches.find(sig.uId);
			if (batchIt != m_pInternalData->signalExecutionBatches.end())
			{
				for (auto& batch : batchIt->second)
				{
					m_pInternalData->scheduler.RunSignalTasksFromRoot(e, sig, &runners[0], &batch.runnerIndices[0], (uint32)batch.runnerIndices.size());
				}
			}
			else
			{
				for (auto& runner : runners)
				{
					TriggerRunnerFromRoot(e, sig, runner);
				}
			}
#endif
		}
	}

	void SystemCoordinator::TriggerRunner(Entity e, Signal& sig, const SignalRunner& runner, uint32 targets)
	{
#ifdef ENABLE_SYSTEM_PROFILE_TIMERS
		const uint32 systemID = runner.systemID;
		ASSERT(systemID != SignalRunner::INVALID_SYSTEM_ID);
		SystemHelper& helper = m_pInternalData->systemHelpers[systemID];

		helper.timer.Start();
#endif

		m_pInternalData->scheduler.RunSignalTask(e, sig, runner, targets);

#ifdef ENABLE_SYSTEM_PROFILE_TIMERS
		helper.timer.Stop();
#endif
	}

	void SystemCoordinator::TriggerRunnerFromRoot(Entity e, Signal& sig, const SignalRunner& runner)
	{
#ifdef ENABLE_SYSTEM_PROFILE_TIMERS
		const uint32 systemID = runner.systemID;
		ASSERT(systemID != SignalRunner::INVALID_SYSTEM_ID);
		SystemHelper& helper = m_pInternalData->systemHelpers[systemID];

		helper.timer.Start();
#endif

		m_pInternalData->scheduler.RunSignalTaskFromRoot(e, sig, runner);

#ifdef ENABLE_SYSTEM_PROFILE_TIMERS
		helper.timer.Stop();
#endif
	}

	uint32 SystemCoordinator::GetSystemDependencyCount() const
	{
		return m_pInternalData ? (uint32)m_pInternalData->systemDependencies.size() : 0;
	}

	const SystemCoordinator::SystemDependencyInfo* SystemCoordinator::GetSystemDependencyInfo(uint32 uSystemId) const
	{
		if (m_pInternalData == nullptr || uSystemId >= m_pInternalData->systemDependencies.size())
		{
			return nullptr;
		}

		return &m_pInternalData->systemDependencies[uSystemId];
	}

	bool SystemCoordinator::SystemsHaveRequiredComponentOverlap(uint32 uLhsSystemId, uint32 uRhsSystemId) const
	{
		const SystemDependencyInfo* pLhs = GetSystemDependencyInfo(uLhsSystemId);
		const SystemDependencyInfo* pRhs = GetSystemDependencyInfo(uRhsSystemId);
		if (pLhs == nullptr || pRhs == nullptr)
		{
			return false;
		}

		for (uint32 i = 0; i < ARRAY_SIZE(pLhs->uRequiredComponentMask); ++i)
		{
			if ((pLhs->uRequiredComponentMask[i] & pRhs->uRequiredComponentMask[i]) != 0)
			{
				return true;
			}
		}

		return false;
	}

	bool SystemCoordinator::SystemsHaveComponentAccessConflict(uint32 uLhsSystemId, uint32 uRhsSystemId) const
	{
		const SystemDependencyInfo* pLhs = GetSystemDependencyInfo(uLhsSystemId);
		const SystemDependencyInfo* pRhs = GetSystemDependencyInfo(uRhsSystemId);
		if (pLhs == nullptr || pRhs == nullptr)
		{
			return false;
		}

		for (uint32 i = 0; i < ARRAY_SIZE(pLhs->uReadComponentMask); ++i)
		{
			const uint32 uLhsWrites = pLhs->uWriteComponentMask[i];
			const uint32 uRhsWrites = pRhs->uWriteComponentMask[i];
			const uint32 uLhsAccess = pLhs->uReadComponentMask[i] | uLhsWrites;
			const uint32 uRhsAccess = pRhs->uReadComponentMask[i] | uRhsWrites;
			if (((uLhsWrites & uRhsAccess) != 0) || ((uRhsWrites & uLhsAccess) != 0))
			{
				return true;
			}
		}

		return false;
	}

	static bool DependenciesHaveSchedulingConflict(const usg::vector<SystemCoordinator::SystemDependencyInfo>& dependencies, uint32 uLhsSystemId, uint32 uRhsSystemId)
	{
		if (uLhsSystemId >= dependencies.size() || uRhsSystemId >= dependencies.size())
		{
			return true;
		}

		const SystemCoordinator::SystemDependencyInfo& lhs = dependencies[uLhsSystemId];
		const SystemCoordinator::SystemDependencyInfo& rhs = dependencies[uRhsSystemId];
		for (uint32 i = 0; i < ARRAY_SIZE(lhs.uReadComponentMask); ++i)
		{
			const uint32 uLhsWrites = lhs.uWriteComponentMask[i];
			const uint32 uRhsWrites = rhs.uWriteComponentMask[i];
			const uint32 uLhsAccess = lhs.uReadComponentMask[i] | lhs.uParentReadComponentMask[i] | uLhsWrites;
			const uint32 uRhsAccess = rhs.uReadComponentMask[i] | rhs.uParentReadComponentMask[i] | uRhsWrites;
			if (((uLhsWrites & uRhsAccess) != 0) || ((uRhsWrites & uLhsAccess) != 0))
			{
				return true;
			}
		}

		return false;
	}

	void SystemCoordinator::BuildSignalExecutionBatches()
	{
		m_pInternalData->signalExecutionBatches.clear();
		for (auto& signalRunners : m_pInternalData->signalRunners)
		{
			const uint32 uSignalId = signalRunners.first;
			const usg::vector<SignalRunner>& runners = signalRunners.second;
			usg::vector<InternalData::SignalExecutionBatch>& batches = m_pInternalData->signalExecutionBatches[uSignalId];

			for (uint32 i = 0; i < (uint32)runners.size(); ++i)
			{
				const SignalRunner& runner = runners[i];
				InternalData::SignalExecutionBatch* pBatch = batches.empty() ? nullptr : &batches.back();
				bool bConflicts = runner.systemID == SignalRunner::INVALID_SYSTEM_ID;
				if (pBatch != nullptr && pBatch->priority == runner.priority)
				{
					for (uint32 k = 0; !bConflicts && k < (uint32)pBatch->runnerIndices.size(); ++k)
					{
						const SignalRunner& batchRunner = runners[pBatch->runnerIndices[k]];
						bConflicts = batchRunner.systemID == SignalRunner::INVALID_SYSTEM_ID ||
							DependenciesHaveSchedulingConflict(m_pInternalData->systemDependencies, batchRunner.systemID, runner.systemID);
					}
				}

				if (pBatch == nullptr || pBatch->priority != runner.priority || bConflicts)
				{
					InternalData::SignalExecutionBatch batch;
					batch.priority = runner.priority;
					batch.runnerIndices.push_back(i);
					batches.push_back(batch);
				}
				else
				{
					pBatch->runnerIndices.push_back(i);
				}
			}
		}
	}

	uint32 SystemCoordinator::GetSignalExecutionBatchCount(uint32 uSignalId) const
	{
		if (m_pInternalData == nullptr)
		{
			return 0;
		}

		auto it = m_pInternalData->signalExecutionBatches.find(uSignalId);
		return it == m_pInternalData->signalExecutionBatches.end() ? 0 : (uint32)it->second.size();
	}

	bool SystemCoordinator::GetSignalExecutionBatchInfo(uint32 uSignalId, uint32 uBatchIndex, SignalExecutionBatchInfo& out) const
	{
		if (m_pInternalData == nullptr)
		{
			return false;
		}

		auto it = m_pInternalData->signalExecutionBatches.find(uSignalId);
		if (it == m_pInternalData->signalExecutionBatches.end() || uBatchIndex >= it->second.size())
		{
			return false;
		}

		const InternalData::SignalExecutionBatch& batch = it->second[uBatchIndex];
		out.uSignalId = uSignalId;
		out.uBatchIndex = uBatchIndex;
		out.uRunnerCount = (uint32)batch.runnerIndices.size();
		out.priority = batch.priority;
		return true;
	}

	void SystemCoordinator::ConfigureSystemScheduler(uint32 uWorkerCount, uint32 uMaxTasks)
	{
		if (m_pInternalData != nullptr)
		{
			m_pInternalData->scheduler.Init(uWorkerCount, uMaxTasks);
		}
	}

	SystemCoordinator::SystemSchedulerStats SystemCoordinator::GetSystemSchedulerStats() const
	{
		SystemSchedulerStats result;
		if (m_pInternalData == nullptr)
		{
			return result;
		}

		const SystemScheduler::Stats& stats = m_pInternalData->scheduler.GetStats();
		result.uLastSignalId = stats.uLastSignalId;
		result.uLastSignalTaskCount = stats.uLastSignalTaskCount;
		result.uTotalSignalTaskCount = stats.uTotalSignalTaskCount;
		result.uLastRootBranchRunnerCount = stats.uLastRootBranchRunnerCount;
		result.uTotalRootBranchRunnerCount = stats.uTotalRootBranchRunnerCount;
		result.uLastRootBranchTaskCount = stats.uLastRootBranchTaskCount;
		result.uTotalRootBranchTaskCount = stats.uTotalRootBranchTaskCount;
		result.uWorkerCount = stats.uWorkerCount;
		return result;
	}

	SystemCoordinator::SystemCoordinator()
		: m_pMemPoolBuffer(nullptr)
		, m_pEventManager(nullptr)
		, m_pMessageDispatch(nullptr)
		, m_pLuaVM(nullptr)
		, m_uBitfieldsPerKey(0)
		, m_pSystemKeyBuffer(nullptr)
		, m_pSystemData(nullptr)
	{
		static const size_t POOL_SIZE = 256 * 1024;
		m_pMemPoolBuffer = mem::Alloc(MEMTYPE_STANDARD, ALLOC_SYSTEM, POOL_SIZE, 4U);
		m_memPool.Initialize(m_pMemPoolBuffer, POOL_SIZE);

		m_pInternalData = vnew(ALLOC_OBJECT)InternalData();

		RegisterSignal<RunSignal>();
		const uint32 id = LateUpdateSignal::ID;
		ASSERT(id >1);
		RegisterSignal<LateUpdateSignal>();
		RegisterSignal<GPUUpdateSignal>();
		RegisterSignal<OnRaycastHitSignal>();
		RegisterSignal<OnCollisionSignal>();
		RegisterSignal<OnTriggerSignal>();
	}
}

SystemCoordinator::~SystemCoordinator()
{
	ASSERT(!m_pInternalData);
}

void SystemCoordinator::Cleanup(ComponentLoadHandles& handles)
{
	Clear(handles);
	m_pInternalData->scheduler.Shutdown();

	vdelete m_pInternalData;
	m_pInternalData = nullptr;

	// HACK: Add back when StringPointerHash iterator is fixed
	// m_memPool.FreeGroup(0);
	mem::Free(MEMTYPE_STANDARD, m_pMemPoolBuffer);
	m_pMemPoolBuffer = nullptr;
}

void SystemCoordinator::Init(EventManager* pEventManager, MessageDispatch* pMessageDispatch, LuaVM* pLuaVM)
{
	ASSERT(m_pEventManager == nullptr && m_pMessageDispatch == nullptr && m_pLuaVM == nullptr);
	m_pEventManager = pEventManager;
	m_pMessageDispatch = pMessageDispatch;
	m_pLuaVM = pLuaVM;
	m_pInternalData->scheduler.Init(0, 0);
}

void SystemCoordinator::RegisterComponent(uint32 uComponentId, uint32 uComponentHash, const ComponentHelper& h)
{
	ASSERT(uComponentId < ComponentEntity::MAX_COMPONENT_TYPES);
	if (GameComponentMgr::IsInitialized(uComponentId))
	{
		return;
	}

	if (uComponentId >= m_pInternalData->componentHelpers.size())
	{
		m_pInternalData->componentHelpers.reserve(uComponentId + 64);
		while (m_pInternalData->componentHelpers.size() <= uComponentId)
		{
			m_pInternalData->componentHelpers.push_back_uninitialized();
		}
	}
	ComponentHelper& helper = m_pInternalData->componentHelpers[uComponentId];
	helper = h;
	helper.Init();
	if (uComponentHash != INVALID_PB_ID)
	{
		m_pInternalData->componentHashLookUp[uComponentHash] = uComponentId;
	}
}

void SystemCoordinator::UpdateSystemList()
{
	uint32* pActiveSystems = ComponentStats::GetComponentFlags();
	SystemData* pData = m_pSystemData;
	KeyIndex* pKey = m_pSystemKeyBuffer;
	const size_t uSystemCount = m_pInternalData->systemHelpers.size();
	for (sint32 i = 0; i < (sint32)uSystemCount; i++)
	{
		if (!pData->bSystemActive)
		{
			bool bCanBeRun = true;
			for (uint32 j = 0; j < pData->uKeyCount; j++)
			{
				bCanBeRun &= (pKey[j].uCmpValue & pActiveSystems[pKey[j].uIndex]) == pKey[j].uCmpValue;
			}
			pData->bSystemActive = bCanBeRun;
		}
		pKey += pData->uKeyCount;
		pData++;
	}

}


void SystemCoordinator::UpdateEntityIO(ComponentEntity* e)
{
	ASSERT(m_pInternalData->uSignalDispatchDepth == 0);

	if (ComponentStats::GetFlagsDirty())
	{
		// Update our list of potentially active systems
		UpdateSystemList();
		ComponentStats::ClearFlagsDirty();
	}
	uint32 uCurrentlyRunningSystems[16];
	usg::MemSet(&uCurrentlyRunningSystems, 0, sizeof(uCurrentlyRunningSystems));
	StringPointerHash<GenericInputOutputs*>& systems = e->GetSystems();
	for (StringPointerHash<GenericInputOutputs*>::Iterator it = systems.Begin(); !it.IsEnd(); ++it)
	{
		const uint32 uKey = it.GetKey().Get();
		const uint32 uIndex = uKey - 1;
		uCurrentlyRunningSystems[uIndex / 32] |= (1<<(uIndex%32));
	}

	e->SetOnCollisionMask(0);

	SystemData* pData = m_pSystemData;
	KeyIndex* pKey = m_pSystemKeyBuffer;
	uint32 uSystemCount = (uint32)m_pInternalData->systemHelpers.size();

	bool bEntityHasRequiredComponents = true;
	for (uint32 i=0; i<uSystemCount; i++)
	{
		if(pData->bSystemActive)
		{
			const SystemHelper& helper = m_pInternalData->systemHelpers[i];
			uint32* pEntityCmp = e->GetRawComponentBitfield();

			bEntityHasRequiredComponents = true;
			for (uint32 j = 0; j < pData->uKeyCount; j++)
			{
				bEntityHasRequiredComponents &= (pKey[j].uCmpValue & pEntityCmp[pKey[j].uIndex]) == pKey[j].uCmpValue;
			}

			const uint32 uIndex = helper.systemTypeID;
			ASSERT(uIndex / 32 < ARRAY_SIZE(uCurrentlyRunningSystems));
			const bool bEntityIsCurrentlyRunning = (uCurrentlyRunningSystems[uIndex / 32] & (1 << (uIndex % 32))) != 0;

			if (bEntityIsCurrentlyRunning || bEntityHasRequiredComponents)
			{
				ComponentGetter componentGetter(e);
				const bool bIsRunning = helper.UpdateInputOutputs(componentGetter, bEntityHasRequiredComponents, bEntityIsCurrentlyRunning);
				if (helper.bIsCollisionListener && bIsRunning)
				{
					e->SetOnCollisionMask(e->GetOnCollisionMask() | helper.uOnCollisionMask);
				}
			}
		}

		pKey += pData->uKeyCount;
		pData++;
	}
}

void SystemCoordinator::RemoveEntityIO(ComponentEntity* e)
{
	ASSERT(m_pInternalData->uSignalDispatchDepth == 0);

	StringPointerHash<GenericInputOutputs*>& systems = e->GetSystems();
	while (systems.Count())
	{
		StringPointerHash<GenericInputOutputs*>::Iterator it = systems.Begin();
		const uint32 uKey = it.GetKey().Get();
		const uint32 uIndex = uKey - 1;
		const SystemHelper& helper = m_pInternalData->systemHelpers[uIndex];
		if (helper.RemoveInputOutputs != nullptr)
		{
			helper.RemoveInputOutputs(e);
		}
	}
}

SystemCoordinator::SystemHelper& SystemCoordinator::GetSystemHelper(uint32 uSystemId)
{
	auto& helpers = m_pInternalData->systemHelpers;
	if (uSystemId >= helpers.size())
	{
		helpers.reserve(uSystemId + 64);
		while (helpers.size() <= uSystemId)
		{
			helpers.push_back(SystemHelper());
		}
	}
	return helpers[uSystemId];
}

void SystemCoordinator::CleanupSystems()
{
	for(uint32 i = 0; i < m_pInternalData->systemHelpers.size(); ++i)
	{
		if (m_pInternalData->systemHelpers[i].Cleanup != nullptr)
		{
			m_pInternalData->systemHelpers[i].Cleanup();
		}
	}
}

void SystemCoordinator::CleanupComponents(ComponentLoadHandles& handles)
{
	GameComponentMgr::Cleanup(handles);
}

void SystemCoordinator::PreloadComponentAssets(const usg::ComponentHeader& hdr, ProtocolBufferFile& file, ComponentLoadHandles& handles)
{
	if (m_pInternalData->componentHashLookUp.count(hdr.id) == 0)
	{
		file.AdvanceBytes(hdr.byteLength);
		return;
	}
	const uint32 uComponentID = m_pInternalData->componentHashLookUp[hdr.id];
	if(m_pInternalData->componentHelpers[uComponentID].PreloadComponentAssets != nullptr)
	{
		m_pInternalData->componentHelpers[uComponentID].PreloadComponentAssets(hdr, file, handles);
	}
}

void SystemCoordinator::LoadAndAttachComponent(const ComponentHeader& hdr, ProtocolBufferFile& file, Entity e)
{
	if (m_pInternalData->componentHashLookUp.count(hdr.id) == 0)
	{
		file.AdvanceBytes(hdr.byteLength);
		return;
	}
	const uint32 uComponentID = m_pInternalData->componentHashLookUp[hdr.id];
	if (m_pInternalData->componentHelpers[uComponentID].LoadAndAttachComponent != nullptr)
	{
		m_pInternalData->componentHelpers[uComponentID].LoadAndAttachComponent(file, e);
	}
}

void SystemCoordinator::CallOnLoaded(ComponentEntity* e, ComponentLoadHandles& handles)
{
	for(uint32 i = 0; i < m_pInternalData->componentHelpers.size(); ++i)
	{
		if (e->GetComponentBitfield(i / BITFIELD_LENGTH) & (1<<(i%BITFIELD_LENGTH)))
		{
			if (m_pInternalData->componentHelpers[i].CallOnLoaded != nullptr)
			{
				m_pInternalData->componentHelpers[i].CallOnLoaded(e, handles);
			}
			else
			{
				e->GetComponent(i)->SetLoaded();
			}
		}
	}
}

void SystemCoordinator::LockRegistration()
{
	// Calculate System keys
	m_uBitfieldsPerKey = ((uint32)m_pInternalData->systemHelpers.size() / BITFIELD_LENGTH) + 1;
	m_pSystemData = (SystemData*)m_memPool.Allocate(m_pInternalData->systemHelpers.size() * sizeof(SystemData), 4, 0, ALLOC_SYSTEM);
	m_pInternalData->systemDependencies.resize(m_pInternalData->systemHelpers.size());

	// Sort Signal Runners by priority
	for (auto& runners : m_pInternalData->signalRunners)
	{
		usg::sort(runners.second.begin(), runners.second.end(), [](const SignalRunner& a, const SignalRunner& b) {
			return a.priority < b.priority;
		});
	}

	uint32 uKeyCount = 0;

	// First figure out how many keys
	for(uint32 i = 0; i < m_pInternalData->systemHelpers.size(); ++i)
	{
		if (m_pInternalData->systemHelpers[i].GetSystemKey != nullptr)
		{
			for (uint32 j = 0; j < m_uBitfieldsPerKey; ++j)
			{
				// Only check against relevant keys
				if (m_pInternalData->systemHelpers[i].GetSystemKey(j) != 0)
				{
					uKeyCount++;
				}
			}
		}
	}

	m_pSystemKeyBuffer = (KeyIndex*)m_memPool.Allocate(uKeyCount * sizeof(KeyIndex), 4, 0, ALLOC_SYSTEM);

	uKeyCount = 0;

	for (uint32 i = 0; i < m_pInternalData->systemHelpers.size(); ++i)
	{
		SystemData* pSystem = &m_pSystemData[i];
		const SystemHelper& helper = m_pInternalData->systemHelpers[i];
		SystemDependencyInfo& dependency = m_pInternalData->systemDependencies[i];
		dependency = SystemDependencyInfo();
		dependency.systemName = helper.systemName;
		dependency.systemTypeID = helper.systemTypeID;
		dependency.priority = helper.priority;
		dependency.bIsCollisionListener = helper.bIsCollisionListener;
		dependency.uOnCollisionMask = helper.uOnCollisionMask;
		dependency.uReadComponentKeyCount = CopySystemDependencyMask(helper.GetSystemReadKey, dependency.uReadComponentMask, ARRAY_SIZE(dependency.uReadComponentMask));
		dependency.uWriteComponentKeyCount = CopySystemDependencyMask(helper.GetSystemWriteKey, dependency.uWriteComponentMask, ARRAY_SIZE(dependency.uWriteComponentMask));
		dependency.uParentReadComponentKeyCount = CopySystemDependencyMask(helper.GetSystemParentReadKey, dependency.uParentReadComponentMask, ARRAY_SIZE(dependency.uParentReadComponentMask));

		KeyIndex* pSystemKeys = &m_pSystemKeyBuffer[uKeyCount];
		KeyIndex* pKeyBufferOffset = pSystemKeys;
		uint32 uSystemKeyCount = 0;
		pSystem->uKeyCount = 0;
		pSystem->bSystemActive = false;
		pSystem->pKeys = pSystemKeys;
		if (helper.GetSystemKey != nullptr)
		{
			for (uint32 j = 0; j < m_uBitfieldsPerKey; ++j)
			{
				// Only check against relevant keys
				const uint32 uSystemKey = helper.GetSystemKey(j);
				if (uSystemKey != 0)
				{
					pKeyBufferOffset->uCmpValue = uSystemKey;
					pKeyBufferOffset->uIndex = j;
					if (j < ARRAY_SIZE(dependency.uRequiredComponentMask))
					{
						dependency.uRequiredComponentMask[j] = uSystemKey;
						dependency.uRequiredComponentKeyCount++;
					}
					pKeyBufferOffset++;
					uSystemKeyCount++;
					uKeyCount++;
				}
			}
			pSystem->uKeyCount = uSystemKeyCount;
		}
	}

	BuildSignalExecutionBatches();
}

#ifdef ENABLE_SYSTEM_PROFILE_TIMERS
void SystemCoordinator::RegisterTimers()
{
	Color colorSystemTimer( 0.4f, 1.0f, 0.4f );

	for(uint32 i = 0; i < m_systemHelpers.GetSize(); ++i)
	{
		SystemHelper& helper = m_systemHelpers[i];
		DebugStats::Inst()->RegisterTimer(helper.systemName, &helper.timer, colorSystemTimer, 0.04f);
	}
}

void SystemCoordinator::ClearTimers()
{
	for(uint32 i = 0; i < m_systemHelpers.GetSize(); ++i)
	{
		m_systemHelpers[i].timer.Clear();
	}
}
#endif

void SystemCoordinator::Clear(ComponentLoadHandles& handles)
{
	CleanupComponents(handles);
	CleanupSystems();

	m_pInternalData->Clear();

	m_pEventManager = nullptr;
	m_pMessageDispatch = nullptr;

	m_uBitfieldsPerKey = 0;
	if(m_pSystemKeyBuffer != nullptr)
	{
		m_memPool.Deallocate(m_pSystemKeyBuffer);
		m_pSystemKeyBuffer = nullptr;
	}

	// HACK: Remove when StringPointerHash iterator is fixed
	m_memPool.FreeGroup(0);
}
