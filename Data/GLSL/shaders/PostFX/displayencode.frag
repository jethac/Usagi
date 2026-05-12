#include "../includes/platformdefines.inc"
#include "../includes/colorspace.inc"

ATTRIB_LOC(0) in vec2 vo_vTexCoord;

SAMPLER_LOC(1, 0) uniform sampler2D sampler0;

layout(location = 0) out vec4 colorOut;

void main(void)
{
	vec4 linearColor = texture(sampler0, vo_vTexCoord);
	colorOut = vec4(LinearToSRGB(linearColor.rgb), linearColor.a);
}
