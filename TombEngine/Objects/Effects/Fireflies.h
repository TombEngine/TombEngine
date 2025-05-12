#pragma once
#include "Game/items.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Effects::Fireflies
{
	enum FirefliesItemFlags
	{
		TargetItemPtr,
		Light,
		TriggerFlags,
		Spawncounter,
		RemoveFliesEffect,
		LightIndex1,
		LightIndex2
	};

	struct FireflyData
	{
		int SpriteSeqID = ID_DEFAULT_SPRITES;
		int SpriteID = SPR_UNDERWATERDUST;
		BlendMode blendMode;
		unsigned int scalar;

		Vector3		Position	   = Vector3::Zero;
		int			RoomNumber	   = 0;
		Vector3		PositionTarget = Vector3::Zero;
		EulerAngles Orientation	   = EulerAngles::Identity;
		float		Velocity	   = 0.0f;

		ItemInfo* TargetItemPtr = nullptr;

		float zVel;
		float Life = 0.0f;
		int Number = 0;

		unsigned char rB;
		unsigned char gB;
		unsigned char bB;
		unsigned char r;
		unsigned char g;
		unsigned char b;

		bool on;
		float size;
		short rotAng;

		int PrevX;
		int PrevY;
		int PrevZ;
		byte PrevR;
		byte PrevG;
		byte PrevB;

		void StoreInterpolationData()
		{
			PrevX = Position.x;
			PrevY = Position.y;
			PrevZ = Position.z;
			PrevR = r;
			PrevG = g;
			PrevB = b;
		}
	};

	extern std::vector<FireflyData> FireflySwarm;

	void InitializeFireflySwarm(short itemNumber);
	void ControlFireflySwarm(short itemNumber);
	void UpdateFireflySwarm();
	void ClearFireflySwarm();
	void SpawnFireflySwarm(ItemInfo& item, int triggerFlags);
	void RemoveFireflies(ItemInfo& item);
}
