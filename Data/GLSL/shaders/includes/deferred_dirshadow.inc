#ifndef _DEFERRED_LIGHTING_INC
#define _DEFERRED_LIGHTING_INC

// Additive pass for a shadowed directional light after the first (LIGHT_INDEX)

// Requires ../includes/deferred_env.inc to be included first

ATTRIB_LOC(0) in vec2 vo_vTexCoord;
ATTRIB_LOC(1) in vec3 vo_vFSVector;
ATTRIB_LOC(2) in vec3 vo_vRayNear;
ATTRIB_LOC(3) in vec3 vo_vRayFar;

layout(location = 0) out vec4 colorOut;

void main(void)
{
	float fDepth = GetDeferredLinearDepth(vo_vTexCoord);
	vec3 vPos = GetViewPosFromRay(fDepth, vo_vRayNear, vo_vRayFar);
	PBRSurface s = GetDeferredSurface(vo_vTexCoord);
	vec3 V = normalize(vo_vRayNear - vo_vRayFar);

	vec3 L = lights[LIGHT_INDEX].vDirection.xyz;
	vec3 vLit = PBR_DirectLight(s, V, L) * lights[LIGHT_INDEX].vDiffuse.rgb;

	int iCascadeSet = LIGHT_INDEX - iCascadeLightStart;
	if (vPos.z < cascades[iCascadeSet].vSplitDistance.w)
	{
		vec3 vWorldPos = (vec4(vPos, 1.0) * mInvViewMat).xyz;
		vLit *= SampleShadowmap(vPos.z, vWorldPos.xyz, iCascadeSet);
	}

	colorOut = vec4(vLit, 1.0);
}

#endif
