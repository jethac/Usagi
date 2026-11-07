/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Draws the frame budget meter (USAGI_BUDGET=2) with the
//	debug renderer: CPU and GPU time as stacked bars against a 60 Hz frame.
*****************************************************************************/
#pragma once

#ifndef USG_DEBUG_FRAME_BUDGET_OVERLAY_H
#define USG_DEBUG_FRAME_BUDGET_OVERLAY_H

namespace usg {

class DebugRender;

void DrawFrameBudgetOverlay(DebugRender* pRender);

}

#endif
