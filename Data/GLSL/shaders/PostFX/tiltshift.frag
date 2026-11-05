#include "../includes/platformdefines.inc"

// Tilt-shift miniature blur: a horizontal band of the screen stays sharp and
// the blur grows above and below it. Screen based, so it behaves the same for
// orthographic and perspective cameras.

BUFFER_LAYOUT(1, UBO_MATERIAL_ID) uniform Material
{
	vec4	vFocus;		// centre (0 = top), half height, 1 / transition, max radius (uv units in y)
	vec4	vPixel;		// 1/width, 1/height, aspect (w/h), unused
};

ATTRIB_LOC(0) in vec2 vo_vTexCoord;

SAMPLER_LOC(1, 0) uniform sampler2D sampler0;

layout(location = 0) out vec4 colorOut;

const int TAPS = 16;
const vec2 g_vDisc[TAPS] = vec2[](
	vec2(-0.613392, 0.617481), vec2(0.170019, -0.040254), vec2(-0.299417, 0.791925), vec2(0.645680, 0.493210),
	vec2(-0.651784, 0.717887), vec2(0.421003, 0.027070), vec2(-0.817194, -0.271096), vec2(-0.705374, -0.668203),
	vec2(0.977050, -0.108615), vec2(0.063326, 0.142369), vec2(0.203528, 0.214331), vec2(-0.667531, 0.326090),
	vec2(-0.098422, -0.295755), vec2(-0.885922, 0.215369), vec2(0.566637, 0.605213), vec2(0.039766, -0.396100));

void main(void)
{
	vec4 vCentre = texture(sampler0, vo_vTexCoord);
	float fDist = abs(vo_vTexCoord.y - vFocus.x) - vFocus.y;
	float fBlur = clamp(fDist * vFocus.z, 0.0, 1.0);
	fBlur *= fBlur;
	if (fBlur <= 0.001)
	{
		colorOut = vCentre;
		return;
	}

	vec2 vRadius = vec2(vFocus.w / vPixel.z, vFocus.w) * fBlur;
	vec4 vSum = vCentre;
	float fWeight = 1.0;
	for (int i = 0; i < TAPS; i++)
	{
		vec4 vTap = texture(sampler0, vo_vTexCoord + g_vDisc[i] * vRadius);
		// Favour bright taps a little for a bokeh-like look
		float w = 1.0 + dot(vTap.rgb, vec3(0.2126, 0.7152, 0.0722)) * 0.2;
		vSum += vTap * w;
		fWeight += w;
	}
	colorOut = vSum / fWeight;
}
