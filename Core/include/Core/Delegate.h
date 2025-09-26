#pragma once
#include <functional>
#include <algorithm>
#include "Containers/TArray.h"

namespace Rebel::Core
{
    // =========================
    // Single-cast Delegate
    // =========================
    template<typename... Args>
    class TDelegate
    {
    public:
        using FuncType = std::function<void(Args...)>;

        void Bind(FuncType func) { BoundFunc = std::move(func); }

        void BindStatic(void(*func)(Args...)) { BoundFunc = func; }

        template<typename T>
        void BindRaw(T* obj, void(T::*method)(Args...)) {
            BoundFunc = [obj, method](Args... args) {
                if (obj)  // << safety check
                    (obj->*method)(std::forward<Args>(args)...);
            };
        }

        void Unbind() { BoundFunc = nullptr; }

        void Broadcast(Args... args) const {
            if (BoundFunc) BoundFunc(std::forward<Args>(args)...);
        }

        Rebel::Core::Bool IsBound() const { return (Rebel::Core::Bool)BoundFunc; }

    private:
        FuncType BoundFunc;
    };


    // =========================
    // Multicast Delegate
    // =========================
    template<typename... Args>
    class TMulticastDelegate
    {
    public:
        using FuncType = std::function<void(Args...)>;

        void Add(FuncType func) { Listeners.Emplace(std::move(func)); }

        void AddStatic(void(*func)(Args...)) { Listeners.Emplace(func); }

        template<typename T>
        void AddRaw(T* obj, void(T::*method)(Args...)) {
            Listeners.Emplace([obj, method](Args... args) {
                if (obj)  // << safety check
                    (obj->*method)(std::forward<Args>(args)...);
            });
        }

        void Clear() { Listeners.clear(); }

        void Broadcast(Args... args) const {
            for (auto& listener : Listeners) {
                listener(args...);
            }
        }

        Rebel::Core::Bool IsEmpty() const { return Listeners.IsEmpty(); }

    private:
        Rebel::Core::Memory::TArray<FuncType> Listeners;
    };



}

// =========================
// Macros
// =========================
#define DECLARE_DELEGATE(DelegateName, ...) \
using DelegateName = Rebel::Core::TDelegate<__VA_ARGS__>;

#define DECLARE_MULTICAST_DELEGATE(DelegateName, ...) \
using DelegateName = Rebel::Core::TMulticastDelegate<__VA_ARGS__>;