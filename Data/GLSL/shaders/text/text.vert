#include "../includes/platformdefines.inc"
#include "../includes/global_2d.inc"
#include "../includes/colorspace.inc"

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


out GeometryData
{
	INT_LOC(0) vec2		go_vPos;
	INT_LOC(1) vec4		go_vColor;
	INT_LOC(2) vec4		go_vBgColor;
	INT_LOC(3) vec2		go_vTexcoord;

} geometryData;


void main(void)
{
	bool bRight = (gl_VertexIndex & 1) == 0;
	bool bTop = gl_VertexIndex >= 2;

	vec2 vLayoutPos = vec2(bRight ? ao_posRange.z : ao_posRange.x, bTop ? ao_posRange.y : ao_posRange.w);
	vec4 vPos = vec4(vLayoutPos, ao_fDepth, 1.0) * proj;

	vec2 vUVMin = GetUV(ao_uvRange.xy);
	vec2 vUVMax = GetUV(ao_uvRange.zw);
	vec4 vColor = toLinear(bTop ? ao_colUpper : ao_colLower);

	geometryData.go_vColor = toLinear(ao_colFg) * vColor;
	geometryData.go_vBgColor = toLinear(ao_colBg) * vColor;
	geometryData.go_vTexcoord = vec2(bRight ? vUVMax.x : vUVMin.x, bTop ? vUVMax.y : vUVMin.y);
	geometryData.go_vPos = vPos.xy;

	gl_Position = vPos;
}
