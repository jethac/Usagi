// Rounded box with a border, as USS draws it: the border band between the
// outer rounded rect and the inner (border-inset) rounded rect, the
// background (optionally textured) inside. Signed distances in pixels give
// one pixel of anti-aliasing at any rotation.
#include "../includes/platformdefines.inc"
#include "../includes/colorspace.inc"

in BoxData
{
	INT_LOC(0) vec2 go_vLocal;
	INT_LOC(1) vec2 go_vSize;
	INT_LOC(2) vec4 go_vRadii;
	INT_LOC(3) vec4 go_vBorder;
	INT_LOC(4) vec2 go_vUV;
	INT_LOC(5) vec4 go_vFill;
	INT_LOC(6) vec4 go_vBorderCol;
	INT_LOC(7) vec4 go_vSlice;
	INT_LOC(8) vec4 go_vSliceUV;
} boxData;

SAMPLER_LOC(1, 0) uniform sampler2D sampler0;

layout(location = 0) out vec4 colorOut;

// Distance from p to a rounded rect spanning [lo, hi] with per-corner radii
// (tl, tr, br, bl); y grows downwards.
float RoundedRectDistance(vec2 p, vec2 lo, vec2 hi, vec4 radii)
{
	vec2 c = (lo + hi) * 0.5;
	vec2 h = max((hi - lo) * 0.5, vec2(0.0));
	vec2 q = p - c;
	float r = q.y < 0.0 ? (q.x < 0.0 ? radii.x : radii.y) : (q.x < 0.0 ? radii.w : radii.z);
	r = min(r, min(h.x, h.y));
	vec2 d = abs(q) - h + vec2(r);
	return min(max(d.x, d.y), 0.0) + length(max(d, vec2(0.0))) - r;
}

// One axis of a 9-slice: the ends keep their size (d0, d1 pixels showing
// u0, u1 of the image), the middle stretches.
float SliceAxis(float x, float size, float d0, float d1, float u0, float u1)
{
	if (x < d0) return d0 > 0.0 ? x / d0 * u0 : 0.0;
	if (x > size - d1) return d1 > 0.0 ? 1.0 - u1 + (x - (size - d1)) / d1 * u1 : 1.0;
	float mid = size - d0 - d1;
	return mid > 0.0 ? u0 + (x - d0) / mid * (1.0 - u0 - u1) : u0;
}

void main(void)
{
	vec2 p = boxData.go_vLocal;
	vec4 b = boxData.go_vBorder;	// l t r b
	float outer = RoundedRectDistance(p, vec2(0.0), boxData.go_vSize, boxData.go_vRadii);
	vec4 innerRadii = max(boxData.go_vRadii - vec4(max(b.x, b.y), max(b.z, b.y), max(b.z, b.w), max(b.x, b.w)), vec4(0.0));
	float inner = RoundedRectDistance(p, b.xy, boxData.go_vSize - b.zw, innerRadii);

	float aa = 0.75;
	float outerCover = clamp(0.5 - outer / aa, 0.0, 1.0);
	float innerCover = clamp(0.5 - inner / aa, 0.0, 1.0);

	vec2 uv = boxData.go_vUV;
	vec4 s = boxData.go_vSlice;
	if (s.x > 0.0 || s.y > 0.0 || s.z > 0.0 || s.w > 0.0)
	{
		// 9-slice in one quad, so the pieces have no seams between them.
		vec2 q = clamp(p, vec2(0.0), boxData.go_vSize);
		vec4 su = boxData.go_vSliceUV;
		uv = GetUV(vec2(SliceAxis(q.x, boxData.go_vSize.x, s.x, s.z, su.x, su.z),
			SliceAxis(q.y, boxData.go_vSize.y, s.y, s.w, su.y, su.w)));
	}
	vec4 tex = texture(sampler0, uv);
#ifdef SRGB_BLEND
	// An sRGB texture samples as linear; the target is sRGB-space.
	tex.rgb = fromLinear(tex.rgb);
#endif
	vec4 fill = boxData.go_vFill * tex;
	vec4 band = boxData.go_vBorderCol;
	// Border band under the background, mixed pre-multiplied so a
	// transparent border does not darken the edge.
	vec3 rgb = mix(band.rgb * band.a, fill.rgb * fill.a, innerCover);
	float alpha = mix(band.a, fill.a, innerCover) * outerCover;
	float cover = mix(band.a, fill.a, innerCover);
	colorOut = vec4(cover > 0.0 ? rgb / cover : vec3(0.0), alpha);
}
