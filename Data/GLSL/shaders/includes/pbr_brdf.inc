#ifndef _PBR_BRDF_INC
#define _PBR_BRDF_INC

const float PBR_PI = 3.14159265359;

float PBRDistributionGGX(float nDotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = (nDotH * nDotH) * (a2 - 1.0) + 1.0;
	return a2 / max(PBR_PI * denom * denom, 0.0001);
}

float PBRGeometrySchlickGGX(float nDotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) * 0.125;
	return nDotV / max(nDotV * (1.0 - k) + k, 0.0001);
}

float PBRGeometrySmith(float nDotV, float nDotL, float roughness)
{
	return PBRGeometrySchlickGGX(nDotV, roughness) * PBRGeometrySchlickGGX(nDotL, roughness);
}

vec3 PBRFresnelSchlick(float cosTheta, vec3 f0)
{
	return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

#endif
