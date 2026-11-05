#ifndef _DEFERRED_ENV_INC
#define _DEFERRED_ENV_INC

// Image based lighting and screen space contact shadows for the deferred
// directional pass. Needs deferred_read.inc (G-buffer samplers, PBR helpers).

SAMPLER_LOC(1, 5) uniform samplerCube sampler5;	// Environment radiance, GGX prefiltered across the mips
SAMPLER_LOC(1, 6) uniform samplerCube sampler6;	// Environment irradiance (cosine convolved, divided by pi)

BUFFER_LAYOUT(1, UBO_MATERIAL_ID) uniform DeferredParams
{
	mat4	mEnvRotation;		// World to environment space
	vec4	vEnvParams;			// x = IBL intensity, y = radiance mip count - 1, z = flat ambient scale, w = horizon fade
	vec4	vContactShadow;		// x = ray length (view units), y = thickness (view units), z = step count, w = strength (0 disables)
	vec4	vShadowParams;		// x = PCF radius scale, y = normal offset (shadow texels), z = unused, w = debug view
	vec4	vAOParams;			// x = radius (view units, 0 disables), y = strength, z = power, w = unused
};

vec3 ViewToEnv(in vec3 vViewDir)
{
	vec3 vWorld = (vec4(vViewDir, 0.0) * mInvViewMat).xyz;
	return (vec4(vWorld, 0.0) * mEnvRotation).xyz;
}

vec3 EvaluateIBL(in PBRSurface s, in vec3 V, in float fOcclusion)
{
	vec3 N = s.vNormal;
	float NoV = max(dot(N, V), 1e-4);
	vec3 R = reflect(-V, N);

	vec3 vIrradiance = texture(sampler6, ViewToEnv(N)).rgb;
	vec3 vDiffuse = PBR_DiffuseColor(s) * vIrradiance;

	float fMaxMip = vEnvParams.y;
	vec3 vEnvR = ViewToEnv(R);
	vec3 vPrefiltered = textureLod(sampler5, vEnvR, s.fRoughness * fMaxMip).rgb;
	vec2 vAB = PBR_EnvBRDFApprox(s.fRoughness, NoV);
	vec3 vSpecular = vPrefiltered * (PBR_F0(s) * vAB.x + vAB.y);

	// Reflections that point below the surface's horizon (from normal maps) are
	// occluded by the surface itself.
	float fHorizon = clamp(1.0 + vEnvParams.w * dot(R, s.vNormal), 0.0, 1.0);
	fHorizon *= fHorizon;

	float fSpecOcc = PBR_SpecularOcclusion(NoV, fOcclusion, s.fRoughness) * fHorizon;
	vec3 vColor = vDiffuse * fOcclusion + vSpecular * fSpecOcc;

	if (s.fClearCoat > 0.0)
	{
		float Fc = PBR_F_Schlick(0.04, NoV) * s.fClearCoat;
		vec3 vCoat = textureLod(sampler5, vEnvR, s.fClearCoatRoughness * fMaxMip).rgb;
		float fCoatOcc = PBR_SpecularOcclusion(NoV, fOcclusion, s.fClearCoatRoughness) * fHorizon;
		vColor = vColor * (1.0 - Fc) + vCoat * (Fc * fCoatOcc);
	}

	return vColor * vEnvParams.x;
}

float InterleavedGradientNoise(in vec2 vPixel)
{
	return fract(52.9829189 * fract(dot(vPixel, vec2(0.06711056, 0.00583715))));
}

// View space position of the scene at an NDC position and view depth
// (perspective or orthographic)
vec3 ViewPosAtNDC(in vec2 vNDC, in float fViewZ)
{
	vec4 vA = vec4(vNDC, 0.0, 1.0) * mInvProjMat;
	vec4 vB = vec4(vNDC, 1.0, 1.0) * mInvProjMat;
	vA.xyz /= vA.w;
	vB.xyz /= vB.w;
	vec3 vDir = vB.xyz - vA.xyz;
	return vA.xyz + vDir * ((fViewZ - vA.z) / vDir.z);
}

// Screen space ambient occlusion evaluated inline in the lighting pass: a
// rotated spiral of taps inside a world-radius disc around the pixel. The disc
// is projected through mProjMat, so the radius means the same thing for
// orthographic and perspective cameras. Used for ambient / IBL only.
#define SSAO_TAPS 12
float ScreenSpaceAO(in vec3 P, in vec3 N)
{
	float fRadius = vAOParams.x;
	if (fRadius <= 0.0)
	{
		return 1.0;
	}

	vec4 vC = vec4(P, 1.0) * mProjMat;
	vec4 vCX = vec4(P + vec3(fRadius, 0.0, 0.0), 1.0) * mProjMat;
	vec4 vCY = vec4(P + vec3(0.0, fRadius, 0.0), 1.0) * mProjMat;
	vec2 vCentre = vC.xy / vC.w;
	vec2 vDiscNDC = vec2(abs(vCX.x / vCX.w - vCentre.x), abs(vCY.y / vCY.w - vCentre.y));

	float fAngle = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;
	float fJitter = InterleavedGradientNoise(gl_FragCoord.yx + 17.0);
	float fOcclusion = 0.0;
	for (int i = 0; i < SSAO_TAPS; i++)
	{
		float t = (float(i) + fJitter) / float(SSAO_TAPS);
		float a = fAngle + float(i) * 2.39996;		// golden angle spiral
		vec2 vNDC = vCentre + vec2(cos(a), sin(a)) * vDiscNDC * sqrt(t);
		vec2 vUV = GetRTUV(vNDC * 0.5 + 0.5);
		float fSceneZ = GetDeferredLinearDepth(vUV) * vNearFar.y;
		vec3 vS = ViewPosAtNDC(vNDC, fSceneZ) - P;
		float fLen = length(vS);
		float fFalloff = clamp(1.0 - fLen / (fRadius * 1.5), 0.0, 1.0);
		fOcclusion += max(dot(vS / max(fLen, 1e-4), N) - 0.15, 0.0) * fFalloff;
	}
	float fAO = clamp(1.0 - vAOParams.y * fOcclusion * (1.0 / float(SSAO_TAPS)) * 1.6, 0.0, 1.0);
	return pow(fAO, vAOParams.z);
}

// Short screen space ray march from the surface toward the light: grounds small
// objects whose shadows are finer than a shadow map texel. Works for
// perspective and orthographic projections (projects through mProjMat).
float ContactShadow(in vec3 vViewPos, in vec3 vViewNormal, in vec3 L)
{
	if (vContactShadow.w <= 0.0)
	{
		return 1.0;
	}

	int iSteps = int(vContactShadow.z);
	float fStep = vContactShadow.x / vContactShadow.z;
	float fJitter = InterleavedGradientNoise(gl_FragCoord.xy);
	// Start slightly off the surface to avoid self shadowing
	vec3 vRayPos = vViewPos + vViewNormal * (vContactShadow.y * 0.25) + L * (fStep * fJitter);
	float fOccluded = 0.0;
	for (int i = 0; i < iSteps; i++)
	{
		vRayPos += L * fStep;
		vec4 vClip = vec4(vRayPos, 1.0) * mProjMat;
		vec2 vNDC = vClip.xy / vClip.w;
		if (any(greaterThan(abs(vNDC), vec2(1.0))))
		{
			break;
		}
		vec2 vUV = GetRTUV(vNDC * 0.5 + 0.5);
		float fSceneZ = GetDeferredLinearDepth(vUV) * vNearFar.y;
		float fDelta = vRayPos.z - fSceneZ;
		if (fDelta > 0.0 && fDelta < vContactShadow.y)
		{
			// Fade the occlusion out toward the end of the ray
			fOccluded = 1.0 - (float(i) / float(iSteps));
			break;
		}
	}
	return 1.0 - fOccluded * vContactShadow.w;
}

#endif
