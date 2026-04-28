#include "Editor/Core/PropertyEditor.h"
#include "Editor/Core/EditorCommandDispatcher.h"
#include "Editor/Core/EditorCommands.h"

#include "Engine/Framework/BaseEngine.h"
#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/PrefabAsset.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/Scene.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ThirdParty/IconsFontAwesome6.h"

#include <cctype>
#include <cstring>

namespace
{
String GetPropertyDisplayName(const String& propertyName)
{
    const char* name = propertyName.c_str();
    if (!name || name[0] == '\0')
        return propertyName;

    if (name[0] == 'm' && name[1] == '_' && name[2] != '\0')
        return String(name + 2);

    if (name[0] == 'b' && name[1] != '\0' && std::isupper(static_cast<unsigned char>(name[1])))
        return String(name + 1);

    return propertyName;
}

void HandlePropertyTransactionLifecycle()
{
    if (ImGui::IsItemActivated())
        EditorCommandDispatcher::BeginTransaction("Edit Property");

    if (ImGui::IsItemDeactivatedAfterEdit())
        EditorCommandDispatcher::CommitTransaction();
}

bool DrawMaterialHandleField(const char* label, const MaterialHandle& currentValue, MaterialHandle& outValue)
{
    bool changed = false;

    auto* renderModule = GEngine->GetModuleManager().GetModule<RenderModule>();
    if (!renderModule)
    {
        ImGui::TextDisabled("%s: <no renderer>", label);
        return false;
    }

    const auto& materials = renderModule->GetMaterials();
    if (materials.IsEmpty())
    {
        ImGui::TextDisabled("%s: <no materials>", label);
        return false;
    }

    int currentIndex = static_cast<int>(currentValue.Id);
    if (currentIndex < 0 || currentIndex >= static_cast<int>(materials.Num()))
        currentIndex = 0;

    char currentLabel[64];
    snprintf(currentLabel, sizeof(currentLabel), "Material %d", currentIndex);

    outValue = currentValue;

    if (ImGui::BeginCombo(label, currentLabel))
    {
        for (int i = 0; i < static_cast<int>(materials.Num()); ++i)
        {
            char itemLabel[64];
            snprintf(itemLabel, sizeof(itemLabel), "Material %d", i);

            const bool selected = (i == currentIndex);
            if (ImGui::Selectable(itemLabel, selected))
            {
                outValue.Id = static_cast<uint32>(i);
                changed = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    return changed;
}

bool IsCoreComponentName(const String& name)
{
    return name == "SceneComponent" || name == "IDComponent" || name == "NameComponent";
}

bool IsObjectComponentType(const Rebel::Core::Reflection::ComponentTypeInfo& info)
{
    return info.Type && info.Type->IsA(EntityComponent::StaticType());
}

const Rebel::Core::Reflection::ComponentTypeInfo* FindComponentInfoForType(const Rebel::Core::Reflection::TypeInfo* type)
{
    if (!type)
        return nullptr;

    for (const auto& info : ComponentRegistry::Get().GetComponents())
    {
        if (info.Type == type)
            return &info;
    }

    return nullptr;
}

bool BeginPropertyRow(const char* label)
{
    if (!GImGui || !GImGui->CurrentTable)
        return false;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    return true;
}

const Rebel::Core::Reflection::TypeInfo* ReadClassPropertyValue(const void* ptr)
{
    const Rebel::Core::Reflection::TypeInfo* type = nullptr;
    if (ptr)
        memcpy(&type, ptr, sizeof(type));
    return type;
}

int ReadEnumValue(const void* ptr, const MemSize size)
{
    if (!ptr)
        return 0;

    switch (size)
    {
    case sizeof(uint8):
        return static_cast<int>(*reinterpret_cast<const uint8*>(ptr));
    case sizeof(uint16):
        return static_cast<int>(*reinterpret_cast<const uint16*>(ptr));
    case sizeof(uint32):
        return static_cast<int>(*reinterpret_cast<const uint32*>(ptr));
    case sizeof(uint64):
        return static_cast<int>(*reinterpret_cast<const uint64*>(ptr));
    default:
        return 0;
    }
}

bool AssetMatchesPropertyConstraint(
    const AssetMeta& meta,
    const Rebel::Core::Reflection::PropertyInfo& prop,
    AssetManagerModule& assetModule)
{
    if (!prop.SubclassBaseType)
        return true;

    if (!meta.Type || meta.Type != PrefabAsset::StaticType())
        return true;

    PrefabAsset* prefab = dynamic_cast<PrefabAsset*>(assetModule.GetManager().Load(meta.ID));
    if (!prefab)
        return false;

    const Rebel::Core::Reflection::TypeInfo* actorType = nullptr;
    if (prefab->m_ActorTypeName.length() > 0)
        actorType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(prefab->m_ActorTypeName);

    return actorType && actorType->IsA(prop.SubclassBaseType);
}
}

void PropertyEditor::DrawPropertyUI(
    void* object,
    const Rebel::Core::Reflection::PropertyInfo& prop,
    const Rebel::Core::Reflection::TypeInfo* ownerType)
{
    using namespace Rebel::Core::Reflection;

    if (prop.ClassType != nullptr)
    {
        void* fieldPtr = GetPropertyPointer(object, prop);

        String displayName = GetPropertyDisplayName(prop.Name);
        const char* uiName = displayName.c_str();

        ImGui::PushID(prop.Name.c_str());
        if (ImGui::TreeNode(uiName))
        {
            DrawReflectedObjectUI(fieldPtr, *prop.ClassType);
            ImGui::TreePop();
        }
        ImGui::PopID();
        return;
    }

    if (!HasFlag(prop.Flags, EPropertyFlags::VisibleInEditor))
        return;

    String displayName = GetPropertyDisplayName(prop.Name);
    const char* uiName = displayName.c_str();
    const bool bUsingTable = BeginPropertyRow(uiName);
    const char* widgetLabel = bUsingTable ? "##Value" : uiName;

    void* fieldPtr = GetPropertyPointer(object, prop);
    const bool editable = HasFlag(prop.Flags, EPropertyFlags::Editable);

    ImGui::PushID(prop.Name.c_str());

    switch (prop.Type)
    {
    case EPropertyType::Int32:
    {
        int* v = reinterpret_cast<int*>(fieldPtr);
        if (editable)
        {
            const int before = *v;
            int after = before;
            const bool changed = ImGui::DragInt(widgetLabel, &after, 1.0f);
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(after)));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::DragInt(widgetLabel, v, 1.0f);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::UInt64:
    {
        uint64* v = reinterpret_cast<uint64*>(fieldPtr);
        if (editable)
        {
            const uint64 before = *v;
            uint64 after = before;
            const bool changed = ImGui::InputScalar(widgetLabel, ImGuiDataType_U64, &after);
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(after)));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::InputScalar(widgetLabel, ImGuiDataType_U64, v);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Float:
    {
        float* v = reinterpret_cast<float*>(fieldPtr);
        if (editable)
        {
            const float before = *v;
            float after = before;
            const bool changed = ImGui::DragFloat(widgetLabel, &after, 0.1f);
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(after)));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::DragFloat(widgetLabel, v, 0.1f);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Bool:
    {
        bool* v = reinterpret_cast<bool*>(fieldPtr);
        if (editable)
        {
            const bool before = *v;
            bool after = before;
            const bool changed = ImGui::Checkbox(widgetLabel, &after);
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(after)));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Checkbox(widgetLabel, v);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::String:
    {
        String* s = reinterpret_cast<String*>(fieldPtr);

        char buffer[256];
        strncpy_s(buffer, s->c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        if (editable)
        {
            const String before = *s;
            const bool changed = ImGui::InputText(widgetLabel, buffer, sizeof(buffer));
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(String(buffer))));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::InputText(widgetLabel, buffer, sizeof(buffer));
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Vector3:
    {
        Vector3* v = reinterpret_cast<Vector3*>(fieldPtr);
        if (editable)
        {
            const Vector3 before = *v;
            Vector3 after = before;
            const bool changed = ImGui::DragFloat3(widgetLabel, &after.x, 0.1f);
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(after)));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::DragFloat3(widgetLabel, &v->x, 0.1f);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Asset:
    {
        auto* ptr = static_cast<AssetPtrBase*>(fieldPtr);
        if (!ptr)
            break;

        const auto type = ptr->GetAssetType();
        auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
        if (!assetModule)
            break;

        const auto& reg = assetModule->GetRegistry().GetAll();

        const AssetHandle before = ptr->GetHandle();
        AssetHandle after = before;

        const char* currentName = "None";
        if ((uint64)before != 0)
        {
            if (const auto* meta = reg.Find(before))
                currentName = meta->Path.c_str();
        }

        if (ImGui::BeginCombo(widgetLabel, currentName))
        {
            if (ImGui::Selectable("None", (uint64)before == 0))
                after = 0;

            for (auto pair : reg)
            {
                const auto meta = pair.Value;
                if (type != meta.Type)
                    continue;

                if (!AssetMatchesPropertyConstraint(meta, prop, *assetModule))
                    continue;

                const bool selected = (before == meta.ID);
                if (ImGui::Selectable(meta.Path.c_str(), selected))
                    after = meta.ID;

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (editable && after != before)
        {
            EditorCommandDispatcher::Execute(std::make_unique<AssignAssetCommand>(ptr, before, after));
        }
        break;
    }
    case EPropertyType::MaterialHandle:
    {
        auto* h = reinterpret_cast<MaterialHandle*>(fieldPtr);
        if (editable)
        {
            const MaterialHandle before = *h;
            MaterialHandle after = before;
            const bool changed = DrawMaterialHandleField(widgetLabel, before, after);
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(after)));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            MaterialHandle ignored{};
            DrawMaterialHandleField(widgetLabel, *h, ignored);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Class:
    {
        const TypeInfo* before = ReadClassPropertyValue(fieldPtr);
        const TypeInfo* after = before;
        const char* currentName = before ? before->Name.c_str() : "None";

        if (editable)
        {
            if (ImGui::BeginCombo(widgetLabel, currentName))
            {
                const bool noneSelected = (before == nullptr);
                if (ImGui::Selectable("None", noneSelected))
                    after = nullptr;

                if (noneSelected)
                    ImGui::SetItemDefaultFocus();

                for (const auto& pair : TypeRegistry::Get().GetTypes())
                {
                    const TypeInfo* type = pair.Value;
                    if (!type || !type->CreateInstance)
                        continue;

                    if (prop.SubclassBaseType && !type->IsA(prop.SubclassBaseType))
                        continue;

                    const bool selected = (type == before);
                    if (ImGui::Selectable(type->Name.c_str(), selected))
                        after = type;

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (after != before)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(before),
                    SetPropertyCommand::PropertyValue(after)));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            (void)ImGui::BeginCombo(widgetLabel, currentName);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Enum:
    {
        if (!prop.Enum)
        {
            ImGui::TextDisabled("%s (enum RTTI missing)", uiName);
            break;
        }

        int currentValue = ReadEnumValue(fieldPtr, prop.Size);
        currentValue = FMath::clamp(currentValue, 0, static_cast<int>(prop.Enum->Count) - 1);
        if (editable)
        {
            const int before = currentValue;
            int after = before;
            const bool changed = ImGui::Combo(
                widgetLabel,
                &after,
                prop.Enum->MemberNames,
                (int)prop.Enum->Count);
            HandlePropertyTransactionLifecycle();
            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<SetPropertyCommand>(
                    object,
                    prop,
                    ownerType,
                    SetPropertyCommand::PropertyValue(static_cast<int32>(before)),
                    SetPropertyCommand::PropertyValue(static_cast<int32>(after))));
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Combo(
                widgetLabel,
                &currentValue,
                prop.Enum->MemberNames,
                (int)prop.Enum->Count);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Unknown:
        ImGui::TextDisabled("%s, %d (unhandled type)", uiName, (int)prop.Type);
        break;
    case EPropertyType::Int8:
    case EPropertyType::UInt8:
    case EPropertyType::Int16:
    case EPropertyType::UInt16:
    case EPropertyType::UInt32:
    case EPropertyType::Int64:
    case EPropertyType::Double:
        break;
    }

    ImGui::PopID();
}

void PropertyEditor::DrawReflectedObjectUI(void* object, const Rebel::Core::Reflection::TypeInfo& type)
{
    if (type.Super)
        DrawReflectedObjectUI(object, *type.Super);

    for (const auto& prop : type.Properties)
        DrawPropertyUI(object, prop, &type);
}

void PropertyEditor::DrawComponentsForActor(
    Actor& actor,
    const Rebel::Core::Reflection::TypeInfo* selectedComponentType,
    EntityComponent* selectedComponent) const
{
    if (!actor || !actor.IsValid())
        return;

    Scene* scene = actor.GetScene();
    if (!scene)
        return;

    auto typeInfo = actor.GetType();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_Framed |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_AllowItemOverlap |
        ImGuiTreeNodeFlags_FramePadding;

    {
        const char* label = typeInfo->Name.c_str();
        ImGui::PushID(label);
        if (ImGui::TreeNodeEx(label, flags, "%s %s", ICON_FA_CIRCLE_INFO, label))
        {
            if (ImGui::BeginTable("ActorProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 138.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                DrawReflectedObjectUI(&actor, *typeInfo);
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    auto drawComponentProperties = [&](const Rebel::Core::Reflection::ComponentTypeInfo& info, void* instance, EntityComponent* componentInstance, const char* idSuffix)
    {
        if (!info.Type || !instance)
            return;
        if (IsCoreComponentName(info.Name) && componentInstance != selectedComponent)
            return;

        if (selectedComponentType != nullptr && info.Type != selectedComponentType)
            return;
        if (selectedComponent != nullptr && componentInstance != selectedComponent)
            return;

        const String displayName = componentInstance && componentInstance->GetEditorName().length() > 0
            ? componentInstance->GetEditorName()
            : info.Name;
        const String labelId = info.Name + "##" + idSuffix;

        ImGui::PushID(labelId.c_str());
        const bool open = ImGui::TreeNodeEx(labelId.c_str(), flags, "%s %s", ICON_FA_PUZZLE_PIECE, displayName.c_str());
        bool removedComponent = false;
        if (ImGui::BeginPopupContextItem())
        {
            const bool canDuplicate = componentInstance && IsObjectComponentType(info);
            if (ImGui::MenuItem(ICON_FA_COPY " Duplicate Component", nullptr, false, canDuplicate))
                EditorCommandDispatcher::Execute(std::make_unique<DuplicateComponentCommand>(&actor, &info, componentInstance));
            if (!canDuplicate && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Only actor-owned object components can be duplicated; data components stay type-unique.");

            const bool canRemove = info.RemoveFn != nullptr;
            if (ImGui::MenuItem(ICON_FA_TRASH " Remove Component", nullptr, false, canRemove))
            {
                EditorCommandDispatcher::Execute(std::make_unique<RemoveComponentCommand>(&actor, &info, componentInstance));
                removedComponent = true;
            }

            ImGui::EndPopup();
        }
        if (removedComponent)
        {
            if (open)
                ImGui::TreePop();
            ImGui::PopID();
            return;
        }
        if (open)
        {
            if (ImGui::BeginTable("ComponentProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 138.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                DrawReflectedObjectUI(instance, *info.Type);
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    };

    int objectComponentIndex = 0;
    for (const auto& componentPtr : actor.GetObjectComponents())
    {
        EntityComponent* component = componentPtr.Get();
        if (!component)
            continue;

        const auto* info = FindComponentInfoForType(component->GetType());
        if (!info)
            continue;

        const String suffix = String("Object") + String(std::to_string(objectComponentIndex++).c_str());
        drawComponentProperties(*info, component, component, suffix.c_str());
    }

    for (const auto& info : ComponentRegistry::Get().GetComponents())
    {
        if (!info.HasFn || !info.GetFn || !info.Type || IsObjectComponentType(info))
            continue;

        if (!info.HasFn(actor))
            continue;

        void* instance = info.GetFn(actor);
        drawComponentProperties(info, instance, nullptr, "Data");
    }

    ImGui::PopStyleVar(2);
}
