#pragma once

#include "Objects/Sink.h"
#include "Scripting/Internal/TEN/Objects/NamedBase.h"

namespace sol { class state; }

class Vec3;

namespace TEN::Scripting::Objects
{
	class Sink : public NamedBase<Sink, SinkInfo&>
	{
	public:
		static void Register(sol::table& parent);

	private:
		// Fields

		SinkInfo& _sink;

	public:
		// Aliases

		using IdentifierType = std::reference_wrapper<SinkInfo>;

		// Constructors, destructors

		Sink(SinkInfo& sink);
		Sink(const Sink& sink) = delete;
		~Sink() = default;

		// Getters

		std::string GetName() const;
		Vec3		GetPosition() const;
		int			GetStrength() const;

		// Setters

		void SetName(const std::string& name);
		void SetPosition(const Vec3& pos);
		void SetStrength(int strength);

		// Operators

		Sink& operator =(const Sink& other) = delete;
	};
}
