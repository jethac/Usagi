#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static bool ReadFile(const std::string& path, std::string& out)
{
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file)
	{
		std::cerr << "failed to open " << path << "\n";
		return false;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	out = stream.str();
	return true;
}

static float LinearToSRGB(float value)
{
	value = std::max(value, 0.0f);
	if (value <= 0.0031308f)
	{
		return value * 12.92f;
	}

	return (1.055f * std::pow(value, 1.0f / 2.4f)) - 0.055f;
}

static uint8_t ToByte(float value)
{
	float srgb = std::min(std::max(LinearToSRGB(value), 0.0f), 1.0f);
	return static_cast<uint8_t>(std::floor((srgb * 255.0f) + 0.5f));
}

static bool ExpectByte(const char* name, float linear, uint8_t expected)
{
	uint8_t actual = ToByte(linear);
	if (actual != expected)
	{
		std::cerr << name << ": expected " << static_cast<int>(expected)
			<< ", got " << static_cast<int>(actual) << "\n";
		return false;
	}
	return true;
}

static bool ValidateShaderSource(const std::string& root)
{
	std::string shader;
	std::string colorspace;
	if (!ReadFile(root + "\\Data\\GLSL\\shaders\\PostFX\\displayencode.frag", shader) ||
		!ReadFile(root + "\\Data\\GLSL\\shaders\\includes\\colorspace.inc", colorspace))
	{
		return false;
	}

	bool ok = true;
	if (shader.find("#include \"../includes/colorspace.inc\"") == std::string::npos)
	{
		std::cerr << "displayencode.frag does not include colorspace.inc\n";
		ok = false;
	}
	if (shader.find("LinearToSRGB(linearColor.rgb)") == std::string::npos)
	{
		std::cerr << "displayencode.frag does not encode sampled RGB through LinearToSRGB\n";
		ok = false;
	}
	if (colorspace.find("0.0031308") == std::string::npos ||
		colorspace.find("12.92") == std::string::npos ||
		colorspace.find("1.0 / 2.4") == std::string::npos)
	{
		std::cerr << "colorspace.inc no longer contains the expected sRGB transfer constants\n";
		ok = false;
	}

	return ok;
}

static bool WriteRamp(const std::string& path)
{
	const int width = 256;
	const int height = 16;
	std::ofstream file(path.c_str(), std::ios::out | std::ios::binary);
	if (!file)
	{
		std::cerr << "failed to write " << path << "\n";
		return false;
	}

	file << "P6\n" << width << " " << height << "\n255\n";
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const float linear = static_cast<float>(x) / static_cast<float>(width - 1);
			const uint8_t byte = ToByte(linear);
			const char pixel[3] = {
				static_cast<char>(byte),
				static_cast<char>(byte),
				static_cast<char>(byte)
			};
			file.write(pixel, sizeof(pixel));
		}
	}

	return true;
}

static bool ValidateRamp()
{
	bool ok = true;
	ok &= ExpectByte("black", -0.25f, 0);
	ok &= ExpectByte("linear zero", 0.0f, 0);
	ok &= ExpectByte("srgb toe boundary", 0.0031308f, 10);
	ok &= ExpectByte("middle gray", 0.18f, 118);
	ok &= ExpectByte("half linear", 0.5f, 188);
	ok &= ExpectByte("white", 1.0f, 255);

	uint8_t last = 0;
	for (int x = 0; x < 256; ++x)
	{
		const float linear = static_cast<float>(x) / 255.0f;
		const uint8_t current = ToByte(linear);
		if (x > 0 && current < last)
		{
			std::cerr << "sRGB ramp is not monotonic at " << x << "\n";
			ok = false;
			break;
		}
		last = current;
	}

	return ok;
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "usage: GammaOutputTests <usagi-root> <output-ppm>\n";
		return 2;
	}

	bool ok = true;
	ok &= ValidateShaderSource(argv[1]);
	ok &= ValidateRamp();
	ok &= WriteRamp(argv[2]);

	if (!ok)
	{
		return 1;
	}

	std::cout << "Gamma output comparison passed\n";
	return 0;
}
