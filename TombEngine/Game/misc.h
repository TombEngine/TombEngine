#pragma once
#include "Game/control/box.h"

#define CHK_ANY(var, flag) (var & flag) != 0
#define CHK_EXI(var, flag) var & flag
#define CHK_NOP(var, flag) !(var & flag)

enum LaraMeshMask
{
	LARA_ONLY_LEGS			  = 0,
	LARA_ONLY_ARMS			  = (1 << 1),
	LARA_ONLY_TORSO			  = (1 << 2),
	LARA_ONLY_HEAD			  = (1 << 3),
	LARA_ONLY_LEFT_ARM		  = (1 << 4),
	LARA_ONLY_RIGHT_ARM		  = (1 << 5),
	LARA_LEGS_TORSO_HEAD	  = (1 << 6),
	LARA_LEGS_TORSO_HEAD_ARMS = (1 << 7)
};

CreatureInfo* GetCreatureInfo(ItemInfo* item);
bool IsNextSectorValid(const ItemInfo& item, const Vector3& dir, float dist, bool canFloat);
