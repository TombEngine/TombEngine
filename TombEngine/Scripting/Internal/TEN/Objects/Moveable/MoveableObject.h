#pragma once
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/NamedBase.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"

class LevelFunc;

namespace sol 
{
	class state;
	template <typename T> struct as_table_t;
}

enum ItemStatus;
enum GAME_OBJECT_ID : short;
enum class EffectType;
class Rotation;
class ScriptColor;
class Vec3;
struct ItemInfo;

using aiBitsArray = std::array<int, 6>;
using aiBitsType = sol::as_table_t<aiBitsArray>;

class Moveable : public NamedBase<Moveable, short>
{
public:
	using IdentifierType = short;

	static void Register(sol::state& state, sol::table& parent);

	Moveable(short num, bool alreadyInitialized = true);
	~Moveable();
	Moveable& operator =(const Moveable& other) = delete;
	Moveable(const Moveable& other) = delete;
	Moveable(Moveable&& other) noexcept;

	[[nodiscard]] GAME_OBJECT_ID GetObjectID() const;
	void SetObjectID(GAME_OBJECT_ID id);

	[[nodiscard]] std::string GetName() const;
	bool SetName(const std::string&);

	[[nodiscard]] bool GetValid() const;
	void Invalidate();

	void Destroy();

	[[nodiscard]] Vec3 GetPos() const;
	[[nodiscard]] Vec3 GetJointPos(int index) const;
	void SetPos(const Vec3& pos, sol::optional<bool> updateRoom);

	[[nodiscard]] Rotation GetRot() const;
	void SetRot(const Rotation& rot);

	[[nodiscard]] int GetStateNumber() const;
	void SetStateNumber(int stateNumber);

	[[nodiscard]] int GetAnimNumber() const;
	void SetAnimNumber(int animNumber);

	[[nodiscard]] int GetFrameNumber() const;
	[[nodiscard]] int GetEndFrame() const;
	void SetFrameNumber(int frameNumber);

	[[nodiscard]] Vec3 GetVelocity() const;
	void SetVelocity(Vec3 velocity);

	[[nodiscard]] ScriptColor GetColor() const;
	void SetColor(const ScriptColor& color);

	[[nodiscard]] short GetHP() const;
	void SetHP(short hp);

	[[nodiscard]] short GetSlotHP() const;

	[[nodiscard]] short GetOCB() const;
	void SetOCB(short ocb);

	[[nodiscard]] EffectType GetEffect() const;
	void SetEffect(EffectType effectType, sol::optional<float> timeout);
	void SetCustomEffect(const ScriptColor& col1, const ScriptColor& col2, sol::optional<float> timeout);

	[[nodiscard]] aiBitsType GetAIBits() const;
	void SetAIBits(aiBitsType const & bits);

	[[nodiscard]] short GetItemFlags(int index = 0) const;
	void SetItemFlags(short value, int index = 0);

	[[nodiscard]] short GetLocationAI() const;
	void SetLocationAI(short value);

	[[nodiscard]] short GetMeshCount() const;
	[[nodiscard]] bool GetMeshVisible(int meshId) const;
	void SetMeshVisible(int meshId, bool isVisible);
	void ShatterMesh(int meshId);

	[[nodiscard]] bool GetMeshSwapped(int meshId) const;
	void SwapMesh(int meshId, int swapSlotId, sol::optional<int> swapMeshIndex);
	void UnswapMesh(int meshId);

	[[nodiscard]] bool GetHitStatus() const;

	[[nodiscard]] bool GetActive() const;
	void SetActive(bool isActive);

	std::unique_ptr<Room> GetRoom() const;
	[[nodiscard]] int GetRoomNumber() const;
	void SetRoomNumber(int roomNumber);

	void AttachObjCamera(short camMeshId, Moveable& mov, short targetMeshId);
	void AnimFromObject(GAME_OBJECT_ID object, int animNumber, int stateID);

	void EnableItem();
	void DisableItem();
	void MakeInvisible();
	void SetVisible(bool isVisible);
	void Explode();
	void Shatter();

	void SetOnHit(const TypeOrNil<LevelFunc>& cb);
	void SetOnKilled(const TypeOrNil<LevelFunc>& cb);
	void SetOnCollidedWithObject(const TypeOrNil<LevelFunc>& cb);
	void SetOnCollidedWithRoom(const TypeOrNil<LevelFunc>& cb);

	[[nodiscard]] short GetStatus() const;
	void SetStatus(ItemStatus value);

	void Init();

	friend bool operator ==(const Moveable&, const Moveable&);
	friend void SetLevelFuncCallback(const TypeOrNil<LevelFunc>& cb, const std::string& callerName, Moveable& mov, std::string& toModify);

	short GetIndex() const;
	
	[[nodiscard]] std::unique_ptr<Moveable> GetCreatureEnemy();
	void ClearCreatureEnemy();
	void SetCreatureEnemy(Moveable& m);
	[[nodiscard]] TypeOrNil<Vec3> GetCreatureTarget();
	void SetCreatureTarget(const Vec3& pos);

	[[nodiscard]] std::unique_ptr<Moveable> GetCreatureEnemy();
	void ClearCreatureEnemy();
	void SetCreatureEnemy(Moveable& m);
	[[nodiscard]] TypeOrNil<Vec3> GetCreatureTarget();
	void SetCreatureTarget(const Vec3& pos);

protected:
	ItemInfo* m_item;

private:
	short m_num;
	bool m_initialized;

	bool MeshExists(int number) const;
};
