#include "framework.h"
#include "Renderer/Renderer.h"

#include "Specific/trutils.h"
#include "Specific/winmain.h"

namespace TEN::Renderer
{
	void Renderer::AddDebugString(const std::string& string, const Vector2& pos, const Color& color, float scale, RendererDebugPage page)
	{
		constexpr auto FLAGS = (int)PrintStringFlags::Outline | (int)PrintStringFlags::Center;

		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		AddString(string, pos, color, scale, FLAGS);
	}

	void Renderer::AddString(int x, int y, const std::string& string, D3DCOLOR color, int flags)
	{
		AddString(string, Vector2(x, y), Color(color), 1.0f, flags);
	}

	void Renderer::AddString(const std::string& string, const Vector2& pos, const Color& color, float scale, int flags)
	{
		if (_isLocked)
			return;

		if (string.empty())
			return;

		try
		{
			auto screenRes = GetScreenResolution();
			auto factor = Vector2(screenRes.x / DISPLAY_SPACE_RES.x, screenRes.y / DISPLAY_SPACE_RES.y);
			float uiScale = (screenRes.x > screenRes.y) ? factor.y : factor.x;
			float fontSpacing = _gameFont->GetLineSpacing();
			float fontScale = REFERENCE_FONT_SIZE / fontSpacing;
			float stringScale = (uiScale * fontScale) * scale;

			auto stringLines = SplitString(TEN::Utils::ToWString(string));

			// Calculate total height for vertical centering.
			float totalHeight = 0.0f;
			for (const auto& line : stringLines)
			{
				if (line.empty())
					totalHeight += fontSpacing * stringScale;
				else
					totalHeight += Vector2(_gameFont->MeasureString(line.c_str())).y * stringScale;
			}

			// Compute vertical offset if vertical centering is requested.
			float yBase = (flags & (int)PrintStringFlags::VerticalCenter) ? (pos.y * uiScale) - (totalHeight / 2.0f) : (pos.y * uiScale);

			float yOffset = 0.0f;
			for (const auto& line : stringLines)
			{
				// Prepare structure for renderer.
				RendererStringToDraw rString;
				rString.String = line;
				rString.Flags = flags;
				rString.X = 0;
				rString.Y = 0;
				rString.Color = color;
				rString.Scale = stringScale;

				// Measure string.
				auto size = line.empty() ? Vector2(0, fontSpacing * rString.Scale) : Vector2(_gameFont->MeasureString(line.c_str())) * rString.Scale;

				if (flags & (int)PrintStringFlags::Center)
				{
					rString.X = (pos.x * factor.x) - (size.x / 2.0f);
				}
				else if (flags & (int)PrintStringFlags::Right)
				{
					rString.X = (pos.x * factor.x) - size.x;
				}
				else
				{
					// Calculate indentation to account for string scaling.
					auto indent = line.empty() ? 0 : _gameFont->FindGlyph(line.at(0))->XAdvance * rString.Scale;
					rString.X = pos.x * factor.x + indent;
				}

				rString.Y = yBase + yOffset;

				if (flags & (int)PrintStringFlags::Blink)
				{
					rString.Color *= _blinkColorValue;
				}

				yOffset += size.y;
				_stringsToDraw.push_back(rString);
			}
		}
		catch (std::exception& ex)
		{
			TENLog(std::string("Unable to process string: '") + string + "'. Exception: " + std::string(ex.what()), LogLevel::Error);
		}
	}

	void Renderer::DrawAllStrings()
	{
		if (_stringsToDraw.empty())
			return;

		SetBlendMode(BlendMode::AlphaBlend);

		float shadowOffset = 1.5f / (REFERENCE_FONT_SIZE / _gameFont->GetLineSpacing());
		_spriteBatch->Begin();

		for (const auto& rString : _stringsToDraw)
		{
			// Draw shadow.
			if (rString.Flags & (int)PrintStringFlags::Outline)
			{
				_gameFont->DrawString(
					_spriteBatch.get(), rString.String.c_str(),
					Vector2(rString.X + shadowOffset * rString.Scale, rString.Y + shadowOffset * rString.Scale),
					Vector4(0.0f, 0.0f, 0.0f, rString.Color.w) * ScreenFadeCurrent,
					0.0f, Vector4::Zero, rString.Scale);
			}

			// Draw string.
			_gameFont->DrawString(
				_spriteBatch.get(), rString.String.c_str(),
				Vector2(rString.X, rString.Y),
				(rString.Color * rString.Color.w) * ScreenFadeCurrent,
				0.0f, Vector4::Zero, rString.Scale);
		}

		_spriteBatch->End();
	}
}
