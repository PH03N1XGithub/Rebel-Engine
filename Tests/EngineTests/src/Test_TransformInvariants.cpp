#include "catch_amalgamated.hpp"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Components/Components.h"

namespace
{
    constexpr float kBasisEps = 1e-4f;
    constexpr float kDriftEps = 1e-5f;

    void RequireUnitVector(const Vector3& vector, float epsilon)
    {
        REQUIRE(glm::length(vector) == Catch::Approx(1.0f).margin(epsilon));
    }

    void RequirePerpendicular(const Vector3& a, const Vector3& b, float epsilon)
    {
        REQUIRE(glm::dot(a, b) == Catch::Approx(0.0f).margin(epsilon));
    }

    void RequireMatrixNear(const Mat4& lhs, const Mat4& rhs, float epsilon)
    {
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                REQUIRE(lhs[col][row] == Catch::Approx(rhs[col][row]).margin(epsilon));
            }
        }
    }
}

TEST_CASE("SceneComponent basis remains orthonormal after arbitrary rotation", "[engine][scene][transform][basis]")
{
    Scene scene;
    Actor& actor = scene.SpawnActor<Actor>();
    auto& root = actor.GetComponent<SceneComponent>();

    root.SetRotationEuler(Vector3(33.7f, -127.3f, 71.9f));
    scene.UpdateTransforms();

    const Vector3 forward = root.GetForwardVector();
    const Vector3 right = root.GetRightVector();
    const Vector3 up = root.GetUpVector();

    RequireUnitVector(forward, kBasisEps);
    RequireUnitVector(right, kBasisEps);
    RequireUnitVector(up, kBasisEps);

    RequirePerpendicular(forward, right, kBasisEps);
    RequirePerpendicular(forward, up, kBasisEps);
    RequirePerpendicular(right, up, kBasisEps);
}

TEST_CASE("Long-run repeated rotations stay stable and drift-free", "[engine][scene][transform][stability]")
{
    Scene scene;
    Actor& actor = scene.SpawnActor<Actor>();
    auto& root = actor.GetComponent<SceneComponent>();

    const Vector3 initialPosition(123.456f, -78.9f, 42.0f);
    const Vector3 initialEuler(17.25f, -122.75f, 41.5f);
    const Vector3 stepEuler(0.11f, -0.07f, 0.05f);
    constexpr int32 iterations = 1000;

    root.SetPosition(initialPosition);
    root.SetRotationEuler(initialEuler);

    for (int32 i = 0; i < iterations; ++i)
    {
        root.SetRotationEuler(root.GetRotationEuler() + stepEuler);
        scene.UpdateTransforms();
    }

    const Vector3 expectedEuler = initialEuler + stepEuler * static_cast<float>(iterations);
    const Quaternion expectedQuat = glm::normalize(glm::quat(glm::radians(expectedEuler)));

    const Vector3 expectedForward = glm::normalize(expectedQuat * Vector3(1.0f, 0.0f, 0.0f));
    const Vector3 expectedRight = glm::normalize(expectedQuat * Vector3(0.0f, -1.0f, 0.0f));
    const Vector3 expectedUp = glm::normalize(expectedQuat * Vector3(0.0f, 0.0f, 1.0f));

    const Vector3 forward = root.GetForwardVector();
    const Vector3 right = root.GetRightVector();
    const Vector3 up = root.GetUpVector();
    const Vector3 worldPosition = Vector3(root.GetWorldTransform()[3]);

    REQUIRE(glm::dot(forward, expectedForward) == Catch::Approx(1.0f).margin(1e-3f));
    REQUIRE(glm::dot(right, expectedRight) == Catch::Approx(1.0f).margin(1e-3f));
    REQUIRE(glm::dot(up, expectedUp) == Catch::Approx(1.0f).margin(1e-3f));

    REQUIRE(worldPosition.x == Catch::Approx(initialPosition.x).margin(1e-5f));
    REQUIRE(worldPosition.y == Catch::Approx(initialPosition.y).margin(1e-5f));
    REQUIRE(worldPosition.z == Catch::Approx(initialPosition.z).margin(1e-5f));
}

TEST_CASE("World transform does not drift under repeated updates", "[engine][scene][transform][no-drift]")
{
    Scene scene;
    Actor& actor = scene.SpawnActor<Actor>();

    auto& root = actor.GetComponent<SceneComponent>();
    root.SetPosition(Vector3(10.0f, -5.0f, 2.0f));
    root.SetRotationEuler(Vector3(20.0f, -35.0f, 80.0f));
    root.SetScale(Vector3(1.0f, 1.0f, 1.0f));

    auto& child = actor.AddObjectComponent<SceneComponent>();
    child.SetPosition(Vector3(3.0f, 1.0f, -2.0f));
    child.SetRotationEuler(Vector3(-12.0f, 44.0f, 7.0f));

    scene.UpdateTransforms();
    const Mat4 baseline = child.GetWorldTransform();

    for (int i = 0; i < 1000; ++i)
    {
        scene.UpdateTransforms();
    }

    const Mat4 current = child.GetWorldTransform();
    RequireMatrixNear(current, baseline, kDriftEps);
}

