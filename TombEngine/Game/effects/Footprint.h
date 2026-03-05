#pragma once

struct ItemInfo;

namespace TEN::Effects::Footprint
{
	struct Footprint
	{
		static constexpr auto COUNT_MAX	   = 64;
		static constexpr auto VERTEX_COUNT = 4;

		unsigned int SpriteIndex = 0;
		bool		 IsRight	 = false;

		float Life			  = 0.0f;
		float LifeStartFading = 0.0f;
		float Opacity		  = 0.0f;
		float OpacityStart	  = 0.0f;

		std::array<Vector3, VERTEX_COUNT> VertexPoints = {};
	};

	extern std::vector<Footprint> Footprints;

	void SpawnFootprint(bool isRight, const std::array<Vector3, Footprint::VERTEX_COUNT>& vertexPoints);
	void SpawnFootprint(const ItemInfo& item, bool isRight);

	void UpdateFootprints();
	void ClearFootprints();
}
