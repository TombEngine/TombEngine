#pragma once
#include "Math/Math.h"
#include "Objects/game_object_ids.h"

using namespace TEN::Math;

namespace TEN::Hud
{
	struct DisplayPickup
	{
		static constexpr auto LIFE_MAX = 3.0f;

		GAME_OBJECT_ID ObjectID = GAME_OBJECT_ID::ID_NO_OBJECT;
		unsigned int   Count	= 0;

		Vector2		Position	= Vector2::Zero;
		Vector2		Origin		= Vector2::Zero;
		Vector2		Target		= Vector2::Zero;
		EulerAngles Orientation = EulerAngles::Identity;

		float Life		   = 0.0f;
		float Scale		   = 0.0f;
		float Opacity	   = 0.0f;
		float HideVelocity = 0.0f;
		float StringScale  = 0.0f;
		float StringScalar = 0.0f;

		Vector2		PrevPosition	= Vector2::Zero;
		EulerAngles PrevOrientation = EulerAngles::Identity;
		float		PrevScale		= 0.0f;
		float		PrevOpacity		= 0.0f;

		bool IsOffscreen() const;
		void Update(bool isHead);

		void StoreInterpolationData()
		{
			PrevPosition = Position;
			PrevOrientation = Orientation;
			PrevScale = Scale;
			PrevOpacity = Opacity;
		}
	};

	class PickupSummaryController
	{
	private:
		// Constants

		static constexpr auto DISPLAY_PICKUP_COUNT_MAX		   = 64;
		static constexpr auto DISPLAY_PICKUP_COUNT_ARG_DEFAULT = 1;

		// Fields

		std::vector<DisplayPickup> _displayPickups = {};

	public:
		// Utilities

		void AddDisplayPickup(GAME_OBJECT_ID objectID, const Vector2& origin, unsigned int count = DISPLAY_PICKUP_COUNT_ARG_DEFAULT);
		void AddDisplayPickup(GAME_OBJECT_ID objectID, const Vector3& pos, unsigned int count = DISPLAY_PICKUP_COUNT_ARG_DEFAULT);
		void AddDisplayPickup(const ItemInfo& item);

		void Update();
		void Draw() const;
		void Clear();

	private:
		// Helpers

		std::vector<Vector2> GetStackPositions() const;
		DisplayPickup&		 GetNewDisplayPickup();
		void				 ClearInactiveDisplayPickups();

		void DrawDebug() const;
	};
}
