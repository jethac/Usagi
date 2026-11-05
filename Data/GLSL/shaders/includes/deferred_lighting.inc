#ifndef _DEFERRED_LIGHTING_INC
#define _DEFERRED_LIGHTING_INC

// Deferred base pass: every unshadowed directional light plus the first
// shadowed one (further shadowed lights are added by deferred_dirshadow.inc),
// image based lighting, flat ambient and emissive.

// Requires ../includes/deferred_env.inc to be included first

ATTRIB_LOC(0) in vec2 vo_vTexCoord;
ATTRIB_LOC(1) in vec3 vo_vFSVector;
ATTRIB_LOC(2) in vec3 vo_vRayNear;
ATTRIB_LOC(3) in vec3 vo_vRayFar;

layout(location = 0) out vec4 colorOut;

float SampleDirShadow(in vec3 vPos, in vec3 vNormal, in vec3 L, in int iCascadeSet)
{
	float fShadow = 1.0;
	if (vPos.z < cascades[iCascadeSet].vSplitDistance.w)
	{
		vec3 vWorldPos = (vec4(vPos, 1.0) * mInvViewMat).xyz;
		fShadow = SampleShadowmap(vPos.z, vWorldPos.xyz, iCascadeSet);
	}
	return min(fShadow, ContactShadow(vPos, vNormal, L));
}

void main(void)
{
	float fDepth = GetDeferredLinearDepth(vo_vTexCoord);
	vec3 vPos = GetViewPosFromRay(fDepth, vo_vRayNear, vo_vRayFar);
	PBRSurface s = GetDeferredSurface(vo_vTexCoord);
	// Toward the eye along this pixel's ray (constant for orthographic views)
	vec3 V = normalize(vo_vRayNear - vo_vRayFar);

	vec3 vColor = vec3(0.0);

	int iLastLight = iDirLightCount - 1;
	if (iCascadeLightStart >= 0 && iCascadeLightStart < iDirLightCount)
	{
		iLastLight = iCascadeLightStart;
	}

	for (int i = 0; i <= iLastLight; i++)
	{
		vec3 L = lights[i].vDirection.xyz;
		vec3 vLit = PBR_DirectLight(s, V, L) * lights[i].vDiffuse.rgb;
		if (i == iCascadeLightStart)
		{
			vLit *= SampleDirShadow(vPos, s.vNormal, L, 0);
		}
		vColor += vLit;
	}

	float fOcclusion = s.fOcclusion * ScreenSpaceAO(vPos, s.vNormal);
	vColor += EvaluateIBL(s, V, fOcclusion);
	vColor += vAmbientLight.rgb * vEnvParams.z * PBR_DiffuseColor(s) * fOcclusion;
	vColor += GetDeferredEmissive(vo_vTexCoord);

	colorOut = vec4(vColor, 1.0);

	// Debug views (USAGI_DEFERRED_DEBUG)
	int iDebug = int(vShadowParams.w);
	if (iDebug == 1) colorOut = vec4(s.vAlbedo, 1.0);
	else if (iDebug == 2) colorOut = vec4(s.vNormal * 0.5 + 0.5, 1.0);
	else if (iDebug == 3) colorOut = vec4(s.fRoughness, s.fMetallic, s.fClearCoat, 1.0);
	else if (iDebug == 4) colorOut = vec4(EvaluateIBL(s, V, s.fOcclusion), 1.0);
	else if (iDebug == 5) colorOut = vec4(vec3(iCascadeLightStart >= 0 ? SampleDirShadow(vPos, s.vNormal, lights[iCascadeLightStart].vDirection.xyz, 0) : 1.0), 1.0);
	else if (iDebug == 6) colorOut = vec4(fract(vPos * 0.1), 1.0);
	else if (iDebug == 7) colorOut = vec4(vec3(fOcclusion), 1.0);
}

#endif
