#pragma once

// Decodes image files into RGBA8 pixels for textures, with the formats and color space rules the DirectXTK WIC loader
// used: PNG, JPEG, BMP, TGA, GIF and DDS with BC1, BC2 or BC3 blocks.

namespace TEN::Renderer::Graphics
{
	struct DecodedImage
	{
		int							Width  = 0;
		int							Height = 0;
		std::vector<unsigned char>	Pixels = {}; // RGBA8, rows without padding.
		bool						IsSrgb = false;
	};

	bool DecodeImage(const unsigned char* data, int dataSize, DecodedImage& image);
	bool IsDdsImage(const unsigned char* data, int dataSize);
}
