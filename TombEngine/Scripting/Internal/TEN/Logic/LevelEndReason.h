#pragma once
#include "Scripting/Internal/ReservedScriptNames.h"

namespace TEN::Scripting
{
	enum class LevelEndReason
	{
		LevelComplete,
		LoadGame,
		ExitToTitle,
		Death,
		Other
	};

	/// Reasons why a level ended.
	// @enum Logic.EndReason
	static const auto LEVEL_END_REASONS = std::unordered_map<std::string, LevelEndReason>
	{
		/// Will be called when the level is completed successfully.
		// @mem LEVEL_COMPLETE
		{ ScriptReserved_EndReasonLevelComplete, LevelEndReason::LevelComplete },

		/// Will be called when the level ends due to loading a saved game.
		// @mem LOAD_GAME
		{ ScriptReserved_EndReasonLoadGame, LevelEndReason::LoadGame },

		/// Will be called when the level ends due to exiting to the title screen.
		// @mem EXIT_TO_TITLE
		{ ScriptReserved_EndReasonExitToTitle, LevelEndReason::ExitToTitle },

		/// Will be called when the level ends due to the player's death.
		// @mem DEATH
		{ ScriptReserved_EndReasonDeath, LevelEndReason::Death },

		/// Will be called when the level ends for any other reason.
		// @mem OTHER
		{ ScriptReserved_EndReasonOther, LevelEndReason::Other },

		// COMPATIBILITY
		{ "LEVELCOMPLETE", LevelEndReason::LevelComplete },
		{ "LOADGAME", LevelEndReason::LoadGame },
		{ "EXITTOTITLE", LevelEndReason::ExitToTitle }
	};
}