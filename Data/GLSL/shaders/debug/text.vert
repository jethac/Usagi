#include "../includes/platformdefines.inc"
#include "../includes/colorspace.inc"

// One instance per character. Its four vertices are the corners of the
// character's quad, drawn as a triangle strip. (This was a geometry shader
// expanding a point; Metal has none.)

BUFFER_LAYOUT(1, UBO_MATERIAL_ID) uniform Material
{
	mat4	mProj;
	// FIXME: Should be vec2 but not supported yet
	vec4	vCharSize;
	vec4	vTexCoordRng;
};

// Input attributes (per instance)
ATTRIB_LOC(0) in vec3 ao_position;
ATTRIB_LOC(1) in vec2 ao_texCoord0;
ATTRIB_LOC(2) in vec4 ao_color;

ATTRIB_LOC(0) out vec4 	go_vColor;
ATTRIB_LOC(1) out vec2 	go_vTexcoord;


void main(void)
{
	vec4 vPos = vec4(ao_position.xy, 0.0, 1.0) * mProj;
	vec4 vTexCoordMinMax = ao_texCoord0.xyxy + vTexCoordRng;

	// The corners in the order the geometry shader emitted them
	vec4 vOffset;
	vec2 vTexCoord;
	switch (gl_VertexIndex)
	{
	case 0:		vOffset = vCharSize.xyzz;	vTexCoord = vTexCoordMinMax.xw;	break;
	case 1:		vOffset = vCharSize.xzzz;	vTexCoord = vTexCoordMinMax.zw;	break;
	case 2:		vOffset = vCharSize.zyzz;	vTexCoord = vTexCoordMinMax.xy;	break;
	default:	vOffset = vec4(0.0);		vTexCoord = vTexCoordMinMax.zy;	break;
	}

	gl_Position		= vPos + vOffset;
	go_vColor		= toLinear(ao_color);
	go_vTexcoord	= vTexCoord;
}
