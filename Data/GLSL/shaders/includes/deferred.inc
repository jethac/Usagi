
// G-buffer layout (metal/roughness PBR)
//  Diffuse  (sRGB RGBA8) : base colour rgb, a = material ambient occlusion
//  Normal   (RGBA16 snorm): view space normal xyz, w = perceptual roughness
//  Emissive (sRGB RGBA8) : emissive rgb, a = foliage (0..1: no screen space AO or contact
//                          shadow, and a little sunlight through it; 0 for everything else)
//  Specular (RGBA8)      : metallic, reflectance (0.5 = 4% F0), clear coat, clear coat roughness
layout(location = 0) out vec4 po_vDiffuse;
layout(location = 2) out vec4 po_vNormal;
//layout(location = 3) out vec4 po_vVelocity;
layout(location = 3) out vec4 po_vEmissive;
layout(location = 4) out vec4 po_vSpecular;

void OutputDeferredPBR(float fEyeDepth, vec3 vViewNormal, vec3 vAlbedo, vec3 vEmissive, float fRoughness, float fMetallic, float fReflectance,
	float fClearCoat, float fClearCoatRoughness, float fOcclusion)
{
	OutputLinearDepth(fEyeDepth);
	po_vNormal		= vec4(vViewNormal, clamp(fRoughness, 0.0, 1.0));
	po_vDiffuse		= vec4(vAlbedo, clamp(fOcclusion, 0.0, 1.0));
	po_vEmissive	= vec4(vEmissive, 0.0);
	po_vSpecular	= clamp(vec4(fMetallic, fReflectance, fClearCoat, fClearCoatRoughness), 0.0, 1.0);
}

// Blinn-Phong style materials (specular colour + power) mapped onto the PBR
// G-buffer: the power becomes a roughness (Beckmann/Phong equivalence) and the
// specular colour's strength a dielectric reflectance.
float LegacySpecPowToRoughness(float fSpecPow)
{
	return sqrt(sqrt(2.0 / (max(fSpecPow, 1.0) + 2.0)));
}

float LegacySpecColToReflectance(vec3 vSpecCol)
{
	// A legacy specular of ~0.3 grey reads as a plain 4% dielectric
	float fLevel = clamp(dot(vSpecCol, vec3(0.333)) / 0.3, 0.0, 1.0);
	return 0.5 * sqrt(fLevel);
}

void OutputDeferredDataLinDepth(vec3 vEyePos, vec3 vViewNormal, vec3 color, vec3 vEmissive, vec3 specCol, float specularPow)
{
	OutputDeferredPBR(vEyePos.z, vViewNormal, color, vEmissive, LegacySpecPowToRoughness(specularPow), 0.0, LegacySpecColToReflectance(specCol), 0.0, 0.0, 1.0);
}

void OutputDeferredDataLinDepth(float fEyeDepth, vec3 vViewNormal, vec3 color, vec3 vEmissive, vec3 specCol, float specularPow)
{
	OutputDeferredPBR(fEyeDepth, vViewNormal, color, vEmissive, LegacySpecPowToRoughness(specularPow), 0.0, LegacySpecColToReflectance(specCol), 0.0, 0.0, 1.0);
}

#if 0
void OutputScreenSpaceVelocity(vec4 vPos, vec4 vPrevPos)
{
	vec2 a = (vPos.xy / vPos.ww);
	vec2 b = (vPrevPos.xy / vPrevPos.ww);

	vec2 vVel = (a-b)*vMotionBlurVars.xy;

	po_vVelocity = vec4(vVel, 0.0, 0.0);
}
#endif
