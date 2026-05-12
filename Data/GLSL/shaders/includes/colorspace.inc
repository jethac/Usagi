vec3 LinearToSRGB(vec3 color)
{
	color = max(color, vec3(0.0));
	vec3 lower = color * 12.92;
	vec3 higher = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
	return vec3(
		color.r <= 0.0031308 ? lower.r : higher.r,
		color.g <= 0.0031308 ? lower.g : higher.g,
		color.b <= 0.0031308 ? lower.b : higher.b);
}
