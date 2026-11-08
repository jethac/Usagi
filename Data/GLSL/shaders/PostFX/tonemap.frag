#include "../includes/platformdefines.inc"
#include "../includes/global_3d.inc"

// HDR -> display. Writes linear values to an sRGB target.

BUFFER_LAYOUT(1, UBO_MATERIAL_ID) uniform Material
{
	vec4	vParams;	// exposure, saturation, contrast, vignette
	vec4	vLUT;		// strength, enabled, aspect, unused
	vec4	vWhite;		// white balance gain (rgb), unused
	vec4	vHaze;		// density, start, max, height falloff
	vec4	vHazeColor;	// colour (rgb, linear HDR), base height
};

ATTRIB_LOC(0) in vec2 vo_vTexCoord;

SAMPLER_LOC(1, 0) uniform sampler2D sampler0;	// HDR scene
SAMPLER_LOC(1, 1) uniform sampler2D sampler1;	// 256x16 grading LUT (sRGB values)
SAMPLER_LOC(1, 2) uniform sampler2D sampler2;	// linear depth (0..1 of the far plane)

// Aerial perspective: fade towards the haze colour with the pixel's horizontal
// distance from the eye, thinning with height. View space from the depth
// along the pixel's ray (perspective and orthographic alike).
vec3 ApplyHaze(vec3 vColor)
{
	if (vHaze.x <= 0.0)
		return vColor;
	float fLin = texture(sampler2, vo_vTexCoord).x;
	vec2 vNDC = vec2(vo_vTexCoord.x * 2.0 - 1.0, vo_vTexCoord.y * 2.0 - 1.0);
	vec4 vA = vec4(vNDC, 0.0, 1.0) * mInvProjMat;
	vec4 vB = vec4(vNDC, 1.0, 1.0) * mInvProjMat;
	vec3 vNear = vA.xyz / vA.w;
	vec3 vFar = vB.xyz / vB.w;
	float fViewZ = fLin * vNearFar.y;
	float fT = (fViewZ - vNear.z) / max(vFar.z - vNear.z, 1e-4);
	vec3 vView = mix(vNear, vFar, fT);
	vec3 vWorld = (vec4(vView, 1.0) * mInvViewMat).xyz;
	float fDist = max(length(vWorld.xz - vEyePos.xz) - vHaze.y, 0.0);
	float fAmount = 1.0 - exp(-fDist * vHaze.x);
	if (vHaze.w > 0.0)
		fAmount *= exp(-max(vWorld.y - vHazeColor.w, 0.0) * vHaze.w);
	return mix(vColor, vHazeColor.rgb, min(fAmount, vHaze.z));
}

layout(location = 0) out vec4 colorOut;

// ACES filmic curve, Stephen Hill's fit of the RRT + ODT
const mat3 ACESInput = mat3(
	0.59719, 0.07600, 0.02840,
	0.35458, 0.90834, 0.13383,
	0.04823, 0.01566, 0.83777);
const mat3 ACESOutput = mat3(
	 1.60475, -0.10208, -0.00327,
	-0.53108,  1.10813, -0.07276,
	-0.07367, -0.00605,  1.07602);

vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}

vec3 ACESFitted(vec3 c)
{
	c = ACESInput * c;
	c = RRTAndODTFit(c);
	c = ACESOutput * c;
	return clamp(c, 0.0, 1.0);
}

vec3 LinearToSRGB(vec3 c)
{
	return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}

vec3 SRGBToLinear(vec3 c)
{
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

vec3 ApplyLUT(vec3 c)
{
	// 16 slices of 16x16, blue picks the slice
	const float N = 16.0;
	float fSlice = c.b * (N - 1.0);
	float fSlice0 = floor(fSlice);
	float fT = fSlice - fSlice0;
	vec2 vUV = vec2((c.r * (N - 1.0) + 0.5) / (N * N), (c.g * (N - 1.0) + 0.5) / N);
	vec3 a = texture(sampler1, vUV + vec2(fSlice0 / N, 0.0)).rgb;
	vec3 b = texture(sampler1, vUV + vec2(min(fSlice0 + 1.0, N - 1.0) / N, 0.0)).rgb;
	return mix(a, b, fT);
}

void main(void)
{
	vec3 vColor = ApplyHaze(texture(sampler0, vo_vTexCoord).rgb) * vWhite.rgb * vParams.x;
	vColor = ACESFitted(vColor);

	float fLuma = dot(vColor, vec3(0.2126, 0.7152, 0.0722));
	vColor = max(mix(vec3(fLuma), vColor, vParams.y), 0.0);

	vec3 vDisplay = LinearToSRGB(vColor);
	vDisplay = clamp((vDisplay - 0.5) * vParams.z + 0.5, 0.0, 1.0);
	if (vLUT.y > 0.0)
	{
		vDisplay = mix(vDisplay, ApplyLUT(vDisplay), vLUT.x);
	}

	// Vignette, round in screen space
	vec2 vD = (vo_vTexCoord - 0.5) * vec2(vLUT.z, 1.0);
	float fVig = 1.0 - vParams.w * smoothstep(0.35, 1.1, dot(vD, vD) * 2.2);
	vDisplay *= fVig;

	// Dither by +-0.5 of an 8 bit step to break up banding in smooth gradients
	float fNoise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	vDisplay = clamp(vDisplay + (fNoise - 0.5) / 255.0, 0.0, 1.0);

	vColor = SRGBToLinear(vDisplay);
	// Luma in alpha for FXAA
	colorOut = vec4(vColor, sqrt(dot(vColor, vec3(0.299, 0.587, 0.114))));
}
