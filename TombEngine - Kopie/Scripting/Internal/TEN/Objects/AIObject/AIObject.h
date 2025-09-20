#pragma once

#include "Scripting/Internal/TEN/Objects/NamedBase.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"
#include "Specific/level.h"

namespace sol {
	class state;
}
class Vec3;

class AIObject : public NamedBase<AIObject, AI_OBJECT&>
{
public:

	using IdentifierType = std::reference_wrapper<AI_OBJECT>;
	AIObject(AI_OBJECT& ref);
	~AIObject() = default;

	AIObject& operator=(AIObject const& other) = delete;
	AIObject(AIObject const& other) = delete;

	static void Register(sol::table& parent);

	Vec3 GetPos() const;
	void SetPos(Vec3 const& pos);

	std::unique_ptr<Room> GetRoom() const;
	int GetRoomNumber() const;
	void SetRoomNumber(short Room);

	std::string GetName() const;
	void SetName(const std::string&);

	GAME_OBJECT_ID GetObjectID() const;
	void SetObjectID(GAME_OBJECT_ID);

	float GetYRot() const;
	void SetYRot(float);

private:
	AI_OBJECT & m_aiObject;
};

