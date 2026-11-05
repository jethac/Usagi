/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Core/_linux/os_ps.h"
#include "Engine/Debug/Exception.h"
#include "Engine/Core/String/String_Util.h"

namespace usg {

	static CriticalSection g_saveDataCriticalSection;
	bool OS_ps::m_bExitRequested = false;

	void OS_ps::RequestExit()
	{
		m_bExitRequested = true;
	}


	void OS_ps::ShowManual()
	{
		DEBUG_PRINT("No manual to show on Linux\n");
	}

	bool OS_ps::GetEnvVar(const char* szName, char* szValueOut, memsize uSize)
	{
		const char* szValue = getenv(szName);
		if (!szValue)
		{
			return false;
		}
		str::Copy(szValueOut, szValue, (uint32)uSize);
		return true;
	}

	void OS_ps::Initialize()
	{
		// Crash signals print a backtrace to stderr, as the Windows build
		// logs a symbolized stack from its unhandled exception filter
		Exception::Init(nullptr, nullptr);
		g_saveDataCriticalSection.Initialize();
	}

	bool OS_ps::ShouldQuit()
	{
		return m_bExitRequested;
	}

	CriticalSection& OS_ps::GetSaveDataCriticalSection()
	{
		return g_saveDataCriticalSection;
	}

	void OS_ps::Update()
	{
	}

	void OS_ps::ShutDown()
	{
	}

	bool OS_ps::GetShouldAllowApplicationEnterBackground()
	{
		return true;
	}

	void OS_ps::SetShouldAllowApplicationEnterBackground(bool)
	{

	}

}
