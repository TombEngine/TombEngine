#include "framework.h"
#include "Game/effects/tomb4fx.h"

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/debris.h"
#include "Game/effects/Drip.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/smoke.h"
#include "Game/effects/Splash.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Drip;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Ripple;
using namespace TEN::Effects::Smoke;
using namespace TEN::Effects::Splash;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

// NOTE: This fixes body part exploding instantly if entity is on ground.
constexpr auto BODY_PART_SPAWN_VERTICAL_OFFSET = CLICK(1);

int NextGunshell = 0;

int NextFireSpark = 1;
int NextSmokeSpark = 0;
int NextBlood = 0;
int NextGunShell = 0;

FIRE_SPARKS FireSparks[MAX_SPARKS_FIRE];
SMOKE_SPARKS SmokeSparks[MAX_SPARKS_SMOKE];
GUNSHELL_STRUCT Gunshells[MAX_GUNSHELL];
BLOOD_STRUCT Blood[MAX_SPARKS_BLOOD];
SHOCKWAVE_STRUCT ShockWaves[MAX_SHOCKWAVE];
std::vector<FIRE_LIST> Fires;

int GetFreeFireSpark()
{
	int sparkNum = NextFireSpark;
	int minIndex = 0;
	int minLife = 4095;
	int i = 0;

	FIRE_SPARKS* spark = &FireSparks[NextFireSpark];
	while (spark->on)
	{
		if (spark->life < minLife)
		{
			minIndex = sparkNum;
			minLife = spark->life;
		}
		if (sparkNum == MAX_SPARKS_FIRE - 1)
		{
			spark = &FireSparks[1];
			sparkNum = 1;
		}
		else
		{
			sparkNum++;
			spark++;
		}

		if (++i >= MAX_SPARKS_FIRE)
		{
			NextFireSpark = minIndex + 1;
			if (NextFireSpark >= MAX_SPARKS_FIRE)
				NextFireSpark = 1;
			return minIndex;
		}
	}

	NextFireSpark = sparkNum + 1;
	if (sparkNum + 1 >= MAX_SPARKS_FIRE)
		NextFireSpark = 1;

	return sparkNum;
}

void TriggerGlobalStaticFlame()
{
	FIRE_SPARKS* spark = &FireSparks[0];

	spark->on = true;
	spark->dR = spark->sR = (GetRandomControl() & 0x3F) - 64;
	spark->dB = 64;
	spark->sB = 64;
	spark->dG = (GetRandomControl() & 0x3F) + 96;
	spark->sG = (GetRandomControl() & 0x3F) + 96;
	spark->colFadeSpeed = 1;
	spark->fadeToBlack = 0;
	spark->life = 8;
	spark->sLife = 8;
	spark->position = Vector3i(
		(GetRandomControl() & 7) - 4,
		0,
		(GetRandomControl() & 7) - 4
	);
	spark->maxYvel = 0;
	spark->gravity = 0;
	spark->friction = 0;
	spark->velocity = Vector3i::Zero;
	spark->flags = SP_NONE;
	spark->dSize = spark->sSize = spark->size = (GetRandomControl() & 0x1F) + -128;
}

void TriggerGlobalFireSmoke()
{
	FIRE_SPARKS* spark = &FireSparks[GetFreeFireSpark()];

	spark->on = 1;
	spark->sR = 0;
	spark->sG = 0;
	spark->sB = 0;
	spark->dR = 32;
	spark->dG = 32;
	spark->dB = 32;
	spark->fadeToBlack = 16;
	spark->colFadeSpeed = (GetRandomControl() & 7) + 32;
	spark->life = spark->sLife = (GetRandomControl() & 0xF) + 57;
	spark->position = Vector3i(
		(GetRandomControl() & 0xF) - 8,
		-256 - (GetRandomControl() & 0x7F),
		(GetRandomControl() & 0xF) - 8
	);
	spark->velocity = Vector3i(
		(GetRandomControl() & 0xFF) - 128,
		-16 - (GetRandomControl() & 0xF),
		(GetRandomControl() & 0xFF) - 128
	);
	spark->friction = 4;

	if (GetRandomControl() & 1)
	{
		spark->flags = 16;
		spark->rotAng = GetRandomControl() & 0xFFF;
		if (GetRandomControl() & 1)
			spark->rotAdd = -16 - (GetRandomControl() & 0xF);
		else
			spark->rotAdd = (GetRandomControl() & 0xF) + 16;
	}
	else
	{
		spark->flags = SP_NONE;
	}

	spark->gravity = -16 - (GetRandomControl() & 0xF);
	spark->maxYvel = -8 - (GetRandomControl() & 7);
	spark->dSize = spark->sSize = spark->size = (GetRandomControl() & 0x7F) + 128;
}

void TriggerGlobalFireFlame()
{
	FIRE_SPARKS* spark = &FireSparks[GetFreeFireSpark()];

	spark->on = true;
	spark->sR = 255;
	spark->sB = 48;
	spark->sG = (GetRandomControl() & 0x1F) + 48;
	spark->dR = (GetRandomControl() & 0x3F) - 64;
	spark->dB = 32;
	spark->dG = (GetRandomControl() & 0x3F) + -128;
	spark->fadeToBlack = 8;
	spark->colFadeSpeed = (GetRandomControl() & 3) + 8;
	spark->life = spark->sLife = (GetRandomControl() & 7) + 32;
	spark->position = Vector3i(
		4 * (GetRandomControl() & 0x1F) - 64,
		0,
		4 * (GetRandomControl() & 0x1F) - 64
	);
	spark->velocity = Vector3i(
		2 * (GetRandomControl() & 0xFF) - 256,
		-16 - (GetRandomControl() & 0xF),
		2 * (GetRandomControl() & 0xFF) - 256
	);
	spark->friction = 5;
	spark->gravity = -32 - (GetRandomControl() & 0x1F);
	spark->maxYvel = -16 - (GetRandomControl() & 7);

	if (GetRandomControl() & 1)
	{
		spark->flags = 16;
		spark->rotAng = GetRandomControl() & 0xFFF;
		if (GetRandomControl() & 1)
			spark->rotAdd = -16 - (GetRandomControl() & 0xF);
		else
			spark->rotAdd = (GetRandomControl() & 0xF) + 16;
	}
	else
	{
		spark->flags = SP_NONE;
	}

	spark->sSize = spark->size = (GetRandomControl() & 0x1F) + 128;
	spark->dSize = spark->size;
}

void TriggerPilotFlame(int itemNumber, int nodeIndex)
{
	auto* item = &g_Level.Items[itemNumber];

	int dx = Camera.pos.x - item->Pose.Position.x;
	int dz = Camera.pos.z - item->Pose.Position.z;
	if (dx < -BLOCK(16) || dx > BLOCK(16) ||
		dz < -BLOCK(16) || dz > BLOCK(16))
	{
		return;
	}

	auto* spark = GetFreeParticle();

	spark->on = 1;
	spark->sR = 48 + (GetRandomControl() & 31);
	spark->sG = spark->sR;
	spark->sB = 192 + (GetRandomControl() & 63);

	spark->dR = 192 + (GetRandomControl() & 63);
	spark->dG = 128 + (GetRandomControl() & 63);
	spark->dB = 32;

	spark->colFadeSpeed = 12 + (GetRandomControl() & 3);
	spark->fadeToBlack = 4;
	spark->sLife = spark->life = (GetRandomControl() & 3) + 20;
	spark->blendMode = BlendMode::Additive;
	spark->extras = 0;
	spark->dynamic = -1;
	spark->fxObj = itemNumber;

	spark->x = (GetRandomControl() & 31) - 16;
	spark->y = (GetRandomControl() & 31) - 16;
	spark->z = (GetRandomControl() & 31) - 16;

	spark->xVel =  (GetRandomControl() & 31) - 16;
	spark->yVel = -(GetRandomControl() & 3);
	spark->zVel =  (GetRandomControl() & 31) - 16;

	spark->flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_ITEM | SP_NODEATTACH;
	spark->nodeNumber = nodeIndex;
	spark->friction = 4;
	spark->gravity = -(GetRandomControl() & 3) - 2;
	spark->maxYvel = -(GetRandomControl() & 3) - 4;
	//spark->def = Objects[EXPLOSION1].mesh_index;
	spark->scalar = 0;
	int size = (GetRandomControl() & 7) + 32;
	spark->size = size / 2;
	spark->dSize = size;
}

static Particle& SetupPoisonParticle(const Color& colorStart, const Color& colorEnd)
{
	auto& part = *GetFreeParticle();
	part.sR = std::clamp<unsigned char>(colorStart.x * UCHAR_MAX, 0, UCHAR_MAX);
	part.sG = std::clamp<unsigned char>(colorStart.y * UCHAR_MAX, 0, UCHAR_MAX);
	part.sB = std::clamp<unsigned char>(colorStart.z * UCHAR_MAX, 0, UCHAR_MAX);
	part.dR = std::clamp<unsigned char>(colorEnd.x * UCHAR_MAX, 0, UCHAR_MAX);
	part.dG = std::clamp<unsigned char>(colorEnd.y * UCHAR_MAX, 0, UCHAR_MAX);
	part.dB = std::clamp<unsigned char>(colorEnd.z * UCHAR_MAX, 0, UCHAR_MAX);
	part.colFadeSpeed = 14;
	part.fadeToBlack = 8;
	part.blendMode = BlendMode::Screen;

	return part;
}

static Particle* SetupFireSpark()
{
	auto* spark = GetFreeParticle();

	spark->sR = (GetRandomControl() & 0x1F) + 48;
	spark->sG = 38;
	spark->sB = 255;
	spark->dR = (GetRandomControl() & 0x3F) - 64;
	spark->dG = (GetRandomControl() & 0x3F) + -128;
	spark->dB = 32;
	spark->colFadeSpeed = 12;
	spark->fadeToBlack = 8;
	spark->blendMode = BlendMode::Additive;

	return spark;
}

static void AttachAndCreateSpark(Particle* spark, const ItemInfo* item, int meshID, Vector3i offset, Vector3i vel, int spriteID = 0)
{
	auto pos1 = GetJointPosition(*item, meshID, Vector3i(-4, -30, -4) + offset);

	spark->x = (GetRandomControl() & 0x1F) + pos1.x - 16;
	spark->y = (GetRandomControl() & 0x1F) + pos1.y - 16;
	spark->z = (GetRandomControl() & 0x1F) + pos1.z - 16;

	auto pos2 = GetJointPosition(*item, meshID, Vector3i(-4, -30, -4) + offset + vel);

	int v = (GetRandomControl() & 0x3F) + 192;

	spark->life = spark->sLife = v / 6;
	spark->SpriteSeqID = ID_DEFAULT_SPRITES;
	spark->SpriteID = spriteID;

	spark->xVel = v * (pos2.x - pos1.x) / 10;
	spark->yVel = v * (pos2.y - pos1.y) / 10;
	spark->zVel = v * (pos2.z - pos1.z) / 10;

	spark->friction = 85;
	spark->gravity = -16 - (GetRandomControl() & 0x1F);
	spark->maxYvel = 0;
	spark->flags = SP_FIRE | SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;

	spark->scalar = 3;
	spark->dSize = (v * ((GetRandomControl() & 7) + 60)) / 256;
	spark->sSize = spark->dSize / 4;
	spark->size = spark->dSize / 2;

	spark->on = 1;
}

void ThrowFire(int itemNumber, int meshID, const Vector3i& offset, const Vector3i& vel, int spriteID)
{
	auto& item = g_Level.Items[itemNumber];

	for (int i = 0; i < 3; i++)
	{
		auto& part = *SetupFireSpark();
		AttachAndCreateSpark(&part, &item, meshID, offset, vel, spriteID);

		part.flags = SP_FIRE | SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
	}
}

void ThrowFire(int itemNumber, const CreatureBiteInfo& bite, const Vector3i& vel, int spriteID)
{
	ThrowFire(itemNumber, bite.BoneID, bite.Position, vel, spriteID);
}

void ThrowPoison(const ItemInfo& item, int boneID, const Vector3& offset, const Vector3& vel, const Color& colorStart, const Color& colorEnd, int spriteID)
{
	constexpr auto COUNT = 2;

	for (int i = 0; i < COUNT; i++)
	{
		auto& part = SetupPoisonParticle(colorStart, colorEnd);
		AttachAndCreateSpark(&part, &item, boneID, offset, vel, spriteID);
		part.flags = SP_POISON | SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
		part.damage = 5;
	}
}

void ThrowPoison(const ItemInfo& item, const CreatureBiteInfo& bite, const Vector3& vel, const Color& colorStart, const Color& colorEnd, int spriteID)
{
	ThrowPoison(item, bite.BoneID, bite.Position, vel, colorStart, colorEnd, spriteID);
}

void UpdateFireProgress()
{
	TriggerGlobalStaticFlame();
	if (!(Wibble & 0xF))
	{
		TriggerGlobalFireFlame();
		if (!(Wibble & 0x1F))
			TriggerGlobalFireSmoke();
	}
}

void AddFire(int x, int y, int z, short roomNum, float size, short fade)
{
	FIRE_LIST newFire;
	
	newFire.fade = (fade == 0 ? 1 : (unsigned char)fade);
	newFire.position = Vector3i(x, y, z);
	newFire.roomNumber = roomNum;
	newFire.size = size;
	newFire.StoreInterpolationData();
	
	Fires.push_back(newFire);
}

void ClearFires()
{
	Fires.clear();
}

void UpdateFireSparks()
{
	UpdateFireProgress();

	for (int i = 0; i < MAX_SPARKS_FIRE; i++)
	{
		FIRE_SPARKS* spark = &FireSparks[i];

		if (spark->on)
		{
			spark->life--;

			if (!spark->life)
			{
				spark->on = false;
				continue;
			}

			spark->StoreInterpolationData();

			if (spark->sLife - spark->life < spark->colFadeSpeed)
			{
				int dl = ((spark->sLife - spark->life) << 16) / spark->colFadeSpeed;

				spark->color = Vector3i(
					spark->sR + (dl * (spark->dR - spark->sR) >> 16),
					spark->sG + (dl * (spark->dG - spark->sG) >> 16),
					spark->sB + (dl * (spark->dB - spark->sB) >> 16)
				);
			}
			else if (spark->life >= spark->fadeToBlack)
			{
				spark->color = Vector3i(spark->dR, spark->dG, spark->dB);
			}
			else
			{
				int dl = ((spark->life - spark->fadeToBlack) << 16) / spark->fadeToBlack + 0x10000;

				spark->color = Vector3i(
					dl * spark->dR >> 16,
					dl * spark->dG >> 16,
					dl * spark->dB >> 16
				);

				if (spark->color.x < 8 && spark->color.y < 8 && spark->color.z < 8)
				{
					spark->on = false;
					continue;
				}
			}

			if (spark->flags & SP_ROTATE)
				spark->rotAng = (spark->rotAng + spark->rotAdd) & 0xFFF;

			float alpha = fmin(1, fmax(0, 1 - (spark->life / (float)spark->sLife)));
			int sprite = (int)Lerp(Objects[ID_FIRE_SPRITES].meshIndex, Objects[ID_FIRE_SPRITES].meshIndex + (-Objects[ID_FIRE_SPRITES].nmeshes) - 1, alpha);
			spark->def = sprite;

			int dl = ((spark->sLife - spark->life) << 16) / spark->sLife;
			spark->velocity.y += spark->gravity;
			if (spark->maxYvel)
			{
				if ((spark->velocity.y < 0 && spark->velocity.y < (spark->maxYvel << 5)) ||
					(spark->velocity.y > 0 && spark->velocity.y > (spark->maxYvel << 5)))
					spark->velocity.y = spark->maxYvel << 5;
			}

			if (spark->friction)
			{
				spark->velocity.x -= spark->velocity.x >> spark->friction;
				spark->velocity.z -= spark->velocity.z >> spark->friction;
			}

			spark->position += spark->velocity / 48;

			spark->size = spark->sSize + ((dl * (spark->dSize - spark->sSize)) / 65536);
		}
	}
}

int GetFreeSmokeSpark() 
{
	SMOKE_SPARKS* spark = &SmokeSparks[NextSmokeSpark];
	int sparkNum = NextSmokeSpark;
	short minLife = 4095;
	short minIndex = 0;
	short count = 0;
	while (spark->on)
	{
		if (spark->life < minLife)
		{
			minIndex = sparkNum;
			minLife = spark->life;
		}

		if (sparkNum == MAX_SPARKS_SMOKE - 1)
		{
			spark = &SmokeSparks[0];
			sparkNum = 0;
		}
		else
		{
			sparkNum++;
			spark++;
		}

		if (++count >= MAX_SPARKS_SMOKE)
		{
			NextSmokeSpark = (minIndex + 1) % MAX_SPARKS_SMOKE;
			return minIndex;
		}
	}

	NextSmokeSpark = (sparkNum + 1) % MAX_SPARKS_SMOKE;

	return sparkNum;
}

void UpdateSmoke()
{
	for (int i = 0; i < MAX_SPARKS_SMOKE; i++)
	{
		SMOKE_SPARKS* spark = &SmokeSparks[i];

		if (spark->on)
		{
			spark->life -= 2;

			if (spark->life <= 0)
			{
				spark->on = false;
				continue;
			}

			spark->StoreInterpolationData();

			if (spark->sLife - spark->life >= spark->colFadeSpeed)
			{
				if (spark->life >= spark->fadeToBlack)
				{
					spark->shade = spark->dShade;
				}
				else
				{
					spark->shade = spark->dShade * (((spark->life - spark->fadeToBlack) << 16) / spark->fadeToBlack + 0x10000) >> 16;
					if (spark->shade < 8)
					{
						spark->on = false;
						continue;
					}
				}
			}
			else
			{
				spark->shade = spark->sShade + ((spark->dShade - spark->sShade) * (((spark->sLife - spark->life) << 16) / spark->colFadeSpeed) >> 16);
			}

			if (spark->shade >= 24)
			{
				if (spark->shade >= 80)
					spark->def = Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_FIRE0;
				else
					spark->def = Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_FIRE1;
			}
			else
			{
				spark->def = Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_FIRE2;
			}

			if (spark->flags & SP_ROTATE)
				spark->rotAng = (spark->rotAng + spark->rotAdd) & 0xFFF;

			int dl = ((spark->sLife - spark->life) << 16) / spark->sLife;

			spark->velocity.y += spark->gravity;
			
			if (spark->maxYvel != 0)
			{
				if (spark->velocity.y < 0)
				{
					if (spark->velocity.y < spark->maxYvel)
					{
						spark->velocity.y = spark->maxYvel;
					}
				}
				else 
				{
					if (spark->velocity.y > spark->maxYvel)
					{
						spark->velocity.y = spark->maxYvel;
					}
				}
			}
			
			if (spark->friction & 0xF)
			{
				spark->velocity.x -= spark->velocity.x >> (spark->friction & 0xF);
				spark->velocity.z -= spark->velocity.z >> (spark->friction & 0xF);
			}

			if (spark->friction & 0xF0)
			{
				spark->velocity.y -= spark->velocity.y >> (spark->friction >> 4);
			}

			spark->position.x += spark->velocity.x >> 5;
			spark->position.y += spark->velocity.y >> 5;
			spark->position.z += spark->velocity.z >> 5;

			if (spark->flags & SP_WIND)
			{
				spark->position.x += Weather.Wind().x;
				spark->position.z += Weather.Wind().z;
			}

			spark->size = spark->sSize + (dl * (spark->dSize - spark->sSize) >> 16);
		}
	}
}

void TriggerGunSmoke(int x, int y, int z, short xv, short yv, short zv, byte initial, LaraWeaponType weaponType, byte count)
{
	TriggerGunSmokeParticles(x, y, z, xv, yv, zv, initial, weaponType, count);
}

void TriggerShatterSmoke(int x, int y, int z)
{
	SMOKE_SPARKS* spark = &SmokeSparks[GetFreeSmokeSpark()];
	
	spark->on = true;
	spark->sShade = 0;
	spark->colFadeSpeed = 4;
	spark->dShade = (GetRandomControl() & 0x1F) + 64;
	spark->fadeToBlack = 24 - (GetRandomControl() & 7);
	spark->blendMode = BlendMode::Additive;
	spark->life = spark->sLife = (GetRandomControl() & 7) + 48;
	spark->position.x = (GetRandomControl() & 0x1F) + x - 16;
	spark->position.y = (GetRandomControl() & 0x1F) + y - 16;
	spark->position.z = (GetRandomControl() & 0x1F) + z - 16;
	spark->velocity.x = 2 * (GetRandomControl() & 0x1FF) - 512;
	spark->velocity.y = 2 * (GetRandomControl() & 0x1FF) - 512;
	spark->velocity.z = 2 * (GetRandomControl() & 0x1FF) - 512;
	spark->friction = 7;
	
	if (GetRandomControl() & 1)
	{
		spark->flags = SP_ROTATE;
		spark->rotAng = GetRandomControl() & 0xFFF;
		if (GetRandomControl() & 1)
			spark->rotAdd = -64 - (GetRandomControl() & 0x3F);
		else
			spark->rotAdd = (GetRandomControl() & 0x3F) + 64;
	}
	else if (g_Level.Rooms[LaraItem->RoomNumber].flags & ENV_FLAG_WIND)
	{
		spark->flags = SP_WIND;
	}
	else
	{
		spark->flags = SP_NONE;
	}

	spark->gravity = -4 - (GetRandomControl() & 3);
	spark->maxYvel = -4 - (GetRandomControl() & 3);
	spark->dSize = (GetRandomControl() & 0x3F) + 64;
	spark->sSize = spark->dSize >> 3;
	spark->size = spark->dSize >> 3;
}

int GetFreeBlood()
{
	BLOOD_STRUCT* blood = &Blood[NextBlood];
	int bloodNum = NextBlood;
	int minLife = 4095;
	int minIndex = 0;
	int count = 0;

	while (blood->on)
	{
		if (blood->life < minLife)
		{
			minIndex = bloodNum;
			minLife = blood->life;
		}

		if (bloodNum == MAX_SPARKS_BLOOD - 1)
		{
			blood = &Blood[0];
			bloodNum = 0;
		}
		else
		{
			blood++;
			bloodNum++;
		}

		if (++count >= MAX_SPARKS_BLOOD)
		{
			NextBlood = (minIndex + 1) & 31;
			return minIndex;
		}
	}

	NextBlood = (bloodNum + 1) & 31;
	return bloodNum;
}

void TriggerBlood(int x, int y, int z, int unk, int num)
{
	for (int i = 0; i < num; i++)
	{
		BLOOD_STRUCT* blood = &Blood[GetFreeBlood()];
		blood->on = 1;
		blood->sShade = 0;
		blood->colFadeSpeed = 4;
		blood->fadeToBlack = 8;
		blood->dShade = (GetRandomControl() & 0x3F) + 48;
		blood->life = blood->sLife = (GetRandomControl() & 7) + 24;
		blood->x = (GetRandomControl() & 0x1F) + x - 16;
		blood->y = (GetRandomControl() & 0x1F) + y - 16;
		blood->z = (GetRandomControl() & 0x1F) + z - 16;
		int a = (unk == -1 ? GetRandomControl() : (GetRandomControl() & 0x1F) + unk - 16) & 0xFFF;
		int b = GetRandomControl() & 0xF;
		blood->zVel = b * phd_cos(a << 4) * 32;
		blood->xVel = -b * phd_sin(a << 4) * 32;
		blood->friction = 4;
		blood->yVel = -((GetRandomControl() & 0xFF) + 128);
		blood->rotAng = GetRandomControl() & 0xFFF;
		blood->rotAdd = (GetRandomControl() & 0x3F) + 64;
		if (GetRandomControl() & 1)
			blood->rotAdd = -blood->rotAdd;
		blood->gravity = (GetRandomControl() & 0x1F) + 31;
		int size = (GetRandomControl() & 7) + 8;
		blood->sSize = blood->size = size;
		blood->dSize = size >> 2;

		blood->PrevPosition.x = blood->x;
		blood->PrevPosition.y = blood->y;
		blood->PrevPosition.z = blood->z;
		blood->PrevRotAng = blood->rotAng;
		blood->PrevSize = blood->size;
		blood->PrevShade = blood->shade;
	}
}

void UpdateBlood()
{
	for (int i = 0; i < MAX_SPARKS_BLOOD; i++)
	{
		BLOOD_STRUCT* blood = &Blood[i];

		if (blood->on)
		{
			blood->life--;

			if (blood->life <= 0)
			{
				blood->on = false;
				continue;
			}

			blood->StoreInterpolationData();

			if (blood->sLife - blood->life >= blood->colFadeSpeed)
			{
				if (blood->life >= blood->fadeToBlack)
				{
					blood->shade = blood->dShade;
				}
				else
				{
					blood->shade = blood->dShade * (((blood->life - blood->fadeToBlack) << 16) / blood->fadeToBlack + 0x10000) >> 16;
					if (blood->shade < 8)
					{
						blood->on = false;
						continue;
					}
				}
			}
			else
			{
				blood->shade = blood->sShade + ((blood->dShade - blood->sShade) * (((blood->sLife - blood->life) << 16) / blood->colFadeSpeed) >> 16);
			}
			
			blood->rotAng = (blood->rotAng + blood->rotAdd) & 0xFFF;
			blood->yVel += blood->gravity;
						
			if (blood->friction & 0xF)
			{
				blood->xVel -= blood->xVel >> (blood->friction & 0xF);
				blood->zVel -= blood->zVel >> (blood->friction & 0xF);
			}

			int dl = ((blood->sLife - blood->life) << 16) / blood->sLife;

			blood->x += blood->xVel >> 5;
			blood->y += blood->yVel >> 5;
			blood->z += blood->zVel >> 5;

			blood->size = blood->sSize + (dl * (blood->dSize - blood->sSize) >> 16);
		}
	}
}

int GetFreeGunshell()
{
	int gsNum = NextGunShell;
	int minLife = 4095;
	int minIndex = 0;
	int i = 0;

	while (true)
	{
		GUNSHELL_STRUCT* gs = &Gunshells[NextGunShell];

		if (!gs->counter)
			break;

		if (gs->counter < minLife)
		{
			minLife = gs->counter;
			minIndex = gsNum;
		}
		if (gsNum == MAX_GUNSHELL - 1)
		{
			gs = &Gunshells[0];
			gsNum = 0;
		}
		else
		{
			gsNum++;
			gs++;
		}

		if (++i >= MAX_GUNSHELL)
		{
			NextGunShell = minIndex + 1;
			if (minIndex + 1 >= MAX_GUNSHELL)
				NextGunShell = 0;
			return minIndex;
		}
	}

	NextGunShell = gsNum + 1;
	if (gsNum + 1 >= MAX_GUNSHELL)
		NextGunShell = 0;

	return gsNum;
}

void TriggerGunShell(short hand, short objNum, LaraWeaponType weaponType)
{
	auto pos = Vector3i::Zero;
	if (hand)
	{
		auto offset = Vector3i::Zero;
		switch (weaponType)
		{
		case LaraWeaponType::Pistol:
			offset = Vector3i(8, 48, 40);
			break;

		case LaraWeaponType::Uzi:
			offset = Vector3i(8, 35, 48);
			break;

		case LaraWeaponType::Shotgun:
			offset = Vector3i(16, 114, 32);
			break;

		case LaraWeaponType::HK:
			offset = Vector3i(16, 114, 96);
			break;

		default:
			break;
		}

		pos = GetJointPosition(LaraItem, LM_RHAND, offset);
	}
	else
	{
		if (weaponType == LaraWeaponType::Pistol)
			pos = GetJointPosition(LaraItem, LM_LHAND, Vector3i(-12, 48, 40));
		else if (weaponType == LaraWeaponType::Uzi)
			pos = GetJointPosition(LaraItem, LM_LHAND, Vector3i(-16, 35, 48));
	}

	if (g_GameFlow->GetSettings()->Weapons[(int)weaponType - 1].Shell)
	{
		auto& gunshell = Gunshells[GetFreeGunshell()];

		gunshell.pos.Position = pos;
		gunshell.pos.Orientation.x = 0;
		gunshell.pos.Orientation.y = 0;
		gunshell.pos.Orientation.z = GetRandomControl();
		gunshell.roomNumber = LaraItem->RoomNumber;
		gunshell.speed = (GetRandomControl() & 0x1F) + 16;
		gunshell.fallspeed = -48 - (GetRandomControl() & 7);
		gunshell.objectNumber = objNum;
		gunshell.counter = (GetRandomControl() & 0x1F) + 60;

		if (hand)
		{
			if (weaponType == LaraWeaponType::Shotgun)
			{
				gunshell.dirXrot =
					Lara.LeftArm.Orientation.y +
					Lara.ExtraTorsoRot.y +
					LaraItem->Pose.Orientation.y -
					(GetRandomControl() & 0xFFF) +
					10240;
				gunshell.pos.Orientation.y +=
					Lara.LeftArm.Orientation.y +
					Lara.ExtraTorsoRot.y +
					LaraItem->Pose.Orientation.y;

				if (gunshell.speed < 24)
					gunshell.speed += 24;
			}
			else
			{
				gunshell.dirXrot =
					Lara.LeftArm.Orientation.y +
					LaraItem->Pose.Orientation.y -
					(GetRandomControl() & 0xFFF) +
					18432;
			}
		}
		else
		{
			gunshell.dirXrot =
				Lara.LeftArm.Orientation.y +
				LaraItem->Pose.Orientation.y +
				(GetRandomControl() & 0xFFF) -
				18432;
		}
	}

	if (LaraItem->MeshBits.TestAny())
	{
		if (weaponType == LaraWeaponType::Shotgun)
			TriggerGunSmoke(pos.x, pos.y, pos.z, 0, 0, 0, 0, LaraWeaponType::Shotgun, 24);
		else
			TriggerGunSmoke(pos.x, pos.y, pos.z, 0, 0, 0, 0, weaponType, 16);
	}
}

void UpdateGunShells()
{
	for (int i = 0; i < MAX_GUNSHELL; i++)
	{
		auto* gunshell = &Gunshells[i];

		if (gunshell->counter)
		{
			gunshell->StoreInterpolationData();

			auto prevPos = gunshell->pos.Position;

			gunshell->counter--;

			short prevRoomNumber = gunshell->roomNumber;

			if (TestEnvironment(ENV_FLAG_WATER, gunshell->roomNumber))
			{
				gunshell->fallspeed++;

				if (gunshell->fallspeed <= 8)
				{
					if (gunshell->fallspeed < 0)
						gunshell->fallspeed >>= 1;
				}
				else
					gunshell->fallspeed = 8;
				
				gunshell->speed -= gunshell->speed >> 1;
			}
			else
				gunshell->fallspeed += g_GameFlow->GetSettings()->Physics.Gravity;

			gunshell->pos.Orientation.x += ((gunshell->speed / 2) + 7) * ANGLE(1.0f);
			gunshell->pos.Orientation.y += gunshell->speed * ANGLE(1.0f);
			gunshell->pos.Orientation.z += ANGLE(23.0f);

			gunshell->pos.Position.x += gunshell->speed * phd_sin(gunshell->dirXrot);
			gunshell->pos.Position.y += gunshell->fallspeed;
			gunshell->pos.Position.z += gunshell->speed * phd_cos(gunshell->dirXrot);

			FloorInfo* floor = GetFloor(gunshell->pos.Position.x, gunshell->pos.Position.y, gunshell->pos.Position.z, &gunshell->roomNumber);
			if (TestEnvironment(ENV_FLAG_WATER, gunshell->roomNumber) &&
				!TestEnvironment(ENV_FLAG_WATER, prevRoomNumber))
			{

				SpawnSplashDrips(Vector3(gunshell->pos.Position.x, g_Level.Rooms[gunshell->roomNumber].TopHeight, gunshell->pos.Position.z), gunshell->roomNumber, 3, true);
				//AddWaterSparks(gs->pos.Position.x, g_Level.Rooms[gs->roomNumber].maxceiling, gs->pos.Position.z, 8);
				SpawnRipple(
					Vector3(gunshell->pos.Position.x, g_Level.Rooms[gunshell->roomNumber].TopHeight, gunshell->pos.Position.z),
					gunshell->roomNumber,
					Random::GenerateFloat(8.0f, 12.0f),
					(int)RippleFlags::SlowFade);
				
				gunshell->fallspeed >>= 5;
				continue;
			}

			int ceiling = GetCeiling(floor, gunshell->pos.Position.x, gunshell->pos.Position.y, gunshell->pos.Position.z);
			if (gunshell->pos.Position.y < ceiling)
			{
				SoundEffect(SFX_TR4_SHOTGUN_SHELL, &gunshell->pos);
				gunshell->speed -= 4;

				if (gunshell->speed < 8)
				{
					gunshell->counter = 0;
					continue;
				}

				gunshell->pos.Position.y = ceiling;
				gunshell->fallspeed = -gunshell->fallspeed;
			}

			int height = GetFloorHeight(floor, gunshell->pos.Position.x, gunshell->pos.Position.y, gunshell->pos.Position.z);
			if (gunshell->pos.Position.y >= height)
			{
				SoundEffect(SFX_TR4_SHOTGUN_SHELL, &gunshell->pos);
				gunshell->speed -= 8;
				if (gunshell->speed >= 8)
				{
					if (prevPos.y <= height)
						gunshell->fallspeed = -gunshell->fallspeed >> 1;
					else
					{
						gunshell->dirXrot += ANGLE(-180.0f);
						gunshell->pos.Position.x = prevPos.x;
						gunshell->pos.Position.z = prevPos.z;
					}
					gunshell->pos.Position.y = prevPos.y;
				}
				else
					gunshell->counter = 0;
			}
		}
	}
}

void AddWaterSparks(int x, int y, int z, int num)
{
	for (int i = 0; i < num; i++)
	{
		auto* spark = GetFreeParticle();

		spark->on = 1;
		spark->sR = 127;
		spark->sG = 127;
		spark->sB = 127;
		spark->dR = 48;
		spark->dG = 48;
		spark->dB = 48;
		spark->colFadeSpeed = 4;
		spark->fadeToBlack = 8;
		spark->life = 10;
		spark->sLife = 10;
		spark->sSize = 8;
		spark->dSize = 32;
		spark->scalar = 1;
		spark->blendMode = BlendMode::Additive;	
		int random = GetRandomControl() & 0xFFF;
		spark->xVel = -phd_sin(random << 4) * 128;
		spark->yVel = -Random::GenerateInt(128, 256);
		spark->zVel = phd_cos(random << 4) * 128;
		spark->friction = 5;
		spark->flags = SP_NONE;
		spark->x = x + (spark->xVel >> 3);
		spark->y = y - (spark->yVel >> 5);
		spark->z = z + (spark->zVel >> 3);
		spark->maxYvel = 0;
		spark->gravity = (GetRandomControl() & 0xF);
	}
}

void SomeSparkEffect(int x, int y, int z, int count)
{
	for (int i = 0; i < count; i++)
	{
		auto* spark = GetFreeParticle();

		spark->on = true;
		spark->sR = 112;
		spark->sG = (GetRandomControl() & 0x1F) + -128;
		spark->sB = (GetRandomControl() & 0x1F) + -128;
		spark->colFadeSpeed = 4;
		spark->fadeToBlack = 8;
		spark->life = 24;
		spark->dR = spark->sR >> 1;
		spark->dG = spark->sG >> 1;
		spark->dB = spark->sB >> 1;
		spark->sLife = 24;
		spark->blendMode = BlendMode::Additive;
		spark->friction = 5;
		int random = GetRandomControl() & 0xFFF;
		spark->xVel = -128 * phd_sin(random << 4);
		spark->yVel = -640 - (byte)GetRandomControl();
		spark->zVel = 128 * phd_cos(random << 4);
		spark->flags = 0;
		spark->x = x + (spark->xVel >> 3);
		spark->y = y - (spark->yVel >> 5);
		spark->z = z + (spark->zVel >> 3);
		spark->maxYvel = 0;
		spark->gravity = (GetRandomControl() & 0xF) + 64;
	}
}

void TriggerUnderwaterExplosion(ItemInfo* item, int flag)
{
	if (flag)
	{
		int x = (GetRandomControl() & 0x1FF) + item->Pose.Position.x - CLICK(1);
		int y = item->Pose.Position.y;
		int z = (GetRandomControl() & 0x1FF) + item->Pose.Position.z - CLICK(1);

		TriggerExplosionBubbles(x, y, z, item->RoomNumber);
		TriggerExplosionSparks(x, y, z, 2, -1, 1, item->RoomNumber);

		int waterHeight = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetWaterTopHeight();
		if (waterHeight != NO_HEIGHT)
			SomeSparkEffect(x, waterHeight, z, 8);
	}
	else
	{
		TriggerExplosionBubble(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, item->RoomNumber);
		TriggerExplosionSparks(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, 2, -2, 1, item->RoomNumber);

		for (int i = 0; i < 3; i++)
			TriggerExplosionSparks(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, 2, -1, 1, item->RoomNumber);

		int waterHeight = GetPointCollision(*item).GetWaterTopHeight();
		if (waterHeight != NO_HEIGHT)
		{
			int dy = item->Pose.Position.y - waterHeight;
			if (dy < 2048)
			{
				SplashSetup.Position = Vector3(item->Pose.Position.x, waterHeight, item->Pose.Position.z);
				SplashSetup.InnerRadius = 160;
				SplashSetup.SplashPower = 2048 - dy;

				SetupSplash(&SplashSetup, item->RoomNumber);
			}
		}
	}
}

void TriggerUnderwaterExplosion(Vector3 position, bool flag)
{
	int roomNumber = FindRoomNumber(position);
	const auto& room = g_Level.Rooms[roomNumber];

	if (!flag)
	{
		int x = (GetRandomControl() & 0x1FF) + position.x - CLICK(1);
		int y = position.y;
		int z = (GetRandomControl() & 0x1FF) + position.z - CLICK(1);



		TriggerExplosionBubbles(x, y, z, room.RoomNumber);
		TriggerExplosionSparks(x, y, z, 2, -1, 1, room.RoomNumber);

		int waterHeight = GetPointCollision(Vector3i(x, y, z), room.RoomNumber).GetWaterTopHeight();
		if (waterHeight != NO_HEIGHT)
			SomeSparkEffect(x, waterHeight, z, 8);
	}
	else
	{
		TriggerExplosionBubble(position.x, position.y, position.z, room.RoomNumber);
		TriggerExplosionSparks(position.x, position.y, position.z, 2, -2, 1, room.RoomNumber);

		for (int i = 0; i < 3; i++)
			TriggerExplosionSparks(position.x, position.y, position.z, 2, -1, 1, room.RoomNumber);

		int waterHeight = GetPointCollision(position, room.RoomNumber).GetWaterTopHeight();
		if (waterHeight != NO_HEIGHT)
		{
			int dy = position.y - waterHeight;
			if (dy < 2048)
			{
				SplashSetup.Position = Vector3(position.x, waterHeight, position.z);
				SplashSetup.InnerRadius = 160;
				SplashSetup.SplashPower = 2048 - dy;

				SetupSplash(&SplashSetup, room.RoomNumber);
			}
		}
	}
}

void ExplodeVehicle(ItemInfo* laraItem, ItemInfo* vehicle)
{
	if (g_Level.Rooms[vehicle->RoomNumber].flags & ENV_FLAG_WATER)
	{
		TriggerUnderwaterExplosion(vehicle, 1);
	}
	else
	{
		TriggerExplosionSparks(vehicle->Pose.Position.x, vehicle->Pose.Position.y, vehicle->Pose.Position.z, 3, -2, 0, vehicle->RoomNumber);
		for (int i = 0; i < 3; i++)
		{
			TriggerExplosionSparks(vehicle->Pose.Position.x, vehicle->Pose.Position.y, vehicle->Pose.Position.z, 3, -1, 0, vehicle->RoomNumber);
		}
	}

	auto* lara = GetLaraInfo(laraItem);

	ExplodingDeath(lara->Context.Vehicle, BODY_DO_EXPLOSION | BODY_STONE_SOUND);
	KillItem(lara->Context.Vehicle);
	vehicle->Status = ITEM_DEACTIVATED;
	SoundEffect(SFX_TR4_EXPLOSION1, &laraItem->Pose);
	SoundEffect(SFX_TR4_EXPLOSION2, &laraItem->Pose);

	SetLaraVehicle(laraItem, nullptr);
	SetAnimation(laraItem, LA_FALL_START);
	laraItem->Animation.IsAirborne = true;
	DoDamage(laraItem, INT_MAX);
}

void ExplodingDeath(short itemNumber, short flags)
{
	ItemInfo* item = &g_Level.Items[itemNumber];
	
	ObjectInfo* obj;
	if (item->IsLara() && Objects[ID_LARA_SKIN].loaded)
		obj = &Objects[ID_LARA_SKIN];
	else
		obj = &Objects[item->ObjectNumber];
	
	auto world = item->Pose.Orientation.ToRotationMatrix();

	// If only BODY_PART_EXPLODE flag exists but not BODY_EXPLODE, add it.
	if ((flags & BODY_PART_EXPLODE) && !(flags & BODY_DO_EXPLOSION))
		flags |= BODY_DO_EXPLOSION;

	for (int i = 0; i < obj->nmeshes; i++)
	{
		Matrix boneMatrix;
		g_Renderer.GetBoneMatrix(itemNumber, i, &boneMatrix);
		boneMatrix = world * boneMatrix;

		if (!item->MeshBits.Test(i))
			continue;

		item->MeshBits.Clear(i);

		if (i == 0 ||  ((GetRandomControl() & 3) != 0 && (flags & BODY_DO_EXPLOSION)))
		{
			short fxNumber = CreateNewEffect(item->RoomNumber);
			if (fxNumber != NO_VALUE)
			{
				FX_INFO* fx = &EffectList[fxNumber];

				fx->pos.Position.x = boneMatrix.Translation().x;
				fx->pos.Position.y = boneMatrix.Translation().y - BODY_PART_SPAWN_VERTICAL_OFFSET;
				fx->pos.Position.z = boneMatrix.Translation().z;

				fx->roomNumber = item->RoomNumber;
				fx->pos.Orientation.x = 0;
				fx->pos.Orientation.y = Random::GenerateAngle();

				if (!(flags & BODY_NO_RAND_VELOCITY))
				{
					if (flags & BODY_MORE_RAND_VELOCITY)
						fx->speed = GetRandomControl() >> 12;
					else
						fx->speed = GetRandomControl() >> 8;
				}

				if (flags & BODY_NO_VERTICAL_VELOCITY)
					fx->fallspeed = 0;
				else
				{
					if (flags & BODY_LESS_IMPULSE)
						fx->fallspeed = -(GetRandomControl() >> 8);
					else
						fx->fallspeed = -(GetRandomControl() >> 12);
				}

				fx->objectNumber = ID_BODY_PART;
				fx->color = item->Model.Color;
				fx->flag2 = flags;
				fx->frameNumber = item->Model.MeshIndex[i];
			}
		}
		else
		{
			ExplodeItemNode(item, i, 0, 128);
		}
	}
}

int GetFreeShockwave()
{
	for (int i = 0; i < MAX_SHOCKWAVE; i++)
	{
		if (!ShockWaves[i].life)
			return i;
	}

	return -1;
}

void TriggerShockwave(Pose* pos, short innerRad, short outerRad, int speed, unsigned char r, unsigned char g, unsigned char b, unsigned char life, EulerAngles rotation, short damage, bool hasSound, bool fadein, bool hasLight, int style)
{
	int s = GetFreeShockwave();
	SHOCKWAVE_STRUCT* sptr;

	if (s != -1)
	{
		sptr = &ShockWaves[s];

		sptr->x = pos->Position.x;
		sptr->y = pos->Position.y;
		sptr->z = pos->Position.z;
		sptr->innerRad = innerRad;
		sptr->outerRad = outerRad;
		sptr->xRot = rotation.x;
		sptr->yRot = rotation.y;
		sptr->zRot = rotation.z;
		sptr->damage = damage;
		sptr->speed = speed;
		sptr->r = r;
		sptr->g = g;
		sptr->b = b;
		sptr->life = life;
		sptr->sLife = life;
		sptr->fadeIn = fadein;
		sptr->HasLight = hasLight;
		
		sptr->sr = 0;
		sptr->sg = 0;
		sptr->sb = 0;
		sptr->style = style;

		if (hasSound)
			SoundEffect(SFX_TR4_DEMIGOD_SIREN_SWAVE, pos);
	}
}

void TriggerShockwaveHitEffect(int x, int y, int z, unsigned char r, unsigned char g, unsigned char b, short rot, int vel)
{
	int dx = LaraItem->Pose.Position.x - x;
	int dz = LaraItem->Pose.Position.z - z;

	if (dx >= -16384 && dx <= 16384 && dz >= -16384 && dz <= 16384)
	{
		auto* spark = GetFreeParticle();
		spark->dB = b;
		spark->on = true;
		spark->sR = 0;
		spark->sG = 0;
		spark->sB = 0;
		spark->dG = g;
		spark->dR = r;
		spark->colFadeSpeed = 4;
		spark->fadeToBlack = 8;
		spark->blendMode = BlendMode::Additive;
		spark->life = spark->sLife = (GetRandomControl() & 3) + 16;

		int speed = (GetRandomControl() & 0xF) + vel;
		spark->xVel = speed * 16 * phd_sin(rot);
		spark->yVel = -512 - (GetRandomControl() & 0x1FF);
		spark->zVel = speed * 16 * phd_cos(rot);

		short angle;
		if (GetRandomControl() & 1)
			angle = rot + ANGLE(90);
		else
			angle = rot - ANGLE(90);

		int shift = (GetRandomControl() & 0x1FF) - 256;
		x += shift * phd_sin(angle);
		z += shift * phd_cos(angle);

		spark->x = (GetRandomControl() & 0x1F) + x - 16;
		spark->y = (GetRandomControl() & 0x1F) + y - 16;
		spark->z = (GetRandomControl() & 0x1F) + z - 16;

		spark->friction = 3;
		spark->flags = SP_EXPDEF | SP_ROTATE | SP_DEF | SP_SCALE;
		spark->rotAng = GetRandomControl() & 0xFFF;
		if (GetRandomControl() & 1)
			spark->rotAdd = -16 - (GetRandomControl() & 0xF);
		else
			spark->rotAdd = (GetRandomControl() & 0xF) + 16;

		spark->scalar = 1;
		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = SPR_UNDERWATERDUST;
		spark->maxYvel = 0;
		spark->gravity = (GetRandomControl() & 0x3F) + 64;
		spark->sSize = spark->size = (GetRandomControl() & 0x1F) + 32;
		spark->dSize = spark->size / 4;
	}
}

void UpdateShockwaves()
{
	for (auto& shockwave : ShockWaves)
	{
		if (shockwave.life <= 0)
			continue;

		shockwave.StoreInterpolationData();

		shockwave.life--;

		// Spawn light.
		if (shockwave.HasLight)
		{
			auto lightColor = Color(shockwave.r / (float)UCHAR_MAX, shockwave.g / (float)UCHAR_MAX, shockwave.b / (float)UCHAR_MAX);
			auto pos = Vector3(shockwave.x, shockwave.y, shockwave.z);
			SpawnDynamicPointLight(pos, lightColor, shockwave.life / 4.0f);
		}

		if (shockwave.style != (int)ShockwaveStyle::Knockback)
		{
			shockwave.outerRad += shockwave.speed;
			if (shockwave.style == (int)ShockwaveStyle::Sophia)
				shockwave.innerRad += shockwave.speed;
		}
		else
		{
			if (shockwave.life > (shockwave.sLife / 2))
			{
				shockwave.outerRad += shockwave.speed;
				shockwave.innerRad += shockwave.speed;
			}
			else
			{
				shockwave.outerRad -= shockwave.speed;
				shockwave.innerRad -= shockwave.speed;
			}
		}

		shockwave.speed -= (shockwave.speed >> 4);

		if (LaraItem->HitPoints > 0 && shockwave.damage)
		{
			const auto& bounds = GetBestFrame(*LaraItem).BoundingBox;
			int dx = LaraItem->Pose.Position.x - shockwave.x;
			int dz = LaraItem->Pose.Position.z - shockwave.z;
			float dist = sqrt(SQUARE(dx) + SQUARE(dz));
			int angle = phd_atan(dz, dx);

			// Damage player if inside shockwave.
			if (shockwave.y > (LaraItem->Pose.Position.y + bounds.Y1) &&
				shockwave.y < (LaraItem->Pose.Position.y + (bounds.Y2 + CLICK(1))) &&
				dist > shockwave.innerRad &&
				dist < shockwave.outerRad)
			{
				TriggerShockwaveHitEffect(
					LaraItem->Pose.Position.x, shockwave.y, LaraItem->Pose.Position.z,
					shockwave.r, shockwave.g, shockwave.b,
					angle, shockwave.speed);
				DoDamage(LaraItem, shockwave.damage);
			}
		}
	}
}

void TriggerExplosionBubble(int x, int y, int z, short roomNumber)
{
	constexpr auto BUBBLE_COUNT = 24;
	auto* spark = GetFreeParticle();

	spark->sR = 128;
	spark->dR = 128;
	spark->dG = 128;
	spark->dB = 128;
	spark->on = 1;
	spark->life = 24;
	spark->sLife = 24;
	spark->sG = 64;
	spark->sB = 0;
	spark->colFadeSpeed = 8;
	spark->fadeToBlack = 12;
	spark->blendMode = BlendMode::Additive;
	spark->x = x;
	spark->y = y;
	spark->z = z;
	spark->xVel = 0;
	spark->yVel = 0;
	spark->zVel = 0;
	spark->friction = 0;
	spark->flags = 2058;
	spark->scalar = 3;
	spark->gravity = 0;
	spark->SpriteSeqID = ID_DEFAULT_SPRITES;
	spark->SpriteID = SPR_BUBBLES;
	spark->maxYvel = 0;
	int size = (GetRandomControl() & 7) + 63;
	spark->sSize = size >> 1;
	spark->size = size >> 1;
	spark->dSize = 2 * size;

	auto sphere = BoundingSphere(Vector3(x, y, z), BLOCK(0.25f));
	for (int i = 0; i < BUBBLE_COUNT; i++)
	{
		auto pos = Random::GeneratePointInSphere(sphere);
		SpawnBubble(pos, roomNumber, (int)BubbleFlags::LargeScale | (int)BubbleFlags::HighAmplitude);
	}
}

void TriggerFenceSparks(int x, int y, int z, int kill, int crane)
{
	auto* spark = GetFreeParticle();

	spark->on = 1;
	spark->sR = (GetRandomControl() & 0x3F) - 0x40;
	spark->sG = (GetRandomControl() & 0x3F) - 0x40;
	spark->sB = (GetRandomControl() & 0x3F) - 0x40;

	spark->dR = GetRandomControl() | 0xC0;
	spark->colFadeSpeed = 16;
	spark->g = 8;
	spark->dG = spark->sR >> 1;
	spark->dB = spark->sR >> 2;

	spark->life = (GetRandomControl() & 7) + 24;
	spark->sLife = (GetRandomControl() & 7) + 24;
	spark->blendMode = BlendMode::Additive;
	spark->dynamic = -1;

	spark->x = x;
	spark->y = y;
	spark->z = z;

	spark->xVel = ((GetRandomControl() & 0xFF) - 128) << 2;
	spark->yVel = (GetRandomControl() & 0xF) - ((kill << 5) + 8) + (crane << 4);
	spark->zVel = ((GetRandomControl() & 0xFF) - 128) << 2;

	if (crane != 0)
		spark->friction = 5;
	else
		spark->friction = 4;

	spark->flags = SP_NONE;
	spark->gravity = (GetRandomControl() & 0xF) + ((crane << 4) + 16);
	spark->maxYvel = 0;
}

void TriggerSmallSplash(int x, int y, int z, int number) 
{
	for (int i = 0; i < number; i++)
	{
		auto* sptr = GetFreeParticle();

		sptr->on = 1;

		sptr->sR = 64;
		sptr->sG = 64;
		sptr->sB = 64;

		sptr->dR = 32;
		sptr->dG = 32;
		sptr->dB = 32;

		sptr->colFadeSpeed = 4;
		sptr->fadeToBlack = 8;

		sptr->life = 24;
		sptr->sLife = 24;

		sptr->blendMode = BlendMode::Additive;

		int angle = GetRandomControl() << 3;

		sptr->xVel = -phd_sin(angle) * 512;
		sptr->yVel = -640 - (GetRandomControl() & 0xFF);
		sptr->zVel = phd_cos(angle) * 512;

		sptr->friction = 5;
		sptr->flags = 0;

		sptr->x = x + (sptr->xVel >> 3);
		sptr->y = y - (sptr->yVel >> 5);
		sptr->z = z + (sptr->zVel >> 3);

		sptr->maxYvel = 0;
		sptr->gravity = (GetRandomControl() & 0xF) + 64; 
	}
}
