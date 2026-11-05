// UXML box: one point per box, expanded by the geometry shader.
#include "../includes/platformdefines.inc"
#include "../includes/colorspace.inc"

layout (location = 0) in vec4 ao_rect;		// x, y, width, height (panel pixels)
layout (location = 1) in vec4 ao_radii;		// top-left, top-right, bottom-right, bottom-left
layout (location = 2) in vec4 ao_border;	// left, top, right, bottom
layout (location = 3) in vec4 ao_uv;		// u0, v0, u1, v1
layout (location = 4) in vec4 ao_fill;		// background, opacity applied
layout (location = 5) in vec4 ao_borderCol;

ATTRIB_LOC(0) out vec4 vo_rect;
ATTRIB_LOC(1) out vec4 vo_radii;
ATTRIB_LOC(2) out vec4 vo_border;
ATTRIB_LOC(3) out vec4 vo_uv;
ATTRIB_LOC(4) out vec4 vo_fill;
ATTRIB_LOC(5) out vec4 vo_borderCol;

void main(void)
{
	gl_Position = vec4(ao_rect.xy, 0.0, 1.0);
	vo_rect = ao_rect;
	vo_radii = ao_radii;
	vo_border = ao_border;
	vo_uv = vec4(GetUV(ao_uv.xy), GetUV(ao_uv.zw));
	vo_fill = vec4(toLinear(ao_fill.rgb), ao_fill.a);
	vo_borderCol = vec4(toLinear(ao_borderCol.rgb), ao_borderCol.a);
}
