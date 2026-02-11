#pragma once

#include "Game/control/box.h"

namespace TEN::Scripting
{
	/// Constants for pathfinding modes. To be used in @{Flow.Settings.Pathfinding.mode} setting.
	// @enum Flow.PathfindingMode
	// @pragma nostrip

	static const auto PATHFINDING_MODES = std::unordered_map<std::string, PathfindingMode>
	{
		/// Breadth-first search or BFS (original Core algorithm).
		// Fast but treats all boxes equally regardless of physical distance.
		// May produce suboptimal paths where 3 large boxes are preferred over 5 small ones.
		// @mem BFS
		{ "BFS", PathfindingMode::BFS },

		/// Dijkstra's algorithm with distance weights.
		// Finds the shortest physical path by using euclidean distance between box centers.
		// More accurate than BFS but explores in all directions uniformly.
		// @mem DIJKSTRA
		{ "DIJKSTRA", PathfindingMode::Dijkstra },

		/// A* algorithm with distance weights and heuristic (default).
		// Combines Dijkstra's accuracy with a heuristic that guides the search toward the creature.
		// Generally the fastest and most accurate option commonly used in games.
		// @mem ASTAR
		{ "ASTAR", PathfindingMode::AStar }
	};
}
