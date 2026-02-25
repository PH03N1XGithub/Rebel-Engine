#pragma once
#include <algorithm>
#include <cassert>
#include <span>
#include <utility>
#include "Core/CoreMemory.h"
#include "Core/CoreMacros.h"

namespace Rebel::Core::Memory {

    // Dynamic array similar to std::vector with optional inline storage
    template<typename T, MemSize InlineCapacity = 0, typename Allocator = DefaultAllocator>
    class TArray {
    public:

        // Constructor
        explicit TArray(const Allocator& inAllocator = Allocator{})
        : allocator(inAllocator)
        {
            // If inline storage is enabled, use the inline buffer as the initial data
            if constexpr (InlineCapacity > 0) {
                data = reinterpret_cast<T*>(inlineBuffer);
                capacity = InlineCapacity;
            }
        }

        // Destructor
        ~TArray() {
            Clear(); // Destroy all contained elements
            if (usingHeap && data) {
                allocator.Free(data, sizeof(*data) * capacity); // Free heap memory if allocated
            }
        }

        // Copy constructor
        TArray(const TArray& other)
            : TArray(other.allocator) // Forward allocator
        {
            Reserve(other.count); // Ensure enough capacity
            for (MemSize i = 0; i < other.count; i++) {
                new(&data[count++]) T(other.data[i]); // Copy construct each element
            }
        }

        // Copy assignment 
        TArray& operator=(const TArray& other) {
            if (this != &other) {
                Clear(); // Destroy existing elements
                Reserve(other.count); // Allocate new memory if needed
                for (MemSize i = 0; i < other.count; i++) {
                    new(&data[count++]) T(other.data[i]); // Copy construct elements
                }
            }
            return *this;
        }

        // Move constructor
        TArray(TArray&& other) noexcept
            : allocator(other.allocator), data(other.data), count(other.count),
              capacity(other.capacity), usingHeap(other.usingHeap)
        {
            if constexpr (InlineCapacity > 0) {
                if (!other.usingHeap) {
                    // Move elements from other's inline buffer to this inline buffer
                    data = reinterpret_cast<T*>(inlineBuffer);
                    capacity = InlineCapacity;
                    for (MemSize i = 0; i < count; i++) {
                        new(&data[i]) T(std::move(other.data[i]));
                        other.data[i].~T();
                    }
                }
                else {
                    // If other used heap, just take ownership
                    other.data = nullptr;
                }
            }
            other.count = 0;
            other.capacity = 0;
            other.usingHeap = false;
        }

        // Move assignment
        TArray& operator=(TArray&& other) noexcept {
            if (this != &other) {
                Clear(); // Destroy current elements
                if (usingHeap && data) {
                    allocator.Free(data , sizeof(*data) * capacity); // Free heap memory if needed
                }

                allocator = other.allocator;
                count = other.count;
                usingHeap = other.usingHeap;

                if constexpr (InlineCapacity > 0) {
                    if (!other.usingHeap) {
                        // Move elements from other's inline buffer to this inline buffer
                        data = reinterpret_cast<T*>(inlineBuffer);
                        capacity = InlineCapacity;
                        for (MemSize i = 0; i < count; i++) {
                            new(&data[i]) T(std::move(other.data[i]));
                            other.data[i].~T();
                        }
                    }
                    else {
                        // Take ownership of other's heap memory
                        data = other.data;
                        capacity = other.capacity;
                        other.data = nullptr;
                    }
                }
                else {
                    // No inline buffer, just take ownership
                    data = other.data;
                    capacity = other.capacity;
                    other.data = nullptr;
                }

                // Reset other
                other.count = 0;
                other.capacity = 0;
                other.usingHeap = false;
            }
            return *this;
        }

        // Element access
        T& operator[](MemSize idx) {
            assert(idx < count && "TArray index out of range");
            return data[idx];
        }
        const T& operator[](MemSize idx) const {
            assert(idx < count && "TArray index out of range");
            return data[idx];
        }

        T& Front() { assert(count > 0); return data[0]; }       // First element
        T& Back() const { assert(count > 0); return data[count-1]; } // Last element

        // Modifiers push_back
        void Add(const T& value) {
            EnsureCapacity(count + 1);           // Make sure we have room
            new(&data[count++]) T(value);        // Copy construct at the end
        }
        // Modifiers push_back
        void Add(T&& value) {
            EnsureCapacity(count + 1);
            new(&data[count++]) T(std::move(value)); // Move construct at the end
        }


        // Construct in-place at the end
        template<typename... Args>
        T& Emplace(Args&&... args) {
            EnsureCapacity(count + 1);
            T* elem = new(&data[count++]) T(std::forward<Args>(args)...);
            return *elem;
        }

        /*void RemoveAt(MemSize idx) {
            assert(idx < count && "TArray RemoveAt out of range");
            data[idx].~T();
            for (MemSize i = idx; i < count-1; i++) {
                new(&data[i]) T(std::move(data[i+1]));
                data[i+1].~T();
            }
            --count;
        }*/

        // Remove element at a specific index
        void RemoveAt(MemSize idx) {
            /*assert(idx < count && "TArray RemoveAt out of range");

            if constexpr (std::is_trivially_copyable_v<T>) {
                // Shift memory directly
                std::memmove(&data[idx], &data[idx + 1], (count - idx - 1) * sizeof(T));
            } else {
                data[idx].~T();
                for (MemSize i = idx; i < count-1; i++) {
                    new(&data[i]) T(std::move(data[i+1]));
                    data[i+1].~T();
                }
            }

            --count;*/
            assert(idx < count && "TArray RemoveAt out of range");
            data[idx].~T();
            for (MemSize i = idx; i < count-1; i++) {
                new(&data[i]) T(std::move(data[i+1]));
                data[i+1].~T();
            }
            --count;
        }

        void EraseAtSwap(MemSize idx)
        {
            assert(idx < count && "TArray EraseAtSwap out of range");
            MemSize last = count - 1;

            if (idx != last)
            {
                // Destroy the element at idx
                data[idx].~T();
                // Move last element into idx
                new (&data[idx]) T(std::move(data[last]));
                // Destroy moved-from last
                data[last].~T();
            }
            else
            {
                // Just pop last
                data[last].~T();
            }

            --count;
        }

        // Remove last element
        void PopBack() {
            assert(count > 0);
            data[--count].~T();
        }

        // Reduce capacity to fit the current number of elements
        void ShrinkToFit() {
            if (usingHeap && count < capacity) {
                Reallocate(count);
            }
        }

        // Insert element at a specific index
        void Insert(MemSize idx, const T& value) {
            /*assert(idx <= count);
            EnsureCapacity(count + 1);
            for (MemSize i = count; i > idx; --i) {
                new(&data[i]) T(std::move(data[i-1]));
                data[i-1].~T();
            }
            new(&data[idx]) T(value);
            ++count;*/

            assert(idx <= count);
            EnsureCapacity(count + 1);

            if constexpr (std::is_trivially_copyable_v<T>) {
                // Shift block with memmove
                std::memmove(&data[idx+1], &data[idx], (count - idx) * sizeof(T));
            } else {
                // Shift manually
                new(&data[count]) T(std::move(data[count-1]));
                for (MemSize i = count-1; i > idx; --i) {
                    data[i].~T();
                    new(&data[i]) T(std::move(data[i-1]));
                }
                data[idx].~T();
            }

            new(&data[idx]) T(value);
            ++count;
        }

        // Destroy all elements but keep capacity
        void Clear() {
            for (MemSize i = 0; i < count; i++) {
                data[i].~T();
            }
            count = 0;
        }

        void Fill(const T& value)
        {
            for (MemSize i = 0; i < count; i++)
                data[i] = value;
        }


        // Pre-allocate memory
        void Reserve(MemSize newCap) {
            if (newCap <= capacity) return;
            Reallocate(newCap);
        }

        // Resize array, constructing or destroying elements as needed
        void Resize(MemSize newSize) {
            if (newSize < count) {
                for (MemSize i = newSize; i < count; i++) {
                    data[i].~T();
                }
            } else {
                EnsureCapacity(newSize);
                for (MemSize i = count; i < newSize; i++) {
                    new(&data[i]) T(); // Default construct
                }
            }
            count = newSize;
        }

        // Info
        MemSize Num() const { return count; }
        MemSize Capacity() const { return capacity; }
        Bool IsEmpty() const { return count == 0; }

        T* Data() { return data; }
        const T* Data() const { return data; }

        T* begin() { return data; }
        T* end()   { return data + count; }

        const T* begin() const { return data; }
        const T* end()   const { return data + count; }
    

    private:
        // Ensure enough capacity, growing as needed
        void EnsureCapacity(MemSize needed) {
            if (needed > capacity) {
                MemSize newCap = capacity == 0 ? 4 : capacity * 2;
                newCap = std::max(newCap, needed);
                Reallocate(newCap);
            }
        }

        // Reallocate memory to a new capacity
        void Reallocate(MemSize newCap) {
            T* newData = static_cast<T*>(allocator.Allocate(sizeof(T) * newCap, alignof(T)));
            if (!ENSURE(newData))
            {
                return;
            }
            
            //assert(newData && "TArray allocation failed");

            // Move or memcpy elements depending on type
            if constexpr (std::is_trivially_copyable_v<T>) {
                if (count > 0) std::memcpy(newData, data, count * sizeof(T));
            } else {
                for (MemSize i = 0; i < count; i++) {
                    new(&newData[i]) T(std::move(data[i]));
                    data[i].~T();
                }
            }

            if (usingHeap && data) allocator.Free(data, sizeof(*data) * capacity);

            data = newData;
            capacity = newCap;
            usingHeap = true;
        }

        /*IAllocator* allocator = nullptr;                 // Memory allocator
        static DefaultAllocator defaultAllocator;        // Default allocator if none provided*/
        Allocator allocator; // stored inline, no indirection
        T* data = nullptr;                               // Pointer to array data
        MemSize count = 0;                              // Number of elements
        MemSize capacity = 0;                           // Allocated capacity
        Bool usingHeap = false;                          // Are we using heap memory?

        // Inline buffer for small arrays
        alignas(T) char inlineBuffer[sizeof(T) * (InlineCapacity > 0 ? InlineCapacity : 1)];
    };

    /*// Define static default allocator
    template<typename T, MemSize InlineCapacity>
    DefaultAllocator TArray<T, InlineCapacity>::defaultAllocator;*/

} // namespace Rebel::Core::Memory

// Alias for TArray
template<typename T, Rebel::Core::MemSize InlineCapacity = 0, typename Allocator = Rebel::Core::Memory::DefaultAllocator>
using TArray = Rebel::Core::Memory::TArray<T, InlineCapacity, Allocator>;
