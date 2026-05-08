#include "framework.h"

#ifdef HAS_DX11

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

		// Create D3D11 texture.
		DXGI_FORMAT dxgiFormat;
		bool isCompressed = false;
		int blockSize = 0;

		switch (texFormat)
		{
		case 74: // DXGI_FORMAT_BC2_UNORM
			dxgiFormat = DXGI_FORMAT_BC2_UNORM;
			isCompressed = true;
			blockSize = 16;
			break;
		case 28: // DXGI_FORMAT_R8G8B8A8_UNORM
			dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case 87: // DXGI_FORMAT_B8G8R8A8_UNORM
			dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
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

	Vector2 DX11SpriteFont::MeasureStringInternal(const wchar_t* str)
	{
		float x = 0, maxX = 0, y = _lineSpacing;
		while (*str)
		{
			if (*str == L'\n')
			{
				maxX = std::max(maxX, x);
				x = 0;
				y += _lineSpacing;
			}
			else
			{
				auto* g = FindGlyphInternal((unsigned int)*str);
				if (g)
				{
					x += g->XOffset;
					if (x < 0) x = 0;

					float w = (float)(g->Subrect.Right - g->Subrect.Left);
					maxX = std::max(maxX, x + w);
					x += w + g->XAdvance;
				}
			}
			str++;
		}
		return Vector2(std::max(maxX, x), y);
	}

	Vector2 DX11SpriteFont::MeasureString(const std::string& str)
	{
		std::wstring wstr(str.begin(), str.end());
		return MeasureStringInternal(wstr.c_str());
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

	void DX11SpriteFont::DrawStringInternal(ISpriteBatch* spriteBatch, const wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		if (!_atlasSRV || _atlasWidth == 0 || _atlasHeight == 0)
			return;

		auto* dxBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
		float invW = 1.0f / (float)_atlasWidth;
		float invH = 1.0f / (float)_atlasHeight;

		float x = position.x - origin.x * scale;
		float y = position.y - origin.y * scale;

		while (*text)
		{
			if (*text == L'\n')
			{
				x = position.x - origin.x * scale;
				y += _lineSpacing * scale;
			}
			else
			{
				auto* g = FindGlyphInternal((unsigned int)*text);
				if (g)
				{
					x += g->XOffset * scale;

					float gy = y + g->YOffset * scale;
					float gw = (float)(g->Subrect.Right - g->Subrect.Left) * scale;
					float gh = (float)(g->Subrect.Bottom - g->Subrect.Top) * scale;

					// Compute atlas UV coordinates for this glyph.
					float u0 = g->Subrect.Left * invW;
					float v0 = g->Subrect.Top * invH;
					float u1 = g->Subrect.Right * invW;
					float v1 = g->Subrect.Bottom * invH;

					// Draw this glyph as a sprite quad via the SpriteBatch.
					RendererRectangle area((int)x, (int)gy, (int)(x + gw), (int)(gy + gh));
					dxBatch->DrawWithSRV(_atlasSRV.Get(), area, u0, v0, u1, v1, color);

					x += (gw + g->XAdvance * scale);
				}
			}
			text++;
		}
	}

	void DX11SpriteFont::DrawString(ISpriteBatch* spriteBatch, const std::string& text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		std::wstring wtext(text.begin(), text.end());
		DrawStringInternal(spriteBatch, wtext.c_str(), position, color, rotation, origin, scale);
	}
}

#endif
