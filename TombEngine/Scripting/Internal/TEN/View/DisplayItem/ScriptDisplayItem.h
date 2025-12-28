#include "Game/Hud/DrawItems/DrawItems.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"

using namespace TEN::Scripting::Types;

namespace TEN::Scripting::DisplayItem
{
	constexpr auto ALL_JOINT_BITS = UINT_MAX;

	class ScriptDisplayItem
	{
	public:
		static void Register(sol::state& state, sol::table& parent);

	private:
		// Fields

		std::string _name = {};

	public:
		// Constructors

		ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID, const Vec3& pos, const Rotation& rot, const Vec3& scale, int meshBits);
		ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID, const Vec3& pos, const Rotation& rot, const Vec3& scale);
		ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID, const Vec3& pos);
		ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID);
		ScriptDisplayItem(const std::string& name);

		// Methods

		void Remove();
		bool Exists() const;

		// Setters

		void SetObjectID(GAME_OBJECT_ID objectID);
		void SetPosition(const Vec3& newPos, TypeOrNil<bool> disableInterpolation);
		void SetRotation(const Rotation& newRot, TypeOrNil<bool> disableInterpolation);
		void SetScale(const Vec3& newScale, TypeOrNil<bool> disableInterpolation);
		void SetColor(const ScriptColor& newColor, TypeOrNil<bool> disableInterpolation);
		void SetMeshBits(int meshbits);
		void SetMeshVisibility(int meshIndex, bool visible);
		void SetMeshRotation(int meshIndex, Rotation angles, TypeOrNil<bool> disableInterpolation);
		void SetVisibility(bool visible);
		void SetFrame(int animation, int frame);

		// Getters

		GAME_OBJECT_ID GetObjectID() const;
		sol::optional <Vec3> GetPosition() const;
		sol::optional <Rotation> GetRotation() const;
		sol::optional <Vec3> GetScale() const;
		sol::optional <ScriptColor> GetColor() const;
		bool GetMeshVisibility(int meshIndex) const;
		sol::optional <Rotation> GetMeshRotation(int meshIndex) const;
		bool GetVisibility() const;
		sol::optional <int> GetAnimNumber() const;
		sol::optional <int> GetFrameNumber() const;
		sol::optional <int> GetEndFrame() const;
		sol::optional<std::pair<Vec2, Vec2>> GetBounds() const;

		// Functions

		static ScriptDisplayItem GetItemByName(const std::string& itemName);
		static void RemoveItem(const std::string& itemName);
		static void ClearItems();
		static bool IfItemExists(const std::string& itemName);
		static bool IfObjectIDExists(const GAME_OBJECT_ID objectID);

		// Static camera functions

		// Setters

		static void SetAmbientLight(const ScriptColor& lightColor);
		static void SetCameraPosition(const Vec3& pos, TypeOrNil<bool> disableInterpolation);
		static void SetCameraTargetPosition(const Vec3& target, TypeOrNil<bool> disableInterpolation);
		static void ResetCamera(TypeOrNil<bool> disableInterpolation);

		// Getters

		static ScriptColor GetAmbientLight();
		static Vec3        GetCameraPosition();
		static Vec3        GetCameraTargetPosition();
	};
}
