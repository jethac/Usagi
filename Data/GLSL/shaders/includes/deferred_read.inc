
// Requires ../includes/pbr.inc to be included first (the packager does not nest includes)

SAMPLER_LOC(1, 0) uniform sampler2D sampler0; 	// Linear depth
SAMPLER_LOC(1, 1) uniform sampler2D sampler1;	// Normal + roughness
SAMPLER_LOC(1, 2) uniform sampler2D sampler2;	// Base colour + material AO
SAMPLER_LOC(1, 3) uniform sampler2D sampler3;	// Emissive
SAMPLER_LOC(1, 4) uniform sampler2D sampler4;	// Metallic, reflectance, clear coat, clear coat roughness

// See deferred.inc for the layout
PBRSurface GetDeferredSurface(in vec2 texCoord)
{
	vec4 vNormal = texture(sampler1, texCoord);
	vec4 vBase = texture(sampler2, texCoord);
	vec4 vSpec = texture(sampler4, texCoord);

	PBRSurface s;
	s.vAlbedo = vBase.rgb;
	s.vNormal = dot(vNormal.xyz, vNormal.xyz) > 0.0 ? normalize(vNormal.xyz) : vec3(0.0, 0.0, 1.0);
	s.fRoughness = vNormal.w;
	s.fMetallic = vSpec.r;
	s.fReflectance = vSpec.g;
	s.fClearCoat = vSpec.b;
	s.fClearCoatRoughness = vSpec.a;
	s.fOcclusion = vBase.a;
	return s;
}

float GetDeferredLinearDepth(in vec2 texCoord)
{
	return texture(sampler0, texCoord).x;
}

// Projection independent view space position. vRayNear/vRayFar are the view
// space points on the pixel's ray at two NDC depths (interpolated from the
// vertex shader); the stored linear depth picks the point along it. Valid for
// perspective and orthographic projections alike.
vec3 GetViewPosFromRay(in float fLinDepth01, in vec3 vRayNear, in vec3 vRayFar)
{
	float fViewZ = fLinDepth01 * vNearFar.y;
	vec3 vDir = vRayFar - vRayNear;
	return vRayNear + vDir * ((fViewZ - vRayNear.z) / vDir.z);
}

// Legacy Blinn-Phong style accessors, derived from the PBR G-buffer so that
// the older light shaders keep working.
float PBRRoughnessToSpecPow(float fRoughness)
{
	float a = max(fRoughness * fRoughness, PBR_MIN_ALPHA);
	return clamp(2.0 / (a * a) - 2.0, 1.0, 2048.0);
}

void GetDeferredDataSpecFS(in vec2 texCoord, in vec3 frustumRay, out vec3 pos, out vec3 normal, out vec3 color, out vec3 specColor, out float specularPow)
{
	PBRSurface s = GetDeferredSurface(texCoord);
	normal	= s.vNormal;
	color	= PBR_DiffuseColor(s);
	specColor = PBR_F0(s);
	specularPow = PBRRoughnessToSpecPow(s.fRoughness);

	pos = (GetDeferredLinearDepth(texCoord) * frustumRay);
}

void GetDeferredDataFS(in vec2 texCoord, in vec3 frustumRay, out vec3 pos, out vec3 normal, out vec3 color)
{
	PBRSurface s = GetDeferredSurface(texCoord);
	normal	= s.vNormal;
	color	= PBR_DiffuseColor(s);

	pos = (GetDeferredLinearDepth(texCoord) * frustumRay);
}

vec3 GetDeferredEmissive(in vec2 texCoord)
{
	return texture(sampler3, texCoord).rgb;
}

vec3 GetDeferredViewPos3D(in vec2 texCoord, in vec3 vVSValue)
{
	vec3 vViewRay = vec3(vVSValue.xy * (vNearFar.y / vVSValue.z), vNearFar.y);
	return vViewRay * GetDeferredLinearDepth(texCoord);
}

void GetDeferredData3D(in vec2 texCoord, in vec3 vVSValue, out vec3 pos, out vec3 normal, out vec3 color)
{
	PBRSurface s = GetDeferredSurface(texCoord);
	normal	= s.vNormal;
	color	= PBR_DiffuseColor(s);
	pos = GetDeferredViewPos3D(texCoord, vVSValue);
}

void GetDeferredDataSpec3D(in vec2 texCoord, in vec3 vVSValue, out vec3 pos, out vec3 normal, out vec3 color, out vec3 specCol, out float specPow)
{
	PBRSurface s = GetDeferredSurface(texCoord);
	normal	= s.vNormal;
	color	= PBR_DiffuseColor(s);
	specPow = PBRRoughnessToSpecPow(s.fRoughness);
	specCol = PBR_F0(s);
	pos = GetDeferredViewPos3D(texCoord, vVSValue);
}
