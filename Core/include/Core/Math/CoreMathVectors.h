#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Core/CoreTypes.h"

namespace Rebel::Core::Math
{
	
template<int32 T>
struct Vector
{
private:
    glm::vec<T, Float, glm::defaultp> m_data;

public:
    // --------------------
    // Constructors
    // --------------------
    Vector() : m_data(0.0f) {}
    Vector(const std::initializer_list<Float>& vals)
    {
        int32 i = 0;
        for (Float v : vals)
        {
            if (i < T) m_data[i++] = v;
        }
    }
    template<typename... Args>
    requires (sizeof...(Args) == T)
    Vector(Args... args) : m_data{ static_cast<Float>(args)... } {}

    /*// Accessors
    Float operator[](int32 index) const { return m_data[index]; }
    Float& operator[](int32 index) { return m_data[index]; }*/

    // Named accessors with requirements
    Float& x() requires (T >= 1) { return m_data[0]; }
    const Float& x() const requires (T >= 1) { return m_data[0]; }

    Float& y() requires (T >= 2) { return m_data[1]; }
    const Float& y() const requires (T >= 2) { return m_data[1]; }

    Float& z() requires (T >= 3) { return m_data[2]; }
    const Float& z() const requires (T >= 3) { return m_data[2]; }

    Float& w() requires (T >= 4) { return m_data[3]; }
    const Float& w() const requires (T >= 4) { return m_data[3]; }

    void Set(const std::initializer_list<Float>& vals)
    {
        int32 i = 0;
        for (Float v : vals)
        {
            if (i < T) m_data[i++] = v;
        }
    }

    // --------------------
    // Modifying functions
    // --------------------
    void Normalize() { m_data = glm::normalize(m_data); }

    Vector<T>& operator+=(const Vector<T>& rhs) { m_data += rhs.m_data; return *this; }
    Vector<T>& operator-=(const Vector<T>& rhs) { m_data -= rhs.m_data; return *this; }
    Vector<T>& operator*=(Float scalar) { m_data *= scalar; return *this; }
    Vector<T>& operator/=(Float scalar) { m_data /= scalar; return *this; }

    // --------------------
    // Non-modifying functions
    // --------------------
    [[nodiscard]] Vector<T> Normalized() const { return Vector<T>(glm::normalize(m_data)); }
    [[nodiscard]] Float Length() const { return glm::length(m_data); }

    [[nodiscard]] Float Dot(const Vector<T>& rhs) const { return glm::dot(m_data, rhs.m_data); }

    // Only for 3D vectors
    template<int M = T>
    [[nodiscard]] Vector<3> Cross(const Vector<3>& rhs) const
        requires (M == 3)
    {
        return Vector<3>(glm::cross(m_data, rhs.m_data));
    }

    static Vector<T> Lerp(const Vector<T>& a, const Vector<T>& b, Float t)
    {
        return Vector<T>(glm::mix(a.m_data, b.m_data, t));
    }

    static Float Distance(const Vector<T>& a, const Vector<T>& b)
    {
        return glm::distance(a.m_data, b.m_data);
    }

    // --------------------
    // Arithmetic operators
    // --------------------
    Vector<T> operator+(const Vector<T>& rhs) const { return Vector<T>(m_data + rhs.m_data); }
    Vector<T> operator-(const Vector<T>& rhs) const { return Vector<T>(m_data - rhs.m_data); }
    Vector<T> operator-() const { return Vector<T>(-m_data); }
    Vector<T> operator*(Float scalar) const { return Vector<T>(m_data * scalar); }
    Vector<T> operator/(Float scalar) const { return Vector<T>(m_data / scalar); }
    
    
    Bool operator==(const Vector<T>& rhs) const { return m_data == rhs.m_data; }
    Bool operator!=(const Vector<T>& rhs) const { return m_data != rhs.m_data; }
    

    // Conversion to GLM
    [[nodiscard]] glm::vec<T, Float, glm::defaultp> ToGLM() const { return m_data; }


    // Private constructor from GLM
    explicit Vector(const glm::vec<T, Float, glm::defaultp>& v) : m_data(v) {}
private:
};

    template<int T, typename S>
    requires std::is_arithmetic_v<S>
    Vector<T> operator*(S scalar, const Vector<T>& v)
    {
        return v * static_cast<Float>(scalar);
    }

	
}

// Type aliases
using Vector2 = Rebel::Core::Math::Vector<2>;
using Vector3 = Rebel::Core::Math::Vector<3>;
using Vector4 = Rebel::Core::Math::Vector<4>;


