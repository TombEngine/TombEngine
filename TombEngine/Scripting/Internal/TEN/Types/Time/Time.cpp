#include "framework.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"

#include "Math/Math.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Specific/clock.h"

namespace TEN::Scripting
{
	constexpr auto TIME_UNIT   = 60;
	constexpr auto CENTISECOND = 100;

	/// Represents a time value in game frames, with support for formatting to hours, minutes, seconds, and centiseconds (1/100th of a second).
	// @tenprimitive Time
	// @pragma nostrip

	void Time::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			Time(), Time(int),
			Time(const std::string&), Time(const sol::table&)>;

		parent.new_usertype<Time>(
			"Time", ctors(),
			sol::call_constructor, ctors(),

			// Meta functions
			sol::meta_function::to_string, &Time::ToString,
			sol::meta_function::equal_to, &Time::operator ==,
			sol::meta_function::less_than, &Time::operator <,
			sol::meta_function::less_than_or_equal_to, &Time::operator <=,

			sol::meta_function::addition, sol::overload(
				[](const Time& time0, const Time& time1) { return (time0 + time1); },
				[](const Time& time, int gameFrames) { return (time + gameFrames); }),
			sol::meta_function::subtraction, sol::overload(
				[](const Time& time0, const Time& time1) { return (time0 - time1); },
				[](const Time& time, int gameFrames) { return (time - gameFrames); }),

			// Methods
			"GetTimeUnits", &Time::GetTimeUnits,
			"GetFrameCount", &Time::GetFrameCount,

			// Readable and writable fields
			"h", sol::property(&Time::GetHours,		   &Time::SetHours),
			"m", sol::property(&Time::GetMinutes,	   &Time::SetMinutes),
			"s", sol::property(&Time::GetSeconds,	   &Time::SetSeconds),
			"c", sol::property(&Time::GetCentiseconds, &Time::SetCentiseconds));
	}

	/// Create a Time object.
	// @function Time
	// @treturn Time A new Time object initialized to zero time.

	/// Create a Time object from a total game frame count (1 second = 30 frames).
	// @function Time
	// @tparam int gameFrames Total game frame count.
	// @treturn Time A new Time object initialized with the given frame count.
	Time::Time(int gameFrames)
	{
		_frameCount = std::clamp(gameFrames, 0, INT_MAX);
	}

	/// Create a Time object from a formatted string.
	// @function Time
	// @tparam string formattedTime Time in the format "HH:MM:SS[.CC]", where [.CC] is centiseconds and is optional.
	// @treturn Time A new Time object parsed from the given string.
	Time::Time(const std::string& formattedTime)
	{
		SetFromFormattedString(formattedTime);
	}

	/// Create a Time object from a time unit table (hours, minutes, seconds, centiseconds).
	// @function Time
	// @tparam table timeUnits A time unit table in the format {HH, MM, SS, [CC]}, where [CC] is optional.
	// @treturn Time A new Time object initialized with the given values.
	Time::Time(const sol::table& hmsTable)
	{
		SetFromTable(hmsTable);
	}

	/// Get the total game frame count.
	// @function Time:GetFrameCount
	// @treturn int Total number of game frames.
	int Time::GetFrameCount() const
	{
		return _frameCount;
	}

	/// Get the time in hours, minutes, seconds, and centiseconds as a table.
	// @function Time:GetTimeUnits
	// @treturn table A table in the format {HH, MM, SS, CC}.
	sol::table Time::GetTimeUnits(sol::this_state state) const
	{
		auto hmsc = GetHmsc();
		auto table = sol::state_view(state).create_table();

		table.add(hmsc.Hours);
		table.add(hmsc.Minutes);
		table.add(hmsc.Seconds);
		table.add(hmsc.Centiseconds);
		return table;
	}

	/// (int) Hours component.
	// @mem h
	int Time::GetHours() const
	{
		auto [h, m, s, c] = GetHmsc();
		return h;
	}

	/// (int) Minutes component.
	// @mem m
	int Time::GetMinutes() const
	{
		auto [h, m, s, c] = GetHmsc();
		return m;
	}

	/// (int) Seconds component.
	// @mem s
	int Time::GetSeconds() const
	{
		auto [h, m, s, c] = GetHmsc();
		return s;
	}

	/// (int) Centiseconds component.
	// @mem c
	int Time::GetCentiseconds() const
	{
		auto [h, m, s, c] = GetHmsc();
		return c;
	}

	void Time::SetHours(int value)
	{
		auto [h, m, s, f] = GetHmsc();
		SetFromHMSC(value, m, s, f);
	}

	void Time::SetMinutes(int value)
	{
		auto [h, m, s, f] = GetHmsc();
		SetFromHMSC(h, value, s, f);
	}

	void Time::SetSeconds(int value)
	{
		auto [h, m, s, c] = GetHmsc();
		SetFromHMSC(h, m, value, c);
	}

	void Time::SetCentiseconds(int value)
	{
		auto [h, m, s, c] = GetHmsc();
		SetFromHMSC(h, m, s, value);
	}

	///  Convert this Time object to a formatted string.
	// @function tostring
	// @tparam Time this Time object.
	// @treturn string A string showing time in "HH:MM:SS.CC" format.
	std::string Time::ToString() const
	{
		auto hmsc = GetHmsc();

		auto stream = std::ostringstream();
		stream << std::setw(2) << std::setfill('0') << hmsc.Hours << ":"
			<< std::setw(2) << std::setfill('0') << hmsc.Minutes << ":"
			<< std::setw(2) << std::setfill('0') << hmsc.Seconds << "."
			<< std::setw(2) << std::setfill('0') << hmsc.Centiseconds;
		return stream.str();
	}

	Time& Time::operator ++()
	{
		_frameCount++;
		return *this;
	}

	Time& Time::operator ++(int)
	{
		_frameCount++;
		return *this;
	}

	Time& Time::operator +=(const Time& time)
	{
		_frameCount += time._frameCount;
		return *this;
	}

	Time& Time::operator -=(const Time& time)
	{
		_frameCount = std::clamp(_frameCount - time._frameCount, 0, INT_MAX);
		return *this;
	}

	Time Time::operator +(int frameCount) const
	{
		return Time(_frameCount + frameCount);
	}

	Time Time::operator -(int frameCount) const
	{
		return Time(std::clamp(_frameCount - frameCount, 0, INT_MAX));
	}

	Time Time::operator +(const Time& time) const
	{
		return Time(_frameCount + time._frameCount);
	}

	Time Time::operator -(const Time& time) const
	{
		return Time(std::clamp(_frameCount - time._frameCount, 0, INT_MAX));
	}

	bool Time::operator <(const Time& time) const
	{
		return _frameCount < time._frameCount;
	}

	bool Time::operator <=(const Time& time) const
	{
		return _frameCount <= time._frameCount;
	}

	bool Time::operator ==(const Time& time) const
	{
		return _frameCount == time._frameCount;
	}

	Time::Hmsc Time::GetHmsc() const
	{
		int totalSeconds = _frameCount / FPS;

		return Hmsc
		{
			totalSeconds / SQUARE(TIME_UNIT),
			(totalSeconds % SQUARE(TIME_UNIT)) / TIME_UNIT,
			totalSeconds % TIME_UNIT,
			(_frameCount * 100 / FPS) % CENTISECOND
		};
	}

	Time::Hmsc Time::ParseFormattedString(const std::string& formattedTime)
	{
		std::regex timeFormat(R"((?:(\d+):)?(\d+):(\d+)(?:\.(\d+))?)");
		std::smatch match;

		if (!std::regex_match(formattedTime, match, timeFormat))
		{
			TENLog("Invalid time format. Supported formats: HH:MM:SS.CC, HH:MM:SS, MM:SS, or MM:SS.CC.", LogLevel::Warning);
			return Time::Hmsc();
		}

		int hours   = match[1].matched ? std::stoi(match[1].str()) : 0;
		int minutes = match[2].matched ? std::stoi(match[2].str()) : 0;
		int seconds = match[3].matched ? std::stoi(match[3].str()) : 0;
		int cents   = match[4].matched ? std::stoi(match[4].str()) : 0;

		return { hours, minutes, seconds, cents };
	}

	void Time::SetFromHMSC(int hours, int minutes, int seconds, int cents)
	{
		_frameCount = (hours * SQUARE(TIME_UNIT) + minutes * TIME_UNIT + seconds) * FPS +
			round((float)cents / ((float)CENTISECOND / (float)FPS));
	}

	void Time::SetFromFormattedString(const std::string& formattedTime)
	{
		auto hmsc = ParseFormattedString(formattedTime);
		SetFromHMSC(hmsc.Hours, hmsc.Minutes, hmsc.Seconds, hmsc.Centiseconds);
	}

	void Time::SetFromTable(const sol::table& hmsTable)
	{
		if (!hmsTable.valid() || hmsTable.size() < 1 || hmsTable.size() > 4)
			throw std::invalid_argument("Invalid time unit table. Expected {HH, MM, SS, [CC]}");

		int hours	= hmsTable.get_or(1, 0);
		int minutes = hmsTable.get_or(2, 0);
		int seconds = hmsTable.get_or(3, 0);
		int cents	= hmsTable.get_or(4, 0);

		SetFromHMSC(hours, minutes, seconds, cents);
	}
}
