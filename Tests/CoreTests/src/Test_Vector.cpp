#include "catch_amalgamated.hpp"
#include "Core/Math/CoreMath.h"
#include <glm/gtc/matrix_transform.hpp>

TEST_CASE("Vector normalization invariant", "[core][math]")
{
    const Vector3 v(3.0f, 4.0f, 0.0f);
    const Vector3 n = FMath::normalize(v);
    const float len = FMath::length(n);

    REQUIRE(len == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("Matrix inverse identity test", "[core][math]")
{
    Mat4 m(1.0f);
    m = FMath::translate(m, Vector3(5.0f, -2.0f, 10.0f));
    m = m * FMath::scale(Mat4(1.0f), Vector3(2.0f, 3.0f, 4.0f));

    const Mat4 inv = FMath::inverse(m);
    const Mat4 id = m * inv;

    REQUIRE(id[0][0] == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(id[1][1] == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(id[2][2] == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(id[3][3] == Catch::Approx(1.0f).margin(1e-4f));
}
