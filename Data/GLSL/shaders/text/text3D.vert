#include "../includes/platformdefines.inc"
#include "../includes/global_3d.inc"
#include "../includes/layout_3d_vs.inc"

// The 2D projection used to lay the text out (Material1)
#include "../includes/layout_3d_gs.inc"

// One instance per character. Its four vertices are the corners of the
// character's quad, drawn as a triangle strip in the order BR, BL, TR, TL.
// (This was a geometry shader expanding a point; Metal has none.)

// Input attributes (per instance)
layout (location = 0) in vec4 ao_posRange;
layout (location = 1) in float ao_fDepth;
layout (location = 2) in vec4 ao_uvRange;
layout (location = 3) in vec4 ao_colUpper;
layout (location = 4) in vec4 ao_colLower;
layout (location = 5) in vec4 ao_colBg;
layout (location = 6) in vec4 ao_colFg;


// Output attributes
ATTRIB_LOC(0) out vec2 go_vPos;
ATTRIB_LOC(1) out vec4 go_vColor;
ATTRIB_LOC(2) out vec4 go_vBgColor;
ATTRIB_LOC(3) out vec2 go_vTexCoord;
ATTRIB_LOC(4) out vec4 go_vFgColor;

void main(void)
{
	vec4 vTransformedPos;
	float fVisibility = GetPosAndVisibility(vTransformedPos);

	bool bRight = (gl_VertexIndex & 1) == 0;
	bool bTop = gl_VertexIndex >= 2;

	vec2 vLayoutPos = vec2(bRight ? ao_posRange.z : ao_posRange.x, bTop ? ao_posRange.y : ao_posRange.w);
	vec4 vPosition = vec4(vLayoutPos, ao_fDepth, 1.0) * proj;
	vPosition.xy += vTransformedPos.xy;
	vPosition.z = vTransformedPos.z;
	vPosition.w = 1.0;

	gl_Position = vPosition;
	go_vPos = vPosition.xy;

	vec2 vUVMin = GetUV(ao_uvRange.xy);
	vec2 vUVMax = GetUV(ao_uvRange.zw);
	go_vTexCoord = vec2(bRight ? vUVMax.x : vUVMin.x, bTop ? vUVMax.y : vUVMin.y);

	vec4 vColor = bTop ? ao_colUpper : ao_colLower;
	vColor.a *= fVisibility;
	go_vColor = vColor;
	go_vBgColor = ao_colBg;
	go_vFgColor = ao_colFg;
}
