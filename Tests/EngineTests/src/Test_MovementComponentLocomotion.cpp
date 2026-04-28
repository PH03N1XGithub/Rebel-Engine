#include "catch_amalgamated.hpp"

#include "Engine/Components/BoxComponent.h"
#include "Engine/Gameplay/Framework/Character.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"
#include "Engine/Gameplay/Framework/Controller.h"
#include "Engine/Gameplay/Framework/GameMode.h"
#include "Engine/Gameplay/Framework/MovementComponent.h"
#include "Engine/Gameplay/Framework/Pawn.h"
#include "Engine/Components/SceneComponent.h"
#include "Engine/Physics/PhysicsModule.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/World.h"

#define private public
#include "Engine/Framework/ModuleManager.h"
#undef private

#include <functional>
#include <sstream>
#include <vector>

namespace
{
    constexpr float kTinyNumber = 1.0e-6f;
    constexpr Vector3 kUp(0.0f, 0.0f, 1.0f);
    constexpr float kSimulationWalkableFloorZ = 0.49f;

    void TickScene(Scene& scene, const float deltaTime)
    {
        scene.PrepareTick();
        scene.TickGroup(ActorTickGroup::PrePhysics, deltaTime);
        scene.TickGroup(ActorTickGroup::PostPhysics, deltaTime);
        scene.TickGroup(ActorTickGroup::PostUpdate, deltaTime);
        scene.FinalizeTick();
    }

    Vector3 NormalizeOrFallback(const Vector3& vector, const Vector3& fallback)
    {
        const float lengthSq = glm::dot(vector, vector);
        if (lengthSq <= kTinyNumber)
            return fallback;

        return vector / glm::sqrt(lengthSq);
    }

    float DistanceToPlane(const Vector3& point, const Vector3& planeNormal)
    {
        return glm::dot(point, planeNormal);
    }

    void RequireOrThrow(const bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    class CharacterMovementComponentHarness final : public CharacterMovementComponent
    {
    public:
        using CharacterMovementComponent::ApplyFalling;
        using CharacterMovementComponent::ApplyCharacterRotation;
        using CharacterMovementComponent::CalcVelocity;
        using CharacterMovementComponent::ClipVelocityAgainstSurface;
        using CharacterMovementComponent::ComputeDesiredFacingDirection;
        using CharacterMovementComponent::ComputeDesiredYaw;
        using CharacterMovementComponent::ComputeGroundMovementDelta;
        using CharacterMovementComponent::ComputeSafeMoveFraction;
        using CharacterMovementComponent::ConstrainToPlane;
        using CharacterMovementComponent::OrientCollisionNormalForSlide;
        using CharacterMovementComponent::ResolvePenetration;
        using CharacterMovementComponent::ShouldUpdateRotation;
        using CharacterMovementComponent::TryApplyGroundSnap;
        using CharacterMovementComponent::UpdateCharacterRotation;
        using CharacterMovementComponent::UpdateMovementModeFromFloor;

        MovementState& MutableState()
        {
            return const_cast<MovementState&>(GetState());
        }
    };

    class RotationTestController final : public Controller
    {
    public:
        void SetYaw(const float yawDegrees)
        {
            SetControlRotation(Vector3(0.0f, 0.0f, yawDegrees));
        }
    };

    void ConfigureGroundedState(CharacterMovementComponentHarness& movement, const Vector3& velocity, const Vector3& floorNormal = kUp)
    {
        MovementState& state = movement.MutableState();
        state = MovementState{};
        state.Mode = MovementMode::Walking;
        state.bGrounded = true;
        state.Velocity = velocity;
        state.CurrentFloor.bBlockingHit = true;
        state.CurrentFloor.bWalkableFloor = true;
        state.CurrentFloor.FloorNormal = NormalizeOrFallback(floorNormal, kUp);
        state.CurrentFloor.FloorDistance = 0.0f;
    }

    struct SimulationConfig
    {
        int32 Frames = 1;
        float DeltaTime = 1.0f / 60.0f;
        Vector3 InitialPosition = Vector3(0.0f);
        Vector3 InitialVelocity = Vector3(0.0f);
        Vector3 ConstantInput = Vector3(0.0f);
        Vector3 FloorNormal = kUp;
        Vector3 WallNormal = Vector3(0.0f);
        bool bStartGrounded = true;
        bool bEnableFloor = true;
        bool bEnableWall = false;
        bool bResolveInitialWallPenetration = false;
        std::function<Vector3(int32, float)> InputForFrame;
    };

    struct SimulationResult
    {
        std::vector<Vector3> Positions;
        std::vector<Vector3> Velocities;
        std::vector<bool> GroundedFrames;
        float MaxIntoWallSpeed = 0.0f;
        float MaxPenetrationDepth = 0.0f;
        float MaxDepenetrationDistance = 0.0f;
        float TotalDepenetrationDistance = 0.0f;
        float MaxGroundedUpwardVelocity = 0.0f;
        int32 FirstReverseFrame = -1;
        bool bEverGrounded = false;

        const Vector3& FinalPosition() const
        {
            return Positions.back();
        }

        const Vector3& FinalVelocity() const
        {
            return Velocities.back();
        }
    };

    FloorResult MakeFloorResult(const Vector3& floorNormal, const float floorDistance, const float walkableFloorZ)
    {
        FloorResult floor{};
        floor.bBlockingHit = true;
        floor.FloorNormal = NormalizeOrFallback(floorNormal, kUp);
        floor.FloorDistance = glm::max(0.0f, floorDistance);
        floor.bWalkableFloor = floor.FloorNormal.z >= walkableFloorZ;
        floor.Hit.bBlockingHit = true;
        floor.Hit.Normal = floor.FloorNormal;
        floor.Hit.Distance = floor.FloorDistance;
        floor.Hit.Position = floor.FloorNormal * floor.FloorDistance;
        return floor;
    }

    void ResolveWallPenetration(
        CharacterMovementComponentHarness& movement,
        SceneComponent& updatedComponent,
        const Vector3& wallNormal,
        const Vector3& moveDelta,
        SimulationResult& result)
    {
        const float penetrationDepth = glm::max(0.0f, -DistanceToPlane(updatedComponent.GetWorldPosition(), wallNormal));
        result.MaxPenetrationDepth = glm::max(result.MaxPenetrationDepth, penetrationDepth);
        if (penetrationDepth <= 0.0f)
            return;

        TraceHit hit{};
        hit.bBlockingHit = true;
        hit.Normal = wallNormal;

        const Vector3 before = updatedComponent.GetWorldPosition();
        const bool resolved = movement.ResolvePenetration(hit, moveDelta);
        REQUIRE(resolved);

        const Vector3 after = updatedComponent.GetWorldPosition();
        const float depenetrationDistance = glm::length(after - before);
        result.MaxDepenetrationDistance = glm::max(result.MaxDepenetrationDistance, depenetrationDistance);
        result.TotalDepenetrationDistance += depenetrationDistance;
        result.MaxPenetrationDepth = glm::max(
            result.MaxPenetrationDepth,
            glm::max(0.0f, -DistanceToPlane(after, wallNormal)));
    }

    Vector3 ResolveWallCollision(
        CharacterMovementComponentHarness& movement,
        const Vector3& start,
        const Vector3& desiredMove,
        const Vector3& wallNormal,
        Vector3& inOutVelocity)
    {
        const Vector3 normalizedWallNormal = NormalizeOrFallback(wallNormal, Vector3(-1.0f, 0.0f, 0.0f));
        const Vector3 target = start + desiredMove;
        const float startDistance = DistanceToPlane(start, normalizedWallNormal);
        const float targetDistance = DistanceToPlane(target, normalizedWallNormal);

        if (targetDistance >= 0.0f)
            return target;

        const float moveDistance = glm::length(desiredMove);
        if (moveDistance <= kTinyNumber)
            return start;

        float hitFraction = 0.0f;
        const float denominator = startDistance - targetDistance;
        if (glm::abs(denominator) > kTinyNumber)
            hitFraction = glm::clamp(startDistance / denominator, 0.0f, 1.0f);

        const float safeFraction = movement.ComputeSafeMoveFraction(moveDistance * hitFraction, moveDistance);
        const Vector3 orientedNormal = movement.OrientCollisionNormalForSlide(normalizedWallNormal, desiredMove, inOutVelocity);
        const Vector3 contactPosition = start + desiredMove * safeFraction;
        const Vector3 remainingMove = movement.ConstrainToPlane(desiredMove * glm::max(0.0f, 1.0f - safeFraction), orientedNormal);

        inOutVelocity = movement.ClipVelocityAgainstSurface(inOutVelocity, orientedNormal);
        return contactPosition + remainingMove;
    }

    SimulationResult SimulateMovement(const SimulationConfig& config)
    {
        CharacterMovementComponentHarness movement;
        SceneComponent updatedComponent;
        updatedComponent.SetWorldPosition(config.InitialPosition);

        movement.SetUpdatedComponent(&updatedComponent);
        movement.BeginPlay();
        movement.SetMaxWalkSpeed(6.0f);
        movement.SetWalkableFloorZ(kSimulationWalkableFloorZ);
        movement.SetWalkableFloorZHysteresis(0.02f);
        movement.SetGroundSnapDistance(0.03f);
        movement.SetGroundedDistanceTolerance(0.05f);
        movement.SetInputDeadZone(0.0f);
        movement.SetMinActiveInputControl(0.0f);
        movement.SetMinAnalogWalkSpeedControl(0.0f);
        movement.SetMinReverseBrakingControl(0.0f);
        movement.SetBrakingSubStepTime(1.0f / 120.0f);

        MovementState& state = movement.MutableState();
        state = MovementState{};
        state.Velocity = config.InitialVelocity;
        state.Mode = config.bStartGrounded ? MovementMode::Walking : MovementMode::Falling;
        state.bGrounded = config.bStartGrounded;
        if (config.bStartGrounded && config.bEnableFloor)
            state.CurrentFloor = MakeFloorResult(config.FloorNormal, 0.0f, kSimulationWalkableFloorZ);

        SimulationResult result;
        result.Positions.reserve(static_cast<size_t>(config.Frames));
        result.Velocities.reserve(static_cast<size_t>(config.Frames));
        result.GroundedFrames.reserve(static_cast<size_t>(config.Frames));

        const Vector3 floorNormal = NormalizeOrFallback(config.FloorNormal, kUp);
        const Vector3 wallNormal = NormalizeOrFallback(config.WallNormal, Vector3(-1.0f, 0.0f, 0.0f));

        for (int32 frame = 0; frame < config.Frames; ++frame)
        {
            const Vector3 start = updatedComponent.GetWorldPosition();
            if (config.bEnableWall && config.bResolveInitialWallPenetration)
                ResolveWallPenetration(movement, updatedComponent, wallNormal, Vector3(0.0f), result);

            const float elapsedTime = static_cast<float>(frame) * config.DeltaTime;
            const Vector3 moveInput = config.InputForFrame
                ? config.InputForFrame(frame, elapsedTime)
                : config.ConstantInput;

            if (state.Mode == MovementMode::Walking)
                movement.CalcVelocity(config.DeltaTime, moveInput);
            else
                movement.ApplyFalling(config.DeltaTime, moveInput);

            Vector3 solvedVelocity = state.Velocity;
            Vector3 desiredMove = state.Mode == MovementMode::Walking
                ? movement.ComputeGroundMovementDelta(solvedVelocity * config.DeltaTime, floorNormal)
                : solvedVelocity * config.DeltaTime;
            Vector3 solvedPosition = start + desiredMove;

            if (config.bEnableWall)
                solvedPosition = ResolveWallCollision(movement, start, desiredMove, wallNormal, solvedVelocity);

            updatedComponent.SetWorldPosition(solvedPosition);
            if (config.bEnableWall)
            {
                ResolveWallPenetration(movement, updatedComponent, wallNormal, desiredMove, result);
                solvedPosition = updatedComponent.GetWorldPosition();
            }

            if (config.bEnableFloor)
            {
                const float signedFloorDistance = DistanceToPlane(solvedPosition, floorNormal);
                if (signedFloorDistance < 0.0f)
                {
                    solvedPosition -= floorNormal * signedFloorDistance;
                    solvedVelocity = movement.ClipVelocityAgainstSurface(solvedVelocity, floorNormal);
                    updatedComponent.SetWorldPosition(solvedPosition);
                }

                state.CurrentFloor = MakeFloorResult(
                    floorNormal,
                    glm::max(0.0f, DistanceToPlane(updatedComponent.GetWorldPosition(), floorNormal)),
                    kSimulationWalkableFloorZ);
                movement.UpdateMovementModeFromFloor(state.CurrentFloor);

                if (state.Mode == MovementMode::Walking && movement.TryApplyGroundSnap())
                {
                    state.CurrentFloor = MakeFloorResult(
                        floorNormal,
                        glm::max(0.0f, DistanceToPlane(updatedComponent.GetWorldPosition(), floorNormal)),
                        kSimulationWalkableFloorZ);
                    movement.UpdateMovementModeFromFloor(state.CurrentFloor);
                }
            }
            else
            {
                state.CurrentFloor = FloorResult{};
                state.Mode = MovementMode::Falling;
                state.bGrounded = false;
            }

            const Vector3 finalPosition = updatedComponent.GetWorldPosition();
            const Vector3 actualVelocity = (finalPosition - start) / glm::max(config.DeltaTime, kTinyNumber);
            if (state.Mode == MovementMode::Walking)
            {
                state.Velocity.x = actualVelocity.x;
                state.Velocity.y = actualVelocity.y;
                state.Velocity.z = 0.0f;
            }
            else
            {
                state.Velocity = actualVelocity;
            }

            if (config.bEnableWall)
            {
                result.MaxIntoWallSpeed = glm::max(
                    result.MaxIntoWallSpeed,
                    glm::max(0.0f, -glm::dot(state.Velocity, wallNormal)));
            }
            if (state.bGrounded)
            {
                result.bEverGrounded = true;
                result.MaxGroundedUpwardVelocity = glm::max(result.MaxGroundedUpwardVelocity, state.Velocity.z);
            }
            if (result.FirstReverseFrame < 0 && state.Velocity.x < 0.0f)
                result.FirstReverseFrame = frame;

            result.Positions.push_back(finalPosition);
            result.Velocities.push_back(state.Velocity);
            result.GroundedFrames.push_back(state.bGrounded);
        }

        return result;
    }

    SimulationResult SimulateMovement(
        const int32 frames,
        const float deltaTime,
        const Vector3& initialVelocity,
        const Vector3& inputVector,
        const Vector3& surfaceNormal)
    {
        const Vector3 normalizedSurfaceNormal = NormalizeOrFallback(surfaceNormal, kUp);
        SimulationConfig config{};
        config.Frames = frames;
        config.DeltaTime = deltaTime;
        config.InitialVelocity = initialVelocity;
        config.ConstantInput = inputVector;

        if (normalizedSurfaceNormal.z >= 0.7f)
        {
            config.bEnableFloor = true;
            config.bStartGrounded = false;
            config.InitialPosition = Vector3(0.0f, 0.0f, 2.0f);
            config.FloorNormal = normalizedSurfaceNormal;
        }
        else
        {
            config.bEnableFloor = true;
            config.bEnableWall = true;
            config.bStartGrounded = true;
            config.InitialPosition = Vector3(-0.5f, 0.0f, 0.0f);
            config.FloorNormal = kUp;
            config.WallNormal = normalizedSurfaceNormal;
        }

        return SimulateMovement(config);
    }
}

TEST_CASE("MovementComponent applies acceleration and translation", "[engine][movement][locomotion]")
{
    Scene scene;
    Pawn& pawn = scene.SpawnActor<Pawn>();
    scene.BeginPlay();

    MovementComponent* movement = pawn.GetMovementComponent();
    REQUIRE(movement != nullptr);

    movement->SetFriction(0.0f);
    movement->SetAirDamping(0.0f);
    movement->SetGravity(0.0f);
    movement->SetMaxSpeed(10000.0f);

    pawn.SetActorLocation(Vector3(0.0f));
    pawn.AddMovementInput(Vector3(1.0f, 0.0f, 0.0f));

    constexpr float deltaTime = 0.1f;
    TickScene(scene, deltaTime);

    const Vector3 velocity = movement->GetVelocity();
    const Vector3 location = pawn.GetActorLocation();

    REQUIRE(velocity.x == Catch::Approx(220.0f));
    REQUIRE(velocity.y == Catch::Approx(0.0f));
    REQUIRE(velocity.z == Catch::Approx(0.0f));
    REQUIRE(location.x == Catch::Approx(22.0f));
}

TEST_CASE("MovementComponent applies friction without input", "[engine][movement][locomotion]")
{
    Scene scene;
    Pawn& pawn = scene.SpawnActor<Pawn>();
    scene.BeginPlay();

    MovementComponent* movement = pawn.GetMovementComponent();
    REQUIRE(movement != nullptr);

    movement->SetMoveAcceleration(0.0f);
    movement->SetFriction(100.0f);
    movement->SetAirDamping(0.0f);
    movement->SetGravity(0.0f);
    movement->SetMaxSpeed(10000.0f);

    pawn.SetActorLocation(Vector3(0.0f));
    movement->SetVelocity(Vector3(100.0f, 0.0f, 0.0f));

    constexpr float deltaTime = 0.1f;
    TickScene(scene, deltaTime);

    const Vector3 velocity = movement->GetVelocity();
    const Vector3 location = pawn.GetActorLocation();

    REQUIRE(velocity.x == Catch::Approx(90.0f));
    REQUIRE(location.x == Catch::Approx(9.0f));
}

TEST_CASE("MovementComponent applies gravity when airborne", "[engine][movement][locomotion]")
{
    Scene scene;
    Pawn& pawn = scene.SpawnActor<Pawn>();
    scene.BeginPlay();

    MovementComponent* movement = pawn.GetMovementComponent();
    REQUIRE(movement != nullptr);

    movement->SetMoveAcceleration(0.0f);
    movement->SetFriction(0.0f);
    movement->SetAirDamping(0.0f);
    movement->SetGravity(1000.0f);

    pawn.SetActorLocation(Vector3(0.0f, 0.0f, 100.0f));
    movement->SetVelocity(Vector3(0.0f));

    constexpr float deltaTime = 0.1f;
    TickScene(scene, deltaTime);

    const Vector3 velocity = movement->GetVelocity();
    const Vector3 location = pawn.GetActorLocation();

    REQUIRE(velocity.z == Catch::Approx(-100.0f));
    REQUIRE(location.z == Catch::Approx(90.0f));
}

TEST_CASE("MovementComponent applies air damping to airborne velocity", "[engine][movement][locomotion]")
{
    Scene scene;
    Pawn& pawn = scene.SpawnActor<Pawn>();
    scene.BeginPlay();

    MovementComponent* movement = pawn.GetMovementComponent();
    REQUIRE(movement != nullptr);

    movement->SetMoveAcceleration(0.0f);
    movement->SetFriction(0.0f);
    movement->SetAirDamping(20.0f);
    movement->SetGravity(0.0f);

    pawn.SetActorLocation(Vector3(0.0f, 0.0f, 100.0f));
    movement->SetVelocity(Vector3(0.0f, 0.0f, -50.0f));

    constexpr float deltaTime = 0.1f;
    TickScene(scene, deltaTime);

    const Vector3 velocity = movement->GetVelocity();
    const Vector3 location = pawn.GetActorLocation();

    REQUIRE(velocity.z == Catch::Approx(-48.0f));
    REQUIRE(location.z == Catch::Approx(95.2f));
}

TEST_CASE("MovementComponent resolves ground contact placeholder", "[engine][movement][locomotion]")
{
    Scene scene;
    Pawn& pawn = scene.SpawnActor<Pawn>();
    scene.BeginPlay();

    MovementComponent* movement = pawn.GetMovementComponent();
    REQUIRE(movement != nullptr);

    movement->SetMoveAcceleration(0.0f);
    movement->SetFriction(0.0f);
    movement->SetAirDamping(0.0f);
    movement->SetGravity(1000.0f);
    movement->SetGroundHeight(0.0f);
    movement->SetGroundTolerance(0.1f);

    pawn.SetActorLocation(Vector3(0.0f, 0.0f, 0.05f));
    movement->SetVelocity(Vector3(0.0f, 0.0f, -100.0f));

    constexpr float deltaTime = 0.1f;
    TickScene(scene, deltaTime);

    const Vector3 velocity = movement->GetVelocity();
    const Vector3 location = pawn.GetActorLocation();

    REQUIRE(movement->IsGrounded());
    REQUIRE(velocity.z == Catch::Approx(0.0f));
    REQUIRE(location.z == Catch::Approx(0.0f));
}

TEST_CASE("MovementComponent clamps horizontal max speed", "[engine][movement][locomotion]")
{
    Scene scene;
    Pawn& pawn = scene.SpawnActor<Pawn>();
    scene.BeginPlay();

    MovementComponent* movement = pawn.GetMovementComponent();
    REQUIRE(movement != nullptr);

    movement->SetMoveAcceleration(10000.0f);
    movement->SetMaxSpeed(300.0f);
    movement->SetFriction(0.0f);
    movement->SetAirDamping(0.0f);
    movement->SetGravity(0.0f);

    constexpr float deltaTime = 0.1f;
    for (int32 i = 0; i < 5; ++i)
    {
        pawn.AddMovementInput(Vector3(1.0f, 0.0f, 0.0f));
        TickScene(scene, deltaTime);
    }

    const Vector3 velocity = movement->GetVelocity();
    const float horizontalSpeed = glm::length(Vector2(velocity.x, velocity.y));
    REQUIRE(horizontalSpeed == Catch::Approx(300.0f));
}

TEST_CASE("CharacterMovement orient-to-movement rotates yaw toward planar input direction", "[engine][movement][locomotion][character][rotation]")
{
    Pawn pawn;
    SceneComponent updatedComponent;
    CharacterMovementComponentHarness movement;

    movement.SetOwner(&pawn);
    movement.SetUpdatedComponent(&updatedComponent);
    movement.SetRotationMode(CharacterRotationMode::OrientToMovement);
    movement.SetRotationRateYaw(90.0f);
    movement.SetUseInputDirectionForRotation(true);

    MovementState& state = movement.MutableState();
    state.Mode = MovementMode::Walking;
    state.bGrounded = true;

    updatedComponent.SetRotationEuler(Vector3(0.0f));
    movement.UpdateCharacterRotation(0.5f, Vector3(0.0f, 1.0f, 0.0f));

    REQUIRE(updatedComponent.GetRotationEuler().z == Catch::Approx(45.0f).margin(1.0e-4f));
}

TEST_CASE("CharacterMovement controller desired rotation rotates toward controller yaw", "[engine][movement][locomotion][character][rotation]")
{
    Pawn pawn;
    SceneComponent updatedComponent;
    CharacterMovementComponentHarness movement;
    RotationTestController controller;

    controller.Possess(&pawn);
    controller.SetYaw(90.0f);

    movement.SetOwner(&pawn);
    movement.SetUpdatedComponent(&updatedComponent);
    movement.SetRotationMode(CharacterRotationMode::UseControllerDesiredRotation);
    movement.SetRotationRateYaw(180.0f);

    MovementState& state = movement.MutableState();
    state.Mode = MovementMode::Walking;
    state.bGrounded = true;

    updatedComponent.SetRotationEuler(Vector3(0.0f));
    movement.UpdateCharacterRotation(0.1f, Vector3(0.0f));

    REQUIRE(updatedComponent.GetRotationEuler().z == Catch::Approx(18.0f).margin(1.0e-4f));
}

TEST_CASE("CharacterMovement input threshold suppresses tiny orient-to-input updates", "[engine][movement][locomotion][character][rotation]")
{
    Pawn pawn;
    SceneComponent updatedComponent;
    CharacterMovementComponentHarness movement;

    movement.SetOwner(&pawn);
    movement.SetUpdatedComponent(&updatedComponent);
    movement.SetRotationMode(CharacterRotationMode::OrientToMovement);
    movement.SetRotationRateYaw(360.0f);

    MovementState& state = movement.MutableState();
    state.Mode = MovementMode::Walking;
    state.bGrounded = true;
    movement.SetUseInputDirectionForRotation(true);
    movement.SetMinRotationInputThreshold(0.2f);
    updatedComponent.SetRotationEuler(Vector3(0.0f, 0.0f, 30.0f));

    movement.UpdateCharacterRotation(0.25f, Vector3(0.1f, 0.0f, 0.0f));

    REQUIRE(updatedComponent.GetRotationEuler().z == Catch::Approx(30.0f).margin(1.0e-4f));
}

TEST_CASE("CharacterMovement speed threshold suppresses tiny orient-to-velocity updates", "[engine][movement][locomotion][character][rotation]")
{
    Pawn pawn;
    SceneComponent updatedComponent;
    CharacterMovementComponentHarness movement;

    movement.SetOwner(&pawn);
    movement.SetUpdatedComponent(&updatedComponent);
    movement.SetRotationMode(CharacterRotationMode::OrientToMovement);
    movement.SetRotationRateYaw(360.0f);
    movement.SetUseInputDirectionForRotation(false);
    movement.SetMinRotationSpeedThreshold(0.2f);

    MovementState& state = movement.MutableState();
    state.Mode = MovementMode::Walking;
    state.bGrounded = true;
    state.Velocity = Vector3(0.1f, 0.0f, 0.0f);

    updatedComponent.SetRotationEuler(Vector3(0.0f, 0.0f, -20.0f));
    movement.UpdateCharacterRotation(0.25f, Vector3(0.0f));

    REQUIRE(updatedComponent.GetRotationEuler().z == Catch::Approx(-20.0f).margin(1.0e-4f));
}

TEST_CASE("CharacterMovement falling rotation can stay locked when air rotation is disabled", "[engine][movement][locomotion][character][rotation]")
{
    Pawn pawn;
    SceneComponent updatedComponent;
    CharacterMovementComponentHarness movement;

    movement.SetOwner(&pawn);
    movement.SetUpdatedComponent(&updatedComponent);
    movement.SetRotationMode(CharacterRotationMode::OrientToMovement);
    movement.SetRotationRateYaw(360.0f);
    movement.SetRotateInAir(false);

    MovementState& state = movement.MutableState();
    state.Mode = MovementMode::Falling;
    state.bGrounded = false;

    updatedComponent.SetRotationEuler(Vector3(0.0f, 0.0f, 15.0f));
    movement.UpdateCharacterRotation(0.25f, Vector3(0.0f, 1.0f, 0.0f));

    REQUIRE(updatedComponent.GetRotationEuler().z == Catch::Approx(15.0f).margin(1.0e-4f));
}

TEST_CASE("CharacterMovement falling rotation uses air rotation multiplier when enabled", "[engine][movement][locomotion][character][rotation]")
{
    Pawn pawn;
    SceneComponent updatedComponent;
    CharacterMovementComponentHarness movement;

    movement.SetOwner(&pawn);
    movement.SetUpdatedComponent(&updatedComponent);
    movement.SetRotationMode(CharacterRotationMode::OrientToMovement);
    movement.SetRotationRateYaw(180.0f);
    movement.SetRotateInAir(true);
    movement.SetAirRotationRateMultiplier(0.5f);

    MovementState& state = movement.MutableState();
    state.Mode = MovementMode::Falling;
    state.bGrounded = false;

    updatedComponent.SetRotationEuler(Vector3(0.0f));
    movement.UpdateCharacterRotation(0.1f, Vector3(0.0f, 1.0f, 0.0f));

    REQUIRE(updatedComponent.GetRotationEuler().z == Catch::Approx(9.0f).margin(1.0e-4f));
}

TEST_CASE("CharacterMovement harness wall slide simulation keeps velocity parallel to wall", "[engine][movement][locomotion][character][wall]")
{
    SimulationConfig config{};
    config.Frames = 120;
    config.DeltaTime = 1.0f / 60.0f;
    config.InitialPosition = Vector3(-0.5f, 0.0f, 0.0f);
    config.InitialVelocity = Vector3(0.0f);
    config.ConstantInput = glm::normalize(Vector3(1.0f, 0.35f, 0.0f));
    config.bStartGrounded = true;
    config.bEnableFloor = true;
    config.bEnableWall = true;
    config.FloorNormal = kUp;
    config.WallNormal = Vector3(-1.0f, 0.0f, 0.0f);

    const SimulationResult result = SimulateMovement(config);
    bool bObservedWallContact = false;
    for (size_t index = 1; index < result.Positions.size(); ++index)
    {
        if (result.Positions[index].x > -0.01f && result.Positions[index - 1].x > -0.01f)
        {
            bObservedWallContact = true;
            REQUIRE(result.Velocities[index].x == Catch::Approx(0.0f).margin(0.03f));
        }
    }

    REQUIRE(bObservedWallContact);
    REQUIRE(result.FinalPosition().x <= 0.0025f);
    REQUIRE(result.FinalVelocity().x == Catch::Approx(0.0f).margin(0.03f));
    REQUIRE(result.FinalVelocity().y > 0.25f);
}

TEST_CASE("CharacterMovement slope landing simulation does not relaunch upward after grounding", "[engine][movement][locomotion][character][slope]")
{
    const std::vector<float> slopeAnglesDegrees{ 0.0f, 15.0f, 30.0f, 45.0f, 60.0f };

    for (const float slopeAngleDegrees : slopeAnglesDegrees)
    {
        const float radians = glm::radians(slopeAngleDegrees);
        const Vector3 slopeNormal = glm::normalize(Vector3(-glm::sin(radians), 0.0f, glm::cos(radians)));

        const SimulationResult result = SimulateMovement(120, 1.0f / 60.0f, Vector3(0.0f, 0.0f, -1.0f), Vector3(0.0f), slopeNormal);

        REQUIRE(result.bEverGrounded);
        REQUIRE(result.FinalVelocity().z == Catch::Approx(0.0f).margin(1.0e-4f));
        REQUIRE(result.MaxGroundedUpwardVelocity == Catch::Approx(0.0f).margin(1.0e-4f));
    }
}

TEST_CASE("CharacterMovement walking result stays stable across delta-time partitions", "[engine][movement][locomotion][character][dt]")
{
    auto runPartition = [](const float deltaTime, const int32 frames)
    {
        SimulationConfig config{};
        config.Frames = frames;
        config.DeltaTime = deltaTime;
        config.InitialPosition = Vector3(0.0f);
        config.InitialVelocity = Vector3(0.0f);
        config.bStartGrounded = true;
        config.bEnableFloor = true;
        config.FloorNormal = kUp;
        config.InputForFrame = [](const int32, const float elapsedTime)
        {
            if (elapsedTime < 0.60f)
                return Vector3(1.0f, 0.0f, 0.0f);
            if (elapsedTime < 1.20f)
                return Vector3(0.35f, 0.0f, 0.0f);

            return Vector3(-0.80f, 0.0f, 0.0f);
        };

        return SimulateMovement(config);
    };

    const SimulationResult dt016 = runPartition(0.016f, 120);
    const SimulationResult dt008 = runPartition(0.008f, 240);
    const SimulationResult dt004 = runPartition(0.004f, 480);

    REQUIRE(dt016.FinalPosition().x == Catch::Approx(dt008.FinalPosition().x).margin(0.08f));
    REQUIRE(dt016.FinalPosition().x == Catch::Approx(dt004.FinalPosition().x).margin(0.08f));
    REQUIRE(dt016.FinalVelocity().x == Catch::Approx(dt008.FinalVelocity().x).margin(0.06f));
    REQUIRE(dt016.FinalVelocity().x == Catch::Approx(dt004.FinalVelocity().x).margin(0.06f));
    REQUIRE(dt008.FinalVelocity().x == Catch::Approx(dt004.FinalVelocity().x).margin(0.04f));
}

TEST_CASE("CharacterMovement reversal simulation reverses direction within expected time", "[engine][movement][locomotion][character][reversal]")
{
    SimulationConfig config{};
    config.Frames = 120;
    config.DeltaTime = 1.0f / 60.0f;
    config.InitialPosition = Vector3(0.0f);
    config.InitialVelocity = Vector3(6.0f, 0.0f, 0.0f);
    config.ConstantInput = Vector3(-1.0f, 0.0f, 0.0f);
    config.bStartGrounded = true;
    config.bEnableFloor = true;
    config.FloorNormal = kUp;

    const SimulationResult result = SimulateMovement(config);

    REQUIRE(result.FirstReverseFrame >= 0);
    REQUIRE(result.FirstReverseFrame <= 45);
    REQUIRE(result.FinalVelocity().x < -0.5f);

    float previousVelocityX = config.InitialVelocity.x;
    for (const Vector3& velocity : result.Velocities)
    {
        REQUIRE(velocity.x <= previousVelocityX + 0.05f);
        previousVelocityX = velocity.x;
    }
}

TEST_CASE("CharacterMovement penetration correction remains stable during repeated wall collisions", "[engine][movement][locomotion][character][penetration]")
{
    SimulationConfig config{};
    config.Frames = 120;
    config.DeltaTime = 1.0f / 60.0f;
    config.InitialPosition = Vector3(0.001f, 0.0f, 0.0f);
    config.InitialVelocity = Vector3(0.0f);
    config.ConstantInput = Vector3(1.0f, 0.1f, 0.0f);
    config.bStartGrounded = true;
    config.bEnableFloor = true;
    config.bEnableWall = true;
    config.bResolveInitialWallPenetration = true;
    config.FloorNormal = kUp;
    config.WallNormal = Vector3(-1.0f, 0.0f, 0.0f);

    const SimulationResult result = SimulateMovement(config);

    REQUIRE(result.MaxPenetrationDepth < 0.0025f);
    REQUIRE(result.MaxDepenetrationDistance < 0.03f);
    REQUIRE(result.TotalDepenetrationDistance < 0.5f);
    REQUIRE(result.FinalPosition().x <= 0.0025f);
}

TEST_CASE("CharacterMovement parameterized slope landing sweep stays grounded without upward launch", "[engine][movement][locomotion][character][slope][sweep]")
{
    const std::vector<int32> slopeAnglesDegrees{ 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60 };

    for (const int32 slopeAngleDegrees : slopeAnglesDegrees)
    {
        const float radians = glm::radians(static_cast<float>(slopeAngleDegrees));
        const Vector3 slopeNormal = glm::normalize(Vector3(-glm::sin(radians), 0.0f, glm::cos(radians)));

        SimulationConfig config{};
        config.Frames = 120;
        config.DeltaTime = 1.0f / 60.0f;
        config.InitialPosition = Vector3(0.0f, 0.0f, 2.0f);
        config.InitialVelocity = Vector3(1.5f, 0.0f, -0.5f);
        config.ConstantInput = Vector3(0.0f);
        config.bStartGrounded = false;
        config.bEnableFloor = true;
        config.FloorNormal = slopeNormal;

        const SimulationResult result = SimulateMovement(config);

        REQUIRE(result.bEverGrounded);
        REQUIRE(result.GroundedFrames.back());
        REQUIRE(result.FinalVelocity().z == Catch::Approx(0.0f).margin(1.0e-4f));
        REQUIRE(result.MaxGroundedUpwardVelocity == Catch::Approx(0.0f).margin(1.0e-4f));
    }
}

TEST_CASE("CharacterMovement parameterized wall collision sweep preserves sliding tangent", "[engine][movement][locomotion][character][wall][sweep]")
{
    const std::vector<int32> wallAnglesDegrees{ 0, 15, 30, 45, 60, 75, 90, 120, 150 };

    for (const int32 wallAngleDegrees : wallAnglesDegrees)
    {
        const float radians = glm::radians(static_cast<float>(wallAngleDegrees));
        const Vector3 wallNormal = glm::normalize(Vector3(-glm::cos(radians), -glm::sin(radians), 0.0f));
        const Vector3 tangent = glm::normalize(Vector3(-wallNormal.y, wallNormal.x, 0.0f));

        SimulationConfig config{};
        config.Frames = 120;
        config.DeltaTime = 1.0f / 60.0f;
        config.InitialPosition = wallNormal * 0.5f;
        config.InitialVelocity = Vector3(0.0f);
        config.ConstantInput = glm::normalize((-wallNormal * 0.9f) + (tangent * 0.4f));
        config.bStartGrounded = true;
        config.bEnableFloor = true;
        config.bEnableWall = true;
        config.FloorNormal = kUp;
        config.WallNormal = wallNormal;

        const SimulationResult result = SimulateMovement(config);
        const float tangentSpeed = glm::dot(result.FinalVelocity(), tangent);
        bool bObservedWallContact = false;
        for (size_t index = 1; index < result.Positions.size(); ++index)
        {
            if (glm::dot(result.Positions[index], wallNormal) < 0.01f
                && glm::dot(result.Positions[index - 1], wallNormal) < 0.01f)
            {
                bObservedWallContact = true;
                REQUIRE(glm::dot(result.Velocities[index], wallNormal) == Catch::Approx(0.0f).margin(0.04f));
            }
        }

        REQUIRE(bObservedWallContact);
        REQUIRE(glm::dot(result.FinalVelocity(), wallNormal) == Catch::Approx(0.0f).margin(0.04f));
        REQUIRE(tangentSpeed > 0.1f);
    }
}

TEST_CASE("CharacterMovement safe-move contact offset stays distance-based across move sizes", "[engine][movement][locomotion][character][math]")
{
    CharacterMovementComponentHarness movement;

    const float shortMoveDistance = 1.0f;
    const float longMoveDistance = 10.0f;
    const float shortHitDistance = 0.60f;
    const float longHitDistance = 6.00f;

    const float shortSafeFraction = movement.ComputeSafeMoveFraction(shortHitDistance, shortMoveDistance);
    const float longSafeFraction = movement.ComputeSafeMoveFraction(longHitDistance, longMoveDistance);
    const float shortAppliedDistance = shortSafeFraction * shortMoveDistance;
    const float longAppliedDistance = longSafeFraction * longMoveDistance;
    const float shortStandOff = shortHitDistance - shortAppliedDistance;
    const float longStandOff = longHitDistance - longAppliedDistance;

    REQUIRE(shortStandOff == Catch::Approx(longStandOff).margin(1.0e-4f));
    REQUIRE(shortAppliedDistance < shortHitDistance);
    REQUIRE(longAppliedDistance < longHitDistance);
}

TEST_CASE("CharacterMovement depenetration on uphill slope stays bounded", "[engine][movement][locomotion][character][math]")
{
    CharacterMovementComponentHarness movement;
    SceneComponent updated;
    updated.SetWorldPosition(Vector3(0.0f, 0.0f, 0.0f));
    movement.SetUpdatedComponent(&updated);

    TraceHit hit{};
    hit.Normal = glm::normalize(Vector3(-0.6f, 0.0f, 0.8f));

    const bool resolved = movement.ResolvePenetration(hit, Vector3(0.0f, 0.0f, -0.25f));
    REQUIRE(resolved);

    const Vector3 displacement = updated.GetWorldPosition();
    const float pushDistance = glm::length(displacement);

    REQUIRE(pushDistance < 0.03f);
    REQUIRE(displacement.z > 0.0f);
}

namespace
{
    constexpr float kWorldSimulationDt = 1.0f / 60.0f;
    constexpr float kPositionTolerance = 0.12f;
    constexpr float kVelocityTolerance = 0.20f;

    struct WorldSimulationContext
    {
        Scene TestScene;
        ModuleManager Modules;
        World TestWorld;
        PhysicsModule* Physics = nullptr;
        uint64 FrameId = 0;

        WorldSimulationContext()
            : TestWorld(&TestScene, &Modules)
        {
            TestScene.SetWorld(&TestWorld);

            auto physicsModule = RUniquePtr<IModule>(new PhysicsModule());
            Physics = static_cast<PhysicsModule*>(physicsModule.Get());
            Modules.m_Modules.Add(std::move(physicsModule));
            Modules.m_ModulesByType[TickType::Physics].Add(Physics);

            Physics->Init();
            REQUIRE(Physics->GetPhysicsSystem() != nullptr);
            Physics->GetPhysicsSystem()->SetFixedDeltaTime(1.0f / 120.0f);
            Physics->GetPhysicsSystem()->SetMaxSubsteps(16);
        }

        ~WorldSimulationContext()
        {
            Modules.ShutdownModules();
        }

        void BeginPlay()
        {
            TestWorld.BeginPlay();
        }

        void WarmUpPhysics(const int32 steps = 2)
        {
            REQUIRE(Physics != nullptr);
            REQUIRE(Physics->GetPhysicsSystem() != nullptr);

            for (int32 index = 0; index < steps; ++index)
            {
                Physics->GetPhysicsSystem()->Step(TestScene, 1.0f / 120.0f);
                TestScene.UpdateTransforms();
            }
        }

        void Tick(const float deltaTime)
        {
            TestWorld.Tick(deltaTime, true, ++FrameId);
        }
    };

    glm::quat RotationFromDegrees(const Vector3& eulerDegrees)
    {
        return glm::quat(glm::radians(eulerDegrees));
    }

    Vector3 TransformPoint(const Vector3& center, const glm::quat& rotation, const Vector3& localPoint)
    {
        return center + (rotation * localPoint);
    }

    Vector3 ComputeRampCenterFromLowPoint(const Vector3& lowTopPoint, const Vector3& halfExtent, const glm::quat& rotation)
    {
        return lowTopPoint - (rotation * Vector3(-halfExtent.x, 0.0f, halfExtent.z));
    }

    Vector3 ComputeBoxTopPoint(const Vector3& center, const Vector3& halfExtent, const glm::quat& rotation, const float localX, const float localY = 0.0f)
    {
        return TransformPoint(center, rotation, Vector3(localX, localY, halfExtent.z));
    }

    Vector3 ComputeTopNormal(const glm::quat& rotation)
    {
        return NormalizeOrFallback(rotation * kUp, kUp);
    }

    BoxComponent& SpawnStaticBox(
        WorldSimulationContext& context,
        const Vector3& center,
        const Vector3& halfExtent,
        const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    {
        Actor& actor = context.TestScene.SpawnActor<Actor>();
        auto& box = actor.AddObjectComponent<BoxComponent>();
        box.BodyType = ERBBodyType::Static;
        box.ObjectChannel = CollisionChannel::WorldStatic;
        box.HalfExtent = halfExtent;
        actor.SetRootComponent(&box);
        box.SetWorldPosition(center);
        box.SetWorldRotationQuat(rotation);
        return box;
    }

    Character& SpawnCharacter(WorldSimulationContext& context, const Vector3& worldPosition)
    {
        Character& character = context.TestScene.SpawnActor<Character>();
        character.SetActorLocation(worldPosition);
        return character;
    }

    CharacterMovementComponent& GetMovement(Character& character)
    {
        CharacterMovementComponent* movement = character.GetCharacterMovementComponent();
        REQUIRE(movement != nullptr);
        return *movement;
    }

    void ConfigureMovementForTests(CharacterMovementComponent& movement)
    {
        movement.SetInputDeadZone(0.0f);
        movement.SetMinActiveInputControl(0.0f);
        movement.SetMinAnalogWalkSpeedControl(0.0f);
        movement.SetMinReverseBrakingControl(0.0f);
        movement.SetBrakingSubStepTime(1.0f / 120.0f);
        movement.SetGroundSnapDistance(0.04f);
        movement.SetGroundedDistanceTolerance(0.05f);
        movement.SetFloorSweepDistance(0.12f);
    }

    void SimulateCharacter(
        WorldSimulationContext& context,
        Character& character,
        const int32 frames,
        const float deltaTime,
        const std::function<Vector3(int32)>& inputForFrame)
    {
        for (int32 frame = 0; frame < frames; ++frame)
        {
            const Vector3 input = inputForFrame ? inputForFrame(frame) : Vector3(0.0f);
            if (glm::dot(input, input) > kTinyNumber)
                character.AddMovementInput(input);

            context.Tick(deltaTime);
        }
    }

    void SimulateConstantInput(
        WorldSimulationContext& context,
        Character& character,
        const int32 frames,
        const Vector3& input,
        const float deltaTime = kWorldSimulationDt)
    {
        SimulateCharacter(context, character, frames, deltaTime, [input](const int32) { return input; });
    }

    void SettleCharacter(WorldSimulationContext& context, Character& character, const int32 frames = 8)
    {
        SimulateConstantInput(context, character, frames, Vector3(0.0f));
    }
}

TEST_CASE("CharacterMovement world simulation walks deterministically on flat ground", "[engine][movement][locomotion][character][world][flat]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));

    Character& character = SpawnCharacter(context, Vector3(-5.0f, 0.0f, 1.02f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character);
    SimulateConstantInput(context, character, 120, Vector3(1.0f, 0.0f, 0.0f));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;

    REQUIRE(position.x > -1.5f);
    REQUIRE(position.z == Catch::Approx(1.0f).margin(kPositionTolerance));
    REQUIRE(position.y == Catch::Approx(0.0f).margin(kPositionTolerance));
    REQUIRE(velocity.x > 1.0f);
    REQUIRE(velocity.y == Catch::Approx(0.0f).margin(kVelocityTolerance));
    REQUIRE(velocity.z == Catch::Approx(0.0f).margin(kVelocityTolerance));
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation walks uphill on a gentle ramp", "[engine][movement][locomotion][character][world][ramp]")
{
    WorldSimulationContext context;

    const Vector3 rampHalfExtent(8.0f, 4.0f, 0.15f);
    const glm::quat rampRotation = RotationFromDegrees(Vector3(0.0f, -20.0f, 0.0f));
    const Vector3 rampLowPoint(-6.0f, 0.0f, 0.0f);
    const Vector3 rampCenter = ComputeRampCenterFromLowPoint(rampLowPoint, rampHalfExtent, rampRotation);
    SpawnStaticBox(context, rampCenter, rampHalfExtent, rampRotation);

    const Vector3 surfacePoint = ComputeBoxTopPoint(rampCenter, rampHalfExtent, rampRotation, -7.2f);
    const Vector3 surfaceNormal = ComputeTopNormal(rampRotation);
    Character& character = SpawnCharacter(context, surfacePoint + surfaceNormal * 1.02f);

    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character, 12);
    const Vector3 startPosition = character.GetActorLocation();
    SimulateConstantInput(context, character, 40, Vector3(1.0f, 0.0f, 0.0f));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;
    REQUIRE(position.x > startPosition.x + 1.0f);
    {
        std::ostringstream oss;
        oss << "Expected ramp ascent, startZ=" << startPosition.z << " finalZ=" << position.z;
        RequireOrThrow(position.z > startPosition.z + 0.05f, oss.str());
    }
    REQUIRE(velocity.x > 0.2f);
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation rejects steep slopes as walking support", "[engine][movement][locomotion][character][world][steep-slope]")
{
    WorldSimulationContext context;

    const Vector3 rampHalfExtent(2.0f, 2.0f, 0.15f);
    const glm::quat rampRotation = RotationFromDegrees(Vector3(0.0f, 60.0f, 0.0f));
    const Vector3 rampLowPoint(0.0f, 0.0f, 0.0f);
    const Vector3 rampCenter = ComputeRampCenterFromLowPoint(rampLowPoint, rampHalfExtent, rampRotation);
    SpawnStaticBox(context, rampCenter, rampHalfExtent, rampRotation);

    const Vector3 steepSurfacePoint = ComputeBoxTopPoint(rampCenter, rampHalfExtent, rampRotation, -1.5f);
    const Vector3 steepSurfaceNormal = ComputeTopNormal(rampRotation);
    Character& character = SpawnCharacter(context, steepSurfacePoint + steepSurfaceNormal * 1.02f);
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SimulateConstantInput(context, character, 45, Vector3(0.0f));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;
    REQUIRE(!movement.GetCurrentFloor().bWalkableFloor);
    REQUIRE(!movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Falling);
    REQUIRE(velocity.z < -0.05f);
    REQUIRE(position.z < steepSurfacePoint.z + 1.0f);
}

TEST_CASE("CharacterMovement world simulation steps onto a valid low obstacle", "[engine][movement][locomotion][character][world][stepup][success]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));
    SpawnStaticBox(context, Vector3(2.0f, 0.0f, 0.04f), Vector3(2.0f, 2.0f, 0.04f));

    Character& character = SpawnCharacter(context, Vector3(-1.5f, 0.0f, 1.02f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character);
    SimulateConstantInput(context, character, 45, Vector3(1.0f, 0.0f, 0.0f));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;
    REQUIRE(position.x > 0.8f);
    {
        std::ostringstream oss;
        oss << "Expected low-step elevation, finalPos=(" << position.x << "," << position.y << "," << position.z << ")";
        RequireOrThrow(position.z > 1.04f, oss.str());
    }
    REQUIRE(velocity.x > 0.2f);
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation keeps low step landing valid near the ledge", "[engine][movement][locomotion][character][world][stepup][edge-landing]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));
    SpawnStaticBox(context, Vector3(1.0f, 0.0f, 0.04f), Vector3(1.0f, 0.75f, 0.04f));

    Character& character = SpawnCharacter(context, Vector3(-1.5f, 0.62f, 1.02f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character);
    SimulateConstantInput(context, character, 50, Vector3(1.0f, 0.0f, 0.0f));
    SettleCharacter(context, character, 12);

    const Vector3 position = character.GetActorLocation();
    const FloorResult& floor = movement.GetCurrentFloor();

    {
        std::ostringstream oss;
        oss << "Expected near-edge low step to stay grounded after StepUp, finalPos=("
            << position.x << "," << position.y << "," << position.z << ") floorDist=" << floor.FloorDistance;
        RequireOrThrow(position.z > 1.04f, oss.str());
    }
    REQUIRE(floor.bBlockingHit);
    REQUIRE(floor.bWalkableFloor);
    REQUIRE(floor.bPerched);
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation fails to step onto a too-tall obstacle", "[engine][movement][locomotion][character][world][stepup][failure]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));
    SpawnStaticBox(context, Vector3(1.0f, 0.0f, 0.12f), Vector3(1.0f, 2.0f, 0.12f));

    Character& character = SpawnCharacter(context, Vector3(-1.5f, 0.0f, 1.02f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character);
    SimulateConstantInput(context, character, 60, Vector3(1.0f, 0.0f, 0.0f));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;
    REQUIRE(position.x < 0.35f);
    REQUIRE(position.z < 1.18f);
    REQUIRE(velocity.x < 1.0f);
    REQUIRE(movement.GetState().Mode != MovementMode::Walking || movement.GetState().bGrounded);
}

TEST_CASE("CharacterMovement world simulation slides along a wall while walking", "[engine][movement][locomotion][character][world][wall-slide]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));
    SpawnStaticBox(context, Vector3(0.55f, 0.0f, 1.5f), Vector3(0.05f, 20.0f, 1.5f));

    Character& character = SpawnCharacter(context, Vector3(-2.0f, -1.0f, 1.02f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character);
    SimulateConstantInput(context, character, 90, glm::normalize(Vector3(1.0f, 1.0f, 0.0f)));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;
    REQUIRE(position.x < 0.35f);
    REQUIRE(position.y > 0.75f);
    REQUIRE(velocity.y > 0.35f);
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation resolves a two-wall corridor slide deterministically", "[engine][movement][locomotion][character][world][corner-crease]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));

    const glm::quat lowerWallRotation = RotationFromDegrees(Vector3(0.0f, 0.0f, 20.0f));
    const glm::quat upperWallRotation = RotationFromDegrees(Vector3(0.0f, 0.0f, -20.0f));
    SpawnStaticBox(context, Vector3(0.0f, -1.25f, 1.5f), Vector3(6.0f, 0.05f, 1.5f), lowerWallRotation);
    SpawnStaticBox(context, Vector3(0.0f, 1.25f, 1.5f), Vector3(6.0f, 0.05f, 1.5f), upperWallRotation);

    Character& character = SpawnCharacter(context, Vector3(-3.0f, 0.0f, 1.02f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character);
    SimulateConstantInput(context, character, 110, glm::normalize(Vector3(1.0f, 0.15f, 0.0f)));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;
    {
        std::ostringstream oss;
        oss << "Expected two-wall progress, finalPos=(" << position.x << "," << position.y << "," << position.z << ")";
        RequireOrThrow(position.x > -0.8f, oss.str());
    }
    REQUIRE(glm::abs(position.y) < 1.1f);
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation finds floor support after settling", "[engine][movement][locomotion][character][world][floor-detection]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));

    Character& character = SpawnCharacter(context, Vector3(0.0f, 0.0f, 1.35f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SimulateConstantInput(context, character, 45, Vector3(0.0f));

    const FloorResult& floor = movement.GetCurrentFloor();
    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;

    REQUIRE(floor.bBlockingHit);
    REQUIRE(floor.bWalkableFloor);
    REQUIRE(floor.FloorDistance <= 0.05f);
    REQUIRE(position.z == Catch::Approx(1.0f).margin(kPositionTolerance));
    REQUIRE(velocity.x == Catch::Approx(0.0f).margin(kVelocityTolerance));
    REQUIRE(velocity.z == Catch::Approx(0.0f).margin(kVelocityTolerance));
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation lands from falling onto a floor", "[engine][movement][locomotion][character][world][landing]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));

    Character& character = SpawnCharacter(context, Vector3(0.0f, 0.0f, 3.5f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SimulateConstantInput(context, character, 180, Vector3(0.25f, 0.0f, 0.0f));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;

    REQUIRE(position.z == Catch::Approx(1.0f).margin(kPositionTolerance));
    REQUIRE(position.x > 0.1f);
    REQUIRE(velocity.z == Catch::Approx(0.0f).margin(kVelocityTolerance));
    REQUIRE(movement.GetCurrentFloor().bWalkableFloor);
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}

TEST_CASE("CharacterMovement world simulation chains step-up into ramp traversal", "[engine][movement][locomotion][character][world][step-ramp]")
{
    WorldSimulationContext context;
    SpawnStaticBox(context, Vector3(0.0f, 0.0f, -0.5f), Vector3(20.0f, 20.0f, 0.5f));
    SpawnStaticBox(context, Vector3(1.2f, 0.0f, 0.04f), Vector3(1.2f, 2.0f, 0.04f));

    const Vector3 rampHalfExtent(6.0f, 4.0f, 0.12f);
    const glm::quat rampRotation = RotationFromDegrees(Vector3(0.0f, -10.0f, 0.0f));
    const Vector3 rampLowPoint(2.4f, 0.0f, 0.08f);
    const Vector3 rampCenter = ComputeRampCenterFromLowPoint(rampLowPoint, rampHalfExtent, rampRotation);
    SpawnStaticBox(context, rampCenter, rampHalfExtent, rampRotation);

    Character& character = SpawnCharacter(context, Vector3(-1.5f, 0.0f, 1.02f));
    context.BeginPlay();
    context.WarmUpPhysics();

    CharacterMovementComponent& movement = GetMovement(character);
    ConfigureMovementForTests(movement);

    SettleCharacter(context, character);
    SimulateConstantInput(context, character, 55, Vector3(1.0f, 0.0f, 0.0f));

    const Vector3 position = character.GetActorLocation();
    const Vector3 velocity = movement.GetState().Velocity;
    REQUIRE(position.x > 2.0f);
    {
        std::ostringstream oss;
        oss << "Expected step+ramp climb, finalPos=(" << position.x << "," << position.y << "," << position.z << ")";
        RequireOrThrow(position.z > 1.04f, oss.str());
    }
    REQUIRE(velocity.x > 0.15f);
    REQUIRE(movement.GetState().bGrounded);
    REQUIRE(movement.GetState().Mode == MovementMode::Walking);
}
