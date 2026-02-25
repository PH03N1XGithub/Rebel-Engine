#pragma once

struct ActorClassInfo
    {
        String Name;                          // "CameraActor"
        const Rebel::Core::Reflection::TypeInfo* Type;
        Actor* (*CreateFn)(Scene* scene); // factory
    };

    class ActorClassRegistry
    {
    public:
        static ActorClassRegistry& Get()
        {
            static ActorClassRegistry instance;
            return instance;
        }

        void Register(const ActorClassInfo& info)
        {
            for (const auto& existing : m_Classes)
            {
                if (existing.Type == info.Type)
                    return;
            }
            m_Classes.Add(info);
        }

        const Rebel::Core::Memory::TArray<ActorClassInfo>& GetAll() const
        {
            return m_Classes;
        }

    private:
        Rebel::Core::Memory::TArray<ActorClassInfo> m_Classes;
    };

    #define REGISTER_ACTOR_CLASS(TYPE)                                   \
    namespace                                                            \
    {                                                                    \
    struct TYPE##__ActorClassRegistration                             \
    {                                                                \
    TYPE##__ActorClassRegistration()                              \
    {                                                            \
    using namespace Rebel::Engine;                            \
    ActorClassInfo info;                                      \
    info.Name = #TYPE;                                        \
    info.Type = TYPE::StaticType();                           \
    info.CreateFn = [](Scene* scene) -> Actor*               \
    {                                                        \
    return scene->CreateActor<TYPE>();                    \
    };                                                       \
    \
    ActorClassRegistry::Get().Register(info);                \
    }                                                            \
    };                                                               \
    static TYPE##__ActorClassRegistration                             \
    s_##TYPE##__ActorClassRegistration;                           \
    }

    
