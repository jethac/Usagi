/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2016
****************************************************************************/
#include "Engine/Common/Common.h"
#include "Engine/Debug/Rendering/DebugRender.h"
#include "Engine/Graphics/Device/GFXDevice.h"
#include API_HEADER(Engine/Graphics/Device, GFXDevice_ps.h)
#include "DebugStats_ps.h"

namespace usg
{

DebugStats_ps::DebugStats_ps()
	: IDebugStatGroup()
	, m_pDevice(nullptr)
{
}

void DebugStats_ps::Init(GFXDevice* pDevice)
{
	m_pDevice = pDevice;
}

void DebugStats_ps::Draw(DebugRender* pRender)
{
	if (!m_pDevice)
	{
		return;
	}

	const GFXDevice_ps& platform = m_pDevice->GetPlatform();
	Color titleColor(0.0f, 1.0f, 1.0f, 1.0f);
	Color textColor(0.0f, 1.0f, 0.0f, 1.0f);
	string line;

	pRender->AddString("Vulkan command buffers", 0.0f, 1.0f, titleColor);

	line = str::ParseString("Submitted command buffers: %u", platform.GetLastSubmittedCommandBufferCount());
	pRender->AddString(line.c_str(), 0.0f, 2.0f, textColor);

	line = str::ParseString("Command record CPU: %.3f ms", platform.GetLastCommandRecordTimeMS());
	pRender->AddString(line.c_str(), 0.0f, 3.0f, textColor);

	line = str::ParseString("vkQueueSubmit CPU: %.3f ms", platform.GetLastQueueSubmitTimeMS());
	pRender->AddString(line.c_str(), 0.0f, 4.0f, textColor);
}

} // namespace usg
