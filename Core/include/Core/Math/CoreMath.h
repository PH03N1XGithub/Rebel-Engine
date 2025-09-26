#pragma once
/*#include "CoreMathVectors.h" // Vectors are the base for almost everything     
#include "CoreMathMatrices.h"    
#include "CoreMathQuaternions.h" // depends on Vectors
#include "CoreMathUtilities.h"  // Utilities come after All base math to be available for combine */

#include <glm/glm.hpp>


namespace Rebel::Core
{
    namespace Math = glm;
}

namespace FMath = Rebel::Core::Math;

using Vector2 = FMath::vec2;
using Vector3 = FMath::vec3;
using Vector4 = FMath::vec4;

using Mat3 = FMath::mat3;
using Mat4 = FMath::mat4;

using Quaternion = FMath::quat;




