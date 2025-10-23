#pragma once

struct ItemInfo;

namespace TEN::Hud
{
	class CrosshairData
	{
	private:
		struct SegmentData
		{
			Vector2 PosOffset	 = Vector2::Zero;
			short	OrientOffset = 0;
		};

	public:
		// Constants

		static constexpr auto COLOR_RED		= Color(1.0f, 0.2f, 0.2f);
		static constexpr auto COLOR_GRAY	= Color(0.7f, 0.7f, 0.7f, 0.7f);
		static constexpr auto SEGMENT_COUNT = 4;

		// Fields

		bool IsActive  = false;
		bool IsPrimary = false;

		std::optional<Vector2> Position	= std::nullopt;
		short	Orientation = 0;
		float	Scale		= 0.0f;
		Vector4 Color		= Vector4::Zero;
		Vector4 ColorTarget = Vector4::Zero;

		float RadiusScale = 0.0f;
		float PulseScale  = 0.0f;

		std::array<SegmentData, SEGMENT_COUNT> Segments = {};

		Vector2 PrevPosition	= Vector2::Zero;
		short	PrevOrientation = 0;
		float	PrevScale		= 0.0f;
		Vector4 PrevColor		= Vector4::Zero;
		std::array<SegmentData, SEGMENT_COUNT> PrevSegments = {};

		// Getters

		float	GetScale(float cameraDist) const;
		float	GetRadius() const;
		Vector2 GetPositionOffset(short orientOffset) const;

		// Setters

		void SetPrimary();
		void SetPeripheral();
		
		// Utilities

		void Update(const Vector3& targetPos, bool isActive, bool doPulse);
		void Draw() const;

		void StoreInterpolationData()
		{
			PrevPosition = *Position;
			PrevOrientation = Orientation;
			PrevScale = Scale;
			PrevColor = Color;
			PrevSegments = Segments;
		}
	};

	class TargetHighlighterController
	{
	private:
		// Members

		std::unordered_map<int, CrosshairData> _crosshairs = {}; // Key = item number.

	public:
		// Utilities

		void Update(const ItemInfo& playerItem);
		void Draw() const;
		void Clear();

	private:
		// Update helpers

		void Update(const std::vector<int>& itemNumbers);

		// Object helpers

		CrosshairData& GetNewCrosshair(int itemNumber);
		void		   AddCrosshair(int itemNumber, const Vector3& targetPos);
		void		   ClearInactiveCrosshairs();

		void DrawDebug() const;
	};
}
