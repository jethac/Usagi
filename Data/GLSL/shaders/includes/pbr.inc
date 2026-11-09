#ifndef _PBR_INC
#define _PBR_INC

// Metal/roughness PBR shading shared by the deferred lighting passes.
// GGX specular with a height-correlated Smith visibility term, Lambert
// diffuse, and an optional clear-coat layer (a second, isotropic GGX lobe
// with a fixed 4% Fresnel, as used for car paint and lacquered plastic).
// Conventions follow Filament: roughness is perceptual (alpha = r^2) and
// dielectric F0 = 0.16 * reflectance^2 (0.5 -> 4%).

#define PBR_PI 3.14159265359
#define PBR_MIN_ALPHA 0.0025

struct PBRSurface
{
	vec3	vAlbedo;		// Base colour, linear
	vec3	vNormal;		// View space
	float	fRoughness;		// Perceptual, 0..1
	float	fMetallic;
	float	fReflectance;	// Dielectric specular level, 0.5 = 4%
	float	fClearCoat;		// Clear-coat layer strength, 0..1
	float	fClearCoatRoughness;
	float	fOcclusion;		// Material (baked) ambient occlusion
};

vec3 PBR_F0(in PBRSurface s)
{
	float fDielectric = 0.16 * s.fReflectance * s.fReflectance;
	return mix(vec3(fDielectric), s.vAlbedo, s.fMetallic);
}

vec3 PBR_DiffuseColor(in PBRSurface s)
{
	return s.vAlbedo * (1.0 - s.fMetallic);
}

float PBR_D_GGX(float NoH, float a)
{
	float a2 = a * a;
	float f = (NoH * a2 - NoH) * NoH + 1.0;
	return a2 / (PBR_PI * f * f);
}

float PBR_V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
	float a2 = a * a;
	float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
	float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
	return 0.5 / max(GGXV + GGXL, 1e-5);
}

float PBR_V_Kelemen(float LoH)
{
	return 0.25 / max(LoH * LoH, 1e-4);
}

vec3 PBR_F_Schlick(vec3 f0, float VoH)
{
	float f = pow(1.0 - VoH, 5.0);
	return f + f0 * (1.0 - f);
}

float PBR_F_Schlick(float f0, float VoH)
{
	float f = pow(1.0 - VoH, 5.0);
	return f + f0 * (1.0 - f);
}

// Outgoing radiance from a unit-intensity light along L, already multiplied by
// NoL. Multiply by the light colour and its visibility (shadow).
vec3 PBR_DirectLight(in PBRSurface s, in vec3 V, in vec3 L)
{
	vec3 N = s.vNormal;
	float NoL = clamp(dot(N, L), 0.0, 1.0);
	if (NoL <= 0.0)
	{
		return vec3(0.0);
	}
	vec3 H = normalize(V + L);
	float NoV = max(abs(dot(N, V)), 1e-4);
	float NoH = clamp(dot(N, H), 0.0, 1.0);
	float LoH = clamp(dot(L, H), 0.0, 1.0);

	float a = max(s.fRoughness * s.fRoughness, PBR_MIN_ALPHA);
	vec3 F = PBR_F_Schlick(PBR_F0(s), LoH);
	vec3 Fr = PBR_D_GGX(NoH, a) * PBR_V_SmithGGXCorrelated(NoV, NoL, a) * F;
	vec3 Fd = PBR_DiffuseColor(s) * (1.0 / PBR_PI);
	vec3 vColor = Fd + Fr;

	if (s.fClearCoat > 0.0)
	{
		float ac = max(s.fClearCoatRoughness * s.fClearCoatRoughness, PBR_MIN_ALPHA);
		float Fc = PBR_F_Schlick(0.04, LoH) * s.fClearCoat;
		float Frc = PBR_D_GGX(NoH, ac) * PBR_V_Kelemen(LoH) * Fc;
		// The coat reflects Fc before the light reaches the base layer
		vColor = vColor * (1.0 - Fc) + vec3(Frc);
	}

	return vColor * NoL;
}

// A directional light with its visibility (fShadow, 0 = in shadow), where a
// share of it (vTint, per channel) still reaches the shadowed side as diffuse:
// light scattered and bounced around the form. fWrap carries that share past
// the terminator, (N.L + wrap) / (1 + wrap). vTint 0 is PBR_DirectLight * fShadow.
vec3 PBR_DirectLightShadowed(in PBRSurface s, in vec3 V, in vec3 L, in float fShadow, in vec3 vTint, in float fWrap)
{
	vec3 vColor = PBR_DirectLight(s, V, L) * fShadow;
	float fNoL = dot(s.vNormal, L);
	float fWrapped = clamp((fNoL + fWrap) / (1.0 + fWrap), 0.0, 1.0);
	float fScattered = max(fWrapped - clamp(fNoL, 0.0, 1.0) * fShadow, 0.0);
	return vColor + PBR_DiffuseColor(s) * (1.0 / PBR_PI) * vTint * fScattered;
}

// Split-sum environment BRDF, analytic fit (Karis, "Physically Based Shading
// on Mobile"), so no LUT texture is needed.
vec2 PBR_EnvBRDFApprox(float fRoughness, float NoV)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = fRoughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
	return vec2(-1.04, 1.04) * a004 + r.zw;
}

// Specular occlusion from ambient occlusion (Lagarde & de Rousiers)
float PBR_SpecularOcclusion(float NoV, float fAO, float fRoughness)
{
	return clamp(pow(NoV + fAO, exp2(-16.0 * fRoughness - 1.0)) - 1.0 + fAO, 0.0, 1.0);
}

#endif
