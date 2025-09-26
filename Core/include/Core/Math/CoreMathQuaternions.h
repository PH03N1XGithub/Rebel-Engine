#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Core/CoreTypes.h"
#include "CoreMathVectors.h"

namespace Rebel::Core::Math
{
	struct Quaternion
    {
    private:
        glm::quat m_data; // GLM quaternion (w, x, y, z)
    
    public:
        // --------------------
        // Constructors
        // --------------------
        Quaternion() : m_data(1.0f, 0.0f, 0.0f, 0.0f) {} // identity quaternion
    
        Quaternion(Float w, Float x, Float y, Float z) : m_data(w, x, y, z) {}
    
        Quaternion(const Vector3& axis, Float angle) // axis-angle constructor
        {
            m_data = glm::angleAxis(angle, axis.ToGLM());
        }
    
        explicit Quaternion(const glm::quat& q) : m_data(q) {}
    
        // --------------------
        // Accessors
        // --------------------
        Float& w() { return m_data.w; }
        const Float& w() const { return m_data.w; }
    
        Float& x() { return m_data.x; }
        const Float& x() const { return m_data.x; }
    
        Float& y() { return m_data.y; }
        const Float& y() const { return m_data.y; }
    
        Float& z() { return m_data.z; }
        const Float& z() const { return m_data.z; }
    
        // --------------------
        // Modifying functions
        // --------------------
        void Normalize() { m_data = glm::normalize(m_data); }
        void Conjugate() { m_data = glm::conjugate(m_data); }
        void Invert() { m_data = glm::inverse(m_data); }
    
        // --------------------
        // Non-modifying functions
        // --------------------
        [[nodiscard]] Quaternion Normalized() const { return Quaternion(glm::normalize(m_data)); }
        [[nodiscard]] Quaternion Conjugated() const { return Quaternion(glm::conjugate(m_data)); }
        [[nodiscard]] Quaternion Inverted() const { return Quaternion(glm::inverse(m_data)); }
    
        static Quaternion Lerp(const Quaternion& a, const Quaternion& b, Float t)
        {
            return Quaternion(glm::lerp(a.m_data, b.m_data, t));
        }
    
        static Quaternion Slerp(const Quaternion& a, const Quaternion& b, Float t)
        {
            return Quaternion(glm::slerp(a.m_data, b.m_data, t));
        }
    
        // --------------------
        // Arithmetic operators
        // --------------------
        Quaternion operator*(const Quaternion& rhs) const { return Quaternion(m_data * rhs.m_data); }
        Vector3 operator*(const Vector3& vec) const {  return Vector3(m_data * vec.ToGLM()); }
    
        Bool operator==(const Quaternion& rhs) const { return m_data == rhs.m_data; }
        Bool operator!=(const Quaternion& rhs) const { return m_data != rhs.m_data; }
    
        // Conversion to GLM
        [[nodiscard]] glm::quat ToGLM() const { return m_data; }
    };
}

typedef Rebel::Core::Math::Quaternion Quat;
