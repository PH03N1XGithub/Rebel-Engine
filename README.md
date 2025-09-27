# Rebel Engine - Core Module

Rebel Engine Core module provides foundational systems for memory management, containers, math, reflection, serialization, multithreading, logging, and profiling. It is designed for high-performance C++ game development with custom allocators, low-level optimizations, and reflection-driven serialization.

## Table of Contents

- [Memory](#memory)
  - [TArray (Dynamic Array)](#tarray-dynamic-array)
  - [TMap (Hash Map)](#tmap-hash-map)
- [Math](#math)
- [Logging](#logging)
- [Delegates](#delegates)
  - [Single-cast TDelegate](#single-cast-tdelegate)
  - [Multi-cast TMulticastDelegate](#multi-cast-tmulticastdelegate)
- [Threading](#threading)
- [Reflection](#reflection)
- [Serialization](#serialization)
- [Profiling](#profiling)

## Memory

### TArray (Dynamic Array)

TArray is a high-performance dynamic array, similar to `std::vector`, with optional inline storage optimization to avoid heap allocations for small arrays.

```cpp
Rebel::Core::Memory::TArray<int> Numbers;
Numbers.Add(42);
Numbers.Emplace(7);

for (auto& n : Numbers) {
    std::cout << n << "\n";
}
```

**Features:**

- Inline buffer optimization (`InlineCapacity`)
- Copy, move constructors/assignments
- Insert, remove, resize
- Supports trivially copyable and non-trivial types
- Iterators (`begin()/end()`) for range-based for loops

**Usage Example:**

```cpp
Rebel::Core::Memory::TArray<String, 8> Names; // Inline storage up to 8 strings
Names.Add("Alice");
Names.Add("Bob");
Names.RemoveAt(0); // Removes "Alice"
```

### TMap (Hash Map)

TMap is a hash map optimized for cache efficiency and SIMD TopHash checks.

```cpp
Rebel::Core::Memory::TMap<String, int> Scores;
Scores.Add("Player1", 100);
Scores["Player2"] = 200;

int* pScore = Scores.Find("Player1");
if (pScore) std::cout << *pScore;
```

**Features:**

- Open addressing with tombstones
- SIMD-assisted TopHash comparison (16 bytes per step)
- Automatic growth when count >= 50% capacity
- Move and copy support

## Math

The math module wraps `glm` for vector, matrix, and quaternion math.

```cpp
using Vector3 = Rebel::Core::Math::vec3;
using Quaternion = Rebel::Core::Math::quat;

Vector3 Position(1.0f, 2.0f, 3.0f);
Quaternion Rot = glm::angleAxis(glm::radians(90.0f), Vector3(0,1,0));
```

**Aliases:**

- Vector2, Vector3, Vector4
- Mat3, Mat4
- Quaternion

## Logging

Custom logging system using `spdlog` for categorized logs.

```cpp
DEFINE_LOG_CATEGORY(CoreLog);
RB_LOG(CoreLog, info, "Engine started with version: %s", "0.1.0");
```

**Log Levels:**
- trace, debug, info, warn, error, critical

## Delegates

### Single-cast TDelegate

```cpp
DECLARE_DELEGATE(FSimpleDelegate, int, float);
FSimpleDelegate MyDelegate;
MyDelegate.BindRaw(this, &MyClass::OnEvent);
MyDelegate.Broadcast(42, 3.14f);
```

### Multi-cast TMulticastDelegate

```cpp
DECLARE_MULTICAST_DELEGATE(FMultiDelegate, const String&);
FMultiDelegate MultiDelegate;
MultiDelegate.Add([](const String& msg){ std::cout << msg; });
MultiDelegate.Broadcast("Hello World");
```

**Features:**

- Bind raw member functions, static functions, lambdas
- Safe unbinding
- Multi-cast supports multiple listeners

## Threading

`BucketScheduler` allows batching of tasks into buckets processed by worker threads.

```cpp
BucketScheduler scheduler(4, 2);
scheduler.AddTask(0, []{ std::cout << "Task 0"; });
scheduler.SetBucketCallback(0, []{ std::cout << "Bucket 0 complete"; });
scheduler.WaitForAllTasks();
```

**Features:**

- Task bucketing
- Async chaining via delegates
- Thread-safe task execution
- Atomic task counters for synchronization

## Reflection

Reflection system for runtime type info, property iteration, and editor/serialization flags.

```cpp
REFLECTABLE_CLASS(MyClass, void)
REFLECT_CLASS(MyClass, void)
    REFLECT_PROPERTY(MyClass, Health, Rebel::Core::Reflection::EPropertyFlags::SaveGame)
    REFLECT_PROPERTY(MyClass, Name, Rebel::Core::Reflection::EPropertyFlags::Editable)
END_REFLECT_CLASS(MyClass)
```

**Features:**

- `EPropertyFlags` (VisibleInEditor, SaveGame, Transient, Editable)
- Type introspection: `IsA()`, `GetType()`
- Generic property access via `PropertyInfo::Get<T>(obj)`

## Serialization

`YamlSerializer` implements `ISerializer` for YAML-based save/load.

```cpp
YamlSerializer serializer;
serializer.BeginObject("Player");
serializer.Write("Health", 100);
serializer.Write("Name", String("Alice"));
serializer.EndObject();
serializer.SaveToFile("savegame.yaml");
```

**Features:**

- Key-value write/read (int, float, bool, String)
- Nested object support
- Automatic reflection-based serialization/deserialization

```cpp
YamlSerializer::Serialize(serializer, myObject);
YamlSerializer::Deserialize(loader, myObject);
```

## Profiling

Scoped timers for performance measurement:

```cpp
PROFILE_SCOPE("HeavyComputation");
// code to profile

ScopedTimer timer("UpdateLoop");
float ms = timer.ElapsedMillis();
```

**Features:**

- RAII-based timers
- Millisecond and second precision
- Integrated with logging for debug output

