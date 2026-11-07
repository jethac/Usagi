#include "../includes/platformdefines.inc"
#include "../includes/global_3d.inc"
#include "../includes/shadow/poisson_values.inc"
#include "../includes/lighting_buffer.inc"
#include "../includes/shadow/globalshadow_read.inc"
#include "../includes/model_common_ps.inc"
#ifndef TRANSLUCENT_PASS
#include "../includes/depth_write.inc"
#endif
#ifdef DEFERRED_SHADING
#include "../includes/deferred.inc"
#else
#include "../includes/forward_lighting.inc"
#endif

// <<GENERATED_CODE>>

#ifdef PORTAL_TEST
#include "../includes/portalclip.inc"
#endif


#ifndef SHADOW_PASS
ATTRIB_LOC(0) in vec4 vo_vTexCoord01;
ATTRIB_LOC(1) in vec4 vo_vTexCoord23;
ATTRIB_LOC(2) in vec4 vo_vColor;
ATTRIB_LOC(3) in vec3 vo_vNormal;
#ifdef HAS_BUMP
ATTRIB_LOC(4) in vec3 vo_vTangent;
ATTRIB_LOC(5) in vec3 vo_vBinormal;
#endif
#ifndef OMNI_DEPTH
ATTRIB_LOC(6) in vec3 vo_vWorldPos;
ATTRIB_LOC(7) in vec3 vo_vViewDir;
ATTRIB_LOC(8) in vec2 vo_vPaintUV;
#endif
#elif defined(PORTAL_TEST)
ATTRIB_LOC(0) in vec3 vo_vWorldPos;
#endif

#ifndef DEFERRED_SHADING
#ifndef SHADOW_PASS
layout(location = 0) out vec4 colorOut;
#endif
#endif


void CheckForDiscard(float fAlpha, float fAlphaRef, int iAlphaCmp)
{
	switch(iAlphaCmp)
	{
		case 0:
			if(fAlpha < fAlphaRef)
				discard;
			break;
		case 1:
			if(fAlpha == fAlphaRef)
				discard;
			break;
		case 2:
			if(fAlpha > fAlphaRef)
				discard;
			break;			
	}
}

// Entry point
void main(void)
{
#ifdef PORTAL_TEST
	if ( ShouldPortalDiscardFragment(vo_vWorldPos) )
	{
		discard;
	}
#endif

#ifndef SHADOW_PASS
	vec3	vSrcColArg[3];
	float	vSrcAlphaArg[3];

	vec3 vNormal = vo_vNormal;

#ifndef OMNI_DEPTH
	// Materials draw without face culling by default, so the side of a surface we
	// see may be the one its normal points away from: a mesh whose winding or
	// normals came out mirrored from the exporter, or the back of a card. Light
	// the visible side: orient the shading normal toward the camera using the
	// surface's geometric normal (screen space derivatives, so smoothed vertex
	// normals near a silhouette are left alone). Without this, inverted props are
	// lit from below: black from the top-down table camera, and only lit by the
	// horizon of the environment from the couch camera.
	vec3 vGeoNormal = cross(dFdx(vo_vViewDir), dFdy(vo_vViewDir));
	// Toward the eye: -view position for a perspective camera, -Z for an orthographic one
	vec3 vToEye = mProjMat[3][3] > 0.5 ? vec3(0.0, 0.0, -1.0) : vo_vViewDir;
	float fFacing = dot(vGeoNormal, vToEye) * dot(vGeoNormal, vo_vNormal);
	float fSideSign = fFacing < 0.0 ? -1.0 : 1.0;
	vNormal *= fSideSign;
#else
	float fSideSign = 1.0;
#endif

#ifdef HAS_BUMP
	if(uMaterial.bBumpMap)
	{
        // FIXME: Use 2 component dxt5 at some point
        // PBR materials sample every map with the first UV set
        vec3 nrm_l = texture(sampler1, uMaterial.bPBR ? vo_vTexCoord01.xy : vo_vTexCoord01.zw).xyz;
        nrm_l.xyz = normalize((nrm_l * 2.0) - 1.0);
        if (uMaterial.bFlipNormalY)
        {
            nrm_l.y = -nrm_l.y;
        }
        // The whole tangent frame turns with the visible side (see above)
        vec3 tan_v = normalize(vo_vTangent) * fSideSign;
        vec3 binrm_v = normalize(vo_vBinormal) * fSideSign;
        vNormal = normalize(nrm_l.x * tan_v + nrm_l.y * binrm_v + nrm_l.z * vNormal);
    }
#endif
	vec3  vViewDir	= normalize( vo_vViewDir );

	vec3 vEmissive = vec3(0.0, 0.0, 0.0);
	vec4 vSpecular = vec4(1.0);

	if(uMaterial.bEmissiveMap)
	{
    	vEmissive = texture( sampler2, vo_vTexCoord01.xy ).rgb; 
    	vEmissive *= uMaterial.emissionMapMul.rgb;
	}
	
	vEmissive += uMaterial.emission.rgb;

#ifndef DEFERRED_SHADING
	vec4 vFragPrimaryColor = vec4(0);
	vec4 vFragSecondaryColor = vec4(0.0, 0.0, 0.0, 0.0);
	DirectionLighting( vNormal, vViewDir, vo_vWorldPos, -vo_vViewDir.z, vFragPrimaryColor, vFragSecondaryColor, uMaterial.specularpow);
	PointLighting( -vo_vViewDir, vNormal, vViewDir, vFragPrimaryColor, vFragSecondaryColor, uMaterial.specularpow);
	SpotLighting( -vo_vViewDir, vNormal, vViewDir, vFragPrimaryColor, vFragSecondaryColor, uMaterial.specularpow);

	vFragPrimaryColor += vAmbientLight;
#endif

	vec4 vSpecRead = vec4(1.0, 1.0, 1.0, 0.0);
	if(uMaterial.bSpecMap)
	{
    	vSpecRead = texture( sampler3, vo_vTexCoord01.zw ); 
	}
	vSpecular.rgb *= uMaterial.specular.rgb;

	vSpecular.rgb *= vSpecRead.rgb; 
	vSpecular *= vo_vColor;

	if(uMaterial.bReflectionMap)
	{
		vec3 fvReflection =  normalize(reflect(vo_vViewDir, vNormal));
		fvReflection = (vec4(fvReflection, 0.0) * mInvViewMat).rgb;
		vEmissive += (texture( sampler4, fvReflection ).rgb) * uMaterial.reflectionfactor * vSpecRead.aaa;
	}


	// other lighting methods will be here.

	vec4 vDiffuse = vec4(1.0, 1.0, 1.0, 1.0);

	if(uMaterial.bDiffuseMap)
	{
		vec4 vRead = texture(sampler0, vo_vTexCoord01.xy);
		vDiffuse.rgb *= vRead.rgb;
		vDiffuse.a = vRead.a;
	}
	// A mesh with no vertex-colour layer arrives here as black, which would zero
	// the albedo and render the surface unlit-black. Treat an all-black vertex
	// colour as white so the material colour still comes through.
	vec4 vVtxCol = vo_vColor;
	if (dot(vVtxCol.rgb, vec3(1.0)) < 0.001) { vVtxCol.rgb = vec3(1.0, 1.0, 1.0); }
	vDiffuse *= vVtxCol;
	vDiffuse *= uMaterial.diffuse;

	// Two-scale layer map: a broad macro variation and a fine detail, each at its own
	// repeat of the base tile so a large tiling surface (a table top, a floor) doesn't
	// show the base tile's repeat at a distance and isn't blurry close up.
	float fLayer = 0.0;
	if (uMaterial.bLayerMap)
	{
		float fMacro = texture(sampler6, vo_vTexCoord01.xy * uMaterial.layerParams.x).r;
		float fDetail = texture(sampler6, vo_vTexCoord01.xy * uMaterial.layerParams.y).g;
		vDiffuse.rgb *= mix(1.0, 2.0 * fMacro, uMaterial.layerParams.z) * mix(1.0, 2.0 * fDetail, uMaterial.layerParams.w);
		fLayer = (fMacro - 0.5) * uMaterial.layerParams.z + (fDetail - 0.5) * uMaterial.layerParams.w;
	}

	// Mark map (bMarkMap, see Model.yml): deposits laid over the surface in world XZ
	float fMarkCover = 0.0;
	if (uMaterial.bMarkMap)
	{
		vec2 vMarkUV = (vo_vWorldPos.xz - uMaterial.markRect.xy) * uMaterial.markRect.zw;
		if (all(greaterThanEqual(vMarkUV, vec2(0.0))) && all(lessThanEqual(vMarkUV, vec2(1.0))))
		{
			vec2 vMark = texture(sampler3, vMarkUV).rg;
			vDiffuse.rgb *= 1.0 - vMark.r * uMaterial.markParams.x;
			fMarkCover = vMark.g;
		}
	}

	// Livery (bLiveryMap, see Model.yml): a premultiplied RGBA image in the "paint" UV set,
	// laid over the albedo. Faces outside paint space (glass, wheels, trim) are untouched.
	if (uMaterial.bLiveryMap)
	{
		vec2 vP = vo_vPaintUV;
		if (all(greaterThanEqual(vP, vec2(0.0))) && all(lessThanEqual(vP, vec2(1.0))))
		{
			vec4 vLivery = texture(sampler6, vP);
			vDiffuse.rgb = vDiffuse.rgb * (1.0 - vLivery.a) + vLivery.rgb;
		}
	}

	// Dev view of the "paint" UV set (iPaintUVDebug, see Model.yml); emissive so it reads unlit
	if (uMaterial.iPaintUVDebug != 0)
	{
		vec2 vP = vo_vPaintUV;
		vec3 vDbg = vec3(0.0, 0.0, 0.25);
		if (all(greaterThanEqual(vP, vec2(0.0))) && all(lessThanEqual(vP, vec2(1.0))))
		{
			if (uMaterial.iPaintUVDebug == 1)
			{
				vec2 vCell = abs(fract(vP * 10.0 + 0.5) - 0.5) / fwidth(vP * 10.0);
				float fLine = 1.0 - clamp(min(vCell.x, vCell.y) - 0.5, 0.0, 1.0);
				vDbg = mix(vec3(vP, 0.0), vec3(1.0), fLine * 0.6);
			}
			else
			{
				vec2 vC = floor(vP * 8.0);
				vDbg = mod(vC.x + vC.y, 2.0) < 0.5 ? vec3(0.9) : vec3(0.1);
			}
		}
		vDiffuse.rgb = vec3(0.0);
		vEmissive = vDbg;
	}

	vec3 rim = vec3(GetRimValue(1.0, 4.0, vNormal.z)) * vSpecular.rgb;

	//vOut.xyz += rim;
	//vOut.xyz += ApplyHemisphereLighting(vec3(0.4)*vSpecular.rgb, vNormal);


#ifdef ALPHA_TEST
	CheckForDiscard(vDiffuse.a * uMaterial.alpha, uMaterial.alphaRef, uMaterial.alphaCmp);
#endif

#ifdef DEFERRED_SHADING
	if (uMaterial.bPBR)
	{
		float fRoughness = uMaterial.roughness;
		float fMetallic = uMaterial.metallic;
		float fOcclusion = 1.0;
		if (uMaterial.bMetalRoughMap)
		{
			vec3 vORM = texture(sampler5, vo_vTexCoord01.xy).rgb;
			fOcclusion = mix(1.0, vORM.r, uMaterial.occlusionStrength);
			fRoughness *= vORM.g;
			fMetallic *= vORM.b;
		}
		fRoughness = clamp(fRoughness * (1.0 + uMaterial.layerRoughness * 2.0 * fLayer), 0.02, 1.0);
		fRoughness = mix(fRoughness, uMaterial.markParams.y, fMarkCover);
		float fClearcoat = uMaterial.clearcoat * (1.0 - fMarkCover * uMaterial.markParams.z);
		OutputDeferredPBR(-vo_vViewDir.z, vNormal, vDiffuse.rgb, vEmissive, fRoughness, fMetallic, uMaterial.reflectance,
			fClearcoat, uMaterial.clearcoatRoughness, fOcclusion);
	}
	else
	{
		// TODO: Make all specular single channel only
		OutputDeferredDataLinDepth(-vo_vViewDir.z, vNormal, vDiffuse.rgb, vEmissive, vSpecular.rgb, uMaterial.specularpow);
	}
#else
	vec4 vOut;
	vOut.rgb = (vDiffuse.rgb * vFragPrimaryColor.rgb) + (vSpecular.rgb * vFragSecondaryColor.rgb);
	vOut.a = vDiffuse.a;
	vOut.xyz += vEmissive;
#ifndef TRANSLUCENT_PASS
	OutputLinearDepth(-vo_vViewDir.z);
#endif
	vOut.a *= uMaterial.alpha;
	//vOut.a = 1.0;
	colorOut = vOut;
#endif

#else
	// Shadow pass
	// colorOut = vec4(1.0, 0.0, 0.0, 0.0);
#endif
}
