#include "../includes/platformdefines.inc"
#include "../includes/global_3d.inc"
#include "../includes/shadow/poisson_values.inc"
#include "../includes/lighting_buffer.inc"
#include "../includes/shadow/globalshadow_read.inc"
#include "../includes/model_common_ps.inc"
#include "../includes/pbr_brdf.inc"
#ifndef TRANSLUCENT_PASS
#include "../includes/depth_write.inc"
#endif
#ifdef DEFERRED_SHADING
#include "../includes/deferred.inc"
#endif

// <<GENERATED_CODE>>

ATTRIB_LOC(0) in vec4 vo_vTexCoord01;
#ifndef SHADOW_PASS
ATTRIB_LOC(1) in vec4 vo_vTexCoord23;
ATTRIB_LOC(2) in vec4 vo_vColor;
ATTRIB_LOC(3) in vec3 vo_vNormal;
#endif
#ifdef HAS_BUMP
ATTRIB_LOC(4) in vec3 vo_vTangent;
ATTRIB_LOC(5) in vec3 vo_vBinormal;
#endif
#ifndef OMNI_DEPTH
ATTRIB_LOC(6) in vec3 vo_vWorldPos;
ATTRIB_LOC(7) in vec3 vo_vViewDir;
#endif

#ifndef DEFERRED_SHADING
#ifndef SHADOW_PASS
layout(location = 0) out vec4 colorOut;
#endif
#endif

#ifndef SHADOW_PASS
vec3 GetPBRNormal()
{
	vec3 vNormal = normalize(vo_vNormal);

#ifdef HAS_BUMP
	if(uPBRMaterial.bBumpMap)
	{
		vec3 nrm_l = texture(sampler1, vo_vTexCoord01.zw).xyz;
		nrm_l.xy *= uPBRMaterial.normalScale;
		nrm_l.xyz = normalize((nrm_l * 2.0) - 1.0);
		vec3 tan_v = normalize(vo_vTangent);
		vec3 binrm_v = normalize(vo_vBinormal);
		vNormal = normalize(nrm_l.x * tan_v + nrm_l.y * binrm_v + nrm_l.z * vNormal);
	}
#endif

	return vNormal;
}

vec3 EvaluatePBRLight(vec3 vLightDir, vec3 vLightColor, vec3 vNormal, vec3 vViewDir, vec3 vAlbedo, vec3 vF0, float fMetallic, float fRoughness)
{
	vec3 vHalf = normalize(vViewDir + vLightDir);
	float fNdotL = max(dot(vNormal, vLightDir), 0.0);
	float fNdotV = max(dot(vNormal, vViewDir), 0.001);
	float fNdotH = max(dot(vNormal, vHalf), 0.0);
	float fHdotV = max(dot(vHalf, vViewDir), 0.0);

	float fD = PBRDistributionGGX(fNdotH, fRoughness);
	float fG = PBRGeometrySmith(fNdotV, fNdotL, fRoughness);
	vec3 vF = PBRFresnelSchlick(fHdotV, vF0);
	vec3 vSpecular = (fD * fG * vF) / max(4.0 * fNdotV * fNdotL, 0.001);

	vec3 vDiffuse = (vec3(1.0) - vF) * (1.0 - fMetallic) * vAlbedo / PBR_PI;
	return (vDiffuse + vSpecular) * vLightColor * fNdotL;
}

vec3 EvaluateDirectionalPBR(vec3 vNormal, vec3 vViewDir, vec3 vWorldPos, float fViewDepth, vec3 vAlbedo, vec3 vF0, float fMetallic, float fRoughness)
{
	vec3 vLighting = vec3(0.0);
	int count = iDirLightCount;

	for(int i = 0; i < count; i++)
	{
		float fShadow = 1.0;
		if(i == iCascadeLightStart)
		{
			fShadow = SampleShadowmap(fViewDepth, vWorldPos, 0);
		}

		vec3 vLightDir = normalize(lights[i].vDirection.xyz);
		vec3 vLightColor = lights[i].vDiffuse.rgb * fShadow;
		vLighting += EvaluatePBRLight(vLightDir, vLightColor, vNormal, vViewDir, vAlbedo, vF0, fMetallic, fRoughness);
	}

	return vLighting;
}

vec3 EvaluatePointPBR(vec3 vPos, vec3 vNormal, vec3 vViewDir, vec3 vAlbedo, vec3 vF0, float fMetallic, float fRoughness)
{
	vec3 vLighting = vec3(0.0);

	for(int i = 0; i < iPointLightCount; i++)
	{
		int iLight = i + iDirLightCount;
		vec3 vLightDir = lights[iLight].vPosition.xyz - vPos.xyz;

		if(dot(vLightDir, vLightDir) < lights[iLight].vRange.z)
		{
			float d = length(vLightDir);
			float fAttenuation = 1.0 - smoothstep(lights[iLight].vRange.w, lights[iLight].vRange.y, d);
			vLightDir /= d;
			vec3 vLightColor = (lights[iLight].vDiffuse.rgb + lights[iLight].vAmbient.rgb) * fAttenuation;
			vLighting += EvaluatePBRLight(vLightDir, vLightColor, vNormal, vViewDir, vAlbedo, vF0, fMetallic, fRoughness);
		}
	}

	return vLighting;
}

vec3 EvaluateSpotPBR(vec3 vPos, vec3 vNormal, vec3 vViewDir, vec3 vAlbedo, vec3 vF0, float fMetallic, float fRoughness)
{
	vec3 vLighting = vec3(0.0);
	int offset = iDirLightCount + iPointLightCount;

	for(int i = 0; i < iSpotLightCount; i++)
	{
		int iLight = i + offset;
		vec3 vLightDir = lights[iLight].vPosition.xyz - vPos.xyz;
		float d = length(vLightDir);
		float fAttenuation = 1.0;

		if(lights[iLight].vRange.y > 0.0)
		{
			vec3 vScaledLightDir = vLightDir.xyz * lights[iLight].vRange.x;
			fAttenuation = clamp(1.0 - dot(vScaledLightDir, vScaledLightDir), 0.0, 1.0);
		}

		vLightDir /= d;
		float fSpotEffect = dot(lights[iLight].vDirection.xyz, -vLightDir);
		if(dot(vNormal, vLightDir) > 0.0 && fSpotEffect > lights[iLight].fCosSpotCutoff)
		{
			fSpotEffect = min(lights[iLight].fCosInnerSpotCutoff, fSpotEffect);
			fSpotEffect = smoothstep(lights[iLight].fCosSpotCutoff, lights[iLight].fCosInnerSpotCutoff, fSpotEffect);
			fAttenuation *= fSpotEffect;
			vec3 vLightColor = lights[iLight].vDiffuse.rgb * fAttenuation;
			vLighting += EvaluatePBRLight(vLightDir, vLightColor, vNormal, vViewDir, vAlbedo, vF0, fMetallic, fRoughness);
		}
	}

	return vLighting;
}
#endif

void main(void)
{
#ifndef SHADOW_PASS
	vec3 vNormal = GetPBRNormal();
	vec3 vViewDir = normalize(vo_vViewDir);

	vec4 vBaseColor = uPBRMaterial.baseColorFactor * vo_vColor;
	if(uPBRMaterial.bBaseColorMap)
	{
		vBaseColor *= texture(sampler0, vo_vTexCoord01.xy);
	}

	vBaseColor.a *= uPBRMaterial.alpha;
	if(vBaseColor.a < uPBRMaterial.alphaCutoff)
	{
		discard;
	}

	float fMetallic = clamp(uPBRMaterial.metallicFactor, 0.0, 1.0);
	float fRoughness = clamp(uPBRMaterial.roughnessFactor, 0.04, 1.0);
	if(uPBRMaterial.bMetallicRoughnessMap)
	{
		vec4 vMetallicRoughness = texture(sampler2, vo_vTexCoord01.xy);
		fRoughness = clamp(vMetallicRoughness.g * fRoughness, 0.04, 1.0);
		fMetallic = clamp(vMetallicRoughness.b * fMetallic, 0.0, 1.0);
	}

	vec3 vEmissive = uPBRMaterial.emissiveFactor.rgb;
	if(uPBRMaterial.bEmissiveMap)
	{
		vEmissive *= texture(sampler3, vo_vTexCoord01.xy).rgb;
	}

	float fOcclusion = 1.0;
	if(uPBRMaterial.bOcclusionMap)
	{
		fOcclusion = mix(1.0, texture(sampler4, vo_vTexCoord01.xy).r, uPBRMaterial.occlusionStrength);
	}

	vec3 vAlbedo = vBaseColor.rgb;
	vec3 vF0 = mix(vec3(0.04), vAlbedo, fMetallic);

#ifdef DEFERRED_SHADING
	float fSpecularPower = mix(8.0, 128.0, 1.0 - fRoughness);
	OutputDeferredDataLinDepth(-vo_vViewDir.z, vNormal, vAlbedo * fOcclusion, vEmissive, vF0, fSpecularPower);
#else
	vec3 vLighting = vAmbientLight.rgb * vAlbedo * fOcclusion;
	vLighting += EvaluateDirectionalPBR(vNormal, vViewDir, vo_vWorldPos, -vo_vViewDir.z, vAlbedo, vF0, fMetallic, fRoughness);
	vLighting += EvaluatePointPBR(-vo_vViewDir, vNormal, vViewDir, vAlbedo, vF0, fMetallic, fRoughness);
	vLighting += EvaluateSpotPBR(-vo_vViewDir, vNormal, vViewDir, vAlbedo, vF0, fMetallic, fRoughness);

	vec3 fvReflection = normalize(reflect(vo_vViewDir, vNormal));
	fvReflection = (vec4(fvReflection, 0.0) * mInvViewMat).rgb;
	vec3 vReflection = texture(sampler5, fvReflection).rgb;
	vec3 vOut = vLighting + (vReflection * vF0 * fMetallic) + vEmissive;
#ifndef TRANSLUCENT_PASS
	OutputLinearDepth(-vo_vViewDir.z);
#endif
	colorOut = vec4(vOut, vBaseColor.a);
#endif
#endif
}
