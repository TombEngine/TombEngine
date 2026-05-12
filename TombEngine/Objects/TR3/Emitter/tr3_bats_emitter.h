#pragma once
#include <vector>

#include "Game/items.h"

namespace TEN::Renderer::Structures
{
	struct RendererSprite;
	struct RendererSpriteToDraw;
}

namespace TEN::Entities::TR3
{
	constexpr auto NUM_TR3_BATS = 64;
	constexpr auto TR3_BATS_PER_EMITTER = 32;

	struct Tr3BatData
	{
		bool On = false;
		Pose Pose = {};
		Vector3i PrevPosition = Vector3i::Zero;
		short RoomNumber = NO_VALUE;

		short Velocity = 0;
		short Counter = 0;
		short WingYoff = 0;
		short PrevWingYoff = 0;

		void StoreInterpolationData()
		{
			PrevPosition = Pose.Position;
			PrevWingYoff = WingYoff;
		}
	};

	extern Tr3BatData Tr3Bats[NUM_TR3_BATS];

	void ClearTr3Bats();
	void InitializeTr3BatsEmitter(short itemNumber);
	void Tr3BatsEmitterControl(short itemNumber);
	void TriggerTr3Bats(ItemInfo* item);
	void UpdateTr3Bats();

	int GetTr3BatSpriteId();
	void AddTr3BatSpritesToDraw(
		std::vector<TEN::Renderer::Structures::RendererSpriteToDraw>& spritesToDraw,
		TEN::Renderer::Structures::RendererSprite* sprite,
		const Tr3BatData& bat,
		const Vector4& ambient,
		const Matrix& viewProjection,
		float interpolationFactor);
}
