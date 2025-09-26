#pragma once
#include <glm/glm.hpp>
#include "Core/CoreTypes.h"

namespace Rebel::Core::Math
{
	template<int Rows, int Cols>
    struct Matrix
    {
    private:
        glm::mat<Cols, Rows, Float, glm::defaultp> m_data; // GLM uses column-major storage
    
    public:
        // --------------------
        // Constructors
        // --------------------
        Matrix() : m_data(1.0f) {} // identity
        Matrix(const std::initializer_list<Float>& vals)
        {
            int32 i = 0;
            for (Float v : vals)
            {
                if (i < Rows * Cols)
                {
                    m_data[i % Cols][i / Cols] = v; // GLM column-major indexing
                    i++;
                }
            }
        }
	    template<typename... Args>
        requires (sizeof...(Args) == Rows * Cols)
        Matrix(Args... args) : m_data{ static_cast<Float>(args)... } {}
    
        // Accessors
        Float operator()(int32 row, int32 col) const { return m_data[col][row]; }
        //Float& operator()(int32 row, int32 col) { return m_data[col][row]; }
	    Float& operator()(int32 row, int32 col) {
            assert(row >= 0 && row < Rows && col >= 0 && col < Cols);
            return m_data[col][row];
        }

    
        // --------------------
        // Modifying functions
        // --------------------
        void SetIdentity() { m_data = glm::mat<Cols, Rows, Float, glm::defaultp>(1.0f); }
        void Transpose() { m_data = glm::transpose(m_data); }
        void Invert() { m_data = glm::inverse(m_data); }
    
        // --------------------
        // Non-modifying functions
        // --------------------
        [[nodiscard]] Matrix<Rows, Cols> Transposed() const { return Matrix<Rows, Cols>(glm::transpose(m_data)); }
        [[nodiscard]] Matrix<Rows, Cols> Inverted() const { return Matrix<Rows, Cols>(glm::inverse(m_data)); }
    
        // --------------------
        // Arithmetic operators
        // --------------------
        Matrix<Rows, Cols> operator+(const Matrix<Rows, Cols>& rhs) const { return Matrix<Rows, Cols>(m_data + rhs.m_data); }
        Matrix<Rows, Cols> operator-(const Matrix<Rows, Cols>& rhs) const { return Matrix<Rows, Cols>(m_data - rhs.m_data); }
	    Matrix<Rows, Cols> operator-() const { return Matrix<Rows, Cols>(-m_data); }
        Matrix<Rows, Cols> operator*(Float scalar) const { return Matrix<Rows, Cols>(m_data * scalar); }
	    Matrix<Rows, Cols> operator/(Float scalar) const { return Matrix<Rows, Cols>(m_data / scalar); }
	    Matrix<Rows, Cols> operator+() const { return *this; }
	    
    
        template<int OtherCols>
        Matrix<Rows, OtherCols> operator*(const Matrix<Cols, OtherCols>& rhs) const
        {
            return Matrix<Rows, OtherCols>(m_data * rhs.m_data);
        }
    
        Bool operator==(const Matrix<Rows, Cols>& rhs) const { return m_data == rhs.m_data; }
        Bool operator!=(const Matrix<Rows, Cols>& rhs) const { return m_data != rhs.m_data; }
    
        // Conversion to GLM
        [[nodiscard]] glm::mat<Cols, Rows, Float, glm::defaultp> ToGLM() const { return m_data; }
    
        explicit Matrix(const glm::mat<Cols, Rows, Float, glm::defaultp>& v) : m_data(v) {}
    };

    template<int R, int C, typename S>
    Matrix<R,C> operator*(S scalar, const Matrix<R,C>& m)
    {
        return m * static_cast<Float>(scalar);
    }

}
// Type aliases
using Mat3 = Rebel::Core::Math::Matrix<3, 3>;
using Mat4 = Rebel::Core::Math::Matrix<4, 4>;
