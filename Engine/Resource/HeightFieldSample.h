/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
****************************************************************************/
#pragma once

namespace usg {

// One height field grid point, as stored in heightfield pak data (the layout of
// PhysX's PxHeightFieldSample, which the format was built around).
struct HeightFieldSample
{
	static constexpr uint8 HoleMaterial = 0x7f;		// materialIndex0 (low 7 bits) of a hole

	sint16	height;
	uint8	materialIndex0;		// top bit: tessellation flag
	uint8	materialIndex1;
};
static_assert(sizeof(HeightFieldSample) == 4, "HeightFieldSample must match the pak data layout");

}
