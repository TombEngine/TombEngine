#pragma once

// ================
// PLATFORM DEFINES
// ================

#include <cstdint>

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__ppc64__)
	#define PLATFORM_64BIT 1
#else
	#define PLATFORM_64BIT 0
#endif

// =========
// LIBRARIES
// =========

// Standard
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <codecvt>
#include <cstdint>
#include <ctime>
#include <chrono>
#include <deque>
#ifdef _MSC_VER
#include <execution>
#endif
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <tuple>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// BASS
#include <bass.h>
#include <bass_fx.h>

// SimpleMath (GLM compatibility layer)
#include "Math/SimpleMath.h"

using namespace TEN::Math;
using namespace TEN::Math::Library;
using namespace TEN::Math::Collision;

// SDL3
#include <SDL3/SDL.h>

// sol
#include <sol.hpp>

// srtparser.h
#include <srtparser.h>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// VLC
#include <vlc/vlc.h>

// =========
// RESOURCES
// =========

#include "Types.h"
#include "Game/Debug/Debug.h"

using namespace TEN::Debug;
