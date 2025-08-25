#pragma once

#include "Scripting/Internal/TEN/Objects/NamedBase.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"

struct LevelCameraInfo;

namespace sol
{
	class state;
}
class Vec3;

class CameraObject : public NamedBase<CameraObject, LevelCameraInfo &>
{
public:
	using IdentifierType = std::reference_wrapper<LevelCameraInfo>;
	CameraObject(LevelCameraInfo& ref);
	~CameraObject() = default;

	CameraObject& operator=(CameraObject const& other) = delete;
	CameraObject(CameraObject const& other) = delete;

	static void Register(sol::table&);
	Vec3 GetPos() const;
	void SetPos(Vec3 const& pos);

	std::unique_ptr<Room> GetRoom() const;
	int GetRoomNumber() const;
	void SetRoomNumber(short room);

	std::string GetName() const;
	void SetName(std::string const &);

	void Play(sol::optional<Moveable&> targetObj);

private:
	LevelCameraInfo & m_camera;
};
