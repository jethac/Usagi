/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Platform bring-up test for Linux. Opens a window through
//	the engine's platform layer, initialises the Vulkan device and clears
//	and presents the display for a number of frames, printing the frame rate.
//	Needs no game data. See build.sh.
//
//	Usage: LinuxClear [frames] [width height]
*****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Game/GameInterface.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include "Engine/Graphics/Device/GFXContext.h"
#include "Engine/Graphics/Device/Display.h"
#include "Engine/Graphics/Textures/RenderTarget.h"
#include "Engine/Core/OS.h"
#include "Engine/Core/String/String_Util.h"
#include OS_HEADER(Engine/Core/Timer, TimeTracker.h)
#include "Engine/Core/_sdl/SDLUtil.h"
#include "Engine/HID/Input.h"
#include "Engine/HID/_sdl/Input_ps.h"
#include "Engine/Resource/ResourceMgr.h"

namespace usg
{
	bool GameMain(const char** dllModules, uint32 uModuleCount);

	static uint32 g_uFrameLimit = 300;

	class LinuxClear : public GameInterface
	{
	public:
		LinuxClear() { m_bIsRunning = false; m_bReqReset = false; }
		virtual ~LinuxClear() {}

		virtual void Init(GFXDevice* pDevice, ResourceMgr* pResMgr) override
		{
			Input::Init();
			m_uFrames = 0;
			m_bIsRunning = true;
		}

		virtual void Cleanup(GFXDevice* pDevice) override
		{
			pDevice->WaitIdle();
		}

		virtual void Update(GFXDevice* pDevice) override
		{
			// Skip the first frames, which include swap chain creation
			if (m_uFrames == 10)
			{
				m_timer.GetDeltaTime();
			}

			if (++m_uFrames >= g_uFrameLimit)
			{
				float fSeconds = m_timer.GetDeltaTime();
				uint32 uWidth = 0, uHeight = 0;
				pDevice->GetDisplay(0)->GetDisplayDimensions(uWidth, uHeight, false);
				fprintf(stderr, "LinuxClear: %u frames at %ux%u, %.1f fps\n", m_uFrames - 10, uWidth, uHeight, (m_uFrames - 10) / fSeconds);
				m_bIsRunning = false;
			}
		}

		virtual void Draw(GFXDevice* pDevice) override
		{
			Display* pDisplay = pDevice->GetDisplay(0);
			pDevice->Begin();
			GFXContext* pContext = pDevice->GetImmediateCtxt();
			pContext->Begin(true);
			pContext->RenderToDisplay(pDisplay, RenderTarget::RT_FLAG_COLOR_0);
			pDisplay->Present();
			pContext->End();
			pDevice->End();
		}

		virtual void OnMessage(GFXDevice* const pDevice, const uint32 messageID, const void* const pParameters) override
		{
			if (messageID == 'WSZE')
			{
				pDevice->GetDisplay(0)->Resize(pDevice);
			}
		}

	private:
		TimeTracker	m_timer;
		uint32		m_uFrames;
	};

	GameInterface* CreateGame()
	{
		return vnew(ALLOC_OBJECT) LinuxClear();
	}

	const char* GetGameName()
	{
		return "LinuxClear";
	}
}

int main(int argc, char** argv)
{
	usg::DisplayMode settings;
	settings.screenDim.x = 0;
	settings.screenDim.y = 0;
	settings.screenDim.width = 1280;
	settings.screenDim.height = 720;
	settings.bWindowed = true;
	settings.parentHndl = nullptr;
	str::Copy(settings.name, "LinuxClear", sizeof(settings.name));

	if (argc > 1)
	{
		usg::g_uFrameLimit = (uint32)atoi(argv[1]) + 10;
	}
	if (argc > 3)
	{
		settings.screenDim.width = atoi(argv[2]);
		settings.screenDim.height = atoi(argv[3]);
	}

	if (!SDLUTIL::Init())
	{
		return 1;
	}

	WindHndl hndl = SDLUTIL::CreateDisplayWindow(settings.name, &settings, false);
	if (!hndl)
	{
		return 1;
	}
	usg::Input::GetPlatform().RegisterHwnd(0, hndl);

	usg::OS::Initialize();
	usg::GameMain(nullptr, 0);
	usg::OS::ShutDown();
	SDLUTIL::Shutdown();

	return 0;
}
