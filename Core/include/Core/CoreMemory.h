#pragma once
#include <cstddef>
#include <utility>
#include <atomic>
#include <new>
#include <type_traits>
#include "CoreTypes.h"
#include "CoreMacros.h"

namespace Rebel::Core::Memory {
   

    

    struct IAllocator
    {
        virtual void* Allocate(MemSize size, MemSize alignment = alignof(std::max_align_t)) = 0;
        virtual void Free(void* ptr, MemSize size) = 0;
        virtual ~IAllocator() = default;
    };
    // Inline allocator (just a helper)
    template<typename T, MemSize InlineCapacity = 0>
    struct TInlineAllocator {
        alignas(T) char buffer[sizeof(T) * InlineCapacity];
        T* Data() { return reinterpret_cast<T*>(buffer); }
        constexpr MemSize Capacity() const { return InlineCapacity; }
    };
    
    struct DefaultAllocator : IAllocator
    {
        void* Allocate(MemSize size, MemSize alignment = alignof(std::max_align_t)) override {
            //std::cout << "DefaultAllocator::Allocate" << size << std::endl;
            return std::malloc(size);
        }
        void Free(void* ptr, MemSize size) override {
            //std::cout << "DefaultAllocator::Free" << size<< std::endl;
            std::free(ptr);
        }
    };

    

    // ===================== ControlBlock =====================
    struct ControlBlock
    {
        std::atomic<MemSize> sharedCount{1};
        std::atomic<MemSize> weakCount{0};
        void* ptr = nullptr;
        void (*destroy_ptr)(void*) = nullptr;
        void (*delete_control)(ControlBlock*) = nullptr;
    };
    
    // ===================== Forward Declarations =====================
    template<typename T> class WeakPtr;
    
    template<typename T>
    class SharedPtr
    {
    public:
        SharedPtr() : control(nullptr) {}
        SharedPtr(const SharedPtr& o) : control(o.control)
        { if(control) control->sharedCount.fetch_add(1,std::memory_order_acq_rel); }
    
        SharedPtr(SharedPtr&& o) noexcept : control(o.control) { o.control=nullptr; }
    
        ~SharedPtr() { Release(); }
    
        SharedPtr& operator=(const SharedPtr& o)
        {
            if(this != &o)
            {
                Release();
                control = o.control;
                if(control) control->sharedCount.fetch_add(1,std::memory_order_acq_rel);
            }
            return *this;
        }
    
        SharedPtr& operator=(SharedPtr&& o) noexcept
        {
            if(this != &o)
            {
                Release();
                control = o.control;
                o.control = nullptr;
            }
            return *this;
        }
    
        T* Get() const { return control ? static_cast<T*>(control->ptr) : nullptr; }
        T& operator*() const { return *static_cast<T*>(control->ptr); }
        T* operator->() const { return static_cast<T*>(control->ptr); }
        explicit operator bool() const { return control && control->ptr; }
    
        MemSize UseCount() const { return control ? control->sharedCount.load(std::memory_order_acquire) : 0; }
    
        WeakPtr<T> Weak() const;

        void Reset(T* ptr = nullptr) {
            Release();
            if(ptr) {
                // Create a temporary ControlBlock for the raw pointer
                ControlBlock* cb = new ControlBlock();
                cb->ptr = ptr;
                cb->destroy_ptr = [](void* p){ delete static_cast<T*>(p); };
                cb->delete_control = [](ControlBlock* c){ delete c; };
                control = cb;
            }
        }
    
    private:
        friend class WeakPtr<T>;
    
        template<typename U, typename... Args> 
        friend SharedPtr<U> MakeShared(Args&&... args);
    
        explicit SharedPtr(ControlBlock* cb) : control(cb) {}
    
        void Release()
        {
            if(!control) return;
            if(control->sharedCount.fetch_sub(1,std::memory_order_acq_rel) == 1)
            {
                control->destroy_ptr(control->ptr);
                control->ptr = nullptr;
                if(control->weakCount.load(std::memory_order_acquire) == 0)
                    control->delete_control(control);
            }
            control = nullptr;
        }
    
        ControlBlock* control = nullptr;
    };
    
    // ===================== WeakPtr =====================
    template<typename T>
    class WeakPtr
    {
    public:
        WeakPtr() : control(nullptr) {}
        WeakPtr(const SharedPtr<T>& sp) : control(sp.control)
        { if(control) control->weakCount.fetch_add(1,std::memory_order_acq_rel); }
        WeakPtr(const WeakPtr& o) : control(o.control)
        { if(control) control->weakCount.fetch_add(1,std::memory_order_acq_rel); }
        WeakPtr(WeakPtr&& o) noexcept : control(o.control) { o.control = nullptr; }
    
        ~WeakPtr()
        {
            if(!control) return;
            if(control->weakCount.fetch_sub(1,std::memory_order_acq_rel) == 1)
                if(control->sharedCount.load(std::memory_order_acquire) == 0)
                    control->delete_control(control);
            control = nullptr;
        }
    
        WeakPtr& operator=(const WeakPtr& o)
        {
            if(this != &o)
            {
                this->~WeakPtr();
                control = o.control;
                if(control) control->weakCount.fetch_add(1,std::memory_order_acq_rel);
            }
            return *this;
        }
    
        WeakPtr& operator=(WeakPtr&& o) noexcept 
        {
            if(this != &o)
            {
                this->~WeakPtr();
                control = o.control;
                o.control = nullptr;
            }
            return *this;
        }
    
        SharedPtr<T> Lock() const
        {
            if(!control) return SharedPtr<T>();
            MemSize cnt = control->sharedCount.load(std::memory_order_acquire);
            while(cnt != 0)
                if(control->sharedCount.compare_exchange_weak(cnt,cnt+1,std::memory_order_acq_rel))
                    return SharedPtr<T>(control);
            return SharedPtr<T>();
        }
    
    private:
        ControlBlock* control = nullptr;
    };
    
    template<typename T>
    WeakPtr<T> SharedPtr<T>::Weak() const { return WeakPtr<T>(*this); }
    
    // ===================== UniquePtr =====================
    template<typename T>
    class UniquePtr
    {
    public:
        explicit UniquePtr(T* p=nullptr) : ptr(p) {}
        UniquePtr(UniquePtr&& o) noexcept : ptr(o.ptr) { o.ptr=nullptr; }
        UniquePtr& operator=(UniquePtr&& o) noexcept { if(this!=&o){ delete ptr; ptr=o.ptr; o.ptr=nullptr; } return *this; }
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;
        ~UniquePtr(){ delete ptr; }
    
        T* Get() const { return ptr; }
        T* operator->() const { return ptr; }
        T& operator*() const { return *ptr; }
        explicit operator bool() const { return ptr!=nullptr; }
    
        T* Release(){ T* t=ptr; ptr=nullptr; return t; }
        void Reset(T* p=nullptr){ if(ptr!=p){ delete ptr; ptr=p; } }
    
    private:
        T* ptr;
    };
    
    // Array specialization
    template<typename T>
    class UniquePtr<T[]>
    {
    public:
        explicit UniquePtr(T* p=nullptr) : ptr(p) {}
        UniquePtr(UniquePtr&& o) noexcept : ptr(o.ptr){ o.ptr=nullptr; }
        UniquePtr& operator=(UniquePtr&& o) noexcept { if(this!=&o){ delete[] ptr; ptr=o.ptr; o.ptr=nullptr; } return *this; }
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;
        ~UniquePtr(){ delete[] ptr; }
    
        T& operator[](MemSize i) const { return ptr[i]; }
        T* Get() const { return ptr; }
        explicit operator bool() const { return ptr!=nullptr; }
    
        T* Release(){ T* t=ptr; ptr=nullptr; return t; }
        void Reset(T* p=nullptr){ if(ptr!=p){ delete[] ptr; ptr=p; } }
    
    private:
        T* ptr;
    };
    
    // ===================== Helper functions =====================
    
    // Single object
    template<typename T, typename... Args>
    SharedPtr<T> MakeShared(Args&&... args)
    {
        STATIC_ASSERT(!std::is_array_v<T>, "Use MakeShared<T[]>(count) for arrays");
    
        constexpr MemSize ctrlSize = sizeof(ControlBlock);
        const MemSize align = alignof(T);
        char* raw = static_cast<char*>(::operator new(ctrlSize + sizeof(T) + align));
    
        ControlBlock* cb = new (raw) ControlBlock();
    
        void* objMem = raw + ctrlSize;
        MemSize space = sizeof(T) + align;
        void* alignedMem = std::align(align, sizeof(T), objMem, space);
        if(!alignedMem) throw std::bad_alloc();
    
        T* obj = new (alignedMem) T(std::forward<Args>(args)...);
    
        cb->ptr = obj;
        cb->destroy_ptr = [](void* p){ static_cast<T*>(p)->~T(); };
        cb->delete_control = [](ControlBlock* c){ c->~ControlBlock(); ::operator delete(static_cast<void*>(c)); };
    
        return SharedPtr<T>(cb);
    }
    
    // Array version
    template<typename T>
    SharedPtr<T> MakeShared(MemSize count)
    {
        STATIC_ASSERT(std::is_array_v<T>, "Use MakeShared<T[]>(count) for arrays only");
        using ElemType = std::remove_extent_t<T>;
    
        ElemType* arr = new ElemType[count]();
    
        ControlBlock* cb = new ControlBlock();
        cb->ptr = arr;
        cb->destroy_ptr = [count](void* p){ delete[] static_cast<ElemType*>(p); (void)count; };
        cb->delete_control = [](const ControlBlock* c){ delete c; };
    
        return SharedPtr<T>(cb);
    }
    
    // UniquePtr helpers
    template<typename T, typename... Args>
    UniquePtr<T> MakeUnique(Args&&... args)
    {
        STATIC_ASSERT(!std::is_array_v<T>, "Use MakeUnique<T[]>(count) for arrays");
        return UniquePtr<T>(new T(std::forward<Args>(args)...));
    }
    
    template<typename T>
    UniquePtr<T[]> MakeUnique(MemSize count)
    {
        STATIC_ASSERT(std::is_array_v<T>, "Use MakeUnique<T[]>(count) for arrays");
        using ElemType = std::remove_extent_t<T>;
        return UniquePtr<T[]>(new ElemType[count]());
    }
    
} // namespace Rebel::Core::Memory
