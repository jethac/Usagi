/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Main entry point into the project for windows
*****************************************************************************/
#include "Engine/Common/Common.h"
#include <cstdio>
#include <cstdlib>
#include "Engine/Game/GameInterface.h"
#include "Engine/Graphics/GFX.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Graphics/Device/Display.h"
#include "Engine/HID/Input.h"
#include "Engine/Framework/ComponentGetter.h"
#include "Engine/Framework/ComponentSystemInputOutputs.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Core/File/File.h"
#include "Engine/Maths/MathUtil.h"
#include "Engine/Physics/PhysX.h"
#include "Engine/Core/Timer/Timer.h"
#include "Engine/Core/Modules/ModuleManager.h"
#include "Engine/Resource/ResourceMgr.h"
#if (defined PLATFORM_PC || defined PLATFORM_SWITCH_EMU)
#include "Engine/Core/_win/WinUtil.h"
#elif (defined PLATFORM_LINUX)
#include "Engine/Core/_linux/LinuxUtil.h"
#endif
#include "Engine/Core/OS.h"

namespace usg {

bool		InitEngine(const char** dllModules, uint32 uModuleCount);
void		EngineCleanup();

// Boot breadcrumb logging (opt-in via USAGI_BOOT_LOG=1) to localize startup hangs.
static bool BootLogEnabled()
{
	char szEnv[8] = {};
	return OS::GetEnvVar("USAGI_BOOT_LOG", szEnv, sizeof(szEnv));
}

static void BootCrumb(const char* step)
{
	static bool s_enabled = BootLogEnabled();
	if (!s_enabled) return;
#ifdef PLATFORM_PC
	FILE* f = nullptr;
	fopen_s(&f, "_gp_boot.log", "a");
	if (f) { fprintf(f, "%s\n", step); fflush(f); fclose(f); }
#else
	fprintf(stderr, "%s\n", step);
#endif
}

static GFXDevice*	g_pGFXDevice = nullptr;
static bool			g_gameExit = false;
static Timer		g_timer;

bool GameExit()
{
	g_gameExit = true;
	return true;
}

GFXDevice* GetGFXDevice()
{
	return g_pGFXDevice;
}


static void error_callback(int error, const char* description)
{
    fputs(description, stderr);
}

static GameInterface* game;

bool GameInit()
{
	BootCrumb("GameInit: begin");
	Math::SeedRand();
	game->Init(g_pGFXDevice, usg::ResourceMgr::Inst());
	BootCrumb("GameInit: after game->Init");

	if (!game->IsRunning())
	{
		FATAL_RELEASE(false, "Game failed to initialise");
		return false;
	}
	BootCrumb("GameInit: done (running)");

	return true;
}

// Frame rate report (opt-in via USAGI_FPS_LOG=1): average frame rate and
// worst frame time every few seconds, in every build configuration.
static void ReportFrameRate(float fDelta)
{
	static int s_enabled = -1;
	if (s_enabled < 0)
	{
		char szEnv[8] = {};
		s_enabled = OS::GetEnvVar("USAGI_FPS_LOG", szEnv, sizeof(szEnv)) ? 1 : 0;
	}
	if (!s_enabled)
	{
		return;
	}

	static const float s_fInterval = 5.0f;
	static float s_fElapsed = 0.0f;
	static float s_fWorst = 0.0f;
	static uint32 s_uFrames = 0;

	s_fElapsed += fDelta;
	s_fWorst = Math::Max(s_fWorst, fDelta);
	s_uFrames++;
	if (s_fElapsed >= s_fInterval)
	{
		uint32 uWidth = 0, uHeight = 0;
		Display* pDisplay = g_pGFXDevice->GetDisplay(0);
		if (pDisplay)
		{
			pDisplay->GetDisplayDimensions(uWidth, uHeight, false);
		}
		RELEASE_WARNING("FPS: %.1f average over %u frames at %ux%u, worst frame %.2f ms\n",
			s_uFrames / s_fElapsed, s_uFrames, uWidth, uHeight, s_fWorst * 1000.0f);
		s_fElapsed = 0.0f;
		s_fWorst = 0.0f;
		s_uFrames = 0;
	}
}

void GameLoop()
{
	g_timer.Update();
	ReportFrameRate(g_timer.GetDeltaRealTime());
	Input::Update(g_pGFXDevice, g_timer.GetDeltaRealTime());
	game->Update(g_pGFXDevice);
	if(GFX::HasFocus())
	{
		game->Draw(g_pGFXDevice);		
	}
	GFX::PostUpdate();
	OS::Update();
}

void GameCleanup()
{
	game->Cleanup(g_pGFXDevice);
	vdelete game;
	game = nullptr;
	ResourceMgr::Cleanup(g_pGFXDevice);
	EngineCleanup();
}

bool GameMain(const char** dllModules, uint32 uModuleCount)
{
	if (OS::ShouldQuit())
	{
		return true;
	}

	if (!InitEngine(dllModules, uModuleCount))
	{
		FATAL_RELEASE(false, "Hardware setup failed");
		return false;
	}

	if(!GameInit())
		return false;

#if 0
	if (!InitInput())
	{
		return false;
	}
#endif

	// Everything should be set up now, time to finalize any early load modules
	usg::ModuleManager::Inst()->PostInit(g_pGFXDevice);

	if (OS::ShouldQuit())
	{
		GameExit();
		GameCleanup();
		return true;
	}

	// enter main event loop
	while(game->IsRunning() && !g_gameExit )
	{
		GameLoop();

		if(game->ResetReq())
		{
			//mem::FreezeStack(false);
			game = CreateGame();
			game->Init(g_pGFXDevice, usg::ResourceMgr::Inst());
			//mem::FreezeStack(true);
		}
		if (OS::ShouldQuit())
		{
			break;
		}
	}

	GameCleanup();
    
	return true;
}

void GameMessage(const uint32 messageID, const void* const pParameters)
{
	if (game != nullptr)
	{
		game->OnMessage(g_pGFXDevice, messageID, pParameters);
	}
}

bool InitInput()
{
	Input::Init();

	return true;
}

bool InitEngine(const char** dllModules, uint32 uModuleCount)
{
	BootCrumb("InitEngine: begin");
	mem::InitialiseDefault();
	BootCrumb("InitEngine: after mem::InitialiseDefault");
	File::InitFileSystem();
	BootCrumb("InitEngine: after File::InitFileSystem");

	usg::ModuleManager::Inst()->Create();
	usg::ModuleManager::Inst()->PreInit();
	for (uint32 i = 0; i < uModuleCount; i++)
	{
		// We pre-load as some of these may need to be known about when initing gfx and input
		usg::ModuleManager::Inst()->LoadModule(dllModules[i]);
	}
	BootCrumb("InitEngine: before GFX::Initialise");

	g_pGFXDevice = GFX::Initialise();
	BootCrumb("InitEngine: after GFX::Initialise");

	if(!g_pGFXDevice)
	{
		return false;
	}

	game = CreateGame();
	BootCrumb("InitEngine: after CreateGame");
	game->PreGFXInit();
	BootCrumb("InitEngine: after PreGFXInit, before InitDisplay");

#if (defined PLATFORM_PC)
	g_pGFXDevice->InitDisplay(WINUTIL::GetWindow());
#elif (defined PLATFORM_LINUX)
	g_pGFXDevice->InitDisplay(LINUXUTIL::GetWindow());
#else
	g_pGFXDevice->InitAllHardwareDisplays();
#endif
	BootCrumb("InitEngine: after InitDisplay");

	// Moved these out of here to prevent dependencies, should probably remove this file
	//Audio::Create()->Init();

	//physics::init();

	return true;
}


void EngineCleanup()
{ 
	//physics::deinit();
	Input::Cleanup();
	File::FinalizeFileSystem();
	GFX::Reset();
	ComponentSystemInputOutputsSharedBase::Cleanup();
	usg::ModuleManager::Inst()->Cleanup();
	mem::Cleanup();
}

}


