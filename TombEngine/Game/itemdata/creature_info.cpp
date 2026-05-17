#include "framework.h"
#include "Game/itemdata/creature_info.h"

#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"

void CreatureInfo::EnemyHandler::Initialize(const CreatureInfo* creature)
{
	_creature = creature;
}

ItemInfo* CreatureInfo::EnemyHandler::Get() const
{
	return (ItemInfo*)(*this);
}

CreatureInfo::EnemyHandler& CreatureInfo::EnemyHandler::operator=(ItemInfo* item)
{
	_enemy = item;
	return *this;
}

CreatureInfo::EnemyHandler::operator ItemInfo*() const
{
	if (_creature && _creature->Friendly && !_creature->HurtByLara && _enemy == LaraItem)
		return nullptr;

	return _enemy;
}

ItemInfo* CreatureInfo::EnemyHandler::operator->() const
{
	return Get();
}

CreatureInfo::EnemyHandler::operator bool() const
{
	return Get() != nullptr;
}

bool CreatureInfo::EnemyHandler::IsLara() const
{
	auto* ptr = Get();
	return ptr != nullptr && ptr->IsLara();
}