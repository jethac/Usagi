// Paint stroke segment: one instance per segment, four vertices drawn as a
// triangle strip covering the segment's capsule (or square stamp) plus the
// anti-aliasing margin. Paint space (u, v) in 0..1 maps to the whole target,
// v = 0 on row 0.
#include "../includes/platformdefines.inc"

layout (location = 0) in vec4 ao_seg;		// u0, v0, u1, v1
layout (location = 1) in vec4 ao_brush;		// x radius, y 1 = square stamp, z anti-aliasing width (paint units)
layout (location = 2) in vec4 ao_color;		// sRGB colour, a = opacity

out StrokeData
{
	INT_LOC(0) vec2 go_vPos;
	INT_LOC(1) vec4 go_vSeg;
	INT_LOC(2) vec4 go_vBrush;
	INT_LOC(3) vec4 go_vColor;
} strokeData;

void main(void)
{
	vec2 a = ao_seg.xy;
	vec2 b = ao_seg.zw;
	bool bSquare = ao_brush.y > 0.5;
	float r = ao_brush.x + ao_brush.z;
	vec2 d = b - a;
	float len = length(d);
	vec2 dir = (len > 1e-6 && !bSquare) ? d / len : vec2(1.0, 0.0);
	vec2 nrm = vec2(-dir.y, dir.x);
	bool bEnd = (gl_VertexIndex & 1) != 0;
	bool bSide = gl_VertexIndex >= 2;
	vec2 p = (bEnd ? b + dir * r : a - dir * r) + nrm * (bSide ? r : -r);

	strokeData.go_vPos = p;
	strokeData.go_vSeg = ao_seg;
	strokeData.go_vBrush = ao_brush;
	strokeData.go_vColor = ao_color;
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
