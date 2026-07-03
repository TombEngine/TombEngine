#include "framework.h"
#include "Objects/Effects/enemy_missile.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Objects/TR3/Entity/tr3_claw_mutant.h"
#include "Objects/TR4/Entity/tr4_mutant.h"
#include "Objects/TR4/Entity/tr4_demigod.h"
#include "Renderer/RendererEnums.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Entities::TR4;
using namespace TEN::Math;

namespace TEN::Entities::Effects
{
	void TriggerSethMissileFlame(short fxNumber, short xVel, short yVel, short zVel)
	{
		auto& fxInfo = GetFXInfo(g_Level.Items[fxNumber]);

		auto& flame = *GetFreeParticle();

		flame.on = true;
		flame.sR = 0;
		flame.dR = 0;
		flame.sG = (GetRandomControl() & 0x7F) + 32;
		flame.sB = flame.dG + 64;
		flame.dB = (GetRandomControl() & 0x7F) + 32;
		flame.dG = flame.dB + 64;
		flame.fadeToBlack = 8;
		flame.colFadeSpeed = (GetRandomControl() & 3) + 4;
		flame.blendMode = BlendMode::Additive;
		flame.life = flame.sLife = (GetRandomControl() & 3) + 16;
		flame.y = 0;
		flame.x = (GetRandomControl() & 0xF) - 8;
		flame.xVel = xVel;
		flame.yVel = yVel;
		flame.z = (GetRandomControl() & 0xF) - 8;
		flame.zVel = zVel;
		flame.friction = 68;
		flame.flags = 602;
		flame.rotAng = GetRandomControl() & 0xFFF;

		if (GetRandomControl() & 1)
			flame.rotAdd = -32 - (GetRandomControl() & 0x1F);
		else
			flame.rotAdd = (GetRandomControl() & 0x1F) + 32;

		flame.gravity = 0;
		flame.maxYvel = 0;
		flame.fxObj = fxNumber;

		if (fxInfo.Flag1 == 1)
		{
			flame.scalar = 3;
		}
		else
		{
			flame.scalar = 2;
		}

		flame.sSize = flame.size = (GetRandomControl() & 7) + 64;
		flame.dSize = flame.size / 32;
	}

	void TriggerHarpyFlameFlame(short fxNumber, short xVel, short yVel, short zVel)
	{
		auto& flame = *GetFreeParticle();

		flame.on = true;
		flame.sR = 0;
		flame.sG = (GetRandomControl() & 0x7F) + 32;
		flame.sB = flame.dG + 64;
		flame.dB = 0;
		flame.dG = flame.dR = (GetRandomControl() & 0x7F) + 32;
		flame.fadeToBlack = 8;
		flame.colFadeSpeed = (GetRandomControl() & 3) + 4;
		flame.blendMode = BlendMode::Additive;
		flame.life = flame.sLife = (GetRandomControl() & 3) + 16;
		flame.y = 0;
		flame.x = (GetRandomControl() & 0xF) - 8;
		flame.xVel = xVel;
		flame.zVel = zVel;
		flame.z = (GetRandomControl() & 0xF) - 8;
		flame.yVel = yVel;
		flame.friction = 68;
		flame.flags = 602;
		flame.rotAng = GetRandomControl() & 0xFFF;

		if (GetRandomControl() & 1)
		{
			flame.rotAdd = -32 - (GetRandomControl() & 0x1F);
		}
		else
		{
			flame.rotAdd = (GetRandomControl() & 0x1F) + 32;
		}

		flame.gravity = 0;
		flame.maxYvel = 0;
		flame.fxObj = fxNumber;
		flame.scalar = 2;
		flame.sSize = flame.size = (GetRandomControl() & 7) + 64;
		flame.dSize = flame.size / 32;
	}

	void BubblesShatterFunction(ItemInfo& fx, int param1, int param2)
	{
		auto& fxInfo = GetFXInfo(fx);

		ShatterItem.yRot = fx.Pose.Orientation.y;
		ShatterItem.meshIndex = fx.Animation.FrameNumber;
		ShatterItem.color = NEUTRAL_COLOR;
		ShatterItem.sphere.Center = fx.Pose.Position.ToVector3();
		ShatterItem.bit = 0;
		ShatterItem.flags = fxInfo.Flag2 & 0x400;
		ShatterObject(&ShatterItem, 0, param2, fx.RoomNumber, param1);
	}

	void ControlEnemyMissile(short fxNumber)
	{
		auto& fx = g_Level.Items[fxNumber];
		auto& fxInfo = GetFXInfo(fx);

		auto orient = Geometry::GetOrientToPoint(
			Vector3(fx.Pose.Position.x, fx.Pose.Position.y + CLICK(1), fx.Pose.Position.z),
			LaraItem->Pose.Position.ToVector3());

		int maxRotation = 0;
		int maxVelocity = 0;

		if (fxInfo.Flag1 == (int)MissileType::SethLarge)
		{
			maxRotation = ANGLE(2.8f);
			maxVelocity = CLICK(1);
		}
		else
		{
			if (fxInfo.Flag1 == (int)MissileType::CrocgodMutant)
			{
				if (fxInfo.Counter)
					fxInfo.Counter--;

				maxRotation = ANGLE(1.4f);
			}
			else
			{
				maxRotation = ANGLE(4.5f);
			}

			maxVelocity = CLICK(0.75f);
		}

		if (fx.Animation.Velocity.z < maxVelocity)
		{
			if (fxInfo.Flag1 == (int)MissileType::CrocgodMutant)
			{
				fx.Animation.Velocity.z++;
			}
			else
			{
				fx.Animation.Velocity.z += 3;
			}
		}

		if (fx.Animation.Velocity.z < maxVelocity &&
			fxInfo.Flag1 != (int)MissileType::SophiaLeighNormal &&
			fxInfo.Flag1 != (int)MissileType::SophiaLeighLarge &&
			fxInfo.Flag1 != (int)MissileType::ClawMutantPlasma)
		{
			short dy = orient.y - fx.Pose.Orientation.y;
			if (abs(dy) > abs(ANGLE(180.0f)))
				dy = -dy;

			short dx = orient.x - fx.Pose.Orientation.x;
			if (abs(dx) > abs(ANGLE(180.0f)))
				dx = -dx;

			dy >>= 3;
			if (dy < -maxRotation)
			{
				dy = -maxRotation;
			}
			else if (dy > maxRotation)
			{
				dy = maxRotation;
			}

			dx >>= 3;
			if (dx < -maxRotation)
			{
				dx = -maxRotation;
			}
			else if (dx > maxRotation)
			{
				dx = maxRotation;
			}

			fx.Pose.Orientation.x += dx;
			if (fxInfo.Flag1 != (int)MissileType::Demigod3Radial && (fxInfo.Flag1 != (int)MissileType::CrocgodMutant || !fxInfo.Counter))
				fx.Pose.Orientation.y += dy;
		}

		fx.Pose.Orientation.z += 16 * fx.Animation.Velocity.z;
		if (fxInfo.Flag1 == (int)MissileType::CrocgodMutant)
			fx.Pose.Orientation.z += 16 * fx.Animation.Velocity.z;

		auto prevPos = fx.Pose.Position;

		int speed = (fx.Animation.Velocity.z * phd_cos(fx.Pose.Orientation.x));
		fx.Pose.Position.x += speed * phd_sin(fx.Pose.Orientation.y);
		fx.Pose.Position.y += -((fx.Animation.Velocity.z * phd_sin(fx.Pose.Orientation.x))) + fx.Animation.Velocity.y;
		fx.Pose.Position.z += speed * phd_cos(fx.Pose.Orientation.y);

		auto pointColl = GetPointCollision(fx.Pose.Position, fx.RoomNumber);

		if (fx.Pose.Position.y >= pointColl.GetFloorHeight() || fx.Pose.Position.y <= pointColl.GetCeilingHeight())
		{
			fx.Pose.Position = prevPos;

			switch ((MissileType)fxInfo.Flag1)
			{
			case MissileType::SethLarge:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 64, 128, 00, 24, EulerAngles(fx.Pose.Orientation.x - ANGLE(90.0f), 0, 0), 0, true, false, false, (int)ShockwaveStyle::Normal);
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.RoomNumber);
				BubblesShatterFunction(fx, 0, -32);
				break;

			case MissileType::SophiaLeighNormal:
				TriggerShockwave(&fx.Pose, 5, 32, 128, 0, 128, 128, 24, EulerAngles(fx.Pose.Orientation.x - ANGLE(90.0f), 0, 0), 0, true, false, false, (int)ShockwaveStyle::Normal);
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.RoomNumber);
				break;

			case MissileType::SophiaLeighLarge:
				TriggerShockwave(&fx.Pose, 10, 64, 128, 0, 128, 128, 24, EulerAngles(fx.Pose.Orientation.x - ANGLE(90.0f), 0, 0), 0, true, false, false, (int)ShockwaveStyle::Normal);
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.RoomNumber);
				break;

			case MissileType::Demigod3Single:
			case MissileType::Demigod3Radial:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 0, 96, 128, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				BubblesShatterFunction(fx, 0, -32);
				break;

			case MissileType::Demigod2:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 128, 64, 0, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				BubblesShatterFunction(fx, 0, -32);
				break;

			case MissileType::Harpy:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 128, 128, 0, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				BubblesShatterFunction(fx, 0, -32);
				break;

			case MissileType::CrocgodMutant:
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 0, fx.RoomNumber);
				TriggerShockwave(&fx.Pose, 48, 240, 64, 128, 96, 0, 24, EulerAngles::Identity, 15, true, false, false, (int)ShockwaveStyle::Normal);
				
				fx.Pose.Position.y -= 128;
				TriggerShockwave(&fx.Pose, 48, 240, 48, 128, 112, 0, 16, EulerAngles::Identity, 15, true, false, false, (int)ShockwaveStyle::Normal);
				
				fx.Pose.Position.y += 256;
				TriggerShockwave(&fx.Pose, 48, 240, 48, 128, 112, 0, 16, EulerAngles::Identity, 15, true, false, false, (int)ShockwaveStyle::Normal);
				break;

			case MissileType::ClawMutantPlasma:
				for (int i = 0; i < 6; i++)
				{
					SpawnClawMutantPlasmaFlameBall(fxNumber, Vector3(20.0f, 32.0f, 20.0f), Vector3(Random::GenerateFloat(-115.0f, 185.0f), 0, Random::GenerateFloat(-115.0f, 185.0f)), 24);
					SpawnClawMutantPlasmaFlameBall(fxNumber, Vector3(20.0f, 32.0f, 20.0f), Vector3(Random::GenerateFloat(-115.0f, 185.0f), 0, Random::GenerateFloat(-115.0f, 185.0f)), 24);
					SpawnClawMutantPlasmaFlameBall(fxNumber, Vector3(20.0f, 32.0f, 20.0f), Vector3(Random::GenerateFloat(-115.0f, 185.0f), 0, Random::GenerateFloat(-115.0f, 185.0f)), 24);
				}

				break;

			case MissileType::WillardPlasmaBall:
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.roomNumber);
				TriggerShockwave(&fx.pos, 32, 160, 64, 0, 128, 64, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				break;

			default:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 0, 128, 64, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				BubblesShatterFunction(fx, 0, -32);
				break;
			}

			KillItem(fxNumber);
			return;
		}

		if (ItemNearLara(fx.Pose.Position, 200))
		{
			LaraItem->HitStatus = true;
			switch ((MissileType)fxInfo.Flag1)
			{
			case MissileType::SethLarge:
				TriggerShockwave(&fx.Pose, 48, 240, 64, 0, 128, 64, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.RoomNumber);
				ItemCustomBurn(LaraItem, Vector3(0.0f, 0.8f, 0.1f), Vector3(0.0f, 0.9f, 0.8f));
				break;

			case MissileType::SophiaLeighLarge:
				TriggerShockwave(&fx.Pose, 5, 32, 128, 0, 128, 128, 24, EulerAngles(fx.Pose.Orientation.y, 0.0f, 0.0f), fxInfo.Flag2, true, false, false, (int)ShockwaveStyle::Normal);
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.RoomNumber);
				break;

			case MissileType::SophiaLeighNormal:
				TriggerShockwave(&fx.Pose, 10, 64, 128, 0, 128, 128, 24, EulerAngles(fx.Pose.Orientation.y, 0.0f, 0.0f), fxInfo.Flag2, true, false, false, (int)ShockwaveStyle::Normal);
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.RoomNumber);
				break;

			case MissileType::Demigod3Single:
			case MissileType::Demigod3Radial:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 0, 96, 128, 16, EulerAngles::Identity, 10, true, false, false, (int)ShockwaveStyle::Normal);
				BubblesShatterFunction(fx, 0, -32);
				break;

			case MissileType::Demigod2:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 128, 64, 0, 16, EulerAngles::Identity, 5, true, false, false, (int)ShockwaveStyle::Normal);
				BubblesShatterFunction(fx, 0, -32);
				break;

			case MissileType::ClawMutantPlasma:
				DoDamage(LaraItem, fxInfo.Flag2);
				for (int i = 0; i < 3; i++)
				{
					SpawnClawMutantPlasmaFlameBall(fxNumber, Vector3(20.0f, 32.0f, 20.0f), Vector3(Random::GenerateFloat(-115.0f, 185.0f), 0, Random::GenerateFloat(-115.0f, 185.0f)), 24);
					SpawnClawMutantPlasmaFlameBall(fxNumber, Vector3(20.0f, 32.0f, 20.0f), Vector3(Random::GenerateFloat(-115.0f, 185.0f), 0, Random::GenerateFloat(-115.0f, 185.0f)), 24);
					SpawnClawMutantPlasmaFlameBall(fxNumber, Vector3(20.0f, 32.0f, 20.0f), Vector3(Random::GenerateFloat(-115.0f, 185.0f), 0, Random::GenerateFloat(-115.0f, 185.0f)), 24);
				}

				break;

			case MissileType::Harpy:
				TriggerShockwave(&fx.Pose, 32, 160, 64, 128, 128, 0, 16, EulerAngles::Identity, 3, true, false, false, (int)ShockwaveStyle::Normal);
				BubblesShatterFunction(fx, 0, -32);
				break;

			case MissileType::CrocgodMutant:
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 0, fx.RoomNumber);
				TriggerShockwave(&fx.Pose, 48, 240, 64, 128, 96, 0, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				
				fx.Pose.Position.y -= 128;
				TriggerShockwave(&fx.Pose, 48, 240, 48, 128, 112, 0, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				
				fx.Pose.Position.y += 256;
				TriggerShockwave(&fx.Pose, 48, 240, 48, 128, 112, 0, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				ItemBurn(LaraItem);
				break;

			case MissileType::WillardPlasmaBall:
				DoDamage(LaraItem, 100);
				TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 2, fx.roomNumber);
				TriggerShockwave(&fx.pos, 48, 240, 64, 0, 128, 64, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
				ItemCustomBurn(LaraItem, Vector3(0.0f, 0.8f, 0.1f), Vector3(0.0f, 0.9f, 0.8f));
				break;

			default:
				TriggerShockwave(&fx.Pose, 24, 88, 48, 0, 128, 64, 16, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
				break;
			}

			KillItem(fxNumber);
		}
		else
		{
			if (pointColl.GetRoomNumber() != fx.RoomNumber)
				ItemNewRoom(fxNumber, pointColl.GetRoomNumber());

			auto deltaPos = prevPos - fx.Pose.Position;

			if (Wibble & 4)
			{
				switch ((MissileType)fxInfo.Flag1)
				{
				default:
				case MissileType::SethLarge:
					TriggerSethMissileFlame(fxNumber, deltaPos.x * 32, deltaPos.y * 32, deltaPos.z * 32);
					break;

				case MissileType::Harpy:
					TriggerHarpyFlameFlame(fxNumber, deltaPos.x * 16, deltaPos.y * 16, deltaPos.z * 16);
					break;

				case MissileType::Demigod3Single:
				case MissileType::Demigod3Radial:
				case MissileType::Demigod2:
					TriggerDemigodMissileFlame(fxNumber, deltaPos.x * 16, deltaPos.y * 16, deltaPos.z * 16);
					break;

				case MissileType::ClawMutantPlasma:
					for (int i = 0; i < 3; i++)
						SpawnClawMutantPlasmaFlameBall(fxNumber, Vector3(deltaPos.x, deltaPos.y * 16, deltaPos.z), Vector3::Zero, 10.0f);

					break;

				case MissileType::WillardPlasmaBall:
					TriggerWillardPlasmaBallFlame(fxNumber, deltaPos.x * 16, deltaPos.y * 16, deltaPos.z * 16);
					break;


				case MissileType::CrocgodMutant:
					TriggerCrocgodMissileFlame(fxNumber, deltaPos.x * 16, deltaPos.y * 16, deltaPos.z * 16);
					break;
				}
			}
		}

		if (fxInfo.Flag1 == (int)MissileType::ClawMutantPlasma || fxInfo.Flag1 == (int)MissileType::WillardPlasmaBall)
			SpawnDynamicLight(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, 8, 0, 64, 128);
	}

	void TriggerWillardPlasmaBallFlame(short fxNumber, short xVel, short yVel, short zVel)
	{
		const auto& fx = EffectList[fxNumber];
		auto& flame = *GetFreeParticle();

		flame.on = true;
		flame.sR = 48;
		flame.sG = 255;
		flame.sB = 48 + (GetRandomControl() & 31);
		flame.dR = 32;
		flame.dG = 192 + (GetRandomControl() & 63);
		flame.dB = 128 + (GetRandomControl() & 63);
		flame.fadeToBlack = 8;
		flame.colFadeSpeed = (GetRandomControl() & 3) + 12;
		flame.blendMode = BlendMode::Additive;
		flame.life = flame.sLife = (GetRandomControl() & 7) + 24;
		flame.y = 0;
		flame.x = (GetRandomControl() & 0xF) - 8;
		flame.xVel = xVel;
		flame.yVel = yVel;
		flame.z = (GetRandomControl() & 0xF) - 8;
		flame.zVel = zVel;
		flame.friction = 68;
		flame.flags = 602;
		flame.rotAng = GetRandomControl() & 0xFFF;

		if (GetRandomControl() & 1)
			flame.rotAdd = -32 - (GetRandomControl() & 0x1F);
		else
			flame.rotAdd = (GetRandomControl() & 0x1F) + 32;

		flame.gravity = 0;
		flame.maxYvel = 0;
		flame.fxObj = fxNumber;
		flame.scalar = 2;
		flame.sSize = flame.size = (GetRandomControl() & 7) + 64;
		flame.dSize = flame.size / 32;
	}
}