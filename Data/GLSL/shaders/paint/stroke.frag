// Paint stroke segment: coverage from the distance to the segment (round caps)
// or to the square stamp, times the paint mask. Writes linear colour with the
// coverage in alpha; the blend state makes the target premultiplied.
#include "../includes/platformdefines.inc"
#include "../includes/colorspace.inc"

in StrokeData
{
	INT_LOC(0) vec2 go_vPos;
	INT_LOC(1) vec4 go_vSeg;
	INT_LOC(2) vec4 go_vBrush;
	INT_LOC(3) vec4 go_vColor;
} strokeData;

SAMPLER_LOC(0, 0) uniform sampler2D sampler0;	// paint mask

layout(location = 0) out vec4 colorOut;

void main(void)
{
	vec2 p = strokeData.go_vPos;
	vec2 a = strokeData.go_vSeg.xy;
	vec2 b = strokeData.go_vSeg.zw;
	float r = strokeData.go_vBrush.x;
	float aa = max(strokeData.go_vBrush.z, 1e-5);

	float dist;
	if (strokeData.go_vBrush.y > 0.5)
	{
		vec2 q = abs(p - a);
		dist = max(q.x, q.y) - r;
	}
	else
	{
		vec2 ab = b - a;
		float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 1e-12), 0.0, 1.0);
		dist = length(p - (a + ab * t)) - r;
	}
	float fCover = clamp(0.5 - dist / aa, 0.0, 1.0);
	float fMask = texture(sampler0, p).r;
	float fAlpha = fCover * fMask * strokeData.go_vColor.a;
	if (fAlpha <= 0.0)
	{
		discard;
	}
#ifdef ACCUMULATE
	colorOut = vec4(strokeData.go_vColor.rgb * fAlpha, fAlpha);
#else
	colorOut = vec4(toLinear(strokeData.go_vColor.rgb), fAlpha);
#endif
}
