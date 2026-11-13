#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11SpriteFont.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	// Parse the DirectXTK "DXTKfont" binary .spritefont format.
	bool DX11SpriteFont::ParseSpriteFontFile(ID3D11Device* device, const std::string& path)
	{
		std::ifstream file(std::filesystem::path(path), std::ios::binary);
		if (!file)
			return false;

		// Magic header: "DXTKfont".
		char magic[8];
		file.read(magic, 8);
		if (std::string(magic, 8) != "DXTKfont")
		{
			TENLog("Invalid spritefont file format", LogLevel::Error);
			return false;
		}

		// Glyph count.
		unsigned int glyphCount;
		file.read(reinterpret_cast<char*>(&glyphCount), 4);

		// Glyph table.
		struct RawGlyph
		{
			unsigned int Character;
			int32_t TexLeft, TexTop, TexRight, TexBottom;
			float XOffset, YOffset, XAdvance;
		};

		std::vector<RawGlyph> rawGlyphs(glyphCount);
		file.read(reinterpret_cast<char*>(rawGlyphs.data()), glyphCount * sizeof(RawGlyph));

		// Line spacing.
		file.read(reinterpret_cast<char*>(&_lineSpacing), 4);

		// Default character.
		unsigned int defaultChar;
		file.read(reinterpret_cast<char*>(&defaultChar), 4);
		_defaultChar = (wchar_t)defaultChar;

		// Texture data header.
		unsigned int texWidth, texHeight, texFormat, texStride, texRows;
		file.read(reinterpret_cast<char*>(&texWidth), 4);
		file.read(reinterpret_cast<char*>(&texHeight), 4);
		file.read(reinterpret_cast<char*>(&texFormat), 4);
		file.read(reinterpret_cast<char*>(&texStride), 4);
		file.read(reinterpret_cast<char*>(&texRows), 4);

		_atlasWidth = texWidth;
		_atlasHeight = texHeight;

		unsigned int texDataSize = texStride * texRows;
		std::vector<unsigned char> texData(texDataSize);
		file.read(reinterpret_cast<char*>(texData.data()), texDataSize);

		// Create D3D11 texture. The stride and row count of the file already match the format, as with DirectXTK.
		auto dxgiFormat = (DXGI_FORMAT)texFormat;
		switch (dxgiFormat)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B4G4R4A4_UNORM:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			break;

		default:
			TENLog("Unsupported spritefont texture format: " + std::to_string(texFormat), LogLevel::Error);
			return false;
		}

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = texWidth;
		desc.Height = texHeight;
		desc.Format = dxgiFormat;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_IMMUTABLE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = texData.data();
		initData.SysMemPitch = texStride;

		ComPtr<ID3D11Texture2D> texture;
		HRESULT hr = device->CreateTexture2D(&desc, &initData, texture.GetAddressOf());
		if (FAILED(hr))
		{
			TENLog("Failed to create spritefont texture", LogLevel::Error);
			return false;
		}

		// Create SRV.
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = dxgiFormat;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, _atlasSRV.GetAddressOf());
		if (FAILED(hr))
		{
			TENLog("Failed to create spritefont SRV", LogLevel::Error);
			return false;
		}

		// Store glyphs.
		for (const auto& rg : rawGlyphs)
		{
			FontGlyph g;
			g.Character = rg.Character;
			g.Subrect = RendererRectangle(rg.TexLeft, rg.TexTop, rg.TexRight, rg.TexBottom);
			g.XOffset = rg.XOffset;
			g.YOffset = rg.YOffset;
			g.XAdvance = rg.XAdvance;
			_glyphs[rg.Character] = g;
		}

		return true;
	}

	DX11SpriteFont::DX11SpriteFont(ID3D11Device* device, std::string fontPath)
	{
		if (!ParseSpriteFontFile(device, fontPath))
			TENLog("Failed to load spritefont", LogLevel::Error);
	}

	float DX11SpriteFont::GetLineSpacing()
	{
		return _lineSpacing;
	}

	const DX11SpriteFont::FontGlyph* DX11SpriteFont::FindGlyphInternal(unsigned int character) const
	{
		auto it = _glyphs.find(character);
		if (it != _glyphs.end())
			return &it->second;

		it = _glyphs.find((unsigned int)_defaultChar);
		if (it != _glyphs.end())
			return &it->second;

		return nullptr;
	}

	// Walks the glyphs of a string as DirectXTK SpriteFont does: positions are in unscaled font units, a line never starts
	// left of its origin, and empty whitespace glyphs are skipped.
	void DX11SpriteFont::ForEachGlyph(const std::wstring& text, const std::function<void(const FontGlyph&, float, float)>& action) const
	{
		float x = 0.0f;
		float y = 0.0f;

		for (wchar_t character : text)
		{
			if (character == L'\r')
				continue;

			if (character == L'\n')
			{
				x = 0.0f;
				y += _lineSpacing;
				continue;
			}

			const auto* glyph = FindGlyphInternal((unsigned int)character);
			if (glyph == nullptr)
				continue;

			x += glyph->XOffset;
			if (x < 0.0f)
				x = 0.0f;

			float width = (float)(glyph->Subrect.Right - glyph->Subrect.Left);
			float height = (float)(glyph->Subrect.Bottom - glyph->Subrect.Top);
			if (!iswspace(character) || width > 1.0f || height > 1.0f)
				action(*glyph, x, y);

			x += width + glyph->XAdvance;
		}
	}

	Vector2 DX11SpriteFont::MeasureString(const std::string& str)
	{
		auto result = Vector2::Zero;
		ForEachGlyph(TEN::Utils::ToWString(str), [&](const FontGlyph& glyph, float x, float y)
		{
			float width = (float)(glyph.Subrect.Right - glyph.Subrect.Left);
			float height = (float)(glyph.Subrect.Bottom - glyph.Subrect.Top) + glyph.YOffset;
			height = iswspace((wchar_t)glyph.Character) ? _lineSpacing : std::max(height, _lineSpacing);

			result = Vector2::Max(result, Vector2(x + width, y + height));
		});

		return result;
	}

	Glyph DX11SpriteFont::FindGlyph(char c)
	{
		auto* g = FindGlyphInternal((unsigned int)(unsigned char)c);
		Glyph result = {};
		if (g)
		{
			result.Character = g->Character;
			result.Subrect = g->Subrect;
			result.XOffset = g->XOffset;
			result.YOffset = g->YOffset;
			result.XAdvance = g->XAdvance;
		}
		return result;
	}

	void DX11SpriteFont::DrawString(ISpriteBatch* spriteBatch, const std::string& text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		if (!_atlasSRV || _atlasWidth == 0 || _atlasHeight == 0)
			return;

		auto* dxBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
		float invWidth = 1.0f / (float)_atlasWidth;
		float invHeight = 1.0f / (float)_atlasHeight;

		ForEachGlyph(TEN::Utils::ToWString(text), [&](const FontGlyph& glyph, float x, float y)
		{
			float left = position.x + ((x - origin.x) * scale);
			float top = position.y + ((y + glyph.YOffset - origin.y) * scale);
			float width = (float)(glyph.Subrect.Right - glyph.Subrect.Left) * scale;
			float height = (float)(glyph.Subrect.Bottom - glyph.Subrect.Top) * scale;

			dxBatch->DrawWithSRV(_atlasSRV.Get(), left, top, left + width, top + height,
				glyph.Subrect.Left * invWidth, glyph.Subrect.Top * invHeight, glyph.Subrect.Right * invWidth, glyph.Subrect.Bottom * invHeight, color);
		});
	}
}

#endif
