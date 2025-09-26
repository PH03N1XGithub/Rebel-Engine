#pragma once

// -----------------------------
// Standard Library
// -----------------------------
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <functional>
#include <cmath>
#include <chrono>
#include <random>
#include <limits>
#include <sstream>
#include <fstream>
#include <utility>
#include <type_traits>
#include <numeric>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <execution> 

// -----------------------------
// Third-Party Libraries
// -----------------------------
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_color_sinks-inl.h>

#include <yaml-cpp/yaml.h>

// -----------------------------
// Engine Core (only in engine builds)
// -----------------------------
#ifdef REBELENGINE_DLL
#include "Core/Core.h"  // Includes CoreTypes, CoreMath, and Log
#endif
