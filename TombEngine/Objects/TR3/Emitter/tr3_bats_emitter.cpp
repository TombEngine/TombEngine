#include "framework.h"
#include "Objects/TR3/Emitter/tr3_bats_emitter.h"

#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Renderer/Structures/RendererSpriteToDraw.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

namespace TEN::Entities::TR3
{
	using namespace TEN::Renderer::Structures;

	constexpr auto TR3_BAT_SPRITE_ID = 3;
	constexpr auto TR3_BAT_LIFE_MIN	  = 144;
	constexpr auto TR3_BAT_SPEED_MIN	  = 64;
	constexpr auto TR3_BAT_SPEED_MAX	  = 300;
	constexpr auto TR3_BAT_SPEED_ACCEL = 12;
	constexpr auto TR3_BAT_SPEED_DIVISOR = 6;
	constexpr auto TR3_BAT_BODY_WING_AMPLITUDE = 16.0f;
	constexpr auto TR3_BAT_OUTER_WING_AMPLITUDE = 256.0f;

	constexpr std::array<Vector3, 5> TR3_BAT_MESH =
	{
		Vector3(-192.0f, 0.0f, -48.0f),
		Vector3(-192.0f, 0.0f, 48.0f),
		Vector3(96.0f, 0.0f, 0.0f),
		Vector3(-144.0f, 0.0f, -192.0f),
		Vector3(-144.0f, 0.0f, 192.0f)
	};

	constexpr std::array<std::array<int, 3>, 3> TR3_BAT_TRIANGLES =
	{ {
		{ 0, 1, 2 },
		{ 3, 0, 2 },
		{ 1, 4, 2 }
	} };

	constexpr std::array<std::array<int, 3>, 3> TR3_BAT_TRIANGLE_UV =
	{ {
		{ 0, 2, 3 },
		{ 1, 0, 2 },
		{ 0, 1, 2 }
	} };

	Tr3BatData Tr3Bats[NUM_TR3_BATS];

	static void ResetTr3BatInterpolationData(Tr3BatData& bat)
	{
		bat.PrevPosition = bat.Pose.Position;
		bat.PrevWingYoff = bat.WingYoff;
	}

	void ClearTr3Bats()
	{
		for (auto& bat : Tr3Bats)
		{
			bat = {};
			bat.RoomNumber = NO_VALUE;
			ResetTr3BatInterpolationData(bat);
		}
	}

	void InitializeTr3BatsEmitter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.TriggerFlags = 0;

		if (Objects[ID_BATS_EMITTER_TR3].loaded)
			ClearTr3Bats();
	}

	void TriggerTr3Bats(ItemInfo* item)
	{
		short angle = ((item->Pose.Orientation.y >> 4) - 1024) & 0xFFF;

		for (int i = 0; i < NUM_TR3_BATS; i++)
		{
			auto& bat = Tr3Bats[i];

			bat.RoomNumber = item->RoomNumber;
			bat.Pose.Position.x = (GetRandomControl() & 0x1FF) + item->Pose.Position.x - 256;
			bat.Pose.Position.y = item->Pose.Position.y - (GetRandomControl() & 0xFF) + 256;
			bat.Pose.Position.z = (GetRandomControl() & 0x1FF) + item->Pose.Position.z - 256;
			bat.Pose.Orientation.x = 0;
			bat.Pose.Orientation.y = (((GetRandomControl() & 0x7F) + angle - 64) & 0xFFF) << 4;
			bat.Pose.Orientation.z = 0;
			bat.Velocity = (GetRandomControl() & 0x1F) + TR3_BAT_SPEED_MIN;
			bat.Counter = (GetRandomControl() & 7) + TR3_BAT_LIFE_MIN;
			bat.WingYoff = GetRandomControl() & 0x3F;
			bat.On = true;
			ResetTr3BatInterpolationData(bat);
		}
	}

	void Tr3BatsEmitterControl(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		TriggerTr3Bats(&item);
		KillItem(itemNumber);
	}

	void UpdateTr3Bats()
	{
		if (!Objects[ID_BATS_EMITTER_TR3].loaded)
			return;

		for (int i = 0; i < NUM_TR3_BATS; i++)
		{
			auto& bat = Tr3Bats[i];

			if (!bat.On)
				continue;

			bat.StoreInterpolationData();

			if (!(i & 3) && !(GetRandomControl() & 7))
				SoundEffect(SFX_TR4_BATS, &bat.Pose);

			int velocity = bat.Velocity / TR3_BAT_SPEED_DIVISOR;

			bat.Pose.Position.x -= velocity * phd_cos(bat.Pose.Orientation.y);
			bat.Pose.Position.y -= GetRandomControl() & 3;
			bat.Pose.Position.z += velocity * phd_sin(bat.Pose.Orientation.y);
			bat.WingYoff = (bat.WingYoff + 11) & 0x3F;

			if (bat.Counter < 128)
			{
				bat.Pose.Position.y += -4 - (i >> 1);

				if (!(GetRandomControl() & 3))
				{
					bat.Pose.Orientation.y += ((GetRandomControl() & 0xFF) - 128) << 4;
					bat.Velocity += GetRandomControl() & 3;
				}
			}

			bat.Velocity += TR3_BAT_SPEED_ACCEL;

			if (bat.Velocity > TR3_BAT_SPEED_MAX)
				bat.Velocity = TR3_BAT_SPEED_MAX;

			if (bat.Counter && (Wibble & 4))
			{
				bat.Counter--;

				if (!bat.Counter)
					bat.On = false;
			}

			GetFloor(bat.Pose.Position.x, bat.Pose.Position.y, bat.Pose.Position.z, &bat.RoomNumber);
		}
	}

	int GetTr3BatSpriteId()
	{
		return TR3_BAT_SPRITE_ID;
	}

	static void MatchOriginalDoubleSidedWinding(RendererSpriteToDraw& batSprite, const Matrix& viewProjection)
	{
		auto projected0 = Vector4::Transform(Vector4(batSprite.vtx1.x, batSprite.vtx1.y, batSprite.vtx1.z, 1.0f), viewProjection);
		auto projected1 = Vector4::Transform(Vector4(batSprite.vtx2.x, batSprite.vtx2.y, batSprite.vtx2.z, 1.0f), viewProjection);
		auto projected2 = Vector4::Transform(Vector4(batSprite.vtx3.x, batSprite.vtx3.y, batSprite.vtx3.z, 1.0f), viewProjection);

		if (projected0.w == 0.0f || projected1.w == 0.0f || projected2.w == 0.0f)
			return;

		projected0.x /= projected0.w;
		projected0.y /= projected0.w;
		projected1.x /= projected1.w;
		projected1.y /= projected1.w;
		projected2.x /= projected2.w;
		projected2.y /= projected2.w;

		float winding =
			(projected2.x - projected1.x) * (projected0.y - projected1.y) -
			(projected0.x - projected1.x) * (projected2.y - projected1.y);

		if (winding >= 0.0f)
			return;

		std::swap(batSprite.vtx2, batSprite.vtx3);
		std::swap(batSprite.CustomUV[1], batSprite.CustomUV[2]);
		batSprite.vtx4 = batSprite.vtx3;
		batSprite.CustomUV[3] = batSprite.CustomUV[2];
	}

	void AddTr3BatSpritesToDraw(
		std::vector<RendererSpriteToDraw>& spritesToDraw,
		RendererSprite* sprite,
		const Tr3BatData& bat,
		const Vector4& ambient,
		const Matrix& viewProjection,
		float interpolationFactor)
	{
		auto pos = Vector3::Lerp(
			bat.PrevPosition.ToVector3(),
			bat.Pose.Position.ToVector3(),
			interpolationFactor);

		float prevWing = (float)bat.PrevWingYoff;
		float currWing = (float)bat.WingYoff;

		if (currWing < prevWing)
			currWing += 64.0f;

		float wing = fmod(Vector2::Lerp(Vector2(prevWing, 0.0f), Vector2(currWing, 0.0f), interpolationFactor).x, 64.0f);
		float bodyWing = fmod(wing - 32.0f + 64.0f, 64.0f);
		float bodyYOffset = sin((bodyWing / 64.0f) * PI_MUL_2) * TR3_BAT_BODY_WING_AMPLITUDE - 512.0f;
		float outerWingYOffset = sin((wing / 64.0f) * PI_MUL_2) * TR3_BAT_OUTER_WING_AMPLITUDE - 512.0f;
		auto transform = bat.Pose.Orientation.ToRotationMatrix() * Matrix::CreateTranslation(pos);
		auto batColor = Vector4(ambient.x, ambient.y, ambient.z, 1.0f);
		std::array<Vector3, TR3_BAT_MESH.size()> vertices = {};

		for (int i = 0; i < TR3_BAT_MESH.size(); i++)
		{
			auto vertex = TR3_BAT_MESH[i];

			if (i < 3)
				vertex.y += bodyYOffset;
			else
				vertex.y += outerWingYOffset;

			vertices[i] = Vector3::Transform(vertex, transform);
		}

		for (int i = 0; i < TR3_BAT_TRIANGLES.size(); i++)
		{
			const auto& tri = TR3_BAT_TRIANGLES[i];
			const auto& uv = TR3_BAT_TRIANGLE_UV[i];
			auto batSprite = RendererSpriteToDraw{};

			batSprite.Type = SpriteType::ThreeD;
			batSprite.Sprite = sprite;
			batSprite.vtx1 = vertices[tri[0]];
			batSprite.vtx2 = vertices[tri[1]];
			batSprite.vtx3 = vertices[tri[2]];
			batSprite.vtx4 = vertices[tri[2]];
			batSprite.c1 = batColor;
			batSprite.c2 = batColor;
			batSprite.c3 = batColor;
			batSprite.c4 = batColor;
			batSprite.CustomUV[0] = sprite->UV[uv[0]];
			batSprite.CustomUV[1] = sprite->UV[uv[1]];
			batSprite.CustomUV[2] = sprite->UV[uv[2]];
			batSprite.CustomUV[3] = sprite->UV[uv[2]];
			batSprite.BlendMode = BlendMode::AlphaBlend;
			batSprite.pos = (batSprite.vtx1 + batSprite.vtx2 + batSprite.vtx3) / 3.0f;
			batSprite.SoftParticle = true;
			batSprite.UseCustomUV = true;

			MatchOriginalDoubleSidedWinding(batSprite, viewProjection);
			spritesToDraw.push_back(batSprite);
		}
	}
}
