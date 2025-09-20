#pragma once
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <variant>

#include "Game/collision/collide_room.h"
#include "Game/itemdata/creature_info.h"
#include "Game/itemdata/door_data.h"
#include "Game/Lara/lara_struct.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Objects/Generic/Object/Pushable/PushableInfo.h"
#include "Objects/TR2/Vehicles/skidoo_info.h"
#include "Objects/TR2/Vehicles/speedboat_info.h"
#include "Objects/TR3/Vehicles/big_gun_info.h"
#include "Objects/TR3/Vehicles/kayak_info.h"
#include "Objects/TR3/Vehicles/minecart_info.h"
#include "Objects/TR3/Vehicles/quad_bike_info.h"
#include "Objects/TR3/Vehicles/rubber_boat_info.h"
#include "Objects/TR3/Vehicles/upv_info.h"
#include "Objects/TR4/Entity/WraithInfo.h"
#include "Objects/TR4/Vehicles/jeep_info.h"
#include "Objects/TR4/Vehicles/motorbike_info.h"
#include "Objects/TR5/Entity/tr5_laserhead_info.h"
#include "Objects/TR5/Light/tr5_light_info.h"

template<class... Ts> struct visitor : Ts... { using Ts::operator()...; };
template<class... Ts> visitor(Ts...)->visitor<Ts...>; // TODO: Line not needed in C++20.

using namespace TEN::Entities::Creatures::TR5;
using namespace TEN::Entities::Generic;
using namespace TEN::Entities::TR4;
using namespace TEN::Entities::Vehicles;

struct ItemInfo;

class ItemData
{
	std::variant<
		std::nullptr_t,
		char,
		short,
		int,
		long,
		long long,
		unsigned char,
		unsigned short,
		unsigned int,
		unsigned long,
		unsigned long long,
		float,
		double,
		long double,
		std::array<short, 4>,
		GameVector,
		DOOR_DATA,
		PushableInfo,
		ItemInfo*,
		LaraInfo*,
		CollisionInfo,
		CreatureInfo,
		WraithInfo,
		GuardianInfo,
		QuadBikeInfo,
		BigGunInfo,
		MotorbikeInfo,
		JeepInfo,
		KayakInfo,
		SkidooInfo,
		UPVInfo,
		SpeedboatInfo,
		RubberBoatInfo,
		MinecartInfo,
		ElectricalLightInfo,
		BridgeObject
	> data;
	public:
	ItemData();

	template<typename D>
	ItemData(D&& type) : data(std::move(type)) {}

	// Conversion operators to keep original syntax.
	// TODO: Should be removed later and use polymorphism instead.
	template<typename T>
	operator T* ()
	{
		if (std::holds_alternative<T>(data))
		{
			auto& ref = std::get<T>(data);
			return &ref;
		}

		throw std::runtime_error("Attempted to read ItemData as wrong type.");
	}

	template<typename T>
	operator T& ()
	{
		if (std::holds_alternative<T>(data))
		{
			auto& ref = std::get<T>(data);
			return ref;
		}

		throw std::runtime_error("Attempted to read ItemData as wrong type.");
	}

	/* Uncommented, we want to store pointers to global data too (LaraInfo for example).
	template<typename T>
	ItemData& operator =(T* newData)
	{
		data = *newData;
		return *this;
	}
	*/

	ItemData& operator =(std::nullptr_t null)
	{
		data = nullptr;
		return *this;
	}

	template<typename T>
	ItemData& operator =(T& newData)
	{
		data = newData;
		return *this;
	}

	template<typename T>
	ItemData& operator =(T&& newData)
	{
		data = std::move(newData);
		return *this;
	}

	operator bool() const
	{
		return !std::holds_alternative<std::nullptr_t>(data);
	}

	template<typename ... Funcs>
	void apply(Funcs&&... funcs)
	{
		std::visit(
			visitor
			{
				[](auto const&) {},
				std::forward<Funcs>(funcs)...
			},
			data);
	}

	template<typename T>
	bool is() const
	{
		return std::holds_alternative<T>(data);
	}
};
