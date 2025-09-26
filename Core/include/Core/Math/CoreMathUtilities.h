#pragma once
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Core/CoreTypes.h"
#include "CoreMathMatrices.h"
#include "CoreMathQuaternions.h"
#include "CoreMathVectors.h"

namespace Rebel::Core::Math
{

	// --------------------
    // Constants
    // --------------------
    inline constexpr Float PI = glm::pi<Float>();

    // --------------------
    // Conversion
    // --------------------
    inline Float DegToRad(Float deg) { return glm::radians(deg); }
    inline Float RadToDeg(Float rad) { return glm::degrees(rad); }


    // --------------------
    // Clamp, Lerp, SmoothStep
    // --------------------
    template<typename T>
    inline T Clamp(T value, T minVal, T maxVal)
    {
        return std::max(minVal, std::min(maxVal, value));
    }

    template<typename T>
    inline T Lerp(T a, T b, Float t)
    {
        return glm::mix(a, b, t);
    }

    template<typename T>
    inline T SmoothStep(T a, T b, Float t)
    {
        t = Clamp(t, 0.0f, 1.0f);
        t = t * t * (3 - 2 * t);
        return a + (b - a) * t;
    }

    // --------------------
    // Random number generation
    // --------------------
    inline Float RandomFloat(Float min = 0.0f, Float max = 1.0f)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<Float> dist(min, max);
        return dist(gen);
    }

    inline int32 RandomInt(int32 min, int32 max)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int32> dist(min, max);
        return dist(gen);
    }

    // --------------------
    // Angle utilities
    // --------------------
    inline Float WrapAngle(Float angle)
    {
        return glm::mod(angle, PI * 2);
    }

    inline Float LerpAngle(Float a, Float b, Float t)
    {
        return glm::mix(a, b, t); // GLM handles wrapping via radians
    }

    // --------------------
    // Vector utilities
    // --------------------
    template<int T>
    inline Vector<T> Lerp(const Vector<T>& a, const Vector<T>& b, Float t)
    {
        glm::vec<T, Float, glm::defaultp> result = glm::mix(a.ToGLM(), b.ToGLM(), t);
        return Vector<T>(result);
    }

    template<int T>
    inline Float Dot(const Vector<T>& a, const Vector<T>& b)
    {
        return glm::dot(a.ToGLM(), b.ToGLM());
    }

    template<int T>
    inline Vector<T> Cross(const Vector<3>& a, const Vector<3>& b)
    {
        return Vector<3>(glm::cross(a.ToGLM(), b.ToGLM()));
    }

    template<int T>
    inline Float Distance(const Vector<T>& a, const Vector<T>& b)
    {
        return glm::distance(a.ToGLM(), b.ToGLM());
    }

    template<int T>
    inline Vector<T> Normalize(const Vector<T>& v)
    {
        return Vector<T>(glm::normalize(v.ToGLM()));
    }

    // --------------------
    // Quaternion utilities
    // --------------------
    inline Quaternion Lerp(const Quaternion& a, const Quaternion& b, Float t)
    {
        return Quaternion(glm::lerp(a.ToGLM(), b.ToGLM(), t));
    }

    inline Quaternion Slerp(const Quaternion& a, const Quaternion& b, Float t)
    {
        return Quaternion(glm::slerp(a.ToGLM(), b.ToGLM(), t));
    }

    inline Quaternion FromEuler(Float pitch, Float yaw, Float roll)
    {
        return Quaternion(glm::quat(glm::vec3(pitch, yaw, roll)));
    }

    inline Vector3 RotateVector(const Quaternion& q, const Vector3& v)
    {
        return Vector3(q.ToGLM() * v.ToGLM());
    }

    // --------------------
    // Matrix helpers
    // --------------------
    inline Mat4 LookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        return Mat4(glm::lookAt(eye.ToGLM(), target.ToGLM(), up.ToGLM()));
    }

    inline Mat4 Perspective(Float fov, Float aspect, Float nearPlane, Float farPlane)
    {
        return Mat4(glm::perspective(fov, aspect, nearPlane, farPlane));
    }

    inline Mat4 Translate(const Vector3& translation)
    {
        return Mat4(glm::translate(glm::mat4(1.0f), translation.ToGLM()));
    }

    inline Mat4 Rotate(Float angle, const Vector3& axis)
    {
        return Mat4(glm::rotate(glm::mat4(1.0f), angle, axis.ToGLM()));
    }

    inline Mat4 Scale(const Vector3& scale)
    {
        return Mat4(glm::scale(glm::mat4(1.0f), scale.ToGLM()));
    }
    
}
