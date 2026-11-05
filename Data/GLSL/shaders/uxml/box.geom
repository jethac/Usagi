// Expands a box point into a quad (one pixel of padding for anti-aliasing).
#include "../includes/platformdefines.inc"
#include "../includes/global_2d.inc"

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

ATTRIB_LOC(0) in vec4 vo_rect[];
ATTRIB_LOC(1) in vec4 vo_radii[];
ATTRIB_LOC(2) in vec4 vo_border[];
ATTRIB_LOC(3) in vec4 vo_uv[];
ATTRIB_LOC(4) in vec4 vo_fill[];
ATTRIB_LOC(5) in vec4 vo_borderCol[];

out BoxData
{
	INT_LOC(0) vec2 go_vLocal;		// pixels from the box's top-left corner
	INT_LOC(1) vec2 go_vSize;
	INT_LOC(2) vec4 go_vRadii;
	INT_LOC(3) vec4 go_vBorder;
	INT_LOC(4) vec2 go_vUV;
	INT_LOC(5) vec4 go_vFill;
	INT_LOC(6) vec4 go_vBorderCol;
} boxData;

void Emit(vec2 vLocal)
{
	vec4 rect = vo_rect[0];
	boxData.go_vLocal = vLocal;
	boxData.go_vSize = rect.zw;
	boxData.go_vRadii = vo_radii[0];
	boxData.go_vBorder = vo_border[0];
	vec2 t = rect.z > 0.0 && rect.w > 0.0 ? clamp(vLocal / rect.zw, 0.0, 1.0) : vec2(0.0);
	boxData.go_vUV = mix(vo_uv[0].xy, vo_uv[0].zw, t);
	boxData.go_vFill = vo_fill[0];
	boxData.go_vBorderCol = vo_borderCol[0];
	gl_Position = vec4(rect.xy + vLocal, 0.0, 1.0) * proj;
	EmitVertex();
}

void main(void)
{
	vec2 size = vo_rect[0].zw;
	Emit(vec2(-1.0, size.y + 1.0));
	Emit(vec2(size.x + 1.0, size.y + 1.0));
	Emit(vec2(-1.0, -1.0));
	Emit(vec2(size.x + 1.0, -1.0));
	EndPrimitive();
}
