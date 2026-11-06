// UXML box: one instance per box. Its four vertices are the corners of
// the box's quad (with one pixel of padding for anti-aliasing), drawn as a
// triangle strip. (This was a geometry shader expanding a point; Metal has
// none.)
#include "../includes/platformdefines.inc"
#include "../includes/global_2d.inc"
#include "../includes/colorspace.inc"

layout (location = 0) in vec4 ao_rect;		// x, y, width, height (panel pixels)
layout (location = 1) in vec4 ao_radii;		// top-left, top-right, bottom-right, bottom-left
layout (location = 2) in vec4 ao_border;	// left, top, right, bottom
layout (location = 3) in vec4 ao_uv;		// u0, v0, u1, v1; 9-slice: the slices in UV (l, t, r, b)
layout (location = 4) in vec4 ao_fill;		// background, opacity applied
layout (location = 5) in vec4 ao_borderCol;
layout (location = 6) in vec4 ao_slice;		// 9-slice: the slices on screen in pixels (l, t, r, b); 0 = not sliced
layout (location = 7) in vec2 ao_axis;		// the box's x axis (cos, sin): it turns about its top-left corner

out BoxData
{
	INT_LOC(0) vec2 go_vLocal;		// pixels from the box's top-left corner
	INT_LOC(1) vec2 go_vSize;
	INT_LOC(2) vec4 go_vRadii;
	INT_LOC(3) vec4 go_vBorder;
	INT_LOC(4) vec2 go_vUV;
	INT_LOC(5) vec4 go_vFill;
	INT_LOC(6) vec4 go_vBorderCol;
	INT_LOC(7) vec4 go_vSlice;
	INT_LOC(8) vec4 go_vSliceUV;
} boxData;

void main(void)
{
	vec2 size = ao_rect.zw;
	bool bRight = (gl_VertexIndex & 1) != 0;
	bool bTop = gl_VertexIndex >= 2;
	vec2 vLocal = vec2(bRight ? size.x + 1.0 : -1.0, bTop ? -1.0 : size.y + 1.0);

	vec4 uv = vec4(GetUV(ao_uv.xy), GetUV(ao_uv.zw));
	vec2 t = size.x > 0.0 && size.y > 0.0 ? clamp(vLocal / size, 0.0, 1.0) : vec2(0.0);

	boxData.go_vLocal = vLocal;
	boxData.go_vSize = size;
	boxData.go_vRadii = ao_radii;
	boxData.go_vBorder = ao_border;
	boxData.go_vUV = mix(uv.xy, uv.zw, t);
	boxData.go_vSlice = ao_slice;
	boxData.go_vSliceUV = ao_uv;
#ifdef SRGB_BLEND
	// The target stores what we write: keep the authored sRGB colours.
	boxData.go_vFill = ao_fill;
	boxData.go_vBorderCol = ao_borderCol;
#else
	boxData.go_vFill = vec4(toLinear(ao_fill.rgb), ao_fill.a);
	boxData.go_vBorderCol = vec4(toLinear(ao_borderCol.rgb), ao_borderCol.a);
#endif
	vec2 vTurned = vec2(ao_axis.x * vLocal.x - ao_axis.y * vLocal.y, ao_axis.y * vLocal.x + ao_axis.x * vLocal.y);
	gl_Position = vec4(ao_rect.xy + vTurned, 0.0, 1.0) * proj;
}
