#include "framework.h"
#include "Objects/TR4/Object/tr4_senet.h"

#include "Sound/sound.h"
#include "Game/items.h"
#include "Game/control/control.h"
#include "Game/Setup.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_struct.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"

using namespace TEN::Collision::Point;
using namespace TEN::Input;

int SenetPiecesNumber[6];
char SenetDisplacement, ActiveSenetPieces[6], SenetBoard[17];
int SenetTargetX, SenetTargetZ;
char ActivePiece = -1;

const auto GameStixPosition = Vector3i(0, 0, -100);
const ObjectCollisionBounds GameStixBounds =
{
	GameBoundingBox(
		-CLICK(1), CLICK(1),
		-200, 200,
		-CLICK(1), CLICK(1)
	),
	std::pair(
		EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), 0),
		EulerAngles(ANGLE(10.0f), ANGLE(30.0f), 0)
	)
};

void InitializeGameSticks(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	item->ItemFlags[7] = -1;
	//not needed
	//item->data = &item->itemFlags;
	ActivePiece = -1;
	SenetDisplacement = 0;
}

void GameSticksControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	int number, x, z;
	bool flag;
	short piece, probedRoomNumber;

	if (item->ItemFlags[7] > -1)
	{
		if (item->HitPoints == 100)
			SoundEffect(SFX_TR4_SENET_PUZZLE_SPIN, &item->Pose);
		for (int i = 0; i < 4; ++i)
		{
			if (item->HitPoints < 100 - 2 * i)
			{
				item->ItemFlags[i] -= 128 * item->HitPoints;

				if (item->HitPoints < (40 - 2 * i))
				{
					if (abs(item->ItemFlags[i]) < 4096 && item->ItemFlags[7] & 1 << i)
						item->ItemFlags[i] = 0;
					else if ((item->ItemFlags[i] > 28672 || item->ItemFlags[i] < -28672) && !(item->ItemFlags[7] & 1 << i))
						item->ItemFlags[i] = -32768;
				}
			}
		}

		--item->HitPoints;
		if (!item->HitPoints)
		{
			for (int i = 0; i < 3; ++i)
				g_Level.Items[SenetPiecesNumber[i]].TriggerFlags = 0;
			
			item->ItemFlags[7] = -1;

			if (ActivePiece == -1 && !SenetDisplacement)
			{
				RemoveActiveItem(itemNumber);
				item->Status = ITEM_NOT_ACTIVE;
			}
		}
	}
	else if (ActivePiece > -1)
	{
		number = ActivePiece >= 3 ? 2 : 1;
		auto* item2 = &g_Level.Items[SenetPiecesNumber[ActivePiece]];
		item2->Flags |= 32;
		item2->AfterDeath = 48;

		piece = ActiveSenetPieces[ActivePiece];
		if (piece == -1 || piece >= 5)
		{
			if (piece == -1)
				piece = 16;

			x = SenetTargetX + BLOCK(1);
			z = SenetTargetZ + BLOCK(piece - 5);
		}
		else
		{
			x = SenetTargetX + BLOCK(2 * number - 2);
			z = SenetTargetZ + BLOCK(4 - piece);
		}

		if (abs(x - item2->Pose.Position.x) < 128)
			item2->Pose.Position.x = x;
		else if (x > item2->Pose.Position.x)
			item2->Pose.Position.x += 128;
		else
			item2->Pose.Position.x -= 128;
		if (abs(z - item2->Pose.Position.z) < 128)
			item2->Pose.Position.z = z;
		else if (z > item2->Pose.Position.z)
			item2->Pose.Position.z += 128;
		else
			item2->Pose.Position.z -= 128;

		probedRoomNumber = GetPointCollision(Vector3i(item2->Pose.Position.x, item2->Pose.Position.y - 32, item2->Pose.Position.z), item2->RoomNumber).GetRoomNumber();
		if (item2->RoomNumber != probedRoomNumber)
			ItemNewRoom(SenetPiecesNumber[ActivePiece], probedRoomNumber);
		
		if (x == item2->Pose.Position.x &&
			z == item2->Pose.Position.z)
		{
			item2->AfterDeath = 0;

			if (piece == 16)
			{
				if (number == 1)
				{
					SenetPieceExplosionEffect(item2, 0x6060E0, -32);
					SenetPieceExplosionEffect(item2, 0x6060E0, 48);
				}
				else
				{
					SenetPieceExplosionEffect(item2, 0xFF8020, -32);
					SenetPieceExplosionEffect(item2, 0xFF8020, 48);
				}

				KillItem(SenetPiecesNumber[ActivePiece]);

				if (CheckSenetWinner(number))
				{
					for (int i = 0; i < g_Level.NumItems; ++i)
					{
						auto* item3 = &g_Level.Items[i];
						if (item3->ObjectNumber >= ID_GAME_PIECE1 && item3->ObjectNumber <= ID_GAME_PIECE3 ||
							item3->ObjectNumber == ID_ENEMY_PIECE ||
							item3->ObjectNumber == ID_WHEEL_OF_FORTUNE)
						{
							item3->Flags |= IFLAG_INVISIBLE | IFLAG_ACTIVATION_MASK;
							RemoveActiveItem(i);
							item3->Status = ITEM_NOT_ACTIVE;
							item3->AfterDeath = 1;
						}
					}
				}
			}
			else
			{
				for (int i = 0; i < 6; ++i)
				{
					if (ActivePiece != i)
					{
						item2 = &g_Level.Items[SenetPiecesNumber[i]];

						if (x == item2->Pose.Position.x &&
							z == item2->Pose.Position.z)
						{
							SenetPieceExplosionEffect(item2, number == 1 ? 0xFF8020 : 0x6060E0, -64);
							item2->Pose.Position.x = SenetTargetX - BLOCK(4 * number) + BLOCK(7);
							item2->Pose.Position.z = SenetTargetZ + BLOCK(i % 3);
							
							probedRoomNumber = GetPointCollision(Vector3i(item2->Pose.Position.x, item2->Pose.Position.y - 32, item2->Pose.Position.z), item2->RoomNumber).GetRoomNumber();
							if (item2->RoomNumber != probedRoomNumber)
								ItemNewRoom(SenetPiecesNumber[i], probedRoomNumber);
							
							SenetPieceExplosionEffect(item2, number == 1 ? 0xFF8020 : 0x6060E0, -64);
						}
					}
				}
			}

			if (!SenetDisplacement)
			{
				RemoveActiveItem(itemNumber);
				item2->Status = ITEM_NOT_ACTIVE;
			}

			ActivePiece = -1;
		}
	}
	else if (SenetDisplacement == -1)
	{
		_0x0040FAE0(item);
		flag = false;

		if (item->TriggerFlags)
		{
			for (int i = 3; i < 6; ++i)
			{
				if (ActiveSenetPieces[i] != -1 && SenetDisplacement &&
					(ActiveSenetPieces[i] + SenetDisplacement) == 16)
				{
					MakeMove(i, SenetDisplacement);
					if (SenetDisplacement == -1 || !SenetDisplacement)
					{
						flag = true;
						break;
					}
				}

			}

			if (!flag)
			{
				for (int i = 3; i < 6; ++i)
				{
					if (ActiveSenetPieces[i] != -1 && SenetDisplacement &&
						(ActiveSenetPieces[i] + SenetDisplacement) >= 5 &&
						(ActiveSenetPieces[i] + SenetDisplacement) <= 16 &&
						SenetBoard[ActiveSenetPieces[i] + SenetDisplacement] & 1)
					{
						MakeMove(i, SenetDisplacement);
						if (SenetDisplacement == -1 || !SenetDisplacement)
						{
							flag = true;
							break;
						}
					}

				}
			}

			if (!flag && SenetDisplacement != 6)
			{
				for (int i = 3; i < 6; ++i)
				{
					if (ActiveSenetPieces[i] != -1 && SenetDisplacement &&
						ActiveSenetPieces[i] + SenetDisplacement <= 16 && !(ActiveSenetPieces[i] + SenetDisplacement & 3))
					{
						MakeMove(i, SenetDisplacement);
						if (SenetDisplacement == -1 || !SenetDisplacement)
						{
							flag = true;
							break;
						}
					}

				}
			}
		}
		if (!flag)
		{
			for (int i = 3; i < 6; ++i)
			{
				MakeMove(i, SenetDisplacement);
				if (SenetDisplacement == -1 || !SenetDisplacement)
					break;
			}
		}

		if (SenetDisplacement && SenetDisplacement != 6)
			SenetDisplacement = 0;
		else
			SenetDisplacement = -1;
	}
	else if (!SenetDisplacement)
	{
		_0x0040FAE0(item);
		flag = false;

		for (int i = 0; i < 3; ++i)
		{
			if (ActiveSenetPieces[i] != -1 && SenetDisplacement &&
				(ActiveSenetPieces[i] + SenetDisplacement) < 17 &&
				!(SenetBoard[ActiveSenetPieces[i] + SenetDisplacement] & 1))
				flag = true;
		}

		if (!flag)
			SenetDisplacement = SenetDisplacement == 6 ? 0 : -1;
	}
}

void _0x0040FAE0(ItemInfo* item)
{
	SenetDisplacement = 0;
	item->ItemFlags[7] = 0;

	for (int i = 0; i < 4; i++)
	{
		int rnd = GetRandomControl() & 1;
		SenetDisplacement += rnd;

		if (rnd)
			item->ItemFlags[7] |= (1 << i);
	}

	if (!SenetDisplacement)
		SenetDisplacement = 6;

	item->HitPoints = 120;

	for (int i = 0; i < 3; i++)
		g_Level.Items[SenetPiecesNumber[i]].TriggerFlags = 1;
}

void SenetPieceExplosionEffect(ItemInfo* item, int color, int speed)
{
	int radius = speed >= 0 ? 0xA00020 : 0x2000280;
	int clr = color | 0x18000000;
	item->Pose.Position.y -= STEPUP_HEIGHT;
	TriggerShockwave(&item->Pose, radius & 0xFFFF, radius >> 16, speed, clr & 0xFF, (clr >> 8) & 0xFF, (clr >> 16) & 0xFF, 64, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
	TriggerShockwave(&item->Pose, radius & 0xFFFF, radius >> 16, speed, clr & 0xFF, (clr >> 8) & 0xFF, (clr >> 16) & 0xFF, 64, EulerAngles(0x2000, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
	TriggerShockwave(&item->Pose, radius & 0xFFFF, radius >> 16, speed, clr & 0xFF, (clr >> 8) & 0xFF, (clr >> 16) & 0xFF, 64, EulerAngles(0x4000, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
	TriggerShockwave(&item->Pose, radius & 0xFFFF, radius >> 16, speed, clr & 0xFF, (clr >> 8) & 0xFF, (clr >> 16) & 0xFF, 64, EulerAngles(0x6000, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
	item->Pose.Position.y += STEPUP_HEIGHT;
}

void TriggerItemInRoom(short room_number, int object)//originally this is in deltapak
{
	short num = g_Level.Rooms[room_number].itemNumber;
	while (num != NO_VALUE)
	{
		auto* item = &g_Level.Items[num];
		short nex = item->NextItem;

		if (item->ObjectNumber == object)
		{
			AddActiveItem(num);
			item->Status = ITEM_ACTIVE;
			item->Flags |= IFLAG_ACTIVATION_MASK;
		}

		num = nex;
	}
}

bool CheckSenetWinner(short num)//original TR4 numbers :>
{
	if (num == 1)
	{
		int i = 0;
		while (ActiveSenetPieces[i] == -1)
		{
			if (++i >= 3)
			{
				TriggerItemInRoom(0, ID_RAISING_BLOCK2);
				TriggerItemInRoom(19, ID_RAISING_BLOCK2);
				return true;
			}
		}
	}
	else
	{
		int j = 3;
		while (ActiveSenetPieces[j] == -1)
		{
			if (++j >= 6)
			{
				TriggerItemInRoom(20, ID_TRAPDOOR1);
				TriggerItemInRoom(21, ID_TRAPDOOR1);
				TriggerItemInRoom(22, ID_TRAPDOOR1);
				TriggerItemInRoom(81, ID_TRAPDOOR1);
				return true;
			}
		}
	}

	return false;
}

void MakeMove(int piece, int displacement)
{
	int number, i;

	number = piece >= 3 ? 2 : 1;
	if (ActiveSenetPieces[piece] != -1 && displacement &&
		(ActiveSenetPieces[piece] + displacement) <= 16 &&
		!(SenetBoard[ActiveSenetPieces[piece] + displacement] & number))
	{
		SenetBoard[ActiveSenetPieces[piece]] &= ~number;
		if (!ActiveSenetPieces[piece])
		{
			for (i = 3 * number - 3; i < 3 * number; ++i)
			{
				if (i != piece && !ActiveSenetPieces[i])
					SenetBoard[0] |= number;
			}
		}

		ActivePiece = piece;

		ActiveSenetPieces[ActivePiece] += displacement;
		if (ActiveSenetPieces[ActivePiece] > 4)
		{
			SenetBoard[ActiveSenetPieces[ActivePiece]] = 0;
			for (i = 6 - 3 * number; i < 9 - 3 * number; ++i)
			{
				if (ActiveSenetPieces[i] == ActiveSenetPieces[ActivePiece])
				{
					ActiveSenetPieces[i] = 0;
					SenetBoard[0] |= 3 - number;
				}
			}
		}

		if (!(ActiveSenetPieces[ActivePiece] & 3) || SenetDisplacement == 6)
			SenetDisplacement = 0;
		else
			SenetDisplacement = -1;

		if (ActiveSenetPieces[ActivePiece] < 16)
			SenetBoard[ActiveSenetPieces[ActivePiece]] |= number;
		else
			ActiveSenetPieces[ActivePiece] = -1;
	}
}

void GameSticksCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	ItemInfo* item = &g_Level.Items[itemNumber];

	if (IsHeld(In::Action) &&
		laraItem->Animation.ActiveState == LS_IDLE &&
		laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
		Lara.Control.HandStatus == HandStatus::Free &&
		!item->Active || Lara.Control.IsMoving && Lara.Context.InteractedItem == itemNumber)
	{
		laraItem->Pose.Orientation.y ^= 0x8000;

		if (TestLaraPosition(GameStixBounds, item, laraItem))
		{
			if (MoveLaraPosition(GameStixPosition, item, laraItem))
			{
				laraItem->Animation.AnimNumber = LA_SENET_ROLL;
				laraItem->Animation.FrameNumber = 0;
				laraItem->Animation.ActiveState = LS_MISC_CONTROL;
				Lara.Control.IsMoving = false;
				Lara.ExtraTorsoRot = { 0, 0, 0 };
				Lara.Control.HandStatus = HandStatus::Busy;
				item->Status = ITEM_ACTIVE;
				AddActiveItem(itemNumber);
				laraItem->Pose.Orientation.y ^= 0x8000;
				return;
			}

			Lara.Context.InteractedItem = itemNumber;
		}

		laraItem->Pose.Orientation.y ^= 0x8000;
	}
	else
		ObjectCollision(itemNumber, laraItem, coll);
}

void ControlGodHead(short itemNumber)
{
	ItemInfo* item = &g_Level.Items[itemNumber];

	if (TriggerActive(item))
	{
		if (item->Pose.Orientation.y == 0)
			item->Pose.Position.z &= ~1023;
		else if (item->Pose.Orientation.y == 0x4000)
			item->Pose.Position.x &= ~1023;
		else if (item->Pose.Orientation.y == -0x4000)
			item->Pose.Position.x |= 1023;
		else if (item->Pose.Orientation.y == -0x8000)
			item->Pose.Position.z |= 1023;

		if (item->ItemFlags[0])
		{
			if (item->ItemFlags[2])
				item->ItemFlags[2]--;
			else
			{
				if (item->ItemFlags[1] < 128)
					KillItem(itemNumber);
				else
					item->ItemFlags[1] -= 128;
			}
		}
		else
		{
			if (item->ItemFlags[1] > 0x1000)
			{
				item->ItemFlags[0] = 1;
				item->ItemFlags[1] = 4096;
				item->ItemFlags[2] = 210;
			}
			else
				item->ItemFlags[1] += 128;
		}
	}
}

void InitializeGamePiece(short itemNumber)
{
	if (!SenetPiecesNumber[0])
	{
		int i;
		for (i = 1; i < 17; ++i)
			SenetBoard[i] = 0;
		for (i = 0; i < 6; ++i)
			ActiveSenetPieces[i] = 0;
		SenetBoard[0] = 3;
		for (i = 0; i < g_Level.NumItems; ++i)
		{
			auto* item = &g_Level.Items[i];
			if (item->ObjectNumber == ID_GAME_PIECE1)
			{
				SenetPiecesNumber[0] = i;
				SenetTargetX = item->Pose.Position.x + 1024;
				SenetTargetZ = item->Pose.Position.z;
			}
			else if (item->ObjectNumber == ID_GAME_PIECE2)
				SenetPiecesNumber[1] = i;
			else if (item->ObjectNumber == ID_GAME_PIECE3)
				SenetPiecesNumber[2] = i;
			else if (item->ObjectNumber == ID_ENEMY_PIECE)
				SenetPiecesNumber[3 + item->TriggerFlags] = i;
		}
	}
}

void SenetControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (SenetDisplacement > 0 && item->TriggerFlags != 1)
		MakeMove(item->ObjectNumber - ID_GAME_PIECE1, SenetDisplacement);

	RemoveActiveItem(itemNumber);
}
