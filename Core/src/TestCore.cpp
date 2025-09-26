#include "Core/CorePch.h"
#include "Core/Core.h"
#include "Core/Delegate.h"
#include "Core/MultiThreading/BucketScheduler.h"
#include "Core/Serialization/YamlSerializer.h"

namespace Rebel::Core
{
    static float s_Time = 0.0f;

    DEFINE_LOG_CATEGORY(TaskLog);
    
    static std::atomic<int> s_Counter = 0;
    // Declare delegates
    DECLARE_DELEGATE(FOnGameStarted)
    DECLARE_MULTICAST_DELEGATE(FOnEnemyDied, int)

    void GlobalEnemyKilled(int id) {
        printf("Global function: Enemy %d killed\n", id * 2);
    }
    class my_class
    {
    public:
        FOnEnemyDied OnEnemyDied; 
        
    };
    my_class my_obj;
    /*class Player {
    public:
        
        void binddelegate()
        {
            my_obj.OnEnemyDied.AddStatic(GlobalEnemyKilled);
            my_obj.OnEnemyDied.AddRaw(this, &Player::OnEnemyKilled);
        }
        void OnEnemyKilled(int id) {
            printf("Player notified: Enemy %d died\n", id);
        }
        void mttest()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
            auto a = s_Counter++;
            auto b = s_Time;
            RB_LOG(TaskLog, info, "player Running task {}, {} time, Frame {}", 1000,a, b)
        }
    };*/
    
    inline void PrintType(const Reflection::TypeInfo* type)
    {
        using namespace Rebel::Core::Reflection;
        if (!type) return;

        std::cout << "Type: " << type->Name.c_str() << " (size: " << type->Size << ")\n";

        for (auto t = type; t; t = t->Super)
        {
            for (auto& prop : t->Properties)
            {
                std::cout << "  Property: " << prop.Name.c_str()
                          << " (offset: " << prop.Offset
                          << ", size: " << prop.Size
                          << ", flags: ";

                if (HasFlag(prop.Flags, EPropertyFlags::VisibleInEditor)) std::cout << "VisibleInEditor ";
                if (HasFlag(prop.Flags, EPropertyFlags::SaveGame))        std::cout << "SaveGame ";
                if (HasFlag(prop.Flags, EPropertyFlags::Transient))       std::cout << "Transient ";
                if (HasFlag(prop.Flags, EPropertyFlags::Editable))        std::cout << "Editable ";

                std::cout << ")\n";
            }
        }
    }

    void testMT()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        auto a = s_Counter++;
        auto b = s_Time;
        RB_LOG(TaskLog, info, "Running task {}, {} time, Frame {}", 1000,a, b)
    }

    void test2MT()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        auto a = s_Counter++;
        auto b = s_Time;
        RB_LOG(TaskLog, info, "Running task {}, {} time, Frame {}", 100,a, b)
    }

    struct Weapon {
        REFLECTABLE_CLASS(Weapon, void)
        int Damage;
        float Weight;
    };

    REFLECT_CLASS(Weapon, void)
    REFLECT_PROPERTY(Weapon, Damage, Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(Weapon, Weight, Reflection::EPropertyFlags::Editable);
    END_REFLECT_CLASS(Weapon)

    struct Player {
        REFLECTABLE_CLASS(Player, void)
        MemSize Health;
        float Speed;
        Weapon* EquippedWeapon; // pointer treated as Int
    };

    REFLECT_CLASS(Player, void)
    REFLECT_PROPERTY(Player, Health, Reflection::EPropertyFlags::Editable | Reflection::EPropertyFlags::SaveGame);
    REFLECT_PROPERTY(Player, Speed, Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(Player, EquippedWeapon, Reflection::EPropertyFlags::Editable);
    END_REFLECT_CLASS(Player)

    struct ReflectionPlayer
    {

        REFLECTABLE_CLASS(ReflectionPlayer, void)


        // Add a constructor
        ReflectionPlayer() = default;

        ReflectionPlayer(const String& name, int health, float stamina)
            : Name(name), Health(health), Stamina(stamina) {}
        
        String Name;
        int32 Health;
        Float Stamina;
        Bool IsDead = false;
        
    };

    // Reflection
    REFLECT_CLASS(ReflectionPlayer, void)
        REFLECT_PROPERTY(ReflectionPlayer, Name, Rebel::Core::Reflection::EPropertyFlags::SaveGame);
        REFLECT_PROPERTY(ReflectionPlayer, Health, Rebel::Core::Reflection::EPropertyFlags::SaveGame);
        REFLECT_PROPERTY(ReflectionPlayer, Stamina, Rebel::Core::Reflection::EPropertyFlags::SaveGame);
        REFLECT_PROPERTY(ReflectionPlayer, IsDead, Rebel::Core::Reflection::EPropertyFlags::SaveGame);
    END_REFLECT_CLASS(ReflectionPlayer)
    


    using namespace Rebel::Core;
    using namespace Rebel::Core::Memory;

    struct MyData {
        int value;
        MyData(int v) : value(v) { std::cout << "MyData " << value << " created\n"; }
        ~MyData() { std::cout << "MyData " << value << " destroyed\n"; }
    };
    
    void TestCore()
    {
        

        

        /*Threds::BucketScheduler scheduler(4, 10); // 2 buckets, 4 workers
        for (int frame = 0; frame < 3; ++frame) {
            PROFILE_SCOPE("main loop")
            s_Time = static_cast<float>(frame);
            RB_LOG(TaskLog, debug, "Main thread work : {}", frame)

           
            
            // Fill bucket 0
            for (int i = 0; i <10; ++i)
                scheduler.AddTask(0,[](){
                testMT();
            });

            //scheduler.AddTask(0, new my_struct());
            
            
            scheduler.SetBucketCallback(0, [&scheduler](){
                // Add bucket 1 tasks only when bucket 0 is fully done
                for (int i = 0; i < 5; ++i)
                    scheduler.AddTask(1, &test2MT);
            });

            
            scheduler.WaitForAllTasks(); // ensures frame tasks finish before next frame
            //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            RB_LOG(TaskLog, trace, "End Main Loop")

        }*/


        /*using namespace Rebel::Core::Reflection;

        // 1️⃣ Get TypeInfo from registry
        const TypeInfo* actorType = TypeRegistry::Get().GetType("ReflectionPlayer");

        // 2️⃣ Print all properties including inherited ones
        std::cout << "\n=== Actor Type ===\n";
        PrintType(actorType);

        ReflectionPlayer actor;
        const Rebel::Core::Reflection::TypeInfo* typeInfo = actor.GetType();
        if (typeInfo)
        {
            std::cout << "Class name: " << typeInfo->Name.c_str() << "\n";
        }

        ReflectionPlayer reflectionPlayer{ "Geraltdsad3169", 93, 75.653f };

        Rebel::Core::Serialization::YamlSerializer serializer;
        serializer.Serialize(serializer,reflectionPlayer);  // <-- serialize using reflection

        if (serializer.SaveToFile("player.yamlsk"))
            std::cout << "Saved player.yaml\n";
        else
            std::cout << "Failed to save player.yaml\n"; 


        ReflectionPlayer loadedPlayer;
        Rebel::Core::Serialization::YamlSerializer loader;
        if (loader.LoadFromFile("player.yamlsk"))
        {
            loader.Deserialize(serializer,loadedPlayer);  // <-- deserialize using reflection
            std::cout << "Loaded player:\n";
            std::cout << " Name: " << loadedPlayer.Name.c_str() << "\n";
            std::cout << " Health: " << loadedPlayer.Health << "\n";
            std::cout << " Stamina: " << loadedPlayer.Stamina << "\n";
            std::cout << " isDead: " << loadedPlayer.IsDead << "\n";
        }
        else
        {
            std::cout << "Failed to load player.yaml\n";
        }*/


        Player player;
        Weapon sword;
        sword.Damage = 50;
        sword.Weight = 7.5f;
        player.Health = 100;
        player.Speed = 5.0f;
        player.EquippedWeapon = &sword;
        
            const Reflection::TypeInfo* playerType = player.GetType();
            std::cout << "Player type: " << playerType->Name.c_str() << "\n";
        
            // Iterate properties
            for (const auto& prop : playerType->Properties)
            {
                std::cout << "Property: " << prop.Name.c_str() << " | Type: " << static_cast<int>(prop.Type) << " | Size: " << prop.Size << "\n";
        
                // Access dynamically
                if (prop.Type == Reflection::EPropertyType::Int)
                {
                    int* val = prop.Get<int>(&player);
                    std::cout << "  Value (int): " << *val << "\n";
                }
                else if (prop.Type == Reflection::EPropertyType::Float)
                {
                    float* val = prop.Get<float>(&player);
                    std::cout << "  Value (float): " << *val << "\n";
                }
            }
        
            // Check inheritance
            const Reflection::TypeInfo* weaponType = sword.GetType();
            std::cout << "Weapon is Player? " << weaponType->IsA(playerType) << "\n"; // should be 0 (false)
        
        std::cin.get();
        
    

   
    }

}



	


