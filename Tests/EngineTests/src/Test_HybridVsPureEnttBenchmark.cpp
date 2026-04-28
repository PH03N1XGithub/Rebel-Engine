#include "catch_amalgamated.hpp"
#include "Engine/Components/Components.h"

#include <ThirdParty/entt.h>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
	// Concrete primitive for hybrid pointer-storage path.
	struct BenchmarkHybridPrimitive final : ::PrimitiveComponent
	{
		PhysicsShape CreatePhysicsShape() const override
		{
			return PhysicsShape::MakeBox(Vector3(0.5f, 0.5f, 0.5f));
		}
	};

	// Pure EnTT value component used for direct-storage iteration.
	// Note: engine PrimitiveComponent is abstract, so it cannot be emplaced by value.
	struct BenchmarkPurePrimitive
	{
		Vector3 Position = Vector3(0.0f, 0.0f, 0.0f);
	};
}

TEST_CASE("Hybrid vs Pure EnTT Iteration Benchmark (Non-assertive)", "[benchmark]")
{
	constexpr int EntityCount = 10000;
	constexpr int Iterations = 1000;
	constexpr int WarmupIterations = 100;
	const Vector3 Delta(1.0f, 0.0f, 0.0f);

#ifndef NDEBUG
	std::cout << "[benchmark] Warning: non-Release build; timing values are not representative.\n";
#endif

	double hybridMs = 0.0;
	double pureMs = 0.0;

	// Case A: Hybrid Actor+ECS-like storage (registry stores PrimitiveComponent*)
	{
		entt::registry registry;
		std::vector<std::unique_ptr<BenchmarkHybridPrimitive>> ownedComponents;
		ownedComponents.reserve(EntityCount);

		for (int i = 0; i < EntityCount; ++i)
		{
			entt::entity e = registry.create();
			auto comp = std::make_unique<BenchmarkHybridPrimitive>();
			comp->SetWorldPosition(Vector3(0.0f, 0.0f, 0.0f));
			registry.emplace<::PrimitiveComponent*>(e, comp.get());
			ownedComponents.emplace_back(std::move(comp));
		}

		auto view = registry.view<::PrimitiveComponent*>();

		for (int i = 0; i < WarmupIterations; ++i)
		{
			for (auto e : view)
			{
				::PrimitiveComponent* comp = view.get<::PrimitiveComponent*>(e);
				const Vector3 pos = comp->GetWorldPosition();
				comp->SetWorldPosition(pos + Delta);
			}
		}

		for (auto& comp : ownedComponents)
		{
			comp->SetWorldPosition(Vector3(0.0f, 0.0f, 0.0f));
		}

		const auto start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < Iterations; ++i)
		{
			for (auto e : view)
			{
				::PrimitiveComponent* comp = view.get<::PrimitiveComponent*>(e);
				const Vector3 pos = comp->GetWorldPosition();
				comp->SetWorldPosition(pos + Delta);
			}
		}
		const auto end = std::chrono::high_resolution_clock::now();
		hybridMs = std::chrono::duration<double, std::milli>(end - start).count();

		bool foundExpectedX = false;
		for (const auto& comp : ownedComponents)
		{
			const float x = comp->GetWorldPosition().x;
			if (x == Catch::Approx(static_cast<float>(Iterations)).margin(1e-4f))
			{
				foundExpectedX = true;
				break;
			}
		}
		REQUIRE(foundExpectedX);
	}

	// Case B: Pure EnTT value storage (direct component access, no pointer indirection)
	{
		entt::registry registry;
		for (int i = 0; i < EntityCount; ++i)
		{
			entt::entity e = registry.create();
			auto& comp = registry.emplace<BenchmarkPurePrimitive>(e);
			comp.Position = Vector3(0.0f, 0.0f, 0.0f);
		}

		auto view = registry.view<BenchmarkPurePrimitive>();

		for (int i = 0; i < WarmupIterations; ++i)
		{
			for (auto e : view)
			{
				auto& comp = view.get<BenchmarkPurePrimitive>(e);
				comp.Position += Delta;
			}
		}

		for (auto e : view)
		{
			auto& comp = view.get<BenchmarkPurePrimitive>(e);
			comp.Position = Vector3(0.0f, 0.0f, 0.0f);
		}

		const auto start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < Iterations; ++i)
		{
			for (auto e : view)
			{
				auto& comp = view.get<BenchmarkPurePrimitive>(e);
				comp.Position += Delta;
			}
		}
		const auto end = std::chrono::high_resolution_clock::now();
		pureMs = std::chrono::duration<double, std::milli>(end - start).count();

		bool foundExpectedX = false;
		for (auto e : view)
		{
			const auto& comp = view.get<BenchmarkPurePrimitive>(e);
			if (comp.Position.x == Catch::Approx(static_cast<float>(Iterations)).margin(1e-4f))
			{
				foundExpectedX = true;
				break;
			}
		}
		REQUIRE(foundExpectedX);
	}

	const double ratio = (pureMs > 0.0) ? (hybridMs / pureMs) : 0.0;

	std::cout << "Hybrid Time (ms): " << hybridMs << "\n";
	std::cout << "Pure EnTT Time (ms): " << pureMs << "\n";
	std::cout << "Performance Ratio (Hybrid / Pure): " << ratio << "\n";
}

