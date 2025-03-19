#pragma once

#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/NamedBase.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"

namespace sol { class state; }
namespace sol { template <typename T> struct as_table_t; }

enum GAME_OBJECT_ID : short;
enum ItemStatus;
enum class EffectType;
class LevelFunc;
class Vec3;
struct ItemInfo;
namespace TEN::Scripting { class Rotation; };
namespace TEN::Scripting::Types { class ScriptColor; }

using namespace TEN::Scripting;
using namespace TEN::Scripting::Types;

using aiBitsArray = std::array<int, 6>;
using aiBitsType  = sol::as_table_t<aiBitsArray>;

class Moveable : public NamedBase<Moveable, int>
{
public:
	static void Register(sol::state& state, sol::table& parent);

protected:
	ItemInfo* _moveable = nullptr;

private:
	int	 _moveableID  = 0;
	bool _initialized = false;

public:
	using IdentifierType = int;

	// Constructors, destructors

	Moveable(int movID, bool alreadyInitialized = true);
	Moveable(const Moveable& mov) = delete;
	Moveable(Moveable&& mov) noexcept;
	~Moveable();

	// Getters

	int GetIndex() const;
	GAME_OBJECT_ID GetObjectID() const;
	std::string GetName() const;
	bool GetValid() const;
	Vec3 GetPosition() const;
	Vec3 GetJointPos(int jointID, sol::optional<Vec3> offset) const;
	Rotation GetJointRot(int index) const;
	Rotation GetRotation() const;
	Vec3 GetScale() const;
	int GetStateNumber() const;
	int GetTargetStateNumber() const;
	int GetAnimNumber() const;
	int GetAnimSlot() const;
	int GetFrameNumber() const;
	int GetEndFrame() const;
	Vec3 GetVelocity() const;
	ScriptColor GetColor() const;
	short GetHP() const;
	short GetSlotHP() const;
	short GetOcb() const;
	EffectType GetEffect() const;
	aiBitsType GetAIBits() const;
	short GetItemFlags(int index = 0) const;
	short GetLocationAI() const;
	short GetMeshCount() const;
	bool GetMeshVisible(int meshId) const;
	bool GetMeshSwapped(int meshId) const;
	bool GetHitStatus() const;
	bool GetActive() const;
	short GetStatus() const;

	// Setters

	void SetObjectID(GAME_OBJECT_ID id);
	bool SetName(const std::string&);
	void SetPosition(const Vec3& pos, sol::optional<bool> updateRoom);
	std::unique_ptr<Room> GetRoom() const;
	int GetRoomNumber() const;
	void SetRotation(const Rotation& rot);
	void SetScale(const Vec3& scale);
	void SetStateNumber(int stateNumber);
	void SetAnimNumber(int animNumber, sol::optional<int> slotIndex);
	void SetFrameNumber(int frameNumber);
	void SetVelocity(Vec3 velocity);
	void SetColor(const ScriptColor& color);
	void SetHP(short hp);
	void SetOcb(short ocb);
	void SetEffect(EffectType effectType, sol::optional<float> timeout);
	void SetCustomEffect(const ScriptColor& col1, const ScriptColor& col2, sol::optional<float> timeout);
	void SetAIBits(aiBitsType const& bits);
	void SetItemFlags(short value, int index = 0);
	void SetLocationAI(short value);
	void SetMeshVisible(int meshId, bool isVisible);
	void SetActive(bool isActive);
	void SetRoomNumber(int roomNumber);
	void SetStatus(ItemStatus value);
	void SetOnHit(const TypeOrNil<LevelFunc>& cb);
	void SetOnKilled(const TypeOrNil<LevelFunc>& cb);
	void SetOnCollidedWithObject(const TypeOrNil<LevelFunc>& cb);
	void SetOnCollidedWithRoom(const TypeOrNil<LevelFunc>& cb);

	friend void SetLevelFuncCallback(const TypeOrNil<LevelFunc>& cb, const std::string& callerName, Moveable& mov, std::string& toModify);

	// Utilities

	void Initialize();
	void Invalidate();
	void Destroy();
	void ShatterMesh(int meshId);
	void SwapMesh(int meshId, int swapSlotId, sol::optional<int> swapMeshIndex);
	void UnswapMesh(int meshId);
	void AttachObjCamera(short camMeshId, Moveable& mov, short targetMeshId);
	void AnimFromObject(GAME_OBJECT_ID objectID, int animNumber, int stateID);
	void EnableItem(sol::optional<float> timer);
	void DisableItem();
	void MakeInvisible();
	void SetVisible(bool isVisible);
	bool GetCollidable();
	void SetCollidable(bool isCollidable);
	void Explode();
	void Shatter();

	// Operators

	Moveable& operator =(const Moveable& mov) = delete;
	friend bool operator ==(const Moveable&, const Moveable&);

private:
	// Helpers

	bool MeshExists(int number) const;
};
