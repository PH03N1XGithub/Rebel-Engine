#include "Editor/AssetEditors/AnimationAssetEditors.h"

#include "Editor/Core/PropertyEditor.h"
#include "Editor/Core/Graph/EditorGraphPanel.h"
#include "EditorEngine.h"
#include "Engine/Animation/AnimGraphAsset.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/AnimationModule.h"
#include "Engine/Animation/AnimationRuntime.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Animation/SkeletonAsset.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Components/SkeletalMeshComponent.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Input/InputModule.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Scene/Actor.h"
#include "ThirdParty/IconsFontAwesome6.h"
#include "GraphEditor.h"
#include "ImSequencer.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <cmath>
#include <cctype>
#include <cstdio>

namespace
{
constexpr float kCameraSpeedStep = 0.5f;
constexpr float kDefaultAnimationTimelineFps = 30.0f;

String MakeScopedEditorName(const char* label, const char* idPrefix, const AssetHandle assetHandle)
{
    return String(label) +
        String("###") +
        String(idPrefix) +
        String("_") +
        String(std::to_string(static_cast<uint64>(assetHandle)).c_str());
}

String MakeScopedEditorId(const char* idPrefix, const AssetHandle assetHandle)
{
    return String(idPrefix) +
        String("_") +
        String(std::to_string(static_cast<uint64>(assetHandle)).c_str());
}

const char* ToNodeKindLabel(const AnimGraphNodeKind kind)
{
    switch (kind)
    {
    case AnimGraphNodeKind::AnimationClip:
        return "Animation Clip";
    case AnimGraphNodeKind::Blend:
        return "Blend";
    case AnimGraphNodeKind::Output:
        return "Output";
    case AnimGraphNodeKind::StateMachine:
        return "State Machine";
    default:
        return "Unknown";
    }
}

void DockThreePanelLayout(ImGuiID dockspaceId, const ImVec2& size, const char* left, const char* main, const char* right, bool& initialized)
{
    if (initialized || dockspaceId == 0 || size.x <= 1.0f || size.y <= 1.0f)
        return;

    if (ImGuiDockNode* existing = ImGui::DockBuilderGetNode(dockspaceId))
    {
        if (existing->ChildNodes[0] != nullptr)
        {
            initialized = true;
            return;
        }
    }

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID dockLeft = 0;
    ImGuiID dockRight = 0;
    ImGuiID dockMain = dockspaceId;
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.24f, &dockLeft, &dockMain);
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.28f, &dockRight, &dockMain);

    ImGui::DockBuilderDockWindow(left, dockLeft);
    ImGui::DockBuilderDockWindow(main, dockMain);
    ImGui::DockBuilderDockWindow(right, dockRight);
    ImGui::DockBuilderFinish(dockspaceId);
    initialized = true;
}

void DrawSkeletonTreeRecursive(const SkeletonAsset& skeleton, int32 boneIndex, int32& selectedBone)
{
    const char* boneName = (boneIndex >= 0 && boneIndex < skeleton.m_BoneNames.Num())
        ? skeleton.m_BoneNames[boneIndex].c_str()
        : "Bone";

    bool hasChildren = false;
    for (int32 i = 0; i < skeleton.m_Parent.Num(); ++i)
    {
        if (skeleton.m_Parent[i] == boneIndex)
        {
            hasChildren = true;
            break;
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selectedBone == boneIndex)
        flags |= ImGuiTreeNodeFlags_Selected;

    const bool open = ImGui::TreeNodeEx((void*)(intptr_t)boneIndex, flags, "%s %s", ICON_FA_BONE, boneName);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        selectedBone = boneIndex;

    if (!hasChildren || !open)
        return;

    for (int32 i = 0; i < skeleton.m_Parent.Num(); ++i)
    {
        if (skeleton.m_Parent[i] == boneIndex)
            DrawSkeletonTreeRecursive(skeleton, i, selectedBone);
    }

    ImGui::TreePop();
}

SkeletalMeshAsset* FindPreviewMeshForSkeleton(AssetHandle skeletonHandle)
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule || !IsValidAssetHandle(skeletonHandle))
        return nullptr;

    for (const auto& pair : assetModule->GetRegistry().GetAll())
    {
        const AssetMeta& meta = pair.Value;
        if (meta.Type != SkeletalMeshAsset::StaticType())
            continue;

        SkeletalMeshAsset* mesh = dynamic_cast<SkeletalMeshAsset*>(assetModule->GetManager().Load(meta.ID));
        if (mesh && mesh->m_Skeleton.GetHandle() == skeletonHandle)
            return mesh;
    }

    return nullptr;
}

void TickPreviewAnimation(Scene& scene, const float deltaTime)
{
    AnimationModule* animationModule = GEngine ? GEngine->GetModuleManager().GetModule<AnimationModule>() : nullptr;
    if (!animationModule)
        return;

    Scene* activeScene = GEngine->GetActiveScene();
    animationModule->SetSceneContext(&scene);
    animationModule->Tick(glm::max(0.0f, deltaTime));
    animationModule->SetSceneContext(activeScene);
}

bool DrawNodeInputCombo(const char* label, AnimGraphAsset& graph, uint64& input, uint64 excludedNodeID)
{
    const AnimGraphNode* current = graph.FindNode(input);
    const char* currentLabel = current ? current->Name.c_str() : "None";
    bool changed = false;

    if (ImGui::BeginCombo(label, currentLabel))
    {
        if (ImGui::Selectable("None", input == 0))
        {
            input = 0;
            changed = true;
        }

        for (const AnimGraphNode& candidate : graph.m_Nodes)
        {
            if (candidate.ID == excludedNodeID || candidate.Kind == AnimGraphNodeKind::Output)
                continue;

            const bool connectable = graph.CanConnectPose(candidate.ID, excludedNodeID);
            const bool selected = candidate.ID == input;
            String display = candidate.Name + " (" + String(std::to_string(candidate.ID).c_str()) + ")";
            if (!connectable)
                ImGui::BeginDisabled();
            if (ImGui::Selectable(display.c_str(), selected))
            {
                input = candidate.ID;
                changed = true;
            }
            if (!connectable)
                ImGui::EndDisabled();
        }

        ImGui::EndCombo();
    }

    return changed;
}

bool ContainsCaseInsensitive(const String& haystack, const String& needle)
{
    if (needle.length() == 0)
        return true;

    const char* haystackChars = haystack.c_str();
    const char* needleChars = needle.c_str();
    const size_t haystackLength = haystack.length();
    const size_t needleLength = needle.length();
    if (needleLength > haystackLength)
        return false;

    auto lowerChar = [](char value)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    };

    for (size_t startIndex = 0; startIndex + needleLength <= haystackLength; ++startIndex)
    {
        bool matched = true;
        for (size_t needleIndex = 0; needleIndex < needleLength; ++needleIndex)
        {
            if (lowerChar(haystackChars[startIndex + needleIndex]) != lowerChar(needleChars[needleIndex]))
            {
                matched = false;
                break;
            }
        }

        if (matched)
            return true;
    }

    return false;
}

bool DrawAnimGraphSkeletonCombo(AnimGraphAsset& graph)
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule)
        return false;

    const char* currentName = "None";
    if (IsValidAssetHandle(graph.m_SkeletonID))
    {
        if (const AssetMeta* meta = assetModule->GetRegistry().Get(graph.m_SkeletonID))
            currentName = meta->Path.c_str();
    }

    bool changed = false;
    if (ImGui::BeginCombo("Skeleton", currentName))
    {
        if (ImGui::Selectable("None", !IsValidAssetHandle(graph.m_SkeletonID)))
        {
            graph.m_SkeletonID = 0;
            changed = true;
        }

        for (const auto& pair : assetModule->GetRegistry().GetAll())
        {
            const AssetMeta& meta = pair.Value;
            if (meta.Type != SkeletonAsset::StaticType())
                continue;

            const bool selected = meta.ID == graph.m_SkeletonID;
            if (ImGui::Selectable(meta.Path.c_str(), selected))
            {
                graph.m_SkeletonID = meta.ID;
                changed = true;
            }
        }

        ImGui::EndCombo();
    }

    return changed;
}

bool DrawAnimationClipAssetCombo(const char* label, AssetHandle& clipHandle, const AssetHandle requiredSkeletonID)
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule)
        return false;

    const char* currentName = "None";
    if (IsValidAssetHandle(clipHandle))
    {
        if (const AssetMeta* meta = assetModule->GetRegistry().Get(clipHandle))
            currentName = meta->Path.c_str();
    }

    bool changed = false;
    ImGui::PushID(label);
    if (ImGui::BeginCombo(label, currentName))
    {
        static char searchBuffer[256] = "";
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
        const String searchText = String(searchBuffer);
        ImGui::Separator();

        if (ImGui::Selectable("None", !IsValidAssetHandle(clipHandle)))
        {
            clipHandle = 0;
            changed = true;
        }

        for (const auto& pair : assetModule->GetRegistry().GetAll())
        {
            const AssetMeta& meta = pair.Value;
            if (meta.Type != AnimationAsset::StaticType())
                continue;

            AnimationAsset* animation = dynamic_cast<AnimationAsset*>(assetModule->GetManager().Load(meta.ID));
            if (!animation)
                continue;

            if (IsValidAssetHandle(requiredSkeletonID) &&
                IsValidAssetHandle(animation->m_SkeletonID) &&
                animation->m_SkeletonID != requiredSkeletonID)
            {
                continue;
            }

            if (!ContainsCaseInsensitive(meta.Path, searchText) &&
                !ContainsCaseInsensitive(animation->m_ClipName, searchText))
            {
                continue;
            }

            const bool selected = meta.ID == clipHandle;
            if (ImGui::Selectable(meta.Path.c_str(), selected))
            {
                clipHandle = meta.ID;
                changed = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

bool DrawNodeInputCombo(const char* label, AnimGraphAsset& graph, AnimPoseGraph& poseGraph, uint64& input, uint64 excludedNodeID)
{
    const AnimGraphNode* current = graph.FindNode(poseGraph, input);
    const char* currentLabel = current ? current->Name.c_str() : "None";
    bool changed = false;

    if (ImGui::BeginCombo(label, currentLabel))
    {
        if (ImGui::Selectable("None", input == 0))
        {
            input = 0;
            changed = true;
        }

        for (const AnimGraphNode& candidate : poseGraph.Nodes)
        {
            if (candidate.ID == excludedNodeID || candidate.Kind == AnimGraphNodeKind::Output)
                continue;

            const bool connectable = graph.CanConnectPose(poseGraph, candidate.ID, excludedNodeID);
            const bool selected = candidate.ID == input;
            String display = candidate.Name + " (" + String(std::to_string(candidate.ID).c_str()) + ")";
            if (!connectable)
                ImGui::BeginDisabled();
            if (ImGui::Selectable(display.c_str(), selected))
            {
                input = candidate.ID;
                changed = true;
            }
            if (!connectable)
                ImGui::EndDisabled();
        }

        ImGui::EndCombo();
    }

    return changed;
}

bool DrawBlendPropertyCombo(
    const char* label,
    AnimGraphNode& node,
    const Rebel::Core::Reflection::TypeInfo* currentAnimClass,
    const Rebel::Core::Reflection::EPropertyType requiredType)
{
    const char* currentProp = node.BlendParameterName.length() > 0 ? node.BlendParameterName.c_str() : "None";
    bool changed = false;

    if (ImGui::BeginCombo(label, currentProp))
    {
        if (ImGui::Selectable("None", node.BlendParameterName.length() == 0))
        {
            node.BlendParameterName = "";
            changed = true;
        }

        const Rebel::Core::Reflection::TypeInfo* type = currentAnimClass;
        while (type)
        {
            for (const Rebel::Core::Reflection::PropertyInfo& prop : type->Properties)
            {
                if (prop.Type != requiredType)
                    continue;

                const bool selected = prop.Name == node.BlendParameterName;
                if (ImGui::Selectable(prop.Name.c_str(), selected))
                {
                    node.BlendParameterName = prop.Name;
                    changed = true;
                }
            }
            type = type->Super;
        }

        ImGui::EndCombo();
    }

    return changed;
}

bool DrawBlendAlphaControls(AnimGraphNode& node, const Rebel::Core::Reflection::TypeInfo* currentAnimClass)
{
    bool changed = false;
    static const char* modeNames[] = { "Fixed Alpha", "Float Property", "Bool Property" };

    int mode = static_cast<int>(node.BlendAlphaMode);
    if (mode < 0 || mode >= IM_ARRAYSIZE(modeNames))
        mode = 0;
    if (ImGui::Combo("Alpha Mode", &mode, modeNames, IM_ARRAYSIZE(modeNames)))
    {
        node.BlendAlphaMode = static_cast<AnimBlendAlphaMode>(mode);
        changed = true;
    }

    if (node.BlendAlphaMode == AnimBlendAlphaMode::Fixed)
    {
        if (ImGui::SliderFloat("Alpha", &node.BlendAlpha, 0.0f, 1.0f))
            changed = true;
        if (ImGui::DragFloat("Blend Time", &node.BlendTime, 0.01f, 0.0f, 10.0f))
            changed = true;
        return changed;
    }

    using Rebel::Core::Reflection::EPropertyType;
    if (node.BlendAlphaMode == AnimBlendAlphaMode::FloatProperty)
    {
        if (DrawBlendPropertyCombo("Float Parameter", node, currentAnimClass, EPropertyType::Float))
            changed = true;
        if (ImGui::DragFloat("Input Min", &node.BlendInputMin, 0.01f))
            changed = true;
        if (ImGui::DragFloat("Input Max", &node.BlendInputMax, 0.01f))
            changed = true;
        if (ImGui::SliderFloat("Fallback Alpha", &node.BlendAlpha, 0.0f, 1.0f))
            changed = true;
        if (ImGui::DragFloat("Blend Time", &node.BlendTime, 0.01f, 0.0f, 10.0f))
            changed = true;
        ImGui::TextDisabled("Alpha = saturate((parameter - min) / (max - min)).");
        return changed;
    }

    if (DrawBlendPropertyCombo("Bool Parameter", node, currentAnimClass, EPropertyType::Bool))
        changed = true;
    if (ImGui::Checkbox("Invert Bool", &node.bBlendInvertBool))
        changed = true;
    if (ImGui::SliderFloat("Fallback Alpha", &node.BlendAlpha, 0.0f, 1.0f))
        changed = true;
    if (ImGui::DragFloat("Blend Time", &node.BlendTime, 0.01f, 0.0f, 10.0f))
        changed = true;
    ImGui::TextDisabled("False selects Input A, true selects Input B.");
    return changed;
}

const char* ToBlendAlphaModeLabel(const AnimBlendAlphaMode mode)
{
    switch (mode)
    {
    case AnimBlendAlphaMode::FloatProperty:
        return "Float";
    case AnimBlendAlphaMode::BoolProperty:
        return "Bool";
    case AnimBlendAlphaMode::Fixed:
    default:
        return "Fixed";
    }
}

bool ConditionUsesFloatValue(const AnimConditionOp op)
{
    return op == AnimConditionOp::FloatGreater ||
        op == AnimConditionOp::FloatLess ||
        op == AnimConditionOp::StateTimeGreater ||
        op == AnimConditionOp::StateTimeLess ||
        op == AnimConditionOp::AnimTimeRemainingLess ||
        op == AnimConditionOp::AnimTimeRemainingRatioLess;
}

bool ConditionUsesIntValue(const AnimConditionOp op)
{
    return op == AnimConditionOp::IntGreater || op == AnimConditionOp::IntLess;
}

bool ConditionUsesEnumValue(const AnimConditionOp op)
{
    return op == AnimConditionOp::EnumEquals || op == AnimConditionOp::EnumNotEquals;
}

const Rebel::Core::Reflection::PropertyInfo* FindReflectedPropertyByName(
    const Rebel::Core::Reflection::TypeInfo* type,
    const String& propertyName)
{
    const Rebel::Core::Reflection::TypeInfo* current = type;
    while (current)
    {
        for (const Rebel::Core::Reflection::PropertyInfo& prop : current->Properties)
        {
            if (prop.Name == propertyName)
                return &prop;
        }

        current = current->Super;
    }

    return nullptr;
}

String FormatFloatCompact(const float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3g", value);
    return String(buffer);
}

String MakeConditionSummary(
    const AnimTransitionCondition& condition,
    const Rebel::Core::Reflection::TypeInfo* currentAnimClass = nullptr)
{
    const char* prop = condition.PropertyName.length() > 0 ? condition.PropertyName.c_str() : "<property>";
    switch (condition.Op)
    {
    case AnimConditionOp::BoolIsTrue:
        return String(prop) + " == true";
    case AnimConditionOp::BoolIsFalse:
        return String(prop) + " == false";
    case AnimConditionOp::FloatGreater:
        return String(prop) + " > " + FormatFloatCompact(condition.FloatValue);
    case AnimConditionOp::FloatLess:
        return String(prop) + " < " + FormatFloatCompact(condition.FloatValue);
    case AnimConditionOp::IntGreater:
        return String(prop) + " > " + String(std::to_string(condition.IntValue).c_str());
    case AnimConditionOp::IntLess:
        return String(prop) + " < " + String(std::to_string(condition.IntValue).c_str());
    case AnimConditionOp::StateTimeGreater:
        return String("StateTime > ") + FormatFloatCompact(condition.FloatValue);
    case AnimConditionOp::StateTimeLess:
        return String("StateTime < ") + FormatFloatCompact(condition.FloatValue);
    case AnimConditionOp::AnimTimeRemainingLess:
        return String("AnimTimeRemaining < ") + FormatFloatCompact(condition.FloatValue);
    case AnimConditionOp::AnimTimeRemainingRatioLess:
        return String("AnimTimeRemainingRatio < ") + FormatFloatCompact(condition.FloatValue);
    case AnimConditionOp::EnumEquals:
    case AnimConditionOp::EnumNotEquals:
    {
        String rhs = String(std::to_string(condition.IntValue).c_str());
        if (const Rebel::Core::Reflection::PropertyInfo* enumProp =
                FindReflectedPropertyByName(currentAnimClass, condition.PropertyName))
        {
            if (enumProp->Type == Rebel::Core::Reflection::EPropertyType::Enum &&
                enumProp->Enum &&
                condition.IntValue >= 0 &&
                condition.IntValue < static_cast<int32>(enumProp->Enum->Count))
            {
                rhs = enumProp->Enum->MemberNames[condition.IntValue];
            }
        }

        const char* equalityOp = condition.Op == AnimConditionOp::EnumEquals ? "==" : "!=";
        return String(prop) + " " + equalityOp + " " + rhs;
    }
    default:
        return "Unknown condition";
    }
}

String MakeConditionsSummary(
    const TArray<AnimTransitionCondition>& conditions,
    const Rebel::Core::Reflection::TypeInfo* currentAnimClass = nullptr)
{
    if (conditions.IsEmpty())
        return "Always";

    String summary;
    for (int32 i = 0; i < conditions.Num(); ++i)
    {
        if (i > 0)
            summary = summary + " && ";
        summary = summary + MakeConditionSummary(conditions[i], currentAnimClass);
    }
    return summary;
}

bool DrawConditionList(
    const char* label,
    TArray<AnimTransitionCondition>& conditions,
    const Rebel::Core::Reflection::TypeInfo* currentAnimClass)
{
    bool changed = false;
    static const char* opNames[] = {
        "Bool Is True",
        "Bool Is False",
        "Float >",
        "Float <",
        "Int >",
        "Int <",
        "State Time >",
        "State Time <",
        "Anim Remaining <",
        "Anim Remaining Ratio <",
        "Enum ==",
        "Enum !="
    };

    ImGui::Text("%s", label);
    ImGui::SameLine();
    if (ImGui::SmallButton("Add"))
    {
        conditions.Emplace();
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear") && !conditions.IsEmpty())
    {
        conditions.Clear();
        changed = true;
    }

    const String summary = MakeConditionsSummary(conditions, currentAnimClass);
    ImGui::TextDisabled("Summary: %s", summary.c_str());

    for (int32 i = 0; i < conditions.Num(); ++i)
    {
        AnimTransitionCondition& condition = conditions[i];
        ImGui::PushID(i);
        ImGui::Separator();
        const String conditionSummary = MakeConditionSummary(condition, currentAnimClass);
        ImGui::TextDisabled("%d. %s", i + 1, conditionSummary.c_str());

        if (ImGui::SmallButton("Up") && i > 0)
        {
            AnimTransitionCondition tmp = conditions[i - 1];
            conditions[i - 1] = conditions[i];
            conditions[i] = tmp;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Down") && i + 1 < conditions.Num())
        {
            AnimTransitionCondition tmp = conditions[i + 1];
            conditions[i + 1] = conditions[i];
            conditions[i] = tmp;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Duplicate"))
        {
            conditions.Insert(i + 1, condition);
            changed = true;
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            conditions.RemoveAt(i);
            changed = true;
            ImGui::PopID();
            break;
        }

        const Rebel::Core::Reflection::PropertyInfo* selectedProperty =
            FindReflectedPropertyByName(currentAnimClass, condition.PropertyName);

        const char* currentProp = selectedProperty ? selectedProperty->Name.c_str() : "None";
        if (currentAnimClass && ImGui::BeginCombo("Property", currentProp))
        {
            const Rebel::Core::Reflection::TypeInfo* type = currentAnimClass;
            while (type)
            {
                for (const Rebel::Core::Reflection::PropertyInfo& prop : type->Properties)
                {
                    using Rebel::Core::Reflection::EPropertyType;
                    if (prop.Type != EPropertyType::Bool &&
                        prop.Type != EPropertyType::Float &&
                        prop.Type != EPropertyType::Int32 &&
                        prop.Type != EPropertyType::Enum)
                    {
                        continue;
                    }

                    const bool propSelected = prop.Name == condition.PropertyName;
                    if (ImGui::Selectable(prop.Name.c_str(), propSelected))
                    {
                        condition.PropertyName = prop.Name;
                        changed = true;
                    }
                }
                type = type->Super;
            }
            ImGui::EndCombo();
        }

        selectedProperty = FindReflectedPropertyByName(currentAnimClass, condition.PropertyName);

        int op = static_cast<int>(condition.Op);
        if (ImGui::Combo("Op", &op, opNames, IM_ARRAYSIZE(opNames)))
        {
            condition.Op = static_cast<AnimConditionOp>(op);
            changed = true;
        }

        if (ConditionUsesFloatValue(condition.Op))
        {
            if (ImGui::DragFloat("Value", &condition.FloatValue, 0.01f))
                changed = true;
        }
        else if (ConditionUsesEnumValue(condition.Op))
        {
            if (selectedProperty &&
                selectedProperty->Type == Rebel::Core::Reflection::EPropertyType::Enum &&
                selectedProperty->Enum &&
                selectedProperty->Enum->Count > 0)
            {
                int enumValue = condition.IntValue;
                enumValue = FMath::clamp(enumValue, 0, static_cast<int>(selectedProperty->Enum->Count) - 1);
                if (ImGui::Combo("Value", &enumValue, selectedProperty->Enum->MemberNames, static_cast<int>(selectedProperty->Enum->Count)))
                {
                    condition.IntValue = enumValue;
                    changed = true;
                }
            }
            else if (ImGui::DragInt("Value", &condition.IntValue))
            {
                changed = true;
            }
        }
        else if (ConditionUsesIntValue(condition.Op))
        {
            if (ImGui::DragInt("Value", &condition.IntValue))
                changed = true;
        }

        ImGui::PopID();
    }

    return changed;
}

bool IsPoseNodeInvalid(
    const AnimGraphAsset& graph,
    const AnimPoseGraph* poseGraph,
    const AnimGraphNode& node,
    String& outReason)
{
    const bool reachable = poseGraph
        ? graph.IsNodeReachableFromOutput(*poseGraph, node.ID)
        : graph.IsNodeReachableFromOutput(node.ID);
    if (node.Kind != AnimGraphNodeKind::Output && !reachable)
    {
        outReason = "Not connected to output";
        return true;
    }

    if (node.Kind == AnimGraphNodeKind::AnimationClip && !IsValidAssetHandle(node.AnimationClip))
    {
        outReason = "Missing animation clip";
        return true;
    }

    if (node.Kind == AnimGraphNodeKind::Blend)
    {
        if (node.InputA == 0 || node.InputB == 0)
        {
            outReason = "Blend input missing";
            return true;
        }
    }

    if (node.Kind == AnimGraphNodeKind::Output && node.InputPose == 0)
    {
        outReason = "Output pose not connected";
        return true;
    }

    if (node.Kind == AnimGraphNodeKind::StateMachine && !graph.FindStateMachine(node.StateMachineID))
    {
        outReason = "Missing state machine data";
        return true;
    }

    return false;
}

uint64 MakeStatePoseGraphRuntimeScopeIDForEditor(const uint64 stateMachineID, const uint64 stateID)
{
    return (stateMachineID << 32) ^ stateID;
}

SkeletalMeshComponent* GetPreviewSkeletalMeshComponent(Actor* previewActor)
{
    if (!previewActor || !previewActor->IsValid())
        return nullptr;
    return &previewActor->GetComponent<SkeletalMeshComponent>();
}

void SetPreviewPlaybackTime(SkeletalMeshComponent& comp, const float playbackTime)
{
    const float clampedTime = glm::max(0.0f, playbackTime);
    comp.PlaybackTime = clampedTime;
    comp.OverridePlaybackTime = clampedTime;

    for (AnimStateMachineRuntime& runtime : comp.StateMachineRuntimes)
    {
        runtime.StateTime = clampedTime;
        runtime.PreviousStateTime = clampedTime;
        runtime.TransitionTime = 0.0f;
        runtime.TransitionDuration = 0.0f;
        runtime.bTransitionActive = false;
    }
}

const AnimBlendNodeRuntime* FindBlendRuntime(const SkeletalMeshComponent* comp, const uint64 scopeID, const uint64 nodeID)
{
    if (!comp)
        return nullptr;

    for (const AnimBlendNodeRuntime& runtime : comp->BlendNodeRuntimes)
    {
        if (runtime.ScopeID == scopeID && runtime.NodeID == nodeID)
            return &runtime;
    }
    return nullptr;
}

const AnimStateMachineRuntime* FindStateMachineRuntime(const SkeletalMeshComponent* comp, const uint64 stateMachineID)
{
    if (!comp)
        return nullptr;

    for (const AnimStateMachineRuntime& runtime : comp->StateMachineRuntimes)
    {
        if (runtime.StateMachineID == stateMachineID)
            return &runtime;
    }
    return nullptr;
}

bool AliasHasTarget(const AnimStateAlias& alias, const uint64 stateID)
{
    for (const AnimStateAliasTarget& target : alias.Targets)
    {
        if (target.StateID == stateID)
            return true;
    }
    for (const uint64 targetID : alias.TargetStateIDs)
    {
        if (targetID == stateID)
            return true;
    }
    return false;
}

AnimStateAliasTarget* FindAliasTarget(AnimStateAlias& alias, const uint64 stateID)
{
    for (AnimStateAliasTarget& target : alias.Targets)
    {
        if (target.StateID == stateID)
            return &target;
    }
    return nullptr;
}

const AnimStateAliasTarget* FindAliasTarget(const AnimStateAlias& alias, const uint64 stateID)
{
    for (const AnimStateAliasTarget& target : alias.Targets)
    {
        if (target.StateID == stateID)
            return &target;
    }
    return nullptr;
}

bool AddAliasTarget(AnimStateAlias& alias, const uint64 stateID)
{
    if (stateID == 0 || AliasHasTarget(alias, stateID))
        return false;

    alias.TargetStateIDs.Add(stateID);
    AnimStateAliasTarget& target = alias.Targets.Emplace();
    target.StateID = stateID;
    target.BlendDuration = alias.BlendDuration;
    if (alias.ToStateID == 0)
        alias.ToStateID = stateID;
    return true;
}

bool RemoveAliasTarget(AnimStateAlias& alias, const uint64 stateID)
{
    bool removed = false;
    for (int32 i = 0; i < alias.Targets.Num(); ++i)
    {
        if (alias.Targets[i].StateID == stateID)
        {
            alias.Targets.RemoveAt(i);
            removed = true;
            break;
        }
    }

    for (int32 i = 0; i < alias.TargetStateIDs.Num(); ++i)
    {
        if (alias.TargetStateIDs[i] == stateID)
        {
            alias.TargetStateIDs.RemoveAt(i);
            alias.ToStateID = alias.TargetStateIDs.Num() > 0 ? alias.TargetStateIDs[0] : 0;
            return true;
        }
    }

    if (alias.ToStateID == stateID)
    {
        alias.ToStateID = 0;
        return true;
    }

    return removed;
}

bool DuplicatePoseNode(AnimGraphAsset& graph, AnimPoseGraph* poseGraph, const uint64 sourceNodeID, uint64& outNewNodeID)
{
    outNewNodeID = 0;
    const AnimGraphNode* source = poseGraph ? graph.FindNode(*poseGraph, sourceNodeID) : graph.FindNode(sourceNodeID);
    if (!source || source->Kind == AnimGraphNodeKind::Output || source->Kind == AnimGraphNodeKind::StateMachine)
        return false;

    AnimGraphNode& copy = poseGraph
        ? graph.AddNode(*poseGraph, source->Kind, source->Name + " Copy")
        : graph.AddNode(source->Kind, source->Name + " Copy");

    const uint64 newID = copy.ID;
    copy = *source;
    copy.ID = newID;
    copy.Name = source->Name + " Copy";
    copy.EditorX = source->EditorX + 36.0f;
    copy.EditorY = source->EditorY + 36.0f;
    outNewNodeID = copy.ID;
    return true;
}

bool DeleteSelectedPoseNode(AnimGraphAsset& graph, AnimPoseGraph* poseGraph, uint64& selectedNodeID)
{
    if (selectedNodeID == 0)
        return false;

    const uint64 deletedNodeID = selectedNodeID;
    const bool deleted = poseGraph
        ? graph.RemoveNode(*poseGraph, deletedNodeID)
        : graph.RemoveNode(deletedNodeID);
    if (!deleted)
        return false;

    if (poseGraph)
        selectedNodeID = poseGraph->OutputNodeID;
    else
        selectedNodeID = graph.m_OutputNodeID;
    return true;
}

bool DeleteSelectedTransition(AnimStateMachine& stateMachine, uint64& selectedTransitionID)
{
    if (selectedTransitionID == 0)
        return false;

    for (int32 i = 0; i < stateMachine.Transitions.Num(); ++i)
    {
        if (stateMachine.Transitions[i].ID == selectedTransitionID)
        {
            stateMachine.Transitions.RemoveAt(i);
            selectedTransitionID = 0;
            return true;
        }
    }
    return false;
}

bool DeleteSelectedAlias(AnimStateMachine& stateMachine, uint64& selectedAliasID)
{
    if (selectedAliasID == 0)
        return false;

    for (int32 i = 0; i < stateMachine.Aliases.Num(); ++i)
    {
        if (stateMachine.Aliases[i].ID == selectedAliasID)
        {
            stateMachine.Aliases.RemoveAt(i);
            selectedAliasID = 0;
            return true;
        }
    }
    return false;
}

bool DeleteSelectedState(AnimStateMachine& stateMachine, uint64& selectedStateID, uint64& selectedTransitionID, uint64& selectedAliasID)
{
    if (selectedStateID == 0 || stateMachine.States.Num() <= 1)
        return false;

    const uint64 deletedStateID = selectedStateID;
    for (int32 i = 0; i < stateMachine.States.Num(); ++i)
    {
        if (stateMachine.States[i].ID == deletedStateID)
        {
            stateMachine.States.RemoveAt(i);
            break;
        }
    }

    for (int32 i = static_cast<int32>(stateMachine.Transitions.Num()); i-- > 0;)
    {
        if (stateMachine.Transitions[i].FromStateID == deletedStateID || stateMachine.Transitions[i].ToStateID == deletedStateID)
            stateMachine.Transitions.RemoveAt(i);
    }

    for (AnimStateAlias& alias : stateMachine.Aliases)
    {
        if (alias.ToStateID == deletedStateID)
            alias.ToStateID = 0;
        for (int32 i = static_cast<int32>(alias.Targets.Num()); i-- > 0;)
        {
            if (alias.Targets[i].StateID == deletedStateID)
                alias.Targets.RemoveAt(i);
        }
        for (int32 i = static_cast<int32>(alias.TargetStateIDs.Num()); i-- > 0;)
        {
            if (alias.TargetStateIDs[i] == deletedStateID)
                alias.TargetStateIDs.RemoveAt(i);
        }
        if (alias.ToStateID == 0 && alias.TargetStateIDs.Num() > 0)
            alias.ToStateID = alias.TargetStateIDs[0];
        for (int32 i = static_cast<int32>(alias.AllowedFromStateIDs.Num()); i-- > 0;)
        {
            if (alias.AllowedFromStateIDs[i] == deletedStateID)
                alias.AllowedFromStateIDs.RemoveAt(i);
        }
    }

    if (stateMachine.EntryStateID == deletedStateID)
        stateMachine.EntryStateID = stateMachine.States.Num() > 0 ? stateMachine.States[0].ID : 0;

    selectedStateID = 0;
    selectedTransitionID = 0;
    selectedAliasID = 0;
    return true;
}

bool ShouldHandleGraphShortcut()
{
    const ImGuiIO& io = ImGui::GetIO();
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !io.WantTextInput &&
        !ImGui::IsAnyItemActive();
}

AssetHandle FindPreviewClipHandleForNode(const AnimGraphAsset& graph, uint64 nodeID)
{
    const AnimGraphNode* node = graph.FindNode(nodeID);
    if (!node)
        return 0;

    if (node->Kind == AnimGraphNodeKind::AnimationClip)
        return node->AnimationClip;

    if (node->Kind == AnimGraphNodeKind::Blend)
    {
        const AssetHandle preferredClip = FindPreviewClipHandleForNode(
            graph,
            node->BlendAlpha < 0.5f ? node->InputA : node->InputB);
        if (IsValidAssetHandle(preferredClip))
            return preferredClip;

        const AssetHandle fallbackClip = FindPreviewClipHandleForNode(
            graph,
            node->BlendAlpha < 0.5f ? node->InputB : node->InputA);
        if (IsValidAssetHandle(fallbackClip))
            return fallbackClip;
    }

    if (node->Kind == AnimGraphNodeKind::Output)
        return FindPreviewClipHandleForNode(graph, node->InputPose);

    if (node->Kind == AnimGraphNodeKind::StateMachine)
    {
        const AnimStateMachine* stateMachine = graph.FindStateMachine(node->StateMachineID);
        if (!stateMachine)
            return 0;

        auto findState = [&](const uint64 stateID) -> const AnimState*
        {
            for (const AnimState& state : stateMachine->States)
            {
                if (state.ID == stateID)
                    return &state;
            }
            return nullptr;
        };

        const AnimState* entryState = findState(stateMachine->EntryStateID);
        if (!entryState && !stateMachine->States.IsEmpty())
            entryState = &stateMachine->States[0];

        auto findClipInState = [&](const AnimState& state) -> AssetHandle
        {
            if (const AnimGraphNode* output = graph.FindOutputNode(state.StateGraph))
            {
                const AnimGraphNode* input = graph.FindNode(state.StateGraph, output->InputPose);
                if (input && input->Kind == AnimGraphNodeKind::AnimationClip && IsValidAssetHandle(input->AnimationClip))
                    return input->AnimationClip;
            }

            for (const AnimGraphNode& stateNode : state.StateGraph.Nodes)
            {
                if (stateNode.Kind == AnimGraphNodeKind::AnimationClip && IsValidAssetHandle(stateNode.AnimationClip))
                    return stateNode.AnimationClip;
            }

            return 0;
        };

        if (entryState)
        {
            const AssetHandle entryClip = findClipInState(*entryState);
            if (IsValidAssetHandle(entryClip))
                return entryClip;
        }

        for (const AnimState& state : stateMachine->States)
        {
            const AssetHandle stateClip = findClipInState(state);
            if (IsValidAssetHandle(stateClip))
                return stateClip;
        }
    }

    return 0;
}

AssetHandle FindPreviewClipHandle(const AnimGraphAsset& graph)
{
    if (const AnimGraphNode* output = graph.FindOutputNode())
    {
        const AssetHandle outputClip = FindPreviewClipHandleForNode(graph, output->InputPose);
        if (IsValidAssetHandle(outputClip))
            return outputClip;
    }

    for (const AnimGraphNode& node : graph.m_Nodes)
    {
        if (node.Kind == AnimGraphNodeKind::AnimationClip && IsValidAssetHandle(node.AnimationClip))
            return node.AnimationClip;
    }

    return 0;
}

void DrawGraphLink(ImDrawList& drawList, const ImVec2& canvasMin, const AnimGraphNode& from, const AnimGraphNode& to, ImU32 color)
{
    const ImVec2 a(canvasMin.x + from.EditorX + 160.0f, canvasMin.y + from.EditorY + 29.0f);
    const ImVec2 b(canvasMin.x + to.EditorX, canvasMin.y + to.EditorY + 29.0f);
    const float tangent = glm::max(40.0f, std::fabs(b.x - a.x) * 0.45f);
    drawList.AddBezierCubic(
        a,
        ImVec2(a.x + tangent, a.y),
        ImVec2(b.x - tangent, b.y),
        b,
        color,
        2.0f);
}

float GetTimelineFrameRate(const AnimationAsset& animation)
{
    return animation.m_TicksPerSecond > 0.0f ? animation.m_TicksPerSecond : kDefaultAnimationTimelineFps;
}

int32 TimeToTimelineFrame(const AnimationAsset& animation, const float timeSeconds)
{
    return static_cast<int32>(glm::round(FMath::max(0.0f, timeSeconds) * GetTimelineFrameRate(animation)));
}

float TimelineFrameToTime(const AnimationAsset& animation, const int32 frame)
{
    return FMath::max(0, frame) / GetTimelineFrameRate(animation);
}

int32 GetTimelineFrameMax(const AnimationAsset& animation)
{
    return glm::max(1, TimeToTimelineFrame(animation, glm::max(0.001f, animation.m_DurationSeconds)));
}

class AnimationClipSequence final : public ImSequencer::SequenceInterface
{
public:
    explicit AnimationClipSequence(AnimationAsset& animation)
        : m_Animation(animation)
        , m_StartFrame(0)
        , m_EndFrame(GetTimelineFrameMax(animation))
    {
    }

    int GetFrameMin() const override { return 0; }
    int GetFrameMax() const override { return GetTimelineFrameMax(m_Animation); }
    int GetItemCount() const override { return 1; }
    const char* GetItemLabel(int index) const override
    {
        if (index != 0)
            return "";

        return m_Animation.m_ClipName.length() > 0 ? m_Animation.m_ClipName.c_str() : "Animation Clip";
    }
    const char* GetCollapseFmt() const override { return "%d Frames / clip timeline"; }

    void Get(int index, int** start, int** end, int* type, unsigned int* color) override
    {
        if (type)
            *type = 0;
        if (color)
            *color = 0x8846B7FF;
        if (index != 0)
            return;
        if (start)
            *start = &m_StartFrame;
        if (end)
            *end = &m_EndFrame;
    }

private:
    AnimationAsset& m_Animation;
    int m_StartFrame = 0;
    int m_EndFrame = 0;
};

class AnimPoseGraphEditorDelegate final : public GraphEditor::Delegate
{
public:
    AnimPoseGraphEditorDelegate(
        AnimGraphAsset& graph,
        AnimPoseGraph* poseGraph,
        EditorGraphDocumentState& documentState,
        EditorGraphContextEvent& contextEvent,
        bool& dirty,
        bool& previewDirty,
        String& statusMessage,
        const ImVec2& canvasScreenPos,
        SkeletalMeshComponent* previewComponent,
        uint64 runtimeScopeID)
        : m_Graph(graph)
        , m_PoseGraph(poseGraph)
        , m_DocumentState(documentState)
        , m_ContextEvent(contextEvent)
        , m_Dirty(dirty)
        , m_PreviewDirty(previewDirty)
        , m_StatusMessage(statusMessage)
        , m_CanvasScreenPos(canvasScreenPos)
        , m_PreviewComponent(previewComponent)
        , m_RuntimeScopeID(runtimeScopeID)
    {
        RebuildLinks();
    }

    bool AllowedLink(GraphEditor::NodeIndex from, GraphEditor::NodeIndex to) override
    {
        // GraphEditor passes these arguments as target, source while validating a candidate link.
        const AnimGraphNode* target = NodeAt(from);
        const AnimGraphNode* source = NodeAt(to);
        return source && target && CanConnectPose(source->ID, target->ID);
    }

    void SelectNode(GraphEditor::NodeIndex nodeIndex, bool selected) override
    {
        const AnimGraphNode* node = NodeAt(nodeIndex);
        if (!node)
            return;

        if (selected)
            m_DocumentState.SelectedNodeID = node->ID;
        else if (m_DocumentState.SelectedNodeID == node->ID)
            m_DocumentState.SelectedNodeID = 0;
    }

    void MoveSelectedNodes(const ImVec2 delta) override
    {
        bool moved = false;
        for (AnimGraphNode& node : Nodes())
        {
            if (node.ID != m_DocumentState.SelectedNodeID)
                continue;

            node.EditorX += delta.x;
            node.EditorY += delta.y;
            moved = true;
        }

        if (moved)
            Touch(false);
    }

    void AddLink(
        GraphEditor::NodeIndex inputNodeIndex,
        GraphEditor::SlotIndex,
        GraphEditor::NodeIndex outputNodeIndex,
        GraphEditor::SlotIndex outputSlotIndex) override
    {
        const AnimGraphNode* source = NodeAt(inputNodeIndex);
        AnimGraphNode* target = MutableNodeAt(outputNodeIndex);
        if (!source || !target || !CanConnectPose(source->ID, target->ID))
            return;

        if (target->Kind == AnimGraphNodeKind::Blend)
        {
            if (outputSlotIndex == 0)
                target->InputA = source->ID;
            else if (outputSlotIndex == 1)
                target->InputB = source->ID;
            else
                return;
        }
        else if (target->Kind == AnimGraphNodeKind::Output)
        {
            if (outputSlotIndex != 0)
                return;
            target->InputPose = source->ID;
            SetOutputNodeID(target->ID);
        }
        else
        {
            return;
        }

        Touch(true);
        RebuildLinks();
    }

    void DelLink(GraphEditor::LinkIndex linkIndex) override
    {
        if (linkIndex >= m_Links.Num())
            return;

        const GraphEditor::Link link = m_Links[linkIndex];
        AnimGraphNode* target = MutableNodeAt(link.mOutputNodeIndex);
        const AnimGraphNode* source = NodeAt(link.mInputNodeIndex);
        if (!target || !source)
            return;

        if (target->Kind == AnimGraphNodeKind::Blend)
        {
            if (link.mOutputSlotIndex == 0 && target->InputA == source->ID)
                target->InputA = 0;
            else if (link.mOutputSlotIndex == 1 && target->InputB == source->ID)
                target->InputB = 0;
            else
                return;
        }
        else if (target->Kind == AnimGraphNodeKind::Output)
        {
            if (link.mOutputSlotIndex == 0 && target->InputPose == source->ID)
                target->InputPose = 0;
            else
                return;
        }
        else
        {
            return;
        }

        Touch(true);
        RebuildLinks();
    }

    void CustomDraw(ImDrawList* drawList, ImRect rectangle, GraphEditor::NodeIndex nodeIndex) override
    {
        const AnimGraphNode* node = NodeAt(nodeIndex);
        if (!drawList || !node)
            return;

        const ImU32 textColor = IM_COL32(235, 238, 244, 255);
        const ImU32 mutedColor = IM_COL32(160, 168, 182, 255);
        drawList->AddText(ImVec2(rectangle.Min.x + 10.0f, rectangle.Min.y + 8.0f), textColor, node->Name.c_str());
        String subtitle = ToNodeKindLabel(node->Kind);
        if (node->Kind == AnimGraphNodeKind::Blend)
        {
            subtitle = subtitle + " / " + ToBlendAlphaModeLabel(node->BlendAlphaMode);
            if (const AnimBlendNodeRuntime* runtime = FindBlendRuntime(m_PreviewComponent, m_RuntimeScopeID, node->ID))
                subtitle = subtitle + "  alpha " + FormatFloatCompact(runtime->CurrentAlpha);
        }
        drawList->AddText(ImVec2(rectangle.Min.x + 10.0f, rectangle.Min.y + 30.0f), mutedColor, subtitle.c_str());

        String warning;
        if (IsPoseNodeInvalid(m_Graph, m_PoseGraph, *node, warning))
        {
            drawList->AddText(ImVec2(rectangle.Min.x + 10.0f, rectangle.Min.y + 50.0f), IM_COL32(240, 190, 80, 255), warning.c_str());
            drawList->AddRect(rectangle.Min, rectangle.Max, IM_COL32(240, 190, 80, 220), 5.0f, 0, 2.0f);
        }
    }

    void RightClick(GraphEditor::NodeIndex nodeIndex, GraphEditor::SlotIndex, GraphEditor::SlotIndex) override
    {
        const ImGuiIO& io = ImGui::GetIO();
        m_DocumentState.ContextGraphPosition = EditorGraphPanel::ScreenToGraphPosition(
            io.MousePos,
            m_CanvasScreenPos,
            m_DocumentState.ViewState);

        m_ContextEvent = {};
        m_ContextEvent.GraphPosition = m_DocumentState.ContextGraphPosition;

        if (const AnimGraphNode* node = NodeAt(nodeIndex))
        {
            m_DocumentState.SelectedNodeID = node->ID;
            m_ContextEvent.Target = EditorGraphContextTarget::Node;
            m_ContextEvent.NodeID = node->ID;
            return;
        }

        m_ContextEvent.Target = EditorGraphContextTarget::Canvas;
    }

    const size_t GetTemplateCount() override { return m_PoseGraph ? 3 : 4; }
    const GraphEditor::Template GetTemplate(GraphEditor::TemplateIndex index) override
    {
        const size_t count = GetTemplateCount();
        return Templates()[index < count ? index : 0];
    }

    const size_t GetNodeCount() override { return static_cast<size_t>(Nodes().Num()); }
    const GraphEditor::Node GetNode(GraphEditor::NodeIndex index) override
    {
        const AnimGraphNode* node = NodeAt(index);
        if (!node)
            return {};

        return GraphEditor::Node{
            node->Name.c_str(),
            TemplateIndexForKind(node->Kind),
            ImRect(ImVec2(node->EditorX, node->EditorY), ImVec2(node->EditorX + 170.0f, node->EditorY + 68.0f)),
            node->ID == m_DocumentState.SelectedNodeID
        };
    }

    const size_t GetLinkCount() override { return static_cast<size_t>(m_Links.Num()); }
    const GraphEditor::Link GetLink(GraphEditor::LinkIndex index) override
    {
        return index < m_Links.Num() ? m_Links[index] : GraphEditor::Link{};
    }

private:
    GraphEditor::TemplateIndex TemplateIndexForKind(AnimGraphNodeKind kind) const
    {
        switch (kind)
        {
        case AnimGraphNodeKind::Blend:
            return 1;
        case AnimGraphNodeKind::Output:
            return 2;
        case AnimGraphNodeKind::StateMachine:
            return m_PoseGraph ? 0 : 3;
        case AnimGraphNodeKind::AnimationClip:
        default:
            return 0;
        }
    }

    static const GraphEditor::Template* Templates()
    {
        static const char* clipOutputs[] = { "Pose" };
        static const char* blendInputs[] = { "A", "B" };
        static const char* blendOutputs[] = { "Pose" };
        static const char* outputInputs[] = { "Pose" };
        static const char* stateMachineOutputs[] = { "Pose" };
        static ImU32 poseInputColors[] = { IM_COL32(105, 178, 255, 255), IM_COL32(255, 180, 96, 255) };
        static ImU32 poseOutputColors[] = { IM_COL32(105, 178, 255, 255) };
        static ImU32 outputInputColors[] = { IM_COL32(235, 190, 90, 255) };

        static GraphEditor::Template templates[] = {
            { IM_COL32(80, 150, 230, 255), IM_COL32(31, 35, 42, 255), IM_COL32(42, 48, 58, 255), 0, nullptr, nullptr, 1, clipOutputs, poseOutputColors },
            { IM_COL32(230, 150, 80, 255), IM_COL32(31, 35, 42, 255), IM_COL32(42, 48, 58, 255), 2, blendInputs, poseInputColors, 1, blendOutputs, poseOutputColors },
            { IM_COL32(230, 190, 90, 255), IM_COL32(31, 35, 42, 255), IM_COL32(42, 48, 58, 255), 1, outputInputs, outputInputColors, 0, nullptr, nullptr },
            { IM_COL32(100, 205, 150, 255), IM_COL32(31, 35, 42, 255), IM_COL32(42, 48, 58, 255), 0, nullptr, nullptr, 1, stateMachineOutputs, poseOutputColors }
        };

        return templates;
    }

    TArray<AnimGraphNode>& Nodes() const
    {
        return m_PoseGraph ? m_PoseGraph->Nodes : m_Graph.m_Nodes;
    }

    const AnimGraphNode* NodeAt(GraphEditor::NodeIndex index) const
    {
        const TArray<AnimGraphNode>& nodes = Nodes();
        return index < nodes.Num() ? &nodes[static_cast<int32>(index)] : nullptr;
    }

    AnimGraphNode* MutableNodeAt(GraphEditor::NodeIndex index)
    {
        TArray<AnimGraphNode>& nodes = Nodes();
        return index < nodes.Num() ? &nodes[static_cast<int32>(index)] : nullptr;
    }

    bool CanConnectPose(uint64 sourceID, uint64 targetID) const
    {
        return m_PoseGraph
            ? m_Graph.CanConnectPose(*m_PoseGraph, sourceID, targetID)
            : m_Graph.CanConnectPose(sourceID, targetID);
    }

    void SetOutputNodeID(uint64 nodeID)
    {
        if (m_PoseGraph)
            m_PoseGraph->OutputNodeID = nodeID;
        else
            m_Graph.m_OutputNodeID = nodeID;
    }

    int32 FindNodeIndex(uint64 nodeID) const
    {
        const TArray<AnimGraphNode>& nodes = Nodes();
        for (int32 i = 0; i < nodes.Num(); ++i)
        {
            if (nodes[i].ID == nodeID)
                return i;
        }

        return -1;
    }

    void AddStoredLink(uint64 sourceNodeID, int outputSlot, int32 targetIndex, int inputSlot)
    {
        const int32 sourceIndex = FindNodeIndex(sourceNodeID);
        if (sourceIndex < 0 || targetIndex < 0)
            return;

        m_Links.Add(GraphEditor::Link{
            static_cast<GraphEditor::NodeIndex>(sourceIndex),
            static_cast<GraphEditor::SlotIndex>(outputSlot),
            static_cast<GraphEditor::NodeIndex>(targetIndex),
            static_cast<GraphEditor::SlotIndex>(inputSlot)
        });
    }

    void RebuildLinks()
    {
        m_Links.Clear();
        const TArray<AnimGraphNode>& nodes = Nodes();
        for (int32 i = 0; i < nodes.Num(); ++i)
        {
            const AnimGraphNode& node = nodes[i];
            if (node.Kind == AnimGraphNodeKind::Blend)
            {
                AddStoredLink(node.InputA, 0, i, 0);
                AddStoredLink(node.InputB, 0, i, 1);
            }
            else if (node.Kind == AnimGraphNodeKind::Output)
            {
                AddStoredLink(node.InputPose, 0, i, 0);
            }
        }
    }

    void Touch(bool previewDirty)
    {
        m_Dirty = true;
        if (previewDirty)
            m_PreviewDirty = true;
        m_StatusMessage = "Modified";
    }

private:
    AnimGraphAsset& m_Graph;
    AnimPoseGraph* m_PoseGraph = nullptr;
    EditorGraphDocumentState& m_DocumentState;
    EditorGraphContextEvent& m_ContextEvent;
    bool& m_Dirty;
    bool& m_PreviewDirty;
    String& m_StatusMessage;
    ImVec2 m_CanvasScreenPos;
    SkeletalMeshComponent* m_PreviewComponent = nullptr;
    uint64 m_RuntimeScopeID = 0;
    TArray<GraphEditor::Link> m_Links;
};
class AnimStateMachineGraphDelegate final : public GraphEditor::Delegate
{
private:
    enum class SMNodeKind : uint8 { State, Alias };
    struct SMNodeRef { SMNodeKind Kind = SMNodeKind::State; uint64 ID = 0; };
    struct SMLinkRef { bool bAlias = false; uint64 ID = 0; uint64 TargetStateID = 0; };

public:
    AnimStateMachineGraphDelegate(
        AnimGraphAsset& graph,
        AnimStateMachine& stateMachine,
        uint64& selectedStateID,
        uint64& selectedTransitionID,
        uint64& selectedAliasID,
        bool& dirty,
        bool& previewDirty,
        String& statusMessage,
        EditorGraphDocumentState& documentState,
        EditorGraphContextEvent& contextEvent,
        const ImVec2& canvasScreenPos,
        SkeletalMeshComponent* previewComponent)
        : m_Graph(graph)
        , m_StateMachine(stateMachine)
        , m_SelectedStateID(selectedStateID)
        , m_SelectedTransitionID(selectedTransitionID)
        , m_SelectedAliasID(selectedAliasID)
        , m_Dirty(dirty)
        , m_PreviewDirty(previewDirty)
        , m_StatusMessage(statusMessage)
        , m_DocumentState(documentState)
        , m_ContextEvent(contextEvent)
        , m_CanvasScreenPos(canvasScreenPos)
        , m_PreviewComponent(previewComponent)
    {
        RebuildNodeRefs();
        RebuildLinks();
    }

    bool AllowedLink(GraphEditor::NodeIndex from, GraphEditor::NodeIndex to) override
    {
        const SMNodeRef* fromRef = RefAt(from);
        const SMNodeRef* toRef = RefAt(to);
        return from != to && fromRef && toRef && toRef->Kind == SMNodeKind::State;
    }

    void SelectNode(GraphEditor::NodeIndex nodeIndex, bool selected) override
    {
        const SMNodeRef* ref = RefAt(nodeIndex);
        if (!ref)
            return;

        if (selected)
        {
            m_SelectedTransitionID = 0;
            m_SelectedStateID = ref->Kind == SMNodeKind::State ? ref->ID : 0;
            m_SelectedAliasID = ref->Kind == SMNodeKind::Alias ? ref->ID : 0;
        }
        else
        {
            if (ref->Kind == SMNodeKind::State && m_SelectedStateID == ref->ID)
                m_SelectedStateID = 0;
            if (ref->Kind == SMNodeKind::Alias && m_SelectedAliasID == ref->ID)
                m_SelectedAliasID = 0;
        }
    }

    void MoveSelectedNodes(const ImVec2 delta) override
    {
        for (AnimState& state : m_StateMachine.States)
        {
            if (state.ID == m_SelectedStateID)
            {
                state.EditorX += delta.x;
                state.EditorY += delta.y;
                Touch(true);
                return;
            }
        }

        for (AnimStateAlias& alias : m_StateMachine.Aliases)
        {
            if (alias.ID == m_SelectedAliasID)
            {
                alias.EditorX += delta.x;
                alias.EditorY += delta.y;
                Touch(true);
                return;
            }
        }
    }

    void AddLink(GraphEditor::NodeIndex inputNodeIndex, GraphEditor::SlotIndex, GraphEditor::NodeIndex outputNodeIndex, GraphEditor::SlotIndex) override
    {
        const SMNodeRef* fromRef = RefAt(inputNodeIndex);
        const AnimState* to = StateAt(outputNodeIndex);
        if (!fromRef || !to)
            return;

        if (fromRef->Kind == SMNodeKind::Alias)
        {
            AnimStateAlias* alias = MutableAliasByID(fromRef->ID);
            if (!alias)
                return;
            if (!AddAliasTarget(*alias, to->ID))
                return;
            if (AnimStateAliasTarget* target = FindAliasTarget(*alias, to->ID))
            {
                if (target->Conditions.IsEmpty())
                    target->Conditions.Emplace();
            }
            m_SelectedAliasID = alias->ID;
            m_SelectedStateID = 0;
            m_SelectedTransitionID = 0;
            Touch(true);
            RebuildLinks();
            return;
        }

        const AnimState* from = StateAt(inputNodeIndex);
        if (!from || from->ID == to->ID)
            return;

        for (const AnimTransition& transition : m_StateMachine.Transitions)
        {
            if (transition.FromStateID == from->ID && transition.ToStateID == to->ID)
                return;
        }

        AnimTransition& transition = m_Graph.AddTransition(m_StateMachine, from->ID, to->ID);
        transition.Conditions.Emplace();
        m_SelectedTransitionID = transition.ID;
        m_SelectedStateID = 0;
        m_SelectedAliasID = 0;
        Touch(true);
        RebuildLinks();
    }

    void DelLink(GraphEditor::LinkIndex linkIndex) override
    {
        if (linkIndex >= m_LinkRefs.Num())
            return;

        const SMLinkRef linkRef = m_LinkRefs[static_cast<int32>(linkIndex)];
        if (linkRef.bAlias)
        {
            if (AnimStateAlias* alias = MutableAliasByID(linkRef.ID))
            {
                RemoveAliasTarget(*alias, linkRef.TargetStateID);
                Touch(true);
                RebuildLinks();
            }
            return;
        }

        for (int32 i = 0; i < m_StateMachine.Transitions.Num(); ++i)
        {
            if (m_StateMachine.Transitions[i].ID == linkRef.ID)
            {
                m_StateMachine.Transitions.RemoveAt(i);
                if (m_SelectedTransitionID == linkRef.ID)
                    m_SelectedTransitionID = 0;
                Touch(true);
                RebuildLinks();
                return;
            }
        }
    }

    void CustomDraw(ImDrawList* drawList, ImRect rectangle, GraphEditor::NodeIndex nodeIndex) override
    {
        const SMNodeRef* ref = RefAt(nodeIndex);
        if (!drawList || !ref)
            return;

        const ImU32 textColor = IM_COL32(235, 238, 244, 255);
        const ImU32 mutedColor = IM_COL32(160, 168, 182, 255);
        const AnimStateMachineRuntime* runtime = FindStateMachineRuntime(m_PreviewComponent, m_StateMachine.ID);
        if (ref->Kind == SMNodeKind::Alias)
        {
            const AnimStateAlias* alias = AliasByID(ref->ID);
            if (!alias)
                return;
            drawList->AddText(ImVec2(rectangle.Min.x + 10.0f, rectangle.Min.y + 8.0f), textColor, alias->Name.c_str());
            const uint64 targetCount = static_cast<uint64>(alias->Targets.Num() > 0 ? alias->Targets.Num() : alias->TargetStateIDs.Num());
            const String subtitle = String(alias->bGlobalAlias ? "Global Alias" : "Alias") +
                " / " + String(std::to_string(targetCount).c_str()) + " target(s)";
            drawList->AddText(ImVec2(rectangle.Min.x + 10.0f, rectangle.Min.y + 30.0f), mutedColor, subtitle.c_str());
            return;
        }

        const AnimState* state = StateByID(ref->ID);
        if (!state)
            return;
        drawList->AddText(ImVec2(rectangle.Min.x + 10.0f, rectangle.Min.y + 8.0f), textColor, state->Name.c_str());
        String subtitle = state->ID == m_StateMachine.EntryStateID ? "Entry State" : "State";
        if (runtime && runtime->CurrentStateID == state->ID)
            subtitle = subtitle + (runtime->bTransitionActive ? " / Blending In" : " / Active");
        else if (runtime && runtime->PreviousStateID == state->ID && runtime->bTransitionActive)
            subtitle = subtitle + " / Blending Out";
        drawList->AddText(ImVec2(rectangle.Min.x + 10.0f, rectangle.Min.y + 30.0f), mutedColor, subtitle.c_str());
        if (runtime && runtime->CurrentStateID == state->ID)
            drawList->AddRect(rectangle.Min, rectangle.Max, IM_COL32(80, 220, 140, 240), 5.0f, 0, 3.0f);
        else if (runtime && runtime->PreviousStateID == state->ID && runtime->bTransitionActive)
            drawList->AddRect(rectangle.Min, rectangle.Max, IM_COL32(240, 190, 80, 220), 5.0f, 0, 2.0f);
    }

    void RightClick(GraphEditor::NodeIndex nodeIndex, GraphEditor::SlotIndex, GraphEditor::SlotIndex) override
    {
        const ImGuiIO& io = ImGui::GetIO();
        m_DocumentState.ContextGraphPosition = EditorGraphPanel::ScreenToGraphPosition(
            io.MousePos,
            m_CanvasScreenPos,
            m_DocumentState.ViewState);

        m_ContextEvent = {};
        m_ContextEvent.GraphPosition = m_DocumentState.ContextGraphPosition;

        if (const SMNodeRef* ref = RefAt(nodeIndex))
        {
            m_SelectedTransitionID = 0;
            m_SelectedStateID = ref->Kind == SMNodeKind::State ? ref->ID : 0;
            m_SelectedAliasID = ref->Kind == SMNodeKind::Alias ? ref->ID : 0;
            m_ContextEvent.Target = EditorGraphContextTarget::Node;
            m_ContextEvent.NodeID = ref->ID;
            return;
        }

        m_ContextEvent.Target = EditorGraphContextTarget::Canvas;
    }

    const size_t GetTemplateCount() override { return 2; }
    const GraphEditor::Template GetTemplate(GraphEditor::TemplateIndex index) override
    {
        static const char* transitionSlots[] = { "Transition" };
        static ImU32 transitionColors[] = { IM_COL32(100, 205, 150, 255) };
        static const char* aliasSlots[] = { "Alias" };
        static ImU32 aliasColors[] = { IM_COL32(245, 181, 90, 255) };
        if (index == 1)
            return GraphEditor::Template{ IM_COL32(245, 181, 90, 255), IM_COL32(36, 31, 24, 255), IM_COL32(52, 43, 31, 255), 0, nullptr, nullptr, 1, aliasSlots, aliasColors };
        return GraphEditor::Template{ IM_COL32(100, 205, 150, 255), IM_COL32(31, 35, 42, 255), IM_COL32(42, 48, 58, 255), 1, transitionSlots, transitionColors, 1, transitionSlots, transitionColors };
    }

    const size_t GetNodeCount() override { return static_cast<size_t>(m_NodeRefs.Num()); }
    const GraphEditor::Node GetNode(GraphEditor::NodeIndex index) override
    {
        const SMNodeRef* ref = RefAt(index);
        if (!ref)
            return {};
        if (ref->Kind == SMNodeKind::Alias)
        {
            const AnimStateAlias* alias = AliasByID(ref->ID);
            return alias ? GraphEditor::Node{ alias->Name.c_str(), 1, ImRect(ImVec2(alias->EditorX, alias->EditorY), ImVec2(alias->EditorX + 170.0f, alias->EditorY + 68.0f)), alias->ID == m_SelectedAliasID } : GraphEditor::Node{};
        }
        const AnimState* state = StateByID(ref->ID);
        return state ? GraphEditor::Node{ state->Name.c_str(), 0, ImRect(ImVec2(state->EditorX, state->EditorY), ImVec2(state->EditorX + 170.0f, state->EditorY + 68.0f)), state->ID == m_SelectedStateID } : GraphEditor::Node{};
    }

    const size_t GetLinkCount() override { return static_cast<size_t>(m_Links.Num()); }
    const GraphEditor::Link GetLink(GraphEditor::LinkIndex index) override { return index < m_Links.Num() ? m_Links[index] : GraphEditor::Link{}; }

private:
    const SMNodeRef* RefAt(GraphEditor::NodeIndex index) const { return index < m_NodeRefs.Num() ? &m_NodeRefs[static_cast<int32>(index)] : nullptr; }
    const AnimState* StateAt(GraphEditor::NodeIndex index) const { const SMNodeRef* ref = RefAt(index); return ref && ref->Kind == SMNodeKind::State ? StateByID(ref->ID) : nullptr; }

    const AnimState* StateByID(uint64 stateID) const
    {
        for (const AnimState& state : m_StateMachine.States)
            if (state.ID == stateID)
                return &state;
        return nullptr;
    }

    const AnimStateAlias* AliasByID(uint64 aliasID) const
    {
        for (const AnimStateAlias& alias : m_StateMachine.Aliases)
            if (alias.ID == aliasID)
                return &alias;
        return nullptr;
    }

    AnimStateAlias* MutableAliasByID(uint64 aliasID)
    {
        for (AnimStateAlias& alias : m_StateMachine.Aliases)
            if (alias.ID == aliasID)
                return &alias;
        return nullptr;
    }

    int32 FindNodeIndex(SMNodeKind kind, uint64 id) const
    {
        for (int32 i = 0; i < m_NodeRefs.Num(); ++i)
            if (m_NodeRefs[i].Kind == kind && m_NodeRefs[i].ID == id)
                return i;
        return -1;
    }

    void RebuildNodeRefs()
    {
        m_NodeRefs.Clear();
        for (const AnimState& state : m_StateMachine.States)
        {
            SMNodeRef& ref = m_NodeRefs.Emplace();
            ref.Kind = SMNodeKind::State;
            ref.ID = state.ID;
        }
        for (const AnimStateAlias& alias : m_StateMachine.Aliases)
        {
            SMNodeRef& ref = m_NodeRefs.Emplace();
            ref.Kind = SMNodeKind::Alias;
            ref.ID = alias.ID;
        }
    }

    void RebuildLinks()
    {
        m_Links.Clear();
        m_LinkRefs.Clear();
        for (const AnimTransition& transition : m_StateMachine.Transitions)
        {
            const int32 fromIndex = FindNodeIndex(SMNodeKind::State, transition.FromStateID);
            const int32 toIndex = FindNodeIndex(SMNodeKind::State, transition.ToStateID);
            if (fromIndex < 0 || toIndex < 0)
                continue;
            m_Links.Add(GraphEditor::Link{ static_cast<GraphEditor::NodeIndex>(fromIndex), 0, static_cast<GraphEditor::NodeIndex>(toIndex), 0 });
            SMLinkRef& linkRef = m_LinkRefs.Emplace();
            linkRef.bAlias = false;
            linkRef.ID = transition.ID;
        }
        for (const AnimStateAlias& alias : m_StateMachine.Aliases)
        {
            const int32 fromIndex = FindNodeIndex(SMNodeKind::Alias, alias.ID);
            if (fromIndex < 0)
                continue;
            auto addAliasLink = [&](const uint64 targetStateID)
            {
                const int32 toIndex = FindNodeIndex(SMNodeKind::State, targetStateID);
                if (toIndex < 0)
                    return;
                m_Links.Add(GraphEditor::Link{ static_cast<GraphEditor::NodeIndex>(fromIndex), 0, static_cast<GraphEditor::NodeIndex>(toIndex), 0 });
                SMLinkRef& linkRef = m_LinkRefs.Emplace();
                linkRef.bAlias = true;
                linkRef.ID = alias.ID;
                linkRef.TargetStateID = targetStateID;
            };

            if (!alias.Targets.IsEmpty())
            {
                for (const AnimStateAliasTarget& target : alias.Targets)
                    addAliasLink(target.StateID);
            }
            else
            {
                for (const uint64 targetStateID : alias.TargetStateIDs)
                    addAliasLink(targetStateID);
            }
        }
    }

    void Touch(bool previewDirty)
    {
        m_Dirty = true;
        if (previewDirty)
            m_PreviewDirty = true;
        m_StatusMessage = "Modified";
    }

private:
    AnimGraphAsset& m_Graph;
    AnimStateMachine& m_StateMachine;
    uint64& m_SelectedStateID;
    uint64& m_SelectedTransitionID;
    uint64& m_SelectedAliasID;
    bool& m_Dirty;
    bool& m_PreviewDirty;
    String& m_StatusMessage;
    EditorGraphDocumentState& m_DocumentState;
    EditorGraphContextEvent& m_ContextEvent;
    ImVec2 m_CanvasScreenPos;
    SkeletalMeshComponent* m_PreviewComponent = nullptr;
    TArray<SMNodeRef> m_NodeRefs;
    TArray<GraphEditor::Link> m_Links;
    TArray<SMLinkRef> m_LinkRefs;
};
}

void PreviewAssetEditorBase::BeginEditorWindow(
    const char* baseTitle,
    Asset* asset,
    const ImGuiID documentDockId,
    const ImGuiID documentClassId,
    bool& open,
    bool& visible)
{
    String title = baseTitle;
    if (asset && asset->Path.length() > 0)
        title = title + " - " + asset->Path;

    ImGuiWindowClass windowClass{};
    windowClass.ClassId = documentClassId;
    windowClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&windowClass);
    if (documentDockId != 0)
        ImGui::SetNextWindowDockID(documentDockId, ImGuiCond_Appearing);
    if (m_RequestFocus)
    {
        ImGui::SetNextWindowFocus();
        m_RequestFocus = false;
    }

    open = m_IsOpen;
    visible = ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_MenuBar);
}

void PreviewAssetEditorBase::DrawViewportImage(Scene* scene, const char* label, const bool drawGrid)
{
    RenderModule* renderer = GEngine ? GEngine->GetModuleManager().GetModule<RenderModule>() : nullptr;
    const ImVec2 size = ImGui::GetContentRegionAvail();
    if (renderer && scene && size.x > 32.0f && size.y > 32.0f)
    {
        UpdatePreviewCamera(size);
        const float aspect = size.y > 1.0f ? size.x / size.y : 1.0f;
        renderer->RenderScenePreview(
            *scene,
            m_PreviewCamera.GetCameraView(aspect),
            static_cast<uint32>(size.x),
            static_cast<uint32>(size.y),
            drawGrid,
            false);
        ImGui::Image((ImTextureID)(intptr_t)renderer->GetPreviewTexture(), size);
        m_PreviewViewportHovered = ImGui::IsItemHovered();

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImU32 borderColor = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            ? IM_COL32(178, 128, 51, 255)
            : IM_COL32(58, 61, 70, 255);
        ImGui::GetWindowDrawList()->AddRect(min, max, borderColor, 0.0f, 0, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(min.x + 12.0f, min.y + 12.0f));
        ImGui::BeginGroup();
        ImGui::TextDisabled("%s", label);
        ImGui::TextDisabled("RMB Look + WASDQE Move");
        ImGui::TextDisabled("MMB Pan  |  Wheel Speed");
        ImGui::EndGroup();
        return;
    }

    EndPreviewCameraInteraction();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, IM_COL32(18, 20, 24, 255));
    drawList->AddRect(min, max, IM_COL32(62, 66, 76, 255));
    drawList->AddText(ImVec2(min.x + 14.0f, min.y + 14.0f), IM_COL32(170, 176, 188, 255), label);
    ImGui::Dummy(size);
}

void PreviewAssetEditorBase::ResetPreviewCamera()
{
    m_PreviewPosition = Vector3(-4.0f, -4.0f, 2.5f);
    m_PreviewYaw = 45.0f;
    m_PreviewPitch = -20.0f;
    m_PreviewMoveSpeed = 5.0f;
    m_PreviewCamera = Camera(m_PreviewPosition, m_PreviewYaw, m_PreviewPitch);
    m_PreviewCamera.SetZoom(60.0f);
    m_PreviewCamera.SetMovementSpeed(m_PreviewMoveSpeed);
    EndPreviewCameraInteraction();
}

void PreviewAssetEditorBase::UpdatePreviewCamera(const ImVec2& viewportSize)
{
    if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const bool hovered = m_PreviewViewportHovered;
    const bool rightMouseDown = InputModule::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine);

    if (hovered && rightMouseDown && !m_PreviewFlyActive)
    {
        m_PreviewFlyActive = true;
        if (editor)
            editor->SetEditorCameraCaptured(true);
    }
    else if (!rightMouseDown && m_PreviewFlyActive)
    {
        EndPreviewCameraInteraction();
    }

    if (!hovered && !m_PreviewFlyActive)
        return;

    if (io.MouseWheel != 0.0f)
    {
        m_PreviewMoveSpeed = FMath::max(
            kCameraSpeedStep,
            glm::round((m_PreviewMoveSpeed + io.MouseWheel * kCameraSpeedStep) / kCameraSpeedStep) * kCameraSpeedStep);
        m_PreviewCamera.SetMovementSpeed(m_PreviewMoveSpeed);
    }

    const InputModule::MouseDelta mouseDelta = InputModule::GetMouseDelta();
    if (m_PreviewFlyActive)
    {
        m_PreviewYaw -= mouseDelta.x * 0.15f;
        m_PreviewPitch = FMath::clamp(m_PreviewPitch - mouseDelta.y * 0.15f, -80.0f, 80.0f);
    }

    const float yawRad = FMath::radians(m_PreviewYaw);
    const float pitchRad = FMath::radians(m_PreviewPitch);
    const Vector3 forward(
        FMath::cos(pitchRad) * FMath::cos(yawRad),
        FMath::cos(pitchRad) * FMath::sin(yawRad),
        FMath::sin(pitchRad));
    const Vector3 normalizedForward = FMath::normalize(forward);
    const Vector3 up = Vector3(0.0f, 0.0f, 1.0f);
    Vector3 right = FMath::normalize(FMath::cross(normalizedForward, up));
    if (FMath::length(right) < 1e-3f)
        right = Vector3(1.0f, 0.0f, 0.0f);
    const Vector3 cameraUp = FMath::normalize(FMath::cross(right, normalizedForward));

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        constexpr float panScale = 0.01f;
        m_PreviewPosition -= right * (io.MouseDelta.x * panScale);
        m_PreviewPosition += cameraUp * (io.MouseDelta.y * panScale);
    }

    if (m_PreviewFlyActive)
    {
        const float dt = FMath::max(io.DeltaTime, 1.0f / 240.0f);
        float moveSpeed = InputModule::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) ? m_PreviewMoveSpeed * 2.0f : m_PreviewMoveSpeed;
        moveSpeed *= dt;

        if (InputModule::IsKeyPressed(GLFW_KEY_W)) m_PreviewPosition += normalizedForward * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_S)) m_PreviewPosition -= normalizedForward * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_A)) m_PreviewPosition -= right * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_D)) m_PreviewPosition += right * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_E)) m_PreviewPosition += up * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_Q)) m_PreviewPosition -= up * moveSpeed;
    }

    m_PreviewCamera.SetPosition(m_PreviewPosition);
    m_PreviewCamera.SetRotation(m_PreviewYaw, m_PreviewPitch);
}

void PreviewAssetEditorBase::EndPreviewCameraInteraction()
{
    m_PreviewFlyActive = false;
    m_PreviewViewportHovered = false;
    if (EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine))
        editor->SetEditorCameraCaptured(false);
}

void PreviewAssetEditorBase::CloseBase()
{
    m_AssetHandle = 0;
    m_IsOpen = false;
    m_LayoutInitialized = false;
    EndPreviewCameraInteraction();
}

const Rebel::Core::Reflection::TypeInfo* AnimationAssetEditor::GetSupportedAssetType() const
{
    return AnimationAsset::StaticType();
}

void AnimationAssetEditor::Open(const AssetHandle assetHandle)
{
    m_AssetHandle = assetHandle;
    m_IsOpen = ReloadAsset();
}

bool AnimationAssetEditor::ReloadAsset()
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule)
        return false;

    m_Animation = dynamic_cast<AnimationAsset*>(assetModule->GetManager().Load(m_AssetHandle));
    m_Skeleton = m_Animation && IsValidAssetHandle(m_Animation->m_SkeletonID)
        ? dynamic_cast<SkeletonAsset*>(assetModule->GetManager().Load(m_Animation->m_SkeletonID))
        : nullptr;

    m_PreviewScene.Clear();
    m_PreviewActor = nullptr;
    if (m_Animation && m_Skeleton)
    {
        if (SkeletalMeshAsset* mesh = FindPreviewMeshForSkeleton(m_Animation->m_SkeletonID))
        {
            m_PreviewActor = &m_PreviewScene.SpawnActor<Actor>();
            SkeletalMeshComponent& comp = m_PreviewActor->AddComponent<SkeletalMeshComponent>();
            comp.Mesh = AssetPtr<SkeletalMeshAsset>(mesh->ID);
            comp.Animation = AssetPtr<AnimationAsset>(m_Animation->ID);
            comp.PlayAnimation(AssetPtr<AnimationAsset>(m_Animation->ID), m_Looping, 1.0f, 0.0f);
            comp.bPlayAnimation = m_Playing;
            comp.bLoopAnimation = m_Looping;
            SetPreviewPlaybackTime(comp, m_PlaybackTime);
            comp.bDrawSkeleton = true;
        }
    }

    ResetPreviewCamera();
    m_LayoutInitialized = false;
    return m_Animation != nullptr;
}

void AnimationAssetEditor::EnsureLayout(const ImVec2& size)
{
    const String dockspaceName = MakeScopedEditorId("AnimationAssetEditorDockspace", m_AssetHandle);
    const ImGuiID dockspaceId = ImGui::GetID(dockspaceName.c_str());
    if (m_LayoutInitialized || dockspaceId == 0 || size.x <= 1.0f || size.y <= 1.0f)
        return;

    const String viewportWindow = MakeScopedEditorName("Viewport", "AnimationAssetEditorViewport", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "AnimationAssetEditorDetails", m_AssetHandle);
    const String timelineWindow = MakeScopedEditorName("Timeline", "AnimationAssetEditorTimeline", m_AssetHandle);
    const String tracksWindow = MakeScopedEditorName("Tracks", "AnimationAssetEditorTracks", m_AssetHandle);

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID dockLeft = 0;
    ImGuiID dockRight = 0;
    ImGuiID dockBottom = 0;
    ImGuiID dockMain = dockspaceId;
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, &dockLeft, &dockMain);
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.28f, &dockRight, &dockMain);
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.24f, &dockBottom, &dockMain);
    ImGui::DockBuilderDockWindow(tracksWindow.c_str(), dockLeft);
    ImGui::DockBuilderDockWindow(viewportWindow.c_str(), dockMain);
    ImGui::DockBuilderDockWindow(timelineWindow.c_str(), dockBottom);
    ImGui::DockBuilderDockWindow(detailsWindow.c_str(), dockRight);
    ImGui::DockBuilderFinish(dockspaceId);
    m_LayoutInitialized = true;
}

void AnimationAssetEditor::Draw(ImGuiID documentDockId, ImGuiID documentClassId)
{
    if (!m_IsOpen)
        return;

    bool open = true;
    bool visible = false;
    BeginEditorWindow("Animation Asset Editor", m_Animation, documentDockId, documentClassId, open, visible);
    if (!visible)
    {
        ImGui::End();
        if (!open)
            CloseBase();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::Button((String(m_Playing ? ICON_FA_PAUSE : ICON_FA_PLAY) + " Preview").c_str()))
            m_Playing = !m_Playing;
        ImGui::SameLine();
        if (ImGui::Button((String(ICON_FA_ARROWS_ROTATE) + " Reload").c_str()))
            ReloadAsset();
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &m_Looping);
        ImGui::EndMenuBar();
    }

    bool editorFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImGui::BeginChild("AnimationAssetEditorDockHost", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 dockSize = ImGui::GetContentRegionAvail();
    const String dockspaceName = MakeScopedEditorId("AnimationAssetEditorDockspace", m_AssetHandle);
    ImGui::DockSpace(ImGui::GetID(dockspaceName.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    EnsureLayout(dockSize);
    ImGui::EndChild();
    ImGui::End();

    ImGuiWindowClass panelClass{};
    panelClass.ClassId = documentClassId;
    panelClass.DockingAllowUnclassed = false;

    const String viewportWindow = MakeScopedEditorName("Viewport", "AnimationAssetEditorViewport", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "AnimationAssetEditorDetails", m_AssetHandle);
    const String timelineWindow = MakeScopedEditorName("Timeline", "AnimationAssetEditorTimeline", m_AssetHandle);
    const String tracksWindow = MakeScopedEditorName("Tracks", "AnimationAssetEditorTracks", m_AssetHandle);

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(viewportWindow.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        editorFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (m_PreviewActor && m_PreviewActor->IsValid())
        {
            SkeletalMeshComponent& comp = m_PreviewActor->GetComponent<SkeletalMeshComponent>();
            SetPreviewPlaybackTime(comp, m_PlaybackTime);
            comp.bOverrideAnimationLooping = m_Looping;
            comp.bPlayAnimation = m_Playing;
            comp.bLoopAnimation = m_Looping;
            TickPreviewAnimation(m_PreviewScene, m_Playing ? ImGui::GetIO().DeltaTime : 0.0f);
            m_PlaybackTime = comp.PlaybackTime;
        }
        DrawViewportImage(m_PreviewActor ? &m_PreviewScene : nullptr, "No compatible skeletal mesh found for this animation skeleton.");
    }
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(detailsWindow.c_str()))
    {
        editorFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        DrawDetails();
    }
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(timelineWindow.c_str()))
    {
        editorFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        DrawTimeline();
    }
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(tracksWindow.c_str()))
    {
        editorFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        DrawTracks();
    }
    ImGui::End();

    if (editorFocused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false))
        m_Playing = !m_Playing;

    if (!open)
        CloseBase();
}

void AnimationAssetEditor::DrawDetails()
{
    if (!m_Animation)
        return;

    if (ImGui::BeginTable("AnimationAssetProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 138.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        PropertyEditor::DrawReflectedObjectUI(m_Animation, *m_Animation->GetType());
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Tracks: %d", m_Animation->m_Tracks.Num());
    ImGui::Text("Skeleton: %s", m_Skeleton ? m_Skeleton->Path.c_str() : "None");
}

void AnimationAssetEditor::DrawTimeline()
{
    if (!m_Animation)
        return;

    const float duration = glm::max(0.001f, m_Animation->m_DurationSeconds);
    if (m_PlaybackTime > duration)
        m_PlaybackTime = m_Looping ? AnimationRuntime::NormalizePlaybackTime(m_PlaybackTime, duration, true) : duration;

    const int32 frameMax = GetTimelineFrameMax(*m_Animation);
    m_TimelineCurrentFrame = glm::clamp(TimeToTimelineFrame(*m_Animation, m_PlaybackTime), 0, frameMax);
    m_TimelineFirstFrame = glm::clamp(m_TimelineFirstFrame, 0, frameMax);
    if (m_SelectedTimelineItem >= 1)
        m_SelectedTimelineItem = -1;

    ImGui::Text(
        "Time %.3f / %.3f s    Frame %d / %d    %.2f fps",
        m_PlaybackTime,
        m_Animation->m_DurationSeconds,
        m_TimelineCurrentFrame,
        frameMax,
        GetTimelineFrameRate(*m_Animation));

    int previousFrame = m_TimelineCurrentFrame;
    AnimationClipSequence sequence(*m_Animation);
    ImSequencer::Sequencer(
        &sequence,
        &m_TimelineCurrentFrame,
        &m_TimelineExpanded,
        &m_SelectedTimelineItem,
        &m_TimelineFirstFrame,
        ImSequencer::SEQUENCER_CHANGE_FRAME);

    m_TimelineCurrentFrame = glm::clamp(m_TimelineCurrentFrame, 0, frameMax);
    if (m_TimelineCurrentFrame != previousFrame)
    {
        m_PlaybackTime = glm::min(duration, TimelineFrameToTime(*m_Animation, m_TimelineCurrentFrame));
        m_Playing = false;
        if (SkeletalMeshComponent* previewComp = GetPreviewSkeletalMeshComponent(m_PreviewActor))
        {
            SetPreviewPlaybackTime(*previewComp, m_PlaybackTime);
            TickPreviewAnimation(m_PreviewScene, 0.0f);
            m_PlaybackTime = previewComp->PlaybackTime;
        }
    }

    ImGui::TextDisabled("Clip-level sequencer. Alt + middle mouse pans the sequencer; use the scrollbar to zoom.");
}

void AnimationAssetEditor::DrawTracks()
{
    if (!m_Animation)
        return;

    for (const AnimationTrack& track : m_Animation->m_Tracks)
    {
        ImGui::BulletText("%s  (%d keys)", track.BoneName.c_str(), track.PositionKeys.Num() + track.RotationKeys.Num() + track.ScaleKeys.Num());
    }
}

const Rebel::Core::Reflection::TypeInfo* SkeletonAssetEditor::GetSupportedAssetType() const
{
    return SkeletonAsset::StaticType();
}

void SkeletonAssetEditor::Open(const AssetHandle assetHandle)
{
    m_AssetHandle = assetHandle;
    m_IsOpen = ReloadAsset();
}

bool SkeletonAssetEditor::ReloadAsset()
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    m_Skeleton = assetModule ? dynamic_cast<SkeletonAsset*>(assetModule->GetManager().Load(m_AssetHandle)) : nullptr;
    m_SelectedBone = -1;
    m_LayoutInitialized = false;
    ResetPreviewCamera();
    return RebuildPreview();
}

bool SkeletonAssetEditor::RebuildPreview()
{
    m_PreviewScene.Clear();
    m_PreviewActor = nullptr;
    if (!m_Skeleton)
        return false;

    if (SkeletalMeshAsset* mesh = FindPreviewMeshForSkeleton(m_Skeleton->ID))
    {
        m_PreviewActor = &m_PreviewScene.SpawnActor<Actor>();
        SkeletalMeshComponent& comp = m_PreviewActor->AddComponent<SkeletalMeshComponent>();
        comp.Mesh = AssetPtr<SkeletalMeshAsset>(mesh->ID);
        comp.bDrawSkeleton = true;
        comp.bPlayAnimation = false;
    }

    return true;
}

void SkeletonAssetEditor::EnsureLayout(const ImVec2& size)
{
    const String dockspaceName = MakeScopedEditorId("SkeletonAssetEditorDockspace", m_AssetHandle);
    const String treeWindow = MakeScopedEditorName("Skeleton Tree", "SkeletonAssetEditorTree", m_AssetHandle);
    const String viewportWindow = MakeScopedEditorName("Viewport", "SkeletonAssetEditorViewport", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "SkeletonAssetEditorDetails", m_AssetHandle);
    DockThreePanelLayout(
        ImGui::GetID(dockspaceName.c_str()),
        size,
        treeWindow.c_str(),
        viewportWindow.c_str(),
        detailsWindow.c_str(),
        m_LayoutInitialized);
}

void SkeletonAssetEditor::Draw(ImGuiID documentDockId, ImGuiID documentClassId)
{
    if (!m_IsOpen)
        return;

    bool open = true;
    bool visible = false;
    BeginEditorWindow("Skeleton Asset Editor", m_Skeleton, documentDockId, documentClassId, open, visible);
    if (!visible)
    {
        ImGui::End();
        if (!open)
            CloseBase();
        return;
    }

    if (!open)
    {
        ImGui::End();
        CloseBase();
        return;
    }

    {
        ImGui::BeginChild("SkeletonAssetEditorDockHost", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 dockSize = ImGui::GetContentRegionAvail();
        const String dockspaceName = MakeScopedEditorId("SkeletonAssetEditorDockspace", m_AssetHandle);
        ImGui::DockSpace(ImGui::GetID(dockspaceName.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        EnsureLayout(dockSize);
        ImGui::EndChild();
    }
    ImGui::End();

    ImGuiWindowClass panelClass{};
    panelClass.ClassId = documentClassId;
    panelClass.DockingAllowUnclassed = false;

    const String viewportWindow = MakeScopedEditorName("Viewport", "SkeletonAssetEditorViewport", m_AssetHandle);
    const String treeWindow = MakeScopedEditorName("Skeleton Tree", "SkeletonAssetEditorTree", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "SkeletonAssetEditorDetails", m_AssetHandle);

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(viewportWindow.c_str()))
        DrawViewportImage(m_PreviewActor ? &m_PreviewScene : nullptr, "No skeletal mesh references this skeleton yet.");
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(treeWindow.c_str()))
        DrawSkeletonTree();
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(detailsWindow.c_str()))
        DrawDetails();
    ImGui::End();

    if (!open)
        CloseBase();
}

void SkeletonAssetEditor::DrawSkeletonTree()
{
    if (!m_Skeleton)
        return;

    for (int32 i = 0; i < m_Skeleton->m_Parent.Num(); ++i)
    {
        if (m_Skeleton->m_Parent[i] < 0)
            DrawSkeletonTreeRecursive(*m_Skeleton, i, m_SelectedBone);
    }
}

void SkeletonAssetEditor::DrawDetails()
{
    if (!m_Skeleton)
        return;

    ImGui::Text("Bones: %d", m_Skeleton->m_Parent.Num());
    ImGui::Text("Selected Bone: %d", m_SelectedBone);
    if (m_SelectedBone >= 0 && m_SelectedBone < m_Skeleton->m_BoneNames.Num())
    {
        ImGui::Separator();
        ImGui::Text("Name: %s", m_Skeleton->m_BoneNames[m_SelectedBone].c_str());
        ImGui::Text("Parent: %d", m_Skeleton->m_Parent[m_SelectedBone]);
    }
}

const Rebel::Core::Reflection::TypeInfo* SkeletalMeshAssetEditor::GetSupportedAssetType() const
{
    return SkeletalMeshAsset::StaticType();
}

void SkeletalMeshAssetEditor::Open(const AssetHandle assetHandle)
{
    m_AssetHandle = assetHandle;
    m_IsOpen = ReloadAsset();
}

bool SkeletalMeshAssetEditor::ReloadAsset()
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule)
        return false;

    m_Mesh = dynamic_cast<SkeletalMeshAsset*>(assetModule->GetManager().Load(m_AssetHandle));
    m_Skeleton = m_Mesh && IsValidAssetHandle(m_Mesh->m_Skeleton.GetHandle())
        ? dynamic_cast<SkeletonAsset*>(assetModule->GetManager().Load(m_Mesh->m_Skeleton.GetHandle()))
        : nullptr;
    m_LayoutInitialized = false;
    ResetPreviewCamera();
    return RebuildPreview();
}

bool SkeletalMeshAssetEditor::RebuildPreview()
{
    m_PreviewScene.Clear();
    m_PreviewActor = nullptr;
    if (!m_Mesh)
        return false;

    m_PreviewActor = &m_PreviewScene.SpawnActor<Actor>();
    SkeletalMeshComponent& comp = m_PreviewActor->AddComponent<SkeletalMeshComponent>();
    comp.Mesh = AssetPtr<SkeletalMeshAsset>(m_Mesh->ID);
    comp.bDrawSkeleton = true;
    comp.bPlayAnimation = false;
    return true;
}

void SkeletalMeshAssetEditor::EnsureLayout(const ImVec2& size)
{
    if (m_LayoutInitialized || size.x <= 1.0f || size.y <= 1.0f)
        return;

    const String dockspaceName = MakeScopedEditorId("SkeletalMeshAssetEditorDockspace", m_AssetHandle);
    const String viewportWindow = MakeScopedEditorName("Viewport", "SkeletalMeshAssetEditorViewport", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "SkeletalMeshAssetEditorDetails", m_AssetHandle);
    const ImGuiID dockspaceId = ImGui::GetID(dockspaceName.c_str());
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);
    ImGuiID dockRight = 0;
    ImGuiID dockMain = dockspaceId;
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.30f, &dockRight, &dockMain);
    ImGui::DockBuilderDockWindow(viewportWindow.c_str(), dockMain);
    ImGui::DockBuilderDockWindow(detailsWindow.c_str(), dockRight);
    ImGui::DockBuilderFinish(dockspaceId);
    m_LayoutInitialized = true;
}

void SkeletalMeshAssetEditor::Draw(ImGuiID documentDockId, ImGuiID documentClassId)
{
    if (!m_IsOpen)
        return;

    bool open = true;
    bool visible = false;
    BeginEditorWindow("Skeletal Mesh Asset Editor", m_Mesh, documentDockId, documentClassId, open, visible);
    if (!visible)
    {
        ImGui::End();
        if (!open)
            CloseBase();
        return;
    }

    if (!open)
    {
        ImGui::End();
        CloseBase();
        return;
    }

    {
        ImGui::BeginChild("SkeletalMeshAssetEditorDockHost", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 dockSize = ImGui::GetContentRegionAvail();
        const String dockspaceName = MakeScopedEditorId("SkeletalMeshAssetEditorDockspace", m_AssetHandle);
        ImGui::DockSpace(ImGui::GetID(dockspaceName.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        EnsureLayout(dockSize);
        ImGui::EndChild();
    }
    ImGui::End();

    ImGuiWindowClass panelClass{};
    panelClass.ClassId = documentClassId;
    panelClass.DockingAllowUnclassed = false;

    const String viewportWindow = MakeScopedEditorName("Viewport", "SkeletalMeshAssetEditorViewport", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "SkeletalMeshAssetEditorDetails", m_AssetHandle);

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(viewportWindow.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        DrawViewportImage(&m_PreviewScene, "Skeletal mesh preview unavailable.");
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(detailsWindow.c_str()))
        DrawDetails();
    ImGui::End();

    if (!open)
        CloseBase();
}

void SkeletalMeshAssetEditor::DrawDetails()
{
    if (!m_Mesh)
        return;

    ImGui::Text("Vertices: %d", m_Mesh->Vertices.Num());
    ImGui::Text("Indices: %d", m_Mesh->Indices.Num());
    ImGui::Text("Skeleton: %s", m_Skeleton ? m_Skeleton->Path.c_str() : "None");
}

const Rebel::Core::Reflection::TypeInfo* AnimGraphAssetEditor::GetSupportedAssetType() const
{
    return AnimGraphAsset::StaticType();
}

void AnimGraphAssetEditor::Open(const AssetHandle assetHandle)
{
    m_AssetHandle = assetHandle;
    m_IsOpen = ReloadAsset();
}

bool AnimGraphAssetEditor::ReloadAsset()
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    m_Graph = assetModule ? dynamic_cast<AnimGraphAsset*>(assetModule->GetManager().Load(m_AssetHandle)) : nullptr;
    if (m_Graph)
    {
        m_Graph->EnsureDefaultGraph();
        m_RootGraphDocument.SelectedNodeID = m_Graph->m_OutputNodeID;
    }
    m_LayoutInitialized = false;
    m_Dirty = false;
    m_PreviewDirty = false;
    m_StatusMessage = m_Graph ? "Loaded" : "Load failed";
    RebuildPreview();
    return m_Graph != nullptr;
}

bool AnimGraphAssetEditor::RebuildPreview()
{
    m_PreviewScene.Clear();
    m_PreviewActor = nullptr;
    m_PreviewAnimation = nullptr;
    m_PreviewSkeleton = nullptr;

    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule || !m_Graph)
        return false;

    const AssetHandle clipHandle = FindPreviewClipHandle(*m_Graph);
    m_PreviewAnimation = IsValidAssetHandle(clipHandle)
        ? dynamic_cast<AnimationAsset*>(assetModule->GetManager().Load(clipHandle))
        : nullptr;
    m_PreviewSkeleton = m_PreviewAnimation && IsValidAssetHandle(m_PreviewAnimation->m_SkeletonID)
        ? dynamic_cast<SkeletonAsset*>(assetModule->GetManager().Load(m_PreviewAnimation->m_SkeletonID))
        : nullptr;

    if (!m_PreviewAnimation || !m_PreviewSkeleton)
        return false;

    SkeletalMeshAsset* mesh = FindPreviewMeshForSkeleton(m_PreviewAnimation->m_SkeletonID);
    if (!mesh)
        return false;

    m_PreviewActor = &m_PreviewScene.SpawnActor<Actor>();
    SkeletalMeshComponent& comp = m_PreviewActor->AddComponent<SkeletalMeshComponent>();
    comp.Mesh = AssetPtr<SkeletalMeshAsset>(mesh->ID);
    comp.AnimGraph = AssetPtr<AnimGraphAsset>(m_Graph->ID);
    comp.AnimInstanceClass = m_Graph->m_AnimInstanceClass;
    comp.Animation = AssetPtr<AnimationAsset>(m_PreviewAnimation->ID);
    comp.bPlayAnimation = true;
    comp.bLoopAnimation = m_Looping;
    comp.PlaybackTime = m_PlaybackTime;
    comp.bDrawSkeleton = true;
    ResetPreviewCamera();
    return true;
}

bool AnimGraphAssetEditor::SaveAsset()
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule || !m_Graph || m_Graph->Path.length() == 0)
    {
        m_StatusMessage = "Save failed";
        return false;
    }

    const bool saved = assetModule->SaveAssetToFile(m_Graph->Path + ".rasset", *m_Graph);
    m_Dirty = !saved;
    m_StatusMessage = saved ? "Saved" : "Save failed";
    return saved;
}

void AnimGraphAssetEditor::MarkDirty()
{
    m_Dirty = true;
    m_PreviewDirty = true;
    m_StatusMessage = "Unsaved changes";
}

void AnimGraphAssetEditor::EnsureLayout(const ImVec2& size)
{
    const String dockspaceName = MakeScopedEditorId("AnimGraphAssetEditorDockspace", m_AssetHandle);
    const String graphWindow = MakeScopedEditorName("Graph", "AnimGraphAssetEditorGraph", m_AssetHandle);
    const String viewportWindow = MakeScopedEditorName("Preview", "AnimGraphAssetEditorPreview", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "AnimGraphAssetEditorDetails", m_AssetHandle);
    DockThreePanelLayout(
        ImGui::GetID(dockspaceName.c_str()),
        size,
        graphWindow.c_str(),
        viewportWindow.c_str(),
        detailsWindow.c_str(),
        m_LayoutInitialized);
}

void AnimGraphAssetEditor::Draw(ImGuiID documentDockId, ImGuiID documentClassId)
{
    if (!m_IsOpen)
        return;

    bool open = true;
    bool visible = false;
    BeginEditorWindow("Anim Graph Editor", m_Graph, documentDockId, documentClassId, open, visible);
    if (!visible)
    {
        ImGui::End();
        if (!open)
            CloseBase();
        return;
    }

    if (!open)
    {
        ImGui::End();
        CloseBase();
        return;
    }

    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::Button((String(ICON_FA_FLOPPY_DISK) + " Save").c_str()))
                SaveAsset();
            ImGui::SameLine();
            if (ImGui::Button((String(ICON_FA_ARROWS_ROTATE) + " Reload").c_str()))
                ReloadAsset();
            ImGui::SameLine();
            if (ImGui::Button((String(m_Playing ? ICON_FA_PAUSE : ICON_FA_PLAY) + " Preview").c_str()))
                m_Playing = !m_Playing;
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &m_Looping);
            ImGui::SameLine();
            DrawValidation();
            if (m_Dirty)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.94f, 0.72f, 0.33f, 1.0f), "%s Unsaved", ICON_FA_CIRCLE);
            }
            if (m_StatusMessage.length() > 0)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", m_StatusMessage.c_str());
            }
            ImGui::EndMenuBar();
        }

        ImGui::BeginChild("AnimGraphAssetEditorDockHost", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const ImVec2 dockSize = ImGui::GetContentRegionAvail();
        const String dockspaceName = MakeScopedEditorId("AnimGraphAssetEditorDockspace", m_AssetHandle);
        ImGui::DockSpace(ImGui::GetID(dockspaceName.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        EnsureLayout(dockSize);
        ImGui::EndChild();
    }
    ImGui::End();

    ImGuiWindowClass panelClass{};
    panelClass.ClassId = documentClassId;
    panelClass.DockingAllowUnclassed = false;

    const String viewportWindow = MakeScopedEditorName("Preview", "AnimGraphAssetEditorPreview", m_AssetHandle);
    const String graphWindow = MakeScopedEditorName("Graph", "AnimGraphAssetEditorGraph", m_AssetHandle);
    const String detailsWindow = MakeScopedEditorName("Details", "AnimGraphAssetEditorDetails", m_AssetHandle);

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(viewportWindow.c_str()))
    {
        if (m_PreviewDirty)
        {
            RebuildPreview();
            m_PreviewDirty = false;
        }
        if (m_PreviewActor && m_PreviewActor->IsValid())
        {
            SkeletalMeshComponent& comp = m_PreviewActor->GetComponent<SkeletalMeshComponent>();
            comp.PlaybackTime = m_PlaybackTime;
            comp.bPlayAnimation = m_Playing;
            comp.bLoopAnimation = m_Looping;
            TickPreviewAnimation(m_PreviewScene, m_Playing ? ImGui::GetIO().DeltaTime : 0.0f);
            m_PlaybackTime = comp.PlaybackTime;
        }
        DrawViewportImage(
            m_PreviewActor ? &m_PreviewScene : nullptr,
            "Connect an animation clip with a compatible skeletal mesh to preview graph output.");
    }
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(graphWindow.c_str()))
        DrawGraph();
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(detailsWindow.c_str()))
        DrawDetails();
    ImGui::End();

    if (!open)
        CloseBase();
}

void AnimGraphAssetEditor::DrawGraph()
{
    if (!m_Graph)
        return;

    if (m_EditingStateMachineID != 0 && m_EditingStateID != 0)
        DrawStateGraph();
    else if (m_EditingStateMachineID != 0)
        DrawStateMachineGraph();
    else
        DrawMainGraph();
}

void AnimGraphAssetEditor::DrawMainGraph()
{
    if (!m_Graph)
        return;

    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " Clip").c_str()))
    {
        AnimGraphNode& node = m_Graph->AddNode(AnimGraphNodeKind::AnimationClip, "Animation Clip");
        node.EditorX = 80.0f;
        node.EditorY = 80.0f + 56.0f * m_Graph->m_Nodes.Num();
        m_RootGraphDocument.SelectedNodeID = node.ID;
        MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " Blend").c_str()))
    {
        AnimGraphNode& node = m_Graph->AddNode(AnimGraphNodeKind::Blend, "Blend");
        node.EditorX = 260.0f;
        node.EditorY = 80.0f + 56.0f * m_Graph->m_Nodes.Num();
        m_RootGraphDocument.SelectedNodeID = node.ID;
        MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " State Machine").c_str()))
    {
        AnimStateMachine& stateMachine = m_Graph->AddStateMachine("State Machine");
        AnimGraphNode& node = m_Graph->AddNode(AnimGraphNodeKind::StateMachine, "State Machine");
        node.StateMachineID = stateMachine.ID;
        node.EditorX = 260.0f;
        node.EditorY = 80.0f + 56.0f * m_Graph->m_Nodes.Num();
        m_RootGraphDocument.SelectedNodeID = node.ID;
        MarkDirty();
    }
    ImGui::SameLine();
    if (AnimGraphNode* selectedNode = m_Graph->FindNode(m_RootGraphDocument.SelectedNodeID);
        selectedNode && selectedNode->Kind == AnimGraphNodeKind::StateMachine && selectedNode->StateMachineID != 0)
    {
        if (ImGui::Button("Open State Machine"))
        {
            m_EditingStateMachineID = selectedNode->StateMachineID;
            m_EditingStateID = 0;
            m_SelectedStateID = 0;
            m_SelectedTransitionID = 0;
            m_SelectedAliasID = 0;
            EditorGraphPanel::ClearInteractionState();
            return;
        }
        ImGui::SameLine();
    }
    if (ImGui::Button((String(ICON_FA_LINK) + " To Output").c_str()))
    {
        if (AnimGraphNode* output = m_Graph->FindOutputNode())
        {
            if (m_RootGraphDocument.SelectedNodeID != output->ID && m_Graph->CanConnectPose(m_RootGraphDocument.SelectedNodeID, output->ID))
            {
                output->InputPose = m_RootGraphDocument.SelectedNodeID;
                MarkDirty();
            }
            m_Graph->m_OutputNodeID = output->ID;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER) + " Fit").c_str()))
        m_RootGraphDocument.Fit = GraphEditor::Fit_AllNodes;

    ImGui::Separator();

    const GraphEditor::Options graphOptions = EditorGraphPanel::MakePoseGraphOptions();
    EditorGraphContextEvent contextEvent;
    const ImVec2 graphCanvasScreenPos = ImGui::GetCursorScreenPos();
    AnimPoseGraphEditorDelegate delegate(
        *m_Graph,
        nullptr,
        m_RootGraphDocument,
        contextEvent,
        m_Dirty,
        m_PreviewDirty,
        m_StatusMessage,
        graphCanvasScreenPos,
        GetPreviewSkeletalMeshComponent(m_PreviewActor),
        0);
    EditorGraphPanel::Show(delegate, graphOptions, m_RootGraphDocument, true);

    if (ShouldHandleGraphShortcut())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            if (DeleteSelectedPoseNode(*m_Graph, nullptr, m_RootGraphDocument.SelectedNodeID))
            {
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        {
            uint64 newNodeID = 0;
            if (DuplicatePoseNode(*m_Graph, nullptr, m_RootGraphDocument.SelectedNodeID, newNodeID))
            {
                m_RootGraphDocument.SelectedNodeID = newNodeID;
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }
    }

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        if (AnimGraphNode* selectedNode = m_Graph->FindNode(m_RootGraphDocument.SelectedNodeID);
            selectedNode && selectedNode->Kind == AnimGraphNodeKind::StateMachine && selectedNode->StateMachineID != 0)
        {
            const ImGuiIO& io = ImGui::GetIO();
            const ImVec2 graphMousePos = EditorGraphPanel::ScreenToGraphPosition(
                io.MousePos,
                graphCanvasScreenPos,
                m_RootGraphDocument.ViewState);
            const ImRect selectedRect(
                ImVec2(selectedNode->EditorX, selectedNode->EditorY),
                ImVec2(selectedNode->EditorX + 170.0f, selectedNode->EditorY + 68.0f));

            if (selectedRect.Contains(graphMousePos))
            {
                m_EditingStateMachineID = selectedNode->StateMachineID;
                m_EditingStateID = 0;
                m_SelectedStateID = 0;
                m_SelectedTransitionID = 0;
                m_SelectedAliasID = 0;
                EditorGraphPanel::ClearInteractionState();
                return;
            }
        }
    }

    if (contextEvent.Target == EditorGraphContextTarget::Canvas)
        ImGui::OpenPopup("AnimGraphCanvasContext");
    else if (contextEvent.Target == EditorGraphContextTarget::Node)
        ImGui::OpenPopup("AnimGraphNodeContext");
    if (ImGui::BeginPopup("AnimGraphCanvasContext"))
    {
        ImGui::TextDisabled("Create Pose Node");
        if (ImGui::MenuItem("Animation Clip"))
        {
            AnimGraphNode& node = m_Graph->AddNode(AnimGraphNodeKind::AnimationClip, "Animation Clip");
            node.EditorX = m_RootGraphDocument.ContextGraphPosition.x;
            node.EditorY = m_RootGraphDocument.ContextGraphPosition.y;
            m_RootGraphDocument.SelectedNodeID = node.ID;
            MarkDirty();
        }

        if (ImGui::MenuItem("Blend"))
        {
            AnimGraphNode& node = m_Graph->AddNode(AnimGraphNodeKind::Blend, "Blend");
            node.EditorX = m_RootGraphDocument.ContextGraphPosition.x;
            node.EditorY = m_RootGraphDocument.ContextGraphPosition.y;
            m_RootGraphDocument.SelectedNodeID = node.ID;
            MarkDirty();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("State Machine"))
        {
            AnimStateMachine& stateMachine = m_Graph->AddStateMachine("State Machine");
            AnimGraphNode& node = m_Graph->AddNode(AnimGraphNodeKind::StateMachine, "State Machine");
            node.StateMachineID = stateMachine.ID;
            node.EditorX = m_RootGraphDocument.ContextGraphPosition.x;
            node.EditorY = m_RootGraphDocument.ContextGraphPosition.y;
            m_RootGraphDocument.SelectedNodeID = node.ID;
            MarkDirty();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AnimGraphNodeContext"))
    {
        AnimGraphNode* selectedNode = m_Graph->FindNode(m_RootGraphDocument.SelectedNodeID);
        const bool canUseAsOutput = selectedNode && selectedNode->Kind != AnimGraphNodeKind::Output;
        if (ImGui::MenuItem("Use As Output Input", nullptr, false, canUseAsOutput))
        {
            if (AnimGraphNode* output = m_Graph->FindOutputNode())
            {
                if (m_Graph->CanConnectPose(m_RootGraphDocument.SelectedNodeID, output->ID))
                {
                    output->InputPose = m_RootGraphDocument.SelectedNodeID;
                    m_Graph->m_OutputNodeID = output->ID;
                    MarkDirty();
                }
            }
        }

        const bool canOpenStateMachine = selectedNode && selectedNode->Kind == AnimGraphNodeKind::StateMachine && selectedNode->StateMachineID != 0;
        if (ImGui::MenuItem("Open State Machine", nullptr, false, canOpenStateMachine))
        {
            m_EditingStateMachineID = selectedNode->StateMachineID;
            m_EditingStateID = 0;
            m_SelectedStateID = 0;
            m_SelectedTransitionID = 0;
            m_SelectedAliasID = 0;
            EditorGraphPanel::ClearInteractionState();
        }

        const bool canDuplicate = selectedNode && selectedNode->Kind != AnimGraphNodeKind::Output && selectedNode->Kind != AnimGraphNodeKind::StateMachine;
        if (ImGui::MenuItem("Duplicate Node", "Ctrl+D", false, canDuplicate))
        {
            uint64 newNodeID = 0;
            if (DuplicatePoseNode(*m_Graph, nullptr, m_RootGraphDocument.SelectedNodeID, newNodeID))
            {
                m_RootGraphDocument.SelectedNodeID = newNodeID;
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }

        const bool canDelete = selectedNode && selectedNode->Kind != AnimGraphNodeKind::Output;
        if (ImGui::MenuItem("Delete Node", "Delete", false, canDelete))
        {
            if (DeleteSelectedPoseNode(*m_Graph, nullptr, m_RootGraphDocument.SelectedNodeID))
            {
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }

        ImGui::EndPopup();
    }

    if (m_RootGraphDocument.SelectedNodeID == 0)
    {
        if (const AnimGraphNode* output = m_Graph->FindOutputNode())
            m_RootGraphDocument.SelectedNodeID = output->ID;
    }
}

void AnimGraphAssetEditor::DrawStateMachineGraph()
{
    if (!m_Graph)
        return;

    AnimStateMachine* stateMachine = m_Graph->FindStateMachine(m_EditingStateMachineID);
    if (!stateMachine)
    {
        m_EditingStateMachineID = 0;
        return;
    }

    if (ImGui::Button(ICON_FA_ARROW_LEFT " Anim Graph"))
    {
        m_EditingStateMachineID = 0;
        m_EditingStateID = 0;
        m_SelectedStateID = 0;
        m_SelectedTransitionID = 0;
        m_SelectedAliasID = 0;
        EditorGraphPanel::ClearInteractionState();
        return;
    }
    ImGui::SameLine();
    ImGui::Text("State Machine: %s", stateMachine->Name.c_str());
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " State").c_str()))
    {
        AnimState& state = m_Graph->AddState(*stateMachine, "State");
        state.EditorX = 120.0f + 32.0f * stateMachine->States.Num();
        state.EditorY = 120.0f + 32.0f * stateMachine->States.Num();
        m_SelectedStateID = state.ID;
        m_SelectedTransitionID = 0;
        m_SelectedAliasID = 0;
        MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " Alias").c_str()))
    {
        AnimStateAlias& alias = m_Graph->AddAlias(*stateMachine, "State Alias");
        alias.EditorX = 80.0f + 32.0f * stateMachine->Aliases.Num();
        alias.EditorY = 40.0f + 32.0f * stateMachine->Aliases.Num();
        m_SelectedAliasID = alias.ID;
        m_SelectedStateID = 0;
        m_SelectedTransitionID = 0;
        MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER) + " Fit").c_str()))
        m_StateMachineGraphDocument.Fit = GraphEditor::Fit_AllNodes;
    ImGui::SameLine();
    if (ImGui::Button("Open State") && m_SelectedStateID != 0)
    {
        for (AnimState& state : stateMachine->States)
        {
            if (state.ID == m_SelectedStateID)
            {
                m_EditingStateID = state.ID;
                m_StateGraphDocument.SelectedNodeID = 0;
                m_Graph->EnsureDefaultStateGraph(state);
                EditorGraphPanel::ClearInteractionState();
                return;
            }
        }
    }

    ImGui::Separator();

    const GraphEditor::Options graphOptions = EditorGraphPanel::MakeStateMachineGraphOptions();
    EditorGraphContextEvent contextEvent;
    const ImVec2 graphCanvasScreenPos = ImGui::GetCursorScreenPos();
    AnimStateMachineGraphDelegate delegate(
        *m_Graph,
        *stateMachine,
        m_SelectedStateID,
        m_SelectedTransitionID,
        m_SelectedAliasID,
        m_Dirty,
        m_PreviewDirty,
        m_StatusMessage,
        m_StateMachineGraphDocument,
        contextEvent,
        graphCanvasScreenPos,
        GetPreviewSkeletalMeshComponent(m_PreviewActor));
    EditorGraphPanel::Show(delegate, graphOptions, m_StateMachineGraphDocument, true);

    if (ShouldHandleGraphShortcut() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        bool deleted = DeleteSelectedAlias(*stateMachine, m_SelectedAliasID);
        if (!deleted)
            deleted = DeleteSelectedTransition(*stateMachine, m_SelectedTransitionID);
        if (!deleted)
            deleted = DeleteSelectedState(*stateMachine, m_SelectedStateID, m_SelectedTransitionID, m_SelectedAliasID);
        if (deleted)
        {
            MarkDirty();
            EditorGraphPanel::ClearInteractionState();
        }
    }

    if (contextEvent.Target == EditorGraphContextTarget::Canvas)
        ImGui::OpenPopup("AnimStateMachineCanvasContext");
    else if (contextEvent.Target == EditorGraphContextTarget::Node && m_SelectedStateID != 0)
        ImGui::OpenPopup("AnimStateMachineStateContext");
    else if (contextEvent.Target == EditorGraphContextTarget::Node && m_SelectedAliasID != 0)
        ImGui::OpenPopup("AnimStateMachineAliasContext");

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        for (AnimState& state : stateMachine->States)
        {
            if (state.ID != m_SelectedStateID)
                continue;

            const ImGuiIO& io = ImGui::GetIO();
            const ImVec2 graphMousePos = EditorGraphPanel::ScreenToGraphPosition(
                io.MousePos,
                graphCanvasScreenPos,
                m_StateMachineGraphDocument.ViewState);
            const ImRect selectedRect(
                ImVec2(state.EditorX, state.EditorY),
                ImVec2(state.EditorX + 170.0f, state.EditorY + 68.0f));

            if (selectedRect.Contains(graphMousePos))
            {
                m_EditingStateID = state.ID;
                m_StateGraphDocument.SelectedNodeID = 0;
                m_Graph->EnsureDefaultStateGraph(state);
                EditorGraphPanel::ClearInteractionState();
                return;
            }
        }
    }
    if (ImGui::BeginPopup("AnimStateMachineCanvasContext"))
    {
        if (ImGui::MenuItem("Create State"))
        {
            AnimState& state = m_Graph->AddState(*stateMachine, "State");
            state.EditorX = m_StateMachineGraphDocument.ContextGraphPosition.x;
            state.EditorY = m_StateMachineGraphDocument.ContextGraphPosition.y;
            m_SelectedStateID = state.ID;
            m_SelectedTransitionID = 0;
            m_SelectedAliasID = 0;
            MarkDirty();
        }
        if (ImGui::MenuItem("Create Alias"))
        {
            AnimStateAlias& alias = m_Graph->AddAlias(*stateMachine, "State Alias");
            alias.EditorX = m_StateMachineGraphDocument.ContextGraphPosition.x;
            alias.EditorY = m_StateMachineGraphDocument.ContextGraphPosition.y;
            m_SelectedAliasID = alias.ID;
            m_SelectedStateID = 0;
            m_SelectedTransitionID = 0;
            MarkDirty();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AnimStateMachineStateContext"))
    {
        if (ImGui::MenuItem("Open State Graph", nullptr, false, m_SelectedStateID != 0))
        {
            for (AnimState& state : stateMachine->States)
            {
                if (state.ID == m_SelectedStateID)
                {
                    m_EditingStateID = state.ID;
                    m_StateGraphDocument.SelectedNodeID = 0;
                    m_Graph->EnsureDefaultStateGraph(state);
                    EditorGraphPanel::ClearInteractionState();
                    break;
                }
            }
        }
        if (ImGui::MenuItem("Set As Entry", nullptr, false, m_SelectedStateID != 0))
        {
            stateMachine->EntryStateID = m_SelectedStateID;
            MarkDirty();
        }
        if (ImGui::MenuItem("Delete State", "Delete", false, m_SelectedStateID != 0 && stateMachine->States.Num() > 1))
        {
            if (DeleteSelectedState(*stateMachine, m_SelectedStateID, m_SelectedTransitionID, m_SelectedAliasID))
            {
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AnimStateMachineAliasContext"))
    {
        if (ImGui::MenuItem("Make Global", nullptr, false, m_SelectedAliasID != 0))
        {
            for (AnimStateAlias& alias : stateMachine->Aliases)
            {
                if (alias.ID == m_SelectedAliasID)
                {
                    alias.bGlobalAlias = true;
                    MarkDirty();
                    break;
                }
            }
        }
        if (ImGui::MenuItem("Delete Alias", "Delete", false, m_SelectedAliasID != 0))
        {
            if (DeleteSelectedAlias(*stateMachine, m_SelectedAliasID))
            {
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }
        ImGui::EndPopup();
    }
}

void AnimGraphAssetEditor::DrawStateGraph()
{
    if (!m_Graph)
        return;

    AnimStateMachine* stateMachine = m_Graph->FindStateMachine(m_EditingStateMachineID);
    if (!stateMachine)
    {
        m_EditingStateMachineID = 0;
        m_EditingStateID = 0;
        return;
    }

    AnimState* state = nullptr;
    for (AnimState& candidate : stateMachine->States)
    {
        if (candidate.ID == m_EditingStateID)
        {
            state = &candidate;
            break;
        }
    }

    if (!state)
    {
        m_EditingStateID = 0;
        return;
    }

    m_Graph->EnsureDefaultStateGraph(*state);

    if (ImGui::Button(ICON_FA_ARROW_LEFT " State Machine"))
    {
        m_EditingStateID = 0;
        m_StateGraphDocument.SelectedNodeID = 0;
        EditorGraphPanel::ClearInteractionState();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_HOUSE " Anim Graph"))
    {
        m_EditingStateMachineID = 0;
        m_EditingStateID = 0;
        m_StateGraphDocument.SelectedNodeID = 0;
        EditorGraphPanel::ClearInteractionState();
        return;
    }
    ImGui::SameLine();
    ImGui::Text("State: %s / %s", stateMachine->Name.c_str(), state->Name.c_str());
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " Clip").c_str()))
    {
        AnimGraphNode& node = m_Graph->AddNode(state->StateGraph, AnimGraphNodeKind::AnimationClip, "Animation Clip");
        node.EditorX = 80.0f;
        node.EditorY = 80.0f + 56.0f * state->StateGraph.Nodes.Num();
        if (AnimGraphNode* output = m_Graph->FindOutputNode(state->StateGraph); output && output->InputPose == 0)
            output->InputPose = node.ID;
        m_StateGraphDocument.SelectedNodeID = node.ID;
        MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " Blend").c_str()))
    {
        AnimGraphNode& node = m_Graph->AddNode(state->StateGraph, AnimGraphNodeKind::Blend, "Blend");
        node.EditorX = 260.0f;
        node.EditorY = 80.0f + 56.0f * state->StateGraph.Nodes.Num();
        m_StateGraphDocument.SelectedNodeID = node.ID;
        MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_LINK) + " To Output").c_str()))
    {
        if (AnimGraphNode* output = m_Graph->FindOutputNode(state->StateGraph))
        {
            if (m_StateGraphDocument.SelectedNodeID != output->ID && m_Graph->CanConnectPose(state->StateGraph, m_StateGraphDocument.SelectedNodeID, output->ID))
            {
                output->InputPose = m_StateGraphDocument.SelectedNodeID;
                MarkDirty();
            }
            state->StateGraph.OutputNodeID = output->ID;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((String(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER) + " Fit").c_str()))
        m_StateGraphDocument.Fit = GraphEditor::Fit_AllNodes;

    ImGui::Separator();

    const GraphEditor::Options graphOptions = EditorGraphPanel::MakePoseGraphOptions();
    EditorGraphContextEvent contextEvent;
    const ImVec2 graphCanvasScreenPos = ImGui::GetCursorScreenPos();
    AnimPoseGraphEditorDelegate delegate(
        *m_Graph,
        &state->StateGraph,
        m_StateGraphDocument,
        contextEvent,
        m_Dirty,
        m_PreviewDirty,
        m_StatusMessage,
        graphCanvasScreenPos,
        GetPreviewSkeletalMeshComponent(m_PreviewActor),
        MakeStatePoseGraphRuntimeScopeIDForEditor(stateMachine->ID, state->ID));
    EditorGraphPanel::Show(delegate, graphOptions, m_StateGraphDocument, true);

    if (ShouldHandleGraphShortcut())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            if (DeleteSelectedPoseNode(*m_Graph, &state->StateGraph, m_StateGraphDocument.SelectedNodeID))
            {
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        {
            uint64 newNodeID = 0;
            if (DuplicatePoseNode(*m_Graph, &state->StateGraph, m_StateGraphDocument.SelectedNodeID, newNodeID))
            {
                m_StateGraphDocument.SelectedNodeID = newNodeID;
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }
    }

    if (contextEvent.Target == EditorGraphContextTarget::Canvas)
        ImGui::OpenPopup("AnimStateGraphCanvasContext");
    else if (contextEvent.Target == EditorGraphContextTarget::Node)
        ImGui::OpenPopup("AnimStateGraphNodeContext");
    if (ImGui::BeginPopup("AnimStateGraphCanvasContext"))
    {
        ImGui::TextDisabled("Create Pose Node");
        if (ImGui::MenuItem("Animation Clip"))
        {
            AnimGraphNode& node = m_Graph->AddNode(state->StateGraph, AnimGraphNodeKind::AnimationClip, "Animation Clip");
            node.EditorX = m_StateGraphDocument.ContextGraphPosition.x;
            node.EditorY = m_StateGraphDocument.ContextGraphPosition.y;
            if (AnimGraphNode* output = m_Graph->FindOutputNode(state->StateGraph); output && output->InputPose == 0)
                output->InputPose = node.ID;
            m_StateGraphDocument.SelectedNodeID = node.ID;
            MarkDirty();
        }

        if (ImGui::MenuItem("Blend"))
        {
            AnimGraphNode& node = m_Graph->AddNode(state->StateGraph, AnimGraphNodeKind::Blend, "Blend");
            node.EditorX = m_StateGraphDocument.ContextGraphPosition.x;
            node.EditorY = m_StateGraphDocument.ContextGraphPosition.y;
            m_StateGraphDocument.SelectedNodeID = node.ID;
            MarkDirty();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AnimStateGraphNodeContext"))
    {
        AnimGraphNode* selectedNode = m_Graph->FindNode(state->StateGraph, m_StateGraphDocument.SelectedNodeID);
        const bool canUseAsOutput = selectedNode && selectedNode->Kind != AnimGraphNodeKind::Output;
        if (ImGui::MenuItem("Use As Output Input", nullptr, false, canUseAsOutput))
        {
            if (AnimGraphNode* output = m_Graph->FindOutputNode(state->StateGraph))
            {
                if (m_Graph->CanConnectPose(state->StateGraph, m_StateGraphDocument.SelectedNodeID, output->ID))
                {
                    output->InputPose = m_StateGraphDocument.SelectedNodeID;
                    state->StateGraph.OutputNodeID = output->ID;
                    MarkDirty();
                }
            }
        }

        const bool canDuplicate = selectedNode && selectedNode->Kind != AnimGraphNodeKind::Output;
        if (ImGui::MenuItem("Duplicate Node", "Ctrl+D", false, canDuplicate))
        {
            uint64 newNodeID = 0;
            if (DuplicatePoseNode(*m_Graph, &state->StateGraph, m_StateGraphDocument.SelectedNodeID, newNodeID))
            {
                m_StateGraphDocument.SelectedNodeID = newNodeID;
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }

        const bool canDelete = selectedNode && selectedNode->Kind != AnimGraphNodeKind::Output;
        if (ImGui::MenuItem("Delete Node", "Delete", false, canDelete))
        {
            if (DeleteSelectedPoseNode(*m_Graph, &state->StateGraph, m_StateGraphDocument.SelectedNodeID))
            {
                MarkDirty();
                EditorGraphPanel::ClearInteractionState();
            }
        }

        ImGui::EndPopup();
    }

    if (m_StateGraphDocument.SelectedNodeID == 0)
    {
        if (const AnimGraphNode* output = m_Graph->FindOutputNode(state->StateGraph))
            m_StateGraphDocument.SelectedNodeID = output->ID;
    }
}

void AnimGraphAssetEditor::DrawValidation()
{
    if (!m_Graph)
        return;

    TArray<String> issues;
    const bool valid = m_Graph->Validate(issues);
    const char* label = valid ? ICON_FA_CIRCLE_CHECK " Valid" : ICON_FA_TRIANGLE_EXCLAMATION " Issues";
    ImGui::TextColored(
        valid ? ImVec4(0.46f, 0.82f, 0.52f, 1.0f) : ImVec4(0.94f, 0.72f, 0.33f, 1.0f),
        "%s",
        label);

    if (!valid && ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        for (const String& issue : issues)
            ImGui::BulletText("%s", issue.c_str());
        ImGui::EndTooltip();
    }
}

void AnimGraphAssetEditor::DrawDetails()
{
    if (!m_Graph)
        return;

    TArray<String> issues;
    const bool valid = m_Graph->Validate(issues);
    ImGui::Text("Graph: %d node(s)", m_Graph->m_Nodes.Num());
    ImGui::SameLine();
    ImGui::TextColored(
        valid ? ImVec4(0.46f, 0.82f, 0.52f, 1.0f) : ImVec4(0.94f, 0.72f, 0.33f, 1.0f),
        "%s",
        valid ? "Valid" : "Needs attention");
    if (!valid)
    {
        for (const String& issue : issues)
            ImGui::BulletText("%s", issue.c_str());
    }
    ImGui::Separator();

    if (DrawAnimGraphSkeletonCombo(*m_Graph))
    {
        MarkDirty();
        m_PreviewDirty = true;
    }

    ImGui::Separator();

    const Rebel::Core::Reflection::TypeInfo* currentAnimClass = m_Graph->m_AnimInstanceClass.Get();
    const char* currentAnimClassName = currentAnimClass ? currentAnimClass->Name.c_str() : "None";
    if (ImGui::BeginCombo("Anim Instance Class", currentAnimClassName))
    {
        const Rebel::Core::Reflection::TypeInfo* animInstanceBase = AnimInstance::StaticType();
        for (const auto& pair : Rebel::Core::Reflection::TypeRegistry::Get().GetTypes())
        {
            const Rebel::Core::Reflection::TypeInfo* type = pair.Value;
            if (!type || !type->CreateInstance || !animInstanceBase || !type->IsA(animInstanceBase))
                continue;

            const bool selected = (type == currentAnimClass);
            if (ImGui::Selectable(type->Name.c_str(), selected))
            {
                m_Graph->m_AnimInstanceClass = type;
                MarkDirty();
                m_PreviewDirty = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (currentAnimClass)
    {
        ImGui::TextDisabled("Reflected anim variables:");
        bool anyProperty = false;
        for (const Rebel::Core::Reflection::PropertyInfo& prop : currentAnimClass->Properties)
        {
            if (!Rebel::Core::Reflection::HasFlag(prop.Flags, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor))
                continue;

            anyProperty = true;
            ImGui::BulletText("%s", prop.Name.c_str());
        }

        if (!anyProperty)
            ImGui::TextDisabled("No reflected properties on this class yet.");
    }

    ImGui::Separator();
    const char* graphSkeletonName = "None";
    if (AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
        assetModule && IsValidAssetHandle(m_Graph->m_SkeletonID))
    {
        if (const AssetMeta* meta = assetModule->GetRegistry().Get(m_Graph->m_SkeletonID))
            graphSkeletonName = meta->Path.c_str();
    }

    ImGui::Text("Graph Skeleton: %s", graphSkeletonName);
    ImGui::Text("Preview Clip: %s", m_PreviewAnimation ? m_PreviewAnimation->Path.c_str() : "None");
    ImGui::Text("Preview Skeleton: %s", m_PreviewSkeleton ? m_PreviewSkeleton->Path.c_str() : "None");
    if (!m_PreviewActor)
        ImGui::TextDisabled("Preview needs a reachable clip and a skeletal mesh using the same skeleton.");
    if (SkeletalMeshComponent* previewComp = GetPreviewSkeletalMeshComponent(m_PreviewActor))
    {
        if (ImGui::CollapsingHeader("Preview Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            AnimationLocomotionState& locomotion = previewComp->LocomotionState;
            bool changed = false;
            changed |= ImGui::DragFloat("Horizontal Speed", &locomotion.HorizontalSpeed, 0.05f, 0.0f, 2000.0f);
            changed |= ImGui::DragFloat("Vertical Speed", &locomotion.VerticalSpeed, 0.05f, -2000.0f, 2000.0f);
            changed |= ImGui::DragFloat("Ground Distance", &locomotion.GroundDistance, 0.01f, 0.0f, 1000.0f);
            changed |= ImGui::DragFloat("Time In Air", &locomotion.TimeInAir, 0.01f, 0.0f, 60.0f);
            changed |= ImGui::Checkbox("Is Grounded", &locomotion.bIsGrounded);
            changed |= ImGui::Checkbox("Has Movement Input", &locomotion.bHasMovementInput);
            changed |= ImGui::Checkbox("Jump Started", &locomotion.bJumpStarted);
            changed |= ImGui::Checkbox("Landed", &locomotion.bLanded);
            if (changed)
                previewComp->PlaybackTime = m_PlaybackTime;

            if (AnimInstance* instance = previewComp->GetAnimInstance())
            {
                ImGui::TextDisabled("Runtime State: %s  time %.2f", instance->GetDebugStateName(), instance->GetDebugPlaybackTime());
            }
            for (const AnimStateMachineRuntime& runtime : previewComp->StateMachineRuntimes)
            {
                ImGui::TextDisabled(
                    "SM %llu: current %llu previous %llu blend %.2f / %.2f",
                    static_cast<unsigned long long>(runtime.StateMachineID),
                    static_cast<unsigned long long>(runtime.CurrentStateID),
                    static_cast<unsigned long long>(runtime.PreviousStateID),
                    runtime.TransitionTime,
                    runtime.TransitionDuration);
            }
            for (const AnimBlendNodeRuntime& runtime : previewComp->BlendNodeRuntimes)
            {
                ImGui::TextDisabled(
                    "Blend %llu: alpha %.2f",
                    static_cast<unsigned long long>(runtime.NodeID),
                    runtime.CurrentAlpha);
            }
        }
    }
    ImGui::Separator();

    if (m_EditingStateMachineID != 0 && m_EditingStateID != 0)
    {
        AnimStateMachine* stateMachine = m_Graph->FindStateMachine(m_EditingStateMachineID);
        if (!stateMachine)
            return;

        AnimState* state = nullptr;
        for (AnimState& candidate : stateMachine->States)
        {
            if (candidate.ID == m_EditingStateID)
                state = &candidate;
        }
        if (!state)
            return;

        ImGui::Text("Editing State Graph: %s / %s", stateMachine->Name.c_str(), state->Name.c_str());
        ImGui::Text("State graph: %d node(s)", state->StateGraph.Nodes.Num());
        if (ImGui::Checkbox("Loop", &state->bLoop))
            MarkDirty();

        AnimGraphNode* node = m_Graph->FindNode(state->StateGraph, m_StateGraphDocument.SelectedNodeID);
        if (!node)
        {
            ImGui::TextDisabled("Select a state graph node.");
            return;
        }

        ImGui::Separator();
        char name[128];
        std::snprintf(name, sizeof(name), "%s", node->Name.c_str());
        if (ImGui::InputText("Name", name, sizeof(name)))
        {
            node->Name = String(name);
            MarkDirty();
        }

        ImGui::Text("Kind: %s", ToNodeKindLabel(node->Kind));
        ImGui::InputScalar("ID", ImGuiDataType_U64, &node->ID, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
        ImGui::Text("Output Reachable: %s", m_Graph->IsNodeReachableFromOutput(state->StateGraph, node->ID) ? "Yes" : "No");

        if (node->Kind == AnimGraphNodeKind::AnimationClip)
        {
            if (DrawAnimationClipAssetCombo("Clip", node->AnimationClip, m_Graph->m_SkeletonID))
                MarkDirty();

            if (ImGui::DragFloat("Start Time", &node->ClipStartTime, 0.01f, 0.0f, 600.0f))
            {
                node->ClipStartTime = FMath::max(0.0f, node->ClipStartTime);
                MarkDirty();
            }
            ImGui::TextDisabled("Samples the clip starting from this offset.");
        }
        else if (node->Kind == AnimGraphNodeKind::Blend)
        {
            if (DrawBlendAlphaControls(*node, currentAnimClass))
                MarkDirty();
            if (DrawNodeInputCombo("Input A", *m_Graph, state->StateGraph, node->InputA, node->ID))
                MarkDirty();
            if (DrawNodeInputCombo("Input B", *m_Graph, state->StateGraph, node->InputB, node->ID))
                MarkDirty();
        }
        else if (node->Kind == AnimGraphNodeKind::Output)
        {
            if (DrawNodeInputCombo("Input Pose", *m_Graph, state->StateGraph, node->InputPose, node->ID))
                MarkDirty();
            if (state->StateGraph.OutputNodeID != node->ID)
            {
                state->StateGraph.OutputNodeID = node->ID;
                MarkDirty();
            }
        }

        return;
    }

    if (m_EditingStateMachineID != 0)
    {
        AnimStateMachine* stateMachine = m_Graph->FindStateMachine(m_EditingStateMachineID);
        if (!stateMachine)
            return;

        ImGui::Text("Editing State Machine: %s", stateMachine->Name.c_str());
        char smName[128];
        std::snprintf(smName, sizeof(smName), "%s", stateMachine->Name.c_str());
        if (ImGui::InputText("Name", smName, sizeof(smName)))
        {
            stateMachine->Name = String(smName);
            MarkDirty();
        }

        AnimState* selectedState = nullptr;
        for (AnimState& state : stateMachine->States)
        {
            if (state.ID == m_SelectedStateID)
                selectedState = &state;
        }
        AnimStateAlias* selectedAlias = nullptr;
        for (AnimStateAlias& alias : stateMachine->Aliases)
        {
            if (alias.ID == m_SelectedAliasID)
                selectedAlias = &alias;
        }

        if (selectedState)
        {
            ImGui::Separator();
            ImGui::Text("Selected State");
            char stateName[128];
            std::snprintf(stateName, sizeof(stateName), "%s", selectedState->Name.c_str());
            if (ImGui::InputText("State Name", stateName, sizeof(stateName)))
            {
                selectedState->Name = String(stateName);
                MarkDirty();
            }
            if (ImGui::Button("Open State Graph"))
            {
                m_EditingStateID = selectedState->ID;
                m_StateGraphDocument.SelectedNodeID = 0;
                m_Graph->EnsureDefaultStateGraph(*selectedState);
                EditorGraphPanel::ClearInteractionState();
                return;
            }
            if (ImGui::Checkbox("Loop", &selectedState->bLoop))
                MarkDirty();
            bool isEntry = stateMachine->EntryStateID == selectedState->ID;
            if (ImGui::Checkbox("Entry State", &isEntry) && isEntry)
            {
                stateMachine->EntryStateID = selectedState->ID;
                MarkDirty();
            }
        }

        if (selectedAlias)
        {
            ImGui::Separator();
            ImGui::Text("Selected Alias");
            char aliasName[128];
            std::snprintf(aliasName, sizeof(aliasName), "%s", selectedAlias->Name.c_str());
            if (ImGui::InputText("Alias Name", aliasName, sizeof(aliasName)))
            {
                selectedAlias->Name = String(aliasName);
                MarkDirty();
            }

            if (ImGui::Checkbox("Global Alias", &selectedAlias->bGlobalAlias))
                MarkDirty();

            ImGui::TextDisabled("Target States");
            ImGui::TextWrapped("Each alias target is its own transition edge with its own blend duration and conditions. Runtime uses the first target whose conditions pass.");
            bool changedTargets = false;
            for (const AnimState& state : stateMachine->States)
            {
                bool targeted = AliasHasTarget(*selectedAlias, state.ID);
                const String checkboxLabel = state.Name + "##alias_target_" + String(std::to_string(state.ID).c_str());
                if (ImGui::Checkbox(checkboxLabel.c_str(), &targeted))
                {
                    if (targeted)
                    {
                        if (AddAliasTarget(*selectedAlias, state.ID))
                        {
                            if (AnimStateAliasTarget* target = FindAliasTarget(*selectedAlias, state.ID))
                            {
                                if (target->Conditions.IsEmpty())
                                    target->Conditions.Emplace();
                            }
                            changedTargets = true;
                        }
                    }
                    else
                    {
                        changedTargets |= RemoveAliasTarget(*selectedAlias, state.ID);
                    }
                }
            }

            if (!selectedAlias->TargetStateIDs.IsEmpty() || !selectedAlias->Targets.IsEmpty())
            {
                if (ImGui::Button("Clear Alias Targets"))
                {
                    selectedAlias->TargetStateIDs.Clear();
                    selectedAlias->Targets.Clear();
                    selectedAlias->ToStateID = 0;
                    changedTargets = true;
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.62f, 0.25f, 1.0f), "Alias has no target states.");
            }

            if (changedTargets)
                MarkDirty();

            if (ImGui::DragFloat("Default Alias Blend Duration", &selectedAlias->BlendDuration, 0.01f, 0.0f, 10.0f))
                MarkDirty();

            if (!selectedAlias->Targets.IsEmpty())
            {
                ImGui::Separator();
                ImGui::TextDisabled("Alias Target Transitions");
                for (AnimStateAliasTarget& target : selectedAlias->Targets)
                {
                    const char* targetName = "Missing State";
                    for (const AnimState& state : stateMachine->States)
                    {
                        if (state.ID == target.StateID)
                        {
                            targetName = state.Name.c_str();
                            break;
                        }
                    }

                    ImGui::PushID(static_cast<int>(target.StateID));
                    const String headerLabel = String("Alias -> ") + String(targetName) +
                        "  (" + FormatFloatCompact(target.BlendDuration) + "s)";
                    if (ImGui::TreeNodeEx(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        const String summary = MakeConditionsSummary(target.Conditions);
                        ImGui::TextDisabled("%s", summary.c_str());
                        if (ImGui::DragFloat("Blend Duration", &target.BlendDuration, 0.01f, 0.0f, 10.0f))
                            MarkDirty();
                        if (DrawConditionList("Conditions", target.Conditions, currentAnimClass))
                            MarkDirty();
                        if (ImGui::Button("Remove Target Transition"))
                        {
                            if (RemoveAliasTarget(*selectedAlias, target.StateID))
                                MarkDirty();
                            ImGui::TreePop();
                            ImGui::PopID();
                            break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }

            if (!selectedAlias->bGlobalAlias)
            {
                ImGui::TextDisabled("Allowed From States");
                for (const AnimState& state : stateMachine->States)
                {
                    bool allowed = false;
                    for (const uint64 allowedID : selectedAlias->AllowedFromStateIDs)
                    {
                        if (allowedID == state.ID)
                        {
                            allowed = true;
                            break;
                        }
                    }

                    if (ImGui::Checkbox(state.Name.c_str(), &allowed))
                    {
                        if (allowed)
                        {
                            selectedAlias->AllowedFromStateIDs.Add(state.ID);
                        }
                        else
                        {
                            for (int32 i = 0; i < selectedAlias->AllowedFromStateIDs.Num(); ++i)
                            {
                                if (selectedAlias->AllowedFromStateIDs[i] == state.ID)
                                {
                                    selectedAlias->AllowedFromStateIDs.RemoveAt(i);
                                    break;
                                }
                            }
                        }
                        MarkDirty();
                    }
                }
            }

            if (DrawConditionList("Common Alias Conditions", selectedAlias->Conditions, currentAnimClass))
                MarkDirty();
        }

        ImGui::Separator();
        ImGui::Text("Transitions");
        for (AnimTransition& transition : stateMachine->Transitions)
        {
            ImGui::PushID(static_cast<int>(transition.ID));
            const bool selected = m_SelectedTransitionID == transition.ID;
            const char* fromName = "None";
            const char* toName = "None";
            for (const AnimState& state : stateMachine->States)
            {
                if (state.ID == transition.FromStateID)
                    fromName = state.Name.c_str();
                if (state.ID == transition.ToStateID)
                    toName = state.Name.c_str();
            }
            const String transitionLabel =
                String(fromName) + " -> " + String(toName) +
                "  (" + FormatFloatCompact(transition.BlendDuration) + "s)";
            if (ImGui::Selectable(transitionLabel.c_str(), selected))
            {
                m_SelectedTransitionID = transition.ID;
                m_SelectedStateID = 0;
                m_SelectedAliasID = 0;
            }
            const String transitionSummary = MakeConditionsSummary(transition.Conditions);
            ImGui::TextDisabled("%s", transitionSummary.c_str());

            if (selected)
            {
                auto drawStateCombo = [&](const char* label, uint64& stateID)
                {
                    const char* current = "None";
                    for (const AnimState& state : stateMachine->States)
                    {
                        if (state.ID == stateID)
                            current = state.Name.c_str();
                    }
                    if (ImGui::BeginCombo(label, current))
                    {
                        for (const AnimState& state : stateMachine->States)
                        {
                            const bool stateSelected = state.ID == stateID;
                            if (ImGui::Selectable(state.Name.c_str(), stateSelected))
                            {
                                stateID = state.ID;
                                MarkDirty();
                            }
                        }
                        ImGui::EndCombo();
                    }
                };

                drawStateCombo("From", transition.FromStateID);
                drawStateCombo("To", transition.ToStateID);
                if (ImGui::DragFloat("Blend Duration", &transition.BlendDuration, 0.01f, 0.0f, 10.0f))
                    MarkDirty();

                if (ImGui::Button("Delete Transition"))
                {
                    if (DeleteSelectedTransition(*stateMachine, m_SelectedTransitionID))
                    {
                        MarkDirty();
                        ImGui::PopID();
                        break;
                    }
                }

                if (DrawConditionList("Conditions", transition.Conditions, currentAnimClass))
                    MarkDirty();
            }
            ImGui::PopID();
        }

        return;
    }

    AnimGraphNode* node = m_Graph->FindNode(m_RootGraphDocument.SelectedNodeID);
    if (!node)
    {
        ImGui::TextDisabled("Select a node.");
        return;
    }

    char name[128];
    std::snprintf(name, sizeof(name), "%s", node->Name.c_str());
    if (ImGui::InputText("Name", name, sizeof(name)))
    {
        node->Name = String(name);
        MarkDirty();
    }

    ImGui::Text("Kind: %s", ToNodeKindLabel(node->Kind));
    ImGui::InputScalar("ID", ImGuiDataType_U64, &node->ID, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
    ImGui::Text("Output Reachable: %s", m_Graph->IsNodeReachableFromOutput(node->ID) ? "Yes" : "No");

    if (node->Kind == AnimGraphNodeKind::AnimationClip)
    {
        if (DrawAnimationClipAssetCombo("Clip", node->AnimationClip, m_Graph->m_SkeletonID))
            MarkDirty();

        if (ImGui::DragFloat("Start Time", &node->ClipStartTime, 0.01f, 0.0f, 600.0f))
        {
            node->ClipStartTime = FMath::max(0.0f, node->ClipStartTime);
            MarkDirty();
        }
        ImGui::TextDisabled("Samples the clip starting from this offset.");
    }
    else if (node->Kind == AnimGraphNodeKind::Blend)
    {
        if (DrawBlendAlphaControls(*node, currentAnimClass))
            MarkDirty();
        if (DrawNodeInputCombo("Input A", *m_Graph, node->InputA, node->ID))
            MarkDirty();
        if (DrawNodeInputCombo("Input B", *m_Graph, node->InputB, node->ID))
            MarkDirty();
    }
    else if (node->Kind == AnimGraphNodeKind::Output)
    {
        if (DrawNodeInputCombo("Input Pose", *m_Graph, node->InputPose, node->ID))
            MarkDirty();
        if (m_Graph->m_OutputNodeID != node->ID)
        {
            m_Graph->m_OutputNodeID = node->ID;
            MarkDirty();
        }
    }
    else if (node->Kind == AnimGraphNodeKind::StateMachine)
    {
        AnimStateMachine* stateMachine = m_Graph->FindStateMachine(node->StateMachineID);
        if (!stateMachine)
        {
            if (ImGui::Button("Create State Machine Data"))
            {
                AnimStateMachine& created = m_Graph->AddStateMachine("State Machine");
                node->StateMachineID = created.ID;
                MarkDirty();
            }
        }
        else
        {
            char smName[128];
            std::snprintf(smName, sizeof(smName), "%s", stateMachine->Name.c_str());
            if (ImGui::InputText("State Machine Name", smName, sizeof(smName)))
            {
                stateMachine->Name = String(smName);
                MarkDirty();
            }

            if (ImGui::Button("Add State"))
            {
                AnimState& state = m_Graph->AddState(*stateMachine, "State");
                MarkDirty();
            }

            ImGui::Text("States");
            for (AnimState& state : stateMachine->States)
            {
                ImGui::PushID(static_cast<int>(state.ID));
                char stateName[128];
                std::snprintf(stateName, sizeof(stateName), "%s", state.Name.c_str());
                if (ImGui::InputText("Name", stateName, sizeof(stateName)))
                {
                    state.Name = String(stateName);
                    MarkDirty();
                }
                if (ImGui::Button("Open State Graph"))
                {
                    m_EditingStateMachineID = stateMachine->ID;
                    m_EditingStateID = state.ID;
                    m_StateGraphDocument.SelectedNodeID = 0;
                    m_Graph->EnsureDefaultStateGraph(state);
                    EditorGraphPanel::ClearInteractionState();
                    MarkDirty();
                    ImGui::PopID();
                    return;
                }
                bool isEntry = stateMachine->EntryStateID == state.ID;
                if (ImGui::Checkbox("Entry", &isEntry) && isEntry)
                {
                    stateMachine->EntryStateID = state.ID;
                    MarkDirty();
                }
                ImGui::Separator();
                ImGui::PopID();
            }

            if (stateMachine->States.Num() >= 2 && ImGui::Button("Add Transition"))
            {
                AnimTransition& transition = m_Graph->AddTransition(
                    *stateMachine,
                    stateMachine->States[0].ID,
                    stateMachine->States[1].ID);
                transition.Conditions.Emplace();
                MarkDirty();
            }

            ImGui::Text("Transitions");
            for (AnimTransition& transition : stateMachine->Transitions)
            {
                ImGui::PushID(static_cast<int>(transition.ID));
                auto drawStateCombo = [&](const char* label, uint64& stateID)
                {
                    const char* current = "None";
                    for (const AnimState& state : stateMachine->States)
                    {
                        if (state.ID == stateID)
                            current = state.Name.c_str();
                    }
                    if (ImGui::BeginCombo(label, current))
                    {
                        for (const AnimState& state : stateMachine->States)
                        {
                            const bool selected = state.ID == stateID;
                            if (ImGui::Selectable(state.Name.c_str(), selected))
                            {
                                stateID = state.ID;
                                MarkDirty();
                            }
                        }
                        ImGui::EndCombo();
                    }
                };

                drawStateCombo("From", transition.FromStateID);
                drawStateCombo("To", transition.ToStateID);
                if (ImGui::DragFloat("Blend Duration", &transition.BlendDuration, 0.01f, 0.0f, 10.0f))
                    MarkDirty();

                if (DrawConditionList("Conditions", transition.Conditions, currentAnimClass))
                    MarkDirty();

                ImGui::Separator();
                ImGui::PopID();
            }
        }
    }

    if (node->Kind != AnimGraphNodeKind::Output && !m_Graph->IsNodeReachableFromOutput(node->ID))
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.94f, 0.72f, 0.33f, 1.0f), "%s Not connected to output", ICON_FA_TRIANGLE_EXCLAMATION);
    }
}













