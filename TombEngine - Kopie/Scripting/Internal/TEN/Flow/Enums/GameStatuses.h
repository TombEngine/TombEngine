#pragma once

#include "Game/control/control.h"

namespace TEN::Scripting
{
	/// Constants for game statuses.
	// To be used with @{Flow.GetGameStatus} function.
	// @enum Flow.GameStatus
	// @pragma nostrip

	static const auto GAME_STATUSES = std::unordered_map<std::string, GameStatus>
	{
		/// Normal game status.
		// @mem NORMAL
		{ "NORMAL", GameStatus::Normal },

		/// New game status.
		// @mem NEW_GAME
		{ "NEW_GAME", GameStatus::NewGame },

		/// Load game status.
		// @mem LOAD_GAME
		{ "LOAD_GAME", GameStatus::LoadGame },

		/// Exit to title status.
		// @mem EXIT_TO_TITLE
		{ "EXIT_TO_TITLE", GameStatus::ExitToTitle },

		/// Exit game status.
		// @mem EXIT_GAME
		{ "EXIT_GAME", GameStatus::ExitGame },

		/// Player dead status.
		// @mem LARA_DEAD
		{ "LARA_DEAD", GameStatus::LaraDead }, // TODO: Rename to PLAYER_DEAD

		/// Level complete status.
		// @mem LEVEL_COMPLETE
		{ "LEVEL_COMPLETE", GameStatus::LevelComplete }
	};
}
