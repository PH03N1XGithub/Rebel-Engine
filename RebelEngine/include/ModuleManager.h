#pragma once
#include "BaseModule.h"
#include "Core/Containers/TMap.h"
DEFINE_LOG_CATEGORY(ModuleManagerLog)

class ModuleManager
{
public:
	void RegisterModules()
	{
		using namespace Rebel::Core::Reflection;

		const Rebel::Core::Memory::TMap<String, Rebel::Core::Reflection::TypeInfo*>& types =
			TypeRegistry::Get().GetTypes();
		const TypeInfo* moduleType = TypeRegistry::Get().GetType("IModule");
		if (!moduleType)
			return;

		for (const auto& pair : types)
		{
			const String&   typeName = pair.Key;    // map key
			const TypeInfo* typeInfo = pair.Value;  // ✅ Value is TypeInfo*

			if (!typeInfo)
				continue;

			if (typeInfo->IsA(moduleType) && typeInfo->CreateInstance)
			{
				RB_LOG(ModuleManagerLog, trace, "Discovered Module: {}", typeName.c_str());

				IModule* moduleInstance =
					static_cast<IModule*>(typeInfo->CreateInstance());

				m_Modules.Add(RUniquePtr<IModule>(moduleInstance));
				m_ModulesByType[moduleInstance->GetTickType()].Add(moduleInstance);
			}
		}
	}

	template<typename T>
	T* GetModule()
	{
		// Get the TypeInfo of the module class
		const Rebel::Core::Reflection::TypeInfo* typeInfo = T::StaticType();
		if (!typeInfo)
			return nullptr;

		for (auto& module : m_Modules)
		{
			if (!module) continue;

			const Rebel::Core::Reflection::TypeInfo* modType = module->GetType();
			if (modType && modType->IsA(typeInfo))
			{
				return static_cast<T*>(module.Get());
			}
		}

		return nullptr;
	}

	void InitModules()
	{
		for (auto& module : m_Modules)
		{
			module->Init();
		}
	}

	void ShutdownModules()
	{
		for (auto& module : m_Modules)
		{
			module->Shutdown();
		}
	}

	void TickModulesByType(TickType type, float deltaTime)
	{
		auto* modules = m_ModulesByType.Find(type);
		if (!modules) return;

		for (IModule* module : *modules)
		{
			module->Tick(deltaTime);
		}
	}

	void TickAll(float deltaTime)
	{
		// Custom tick order
		static constexpr TickType TickOrder[] = {
			TickType::PrePhysics,
			TickType::Physics,
			TickType::PostPhysics,
			TickType::PreTick,
			TickType::Tick,
			TickType::PostTick,
			TickType::PreRender,
			TickType::Render,
			TickType::PostRender,
		};

		for (TickType type : TickOrder)
		{
			TickModulesByType(type, deltaTime);
		}
	}
private:
	friend class BaseEngine;
	TArray<RUniquePtr<IModule>> m_Modules; // owns the modules
	TMap<TickType, TArray<IModule*>> m_ModulesByType; // for fast ticking
};
