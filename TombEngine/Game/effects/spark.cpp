#pragma once
#include "framework.h"
#include "Game/effects/spark.h"

#include <array>
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Math/Random.h"

using namespace DirectX::SimpleMath;
using namespace TEN::Math::Random;

namespace TEN::Effects::Spark
{
	std::array<SparkParticle, 128> SparkParticles;

	void UpdateSparkParticles()
	{
		for (int i = 0; i < SparkParticles.size(); i++)
		{
			auto& s = SparkParticles[i];

			if (!s.active)
				continue;
			
			s.StoreInterpolationData();

			s.age += 1;
			if (s.age > s.life)
			{
				s.active = false;
				continue;
			}

			s.velocity.y += s.gravity;
			s.velocity *= s.friction;
			s.pos += s.velocity;
		}
	}

	void DisableSparkParticles()
	{
		for (int i = 0; i < SparkParticles.size(); i++)
			SparkParticles[i].active = false;
	}

	SparkParticle& GetFreeSparkParticle()
	{
		for (int i = 0; i < SparkParticles.size(); i++)
		{
			if (!SparkParticles[i].active)
				return SparkParticles[i];
		}

		return SparkParticles[0];
	}

	void TriggerFlareSparkParticles(const Vector3i& pos, const Vector3i& vel, const Color& color, int roomNumber)
	{
		auto& s = GetFreeSparkParticle();
		s = {};
		s.age = 0;
		s.life = GenerateFloat(10, 20);
		s.friction = 0.98f;
		s.gravity = 1.2f;
		s.width = 8.0f;
		s.height = 48.0f;
		s.room = roomNumber;
		s.pos = pos.ToVector3();
		auto v = vel.ToVector3();
		v += Vector3(GenerateFloat(-64, 64), GenerateFloat(-64, 64), GenerateFloat(-64, 64));
		v.Normalize(v);
		s.velocity = v *GenerateFloat(17,24);
		s.sourceColor = Color(1.0f, 1.0f, 1.0f);
		s.destinationColor = color;
		s.active = true;
	}

	void TriggerRicochetSpark(const GameVector& pos, short angle, int count, const Vector4& colorStart)
	{
		for (int i = 0; i < count; i++) 
		{
			auto& s = GetFreeSparkParticle();
			s = {};
			s.age = 0;
			s.life = GenerateFloat(10, 20);
			s.friction = 0.98f;
			s.gravity = 1.2f;
			s.width = 8.0f;
			s.height = 64.0f;
			s.room = pos.RoomNumber;
			s.pos = pos.ToVector3();
			float ang = TO_RAD(angle);
			Vector3 v = Vector3(sin(ang + GenerateFloat(-PI_DIV_2, PI_DIV_2)), GenerateFloat(-1, 1), cos(ang + GenerateFloat(-PI_DIV_2, PI_DIV_2)));
			v += Vector3(GenerateFloat(-64, 64), GenerateFloat(-64, 64), GenerateFloat(-64, 64));
			v.Normalize(v);
			s.velocity = v * GenerateFloat(17, 24);
			s.sourceColor = colorStart;
			s.destinationColor = Vector4::Zero;
			s.active = true;
		}

		auto& sptr = *GetFreeParticle();

		sptr = {};
		sptr.on = true;
		sptr.dynamic = NO_VALUE;
		sptr.sR = colorStart.x * 0.33f * UCHAR_MAX;
		sptr.sG = colorStart.y * 0.33f * UCHAR_MAX;
		sptr.sB = colorStart.z * 0.33f * UCHAR_MAX;
		sptr.dR = 0;
		sptr.dG = 0;
		sptr.dB = 0;
		sptr.colFadeSpeed = 4;
		sptr.fadeToBlack = 0;
		sptr.life = 4;
		sptr.sLife = 4;
		sptr.blendMode = BlendMode::Additive;
		sptr.x = pos.x;
		sptr.y = pos.y;
		sptr.z = pos.z;
		sptr.xVel = 0;
		sptr.yVel = 0;
		sptr.zVel = 0;
		sptr.flags = SP_SCALE | SP_DEF | SP_ROTATE;
		sptr.rotAng = GenerateAngle(0, 4095);

		if (TestProbability(1 / 2.0f))
			sptr.rotAdd = 64 - GenerateAngle(0, 63);
		else
			sptr.rotAdd = GenerateAngle(0, 63) + 64;

		sptr.scalar = 3;
		sptr.SpriteSeqID = ID_DEFAULT_SPRITES;
		sptr.SpriteID = SPR_BULLETIMPACT;
		sptr.size = GenerateInt(6, 12);
		sptr.sSize = sptr.size;
		sptr.dSize = 1;
		sptr.maxYvel = 0;
		sptr.gravity = 0;
	}

	void TriggerFrictionSpark(const GameVector& pos, const EulerAngles& angle, float length, int count)
	{
		for (int i = 0; i < count; i++)
		{
			auto& s = GetFreeSparkParticle();
			s = {};
			s.age = 0;
			s.life = GenerateFloat(8, 15);
			s.friction = 0.1f;
			s.gravity = 0.0f;
			s.height = length;
			s.width = GenerateFloat(16.0f, 32.0f);
			s.room = pos.RoomNumber;
			s.pos = Vector3(pos.x + GenerateFloat(-16, 16), pos.y + GenerateFloat(-16, 16), pos.z + GenerateFloat(-16, 16));
			float ang = TO_RAD(angle.y);
			float vAng = -TO_RAD(angle.x);
			Vector3 v = Vector3(sin(ang), vAng + GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * GenerateFloat(32, 64);
			s.sourceColor = Vector4(1, 0.7f, 0.4f, 1);
			s.destinationColor = Vector4(0.4f, 0.1f, 0, 0.5f);
			s.active = true;
		}
	}

	void TriggerElectricSpark(const GameVector& pos, const EulerAngles& angle, int count)
	{
		for (int i = 0; i < count; i++)
		{
			auto& s = GetFreeSparkParticle();
			s = {};
			s.age = 0;
			s.life = GenerateFloat(8, 15);
			s.friction = 1.0f;
			s.gravity = 2.0f;
			s.height = GenerateFloat(64.0f, 256.0f);
			s.width = GenerateFloat(8.0f, 16.0f);
			s.room = pos.RoomNumber;
			s.pos = Vector3(pos.x + GenerateFloat(-16, 16), pos.y + GenerateFloat(-16, 16), pos.z + GenerateFloat(-16, 16));
			float ang = TO_RAD(angle.y);
			float vAng = -TO_RAD(angle.x);
			Vector3 v = Vector3(sin(ang), vAng + GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * GenerateFloat(8, 32);
			s.sourceColor = Vector4(0.4f, 0.6f, 1.0f, 1);
			s.destinationColor = Vector4(0.6f, 0.6f, 0.8f, 0.8f);
			s.active = true;
		}
	}

	void TriggerAttackSpark(const Vector3& basePos, const Vector3& color)
	{
		auto& spark = *GetFreeParticle();

		auto sphere = BoundingSphere(basePos, BLOCK(1));
		auto pos = Random::GeneratePointInSphere(sphere);
		auto vel = (basePos - pos) * 8;

		spark.on = true;
		spark.sR = 0;
		spark.sG = 0;
		spark.sB = 0;
		spark.dR = color.x;
		spark.dG = color.y;
		spark.dB = color.z;
		spark.life = 16;
		spark.sLife = 16;
		spark.colFadeSpeed = 4;
		spark.blendMode = BlendMode::Additive;
		spark.fadeToBlack = 4;
		spark.x = (int)round(pos.x);
		spark.y = (int)round(pos.y);
		spark.z = (int)round(pos.z);
		spark.xVel = (int)round(vel.x);
		spark.yVel = (int)round(vel.y);
		spark.zVel = (int)round(vel.z);
		spark.friction = 34;
		spark.maxYvel = 0;
		spark.gravity = 0;
		spark.scalar = 2;
		spark.dSize =
		spark.sSize =
		spark.size = Random::GenerateInt(44, 48);
		spark.flags = SP_NONE;
	}

	void SpawnCyborgSpark(const Vector3& pos)
	{
		auto& spark = *GetFreeParticle();
		
		int velSign = -1;
		int randomInt = Random::GenerateInt();

		spark.sR = -1;
		spark.sG = -1;
		spark.sB = -1;
		spark.dR = -1;
		spark.dG = (randomInt & 167) + 64;
		spark.dB = 192 - spark.dG;
		spark.on = 1;
		spark.colFadeSpeed = 3;
		spark.fadeToBlack = 5;
		spark.life = 10;
		spark.sLife = 10;
		spark.blendMode = BlendMode::Additive;
		spark.friction = 34;
		spark.scalar = 2;
		spark.x = (randomInt & 7) + pos.x - 3;
		spark.y = ((randomInt >> 3) & 7) + pos.y - 3;
		spark.z = ((randomInt >> 6) & 7) + pos.z - 3;
		spark.xVel = (int)(((randomInt >> 2) & 0xFF) + velSign - 128);
		spark.yVel = (int)(((randomInt >> 4) & 0xFF) + velSign - 128);
		spark.zVel = (int)(((randomInt >> 6) & 0xFF) + velSign - 128);
		spark.flags = SP_SCALE;
		spark.size = ((randomInt >> 9) & 3) + 4;
		spark.sSize = ((randomInt >> 9) & 3) + 4;
		spark.dSize = ((randomInt >> 12) & 1) + 1;
		spark.maxYvel = 0;
		spark.gravity = 0;
	}
}
