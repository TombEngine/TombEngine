#pragma once

namespace TEN::Effects::Blood
{
	struct UnderwaterBlood
	{
		unsigned int SpriteIndex = 0;

		Vector3 Position   = Vector3::Zero;
		int		RoomNumber = 0;
		Vector4 Color	   = Vector4::Zero;

		float Life	  = 0.0f;
		float Init	  = 0.0f;
		float Size	  = 0.0f;
		float Opacity = 0.0f;

		Vector3 PrevPosition = Vector3::Zero;
		Vector4 PrevColor	 = Vector4::Zero;
		float	PrevLife	 = 0.0f;
		float	PrevSize	 = 0.0f;
		float	PrevOpacity	 = 0.0f;

		void StoreInterpolationData()
		{
			PrevPosition = Position;
			PrevColor = Color;
			PrevLife = Life;
			PrevSize = Size;
			PrevOpacity = Opacity;
		}
	};

	extern std::vector<UnderwaterBlood> UnderwaterBloodParticles;

	void SpawnUnderwaterBlood(const Vector3& pos, int roomNumber, float size);
	void SpawnUnderwaterBloodCloud(const Vector3& pos, int roomNumber, float sizeMax, unsigned int count);

	void UpdateUnderwaterBloodParticles();
	void ClearUnderwaterBloodParticles();
}
