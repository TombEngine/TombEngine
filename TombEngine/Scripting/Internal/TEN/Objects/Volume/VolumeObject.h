#pragma once

#include "Game/Control/volume.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Objects/NamedBase.h"

class Vec3;
namespace sol { class state; }
namespace TEN::Scripting { class Rotation; }

using namespace TEN::Scripting;

class Volume : public NamedBase<Volume, TriggerVolume&>
{
private:
	TriggerVolume& _volume;

public:
	using IdentifierType = std::reference_wrapper<TriggerVolume>;

	static void Register(sol::table& parent);

	// Constructors and destructors

	Volume(TriggerVolume& volume);
	Volume(const Volume& other) = delete;
	~Volume() = default;

	// Getters

	std::string GetName() const;
	Vec3		GetPos() const;
	Rotation	GetRot() const;
	Vec3		GetScale() const;

	// Setters

	void SetName(const std::string& name);
	void SetRot(const Rotation& rot);
	void SetPos(const Vec3& pos);
	void SetScale(const Vec3& scale);

	// Inquirers

	bool GetActive() const;
	bool IsMoveableInside(const Moveable& mov);

	// Utilities

	void Enable();
	void Disable();
	void ClearActivators();

	// Operators

	Volume& operator =(const Volume& other) = delete;
};
