#pragma once

#include "Game/room.h"
#include "Scripting/Internal/TEN/Objects/NamedBase.h"

namespace sol { class state; }

class Vec3;
namespace TEN::Scripting::Types { class ScriptColor; }

using namespace TEN::Scripting::Types;

namespace TEN::Scripting
{
	class Rotation;

	class Static : public NamedBase<Static, StaticMesh&>
	{
	public:
		static void Register(sol::table& parent);

	private:
		// Fields

		StaticMesh& _static;

	public:
		// Aliases

		using IdentifierType = std::reference_wrapper<StaticMesh>;

		// Constructors, destructors

		Static(StaticMesh& staticObj);
		Static(const Static& staticObj) = delete;
		~Static() = default;

		// Getters

		std::string	GetName() const;
		int			GetSlot() const;
		Vec3		GetPosition() const;
		Rotation	GetRotation() const;
		Vec3		GetScale() const;
		ScriptColor	GetColor() const;
		int			GetHP() const;
		bool		GetActiveStatus() const;
		bool		GetCollidable() const;
		bool		GetSolidStatus() const;

		// Setters

		void SetName(const std::string& name);
		void SetSlot(int slotID);
		void SetPosition(const Vec3& pos);
		void SetRotation(const Rotation& rot);
		void SetScale(const Vec3& scale);
		void SetColor(const ScriptColor& color);
		void SetHP(int hitPoints);
		void SetCollidable(bool status);
		void SetSolidStatus(bool status);

		// Utilities

		void Enable();
		void Disable();
		void Shatter();

		// Operators

		Static& operator =(const Static& staticObj) = delete;

		// COMPATIBILITY

		void SetScale(float scale);
	};
}
