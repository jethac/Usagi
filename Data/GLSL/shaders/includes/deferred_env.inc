#ifndef _DEFERRED_ENV_INC
#define _DEFERRED_ENV_INC

// Image based lighting and screen space contact shadows for the deferred
// directional pass. Needs deferred_read.inc (G-buffer samplers, PBR helpers).

SAMPLER_LOC(1, 5) uniform samplerCube sampler5;	// Environment radiance, GGX prefiltered across the mips
SAMPLER_LOC(1, 6) uniform samplerCube sampler6;	// Environment irradiance (cosine convolved, divided by pi)
SAMPLER_LOC(1, 7) uniform sampler2D sampler7;	// Last frame's lit scene (HDR, quarter resolution), for reflections

BUFFER_LAYOUT(1, UBO_MATERIAL_ID) uniform DeferredParams
{
	mat4	mEnvRotation;		// World to environment space
	vec4	vEnvParams;			// x = IBL intensity, y = radiance mip count - 1, z = flat ambient scale, w = horizon fade
	vec4	vContactShadow;		// x = ray length (view units), y = thickness (view units), z = step count, w = strength (0 disables)
	vec4	vShadowParams;		// x = PCF radius scale, y = normal offset (shadow texels), z = unused, w = debug view
	vec4	vAOParams;			// x = radius (view units, 0 disables), y = strength, z = power, w = unused
	vec4	vSSRParams;			// x = ray steps (0 disables), y = ray length (view units), z = thickness (view units), w = max roughness
	vec4	vSkyColor;			// rgb = hemisphere ambient from above (linear HDR, black = off), w = share of reflections taken from the hemisphere
	vec4	vGroundColor;		// rgb = hemisphere ambient from below, w = unused
	vec4	vShadowTint;		// rgb = share of the primary directional light that reaches its shadow, w = light wrap
	mat4	mPrevViewProj;		// World to last frame's clip space
};

float InterleavedGradientNoise(in vec2 vPixel);

// Screen space reflection: march the reflected ray through the depth buffer
// and, where it hits, read last frame's lit scene at that point (reprojected,
// so a moving camera still lines up). rgb = radiance, a = confidence (0 =
// keep the environment). Works for perspective and orthographic projections.
vec4 TraceSSR(in vec3 P, in vec3 N, in vec3 V)
{
	int iSteps = int(vSSRParams.x);
	vec3 R = reflect(-V, N);
	// Rays back toward the camera soon leave what the depth buffer can see
	float fFacing = clamp(1.0 - 2.0 * dot(R, V), 0.0, 1.0);
	if (iSteps <= 0 || fFacing <= 0.0)
	{
		return vec4(0.0);
	}

	float fStep = vSSRParams.y / float(iSteps);
	float fJitter = InterleavedGradientNoise(gl_FragCoord.xy + 31.0);
	vec3 vRay = P + N * (vSSRParams.z * 0.1) + R * (fStep * fJitter);
	vec3 vPrev = vRay;
	bool bHit = false;
	int iHit = 0;
	for (int i = 0; i < iSteps; i++)
	{
		vPrev = vRay;
		vRay += R * fStep;
		vec4 vClip = vec4(vRay, 1.0) * mProjMat;
		vec2 vNDC = vClip.xy / vClip.w;
		if (any(greaterThan(abs(vNDC), vec2(1.0))))
		{
			break;
		}
		float fSceneZ = GetDeferredLinearDepth(GetRTUV(vNDC * 0.5 + 0.5)) * vNearFar.y;
		float fDelta = vRay.z - fSceneZ;
		if (fDelta > 0.0 && fDelta < vSSRParams.z)
		{
			bHit = true;
			iHit = i;
			break;
		}
	}
	if (!bHit)
	{
		return vec4(0.0);
	}

	// Refine between the last miss and the hit
	vec3 vA = vPrev;
	vec3 vB = vRay;
	for (int j = 0; j < 4; j++)
	{
		vec3 vM = (vA + vB) * 0.5;
		vec4 vClip = vec4(vM, 1.0) * mProjMat;
		float fSceneZ = GetDeferredLinearDepth(GetRTUV(vClip.xy / vClip.w * 0.5 + 0.5)) * vNearFar.y;
		if (vM.z > fSceneZ) vB = vM; else vA = vM;
	}

	// Last frame's scene at the hit
	vec3 vWorld = (vec4(vB, 1.0) * mInvViewMat).xyz;
	vec4 vPrevClip = vec4(vWorld, 1.0) * mPrevViewProj;
	vec2 vPrevNDC = vPrevClip.xy / vPrevClip.w;
	vec2 vEdge = clamp((1.0 - abs(vPrevNDC)) * 8.0, 0.0, 1.0);
	float fConfidence = vEdge.x * vEdge.y * fFacing;
	// Fade out toward the end of the ray
	fConfidence *= clamp(2.0 * (1.0 - float(iHit) / float(iSteps)), 0.0, 1.0);
	if (fConfidence <= 0.0)
	{
		return vec4(0.0);
	}
	vec3 vRadiance = textureLod(sampler7, GetRTUV(vPrevNDC * 0.5 + 0.5), 0.0).rgb;
	return vec4(vRadiance, fConfidence);
}

vec3 ViewToEnv(in vec3 vViewDir)
{
	vec3 vWorld = (vec4(vViewDir, 0.0) * mInvViewMat).xyz;
	return (vec4(vWorld, 0.0) * mEnvRotation).xyz;
}

// Hemisphere ambient: the sky colour from above, the ground's bounce from
// below, along a world space direction. fSharpness narrows the blend toward
// the horizon (1 = the cosine weighted blend for diffuse; larger for mirror
// like reflections).
vec3 HemisphereLight(in vec3 vViewDir, in float fSharpness)
{
	float fUp = (vec4(vViewDir, 0.0) * mInvViewMat).y;
	float t = clamp(0.5 + 0.5 * fUp * fSharpness, 0.0, 1.0);
	return mix(vGroundColor.rgb, vSkyColor.rgb, t);
}

vec3 EvaluateIBL(in PBRSurface s, in vec3 V, in float fOcclusion, in vec3 P)
{
	vec3 N = s.vNormal;
	float NoV = max(dot(N, V), 1e-4);
	vec3 R = reflect(-V, N);

	vec3 vIrradiance = texture(sampler6, ViewToEnv(N)).rgb;
	vec3 vDiffuse = PBR_DiffuseColor(s) * vIrradiance;

	float fMaxMip = vEnvParams.y;
	vec3 vEnvR = ViewToEnv(R);
	vec3 vPrefiltered = textureLod(sampler5, vEnvR, s.fRoughness * fMaxMip).rgb;
	// The hemisphere's sky in reflections (in the environment's units), sharper
	// for smoother surfaces
	float fSkyRefl = vSkyColor.w;
	if (fSkyRefl > 0.0)
	{
		vec3 vSky = HemisphereLight(R, 1.0 / max(s.fRoughness, 0.08)) / max(vEnvParams.x, 1e-3);
		vPrefiltered = mix(vPrefiltered, vSky, fSkyRefl);
	}

	// Screen space reflections take over from the environment where they hit:
	// the clearcoat (varnish, car lacquer) and smooth base specular. The scene
	// is already lit, so it's brought into the environment's units here.
	vec4 vSSR = vec4(0.0);
	float fSSRMaxRough = vSSRParams.w;
	if (vSSRParams.x > 0.0 && (s.fClearCoat > 0.0 || s.fRoughness < fSSRMaxRough))
	{
		vSSR = TraceSSR(P, N, V);
		vSSR.rgb /= max(vEnvParams.x, 1e-3);
	}
	vPrefiltered = mix(vPrefiltered, vSSR.rgb, vSSR.a * clamp(1.0 - s.fRoughness / max(fSSRMaxRough, 1e-3), 0.0, 1.0));
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
		if (fSkyRefl > 0.0)
		{
			vCoat = mix(vCoat, HemisphereLight(R, 1.0 / max(s.fClearCoatRoughness, 0.08)) / max(vEnvParams.x, 1e-3), fSkyRefl);
		}
		vCoat = mix(vCoat, vSSR.rgb, vSSR.a * clamp(1.0 - s.fClearCoatRoughness / max(fSSRMaxRough, 1e-3), 0.0, 1.0));
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
