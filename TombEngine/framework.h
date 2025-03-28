#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <SimpleMath.h>
#include <sol.hpp>
#include <stack>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <queue>
#include <unordered_set>
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;

constexpr auto NO_VALUE = -1;

#include "Game/Debug/Debug.h"
#include "Physics/Physics.h"

using namespace TEN::Debug;
using namespace TEN::Physics;
