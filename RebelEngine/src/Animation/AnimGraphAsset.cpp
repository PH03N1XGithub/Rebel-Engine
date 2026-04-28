#include "Engine/Framework/EnginePch.h"
#include "Engine/Animation/AnimGraphAsset.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Framework/BaseEngine.h"

#include <cmath>
#include <functional>

namespace
{
bool ContainsNodeID(const TArray<uint64>& visited, const uint64 nodeID)
{
    for (const uint64 visitedID : visited)
    {
        if (visitedID == nodeID)
            return true;
    }

    return false;
}

uint64 FindMaxNodeID(const TArray<AnimGraphNode>& nodes)
{
    uint64 maxNodeID = 0;
    for (const AnimGraphNode& node : nodes)
        maxNodeID = FMath::max(maxNodeID, node.ID);

    return maxNodeID;
}

AnimGraphNode* FindNodeInPoseGraph(AnimPoseGraph& poseGraph, const uint64 nodeID)
{
    for (AnimGraphNode& node : poseGraph.Nodes)
    {
        if (node.ID == nodeID)
            return &node;
    }

    return nullptr;
}

const AnimGraphNode* FindNodeInPoseGraph(const AnimPoseGraph& poseGraph, const uint64 nodeID)
{
    for (const AnimGraphNode& node : poseGraph.Nodes)
    {
        if (node.ID == nodeID)
            return &node;
    }

    return nullptr;
}

const AnimGraphNode* FindFirstPoseSource(const AnimPoseGraph& poseGraph)
{
    for (const AnimGraphNode& node : poseGraph.Nodes)
    {
        if (node.Kind != AnimGraphNodeKind::Output)
            return &node;
    }

    return nullptr;
}

const Rebel::Core::Reflection::PropertyInfo* FindPropertyByName(
    const Rebel::Core::Reflection::TypeInfo* type,
    const String& name)
{
    while (type)
    {
        for (const Rebel::Core::Reflection::PropertyInfo& prop : type->Properties)
        {
            if (prop.Name == name)
                return &prop;
        }
        type = type->Super;
    }

    return nullptr;
}

bool ClipMatchesGraphSkeleton(const AssetHandle graphSkeletonID, const AssetHandle clipHandle)
{
    if (!IsValidAssetHandle(graphSkeletonID) || !IsValidAssetHandle(clipHandle) || !GEngine)
        return true;

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
        return true;

    AnimationAsset* animation = dynamic_cast<AnimationAsset*>(assetModule->GetManager().Load(clipHandle));
    if (!animation || !IsValidAssetHandle(animation->m_SkeletonID))
        return true;

    return animation->m_SkeletonID == graphSkeletonID;
}

void ValidateBlendAlphaSource(
    const AnimGraphNode& node,
    const Rebel::Core::Reflection::TypeInfo* animInstanceClass,
    const String& nodeLabel,
    TArray<String>& outIssues)
{
    using Rebel::Core::Reflection::EPropertyType;

    if (node.BlendTime < 0.0f)
        outIssues.Add(nodeLabel + " has a negative blend time.");

    if (node.BlendAlphaMode == AnimBlendAlphaMode::Fixed)
        return;

    const char* expectedType = node.BlendAlphaMode == AnimBlendAlphaMode::FloatProperty ? "float" : "bool";
    const EPropertyType requiredType = node.BlendAlphaMode == AnimBlendAlphaMode::FloatProperty
        ? EPropertyType::Float
        : EPropertyType::Bool;

    if (node.BlendParameterName.length() == 0)
    {
        outIssues.Add(nodeLabel + " uses " + String(expectedType) + " alpha mode but has no parameter selected.");
        return;
    }

    const Rebel::Core::Reflection::PropertyInfo* prop = FindPropertyByName(animInstanceClass, node.BlendParameterName);
    if (!prop)
    {
        outIssues.Add(nodeLabel + " references missing blend parameter '" + node.BlendParameterName + "'.");
        return;
    }

    if (prop->Type != requiredType)
    {
        outIssues.Add(nodeLabel + " blend parameter '" + node.BlendParameterName + "' has the wrong type.");
        return;
    }

    if (node.BlendAlphaMode == AnimBlendAlphaMode::FloatProperty &&
        std::fabs(node.BlendInputMax - node.BlendInputMin) <= 1.0e-6f)
    {
        outIssues.Add(nodeLabel + " has an invalid float blend range.");
    }

}

void SerializeNode(BinaryWriter& ar, const AnimGraphNode& node)
{
    ar << node.ID;
    uint8 kind = static_cast<uint8>(node.Kind);
    ar << kind;
    ar << node.Name;
    ar << node.EditorX;
    ar << node.EditorY;
    ar << node.AnimationClip;
    ar << node.BlendAlpha;
    uint8 blendAlphaMode = static_cast<uint8>(node.BlendAlphaMode);
    ar << blendAlphaMode;
    ar << node.BlendParameterName;
    ar << node.BlendInputMin;
    ar << node.BlendInputMax;
    ar << node.BlendTime;
    ar << node.ClipStartTime;
    ar << node.bBlendInvertBool;
    ar << node.InputA;
    ar << node.InputB;
    ar << node.InputPose;
    ar << node.StateMachineID;
}

void DeserializeNode(BinaryReader& ar, AnimGraphNode& node, const uint32 serializedVersion)
{
    ar >> node.ID;
    uint8 kind = 0;
    ar >> kind;
    node.Kind = static_cast<AnimGraphNodeKind>(kind);
    ar >> node.Name;
    ar >> node.EditorX;
    ar >> node.EditorY;
    ar >> node.AnimationClip;
    ar >> node.BlendAlpha;
    if (serializedVersion >= 6)
    {
        uint8 blendAlphaMode = 0;
        ar >> blendAlphaMode;
        node.BlendAlphaMode = blendAlphaMode <= static_cast<uint8>(AnimBlendAlphaMode::BoolProperty)
            ? static_cast<AnimBlendAlphaMode>(blendAlphaMode)
            : AnimBlendAlphaMode::Fixed;
        ar >> node.BlendParameterName;
        ar >> node.BlendInputMin;
        ar >> node.BlendInputMax;
        if (serializedVersion >= 7)
            ar >> node.BlendTime;
        else
            node.BlendTime = 0.0f;
        if (serializedVersion >= 10)
            ar >> node.ClipStartTime;
        else
            node.ClipStartTime = 0.0f;
        ar >> node.bBlendInvertBool;
    }
    else
    {
        node.BlendAlphaMode = AnimBlendAlphaMode::Fixed;
        node.BlendParameterName = "";
        node.BlendInputMin = 0.0f;
        node.BlendInputMax = 1.0f;
        node.BlendTime = 0.0f;
        node.ClipStartTime = 0.0f;
        node.bBlendInvertBool = false;
    }
    ar >> node.InputA;
    ar >> node.InputB;
    ar >> node.InputPose;
    if (false)
        ar >> node.StateMachineID;
}

void SerializePoseGraph(BinaryWriter& ar, const AnimPoseGraph& poseGraph)
{
    ar << poseGraph.OutputNodeID;
    ar << poseGraph.NextNodeID;

    const uint32 nodeCount = static_cast<uint32>(poseGraph.Nodes.Num());
    ar << nodeCount;
    for (const AnimGraphNode& node : poseGraph.Nodes)
        SerializeNode(ar, node);
}

void DeserializePoseGraph(BinaryReader& ar, AnimPoseGraph& poseGraph, const uint32 serializedVersion)
{
    ar >> poseGraph.OutputNodeID;
    ar >> poseGraph.NextNodeID;

    uint32 nodeCount = 0;
    ar >> nodeCount;
    poseGraph.Nodes.Resize(nodeCount);
    for (uint32 nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
    {
        DeserializeNode(ar, poseGraph.Nodes[nodeIndex], serializedVersion);
        ar >> poseGraph.Nodes[nodeIndex].StateMachineID;
    }
}

bool NodeDependsOnInPoseGraph(
    const AnimPoseGraph& poseGraph,
    const uint64 nodeID,
    const uint64 dependencyNodeID,
    TArray<uint64>& visited)
{
    if (nodeID == 0 || dependencyNodeID == 0)
        return false;
    if (nodeID == dependencyNodeID)
        return true;
    if (ContainsNodeID(visited, nodeID))
        return false;

    visited.Add(nodeID);

    const AnimGraphNode* node = FindNodeInPoseGraph(poseGraph, nodeID);
    if (!node)
        return false;

    if (node->Kind == AnimGraphNodeKind::Blend)
    {
        return NodeDependsOnInPoseGraph(poseGraph, node->InputA, dependencyNodeID, visited) ||
               NodeDependsOnInPoseGraph(poseGraph, node->InputB, dependencyNodeID, visited);
    }

    if (node->Kind == AnimGraphNodeKind::Output)
        return NodeDependsOnInPoseGraph(poseGraph, node->InputPose, dependencyNodeID, visited);

    return false;
}

bool CopyPoseSubtreeToStateGraph(
    const AnimGraphAsset& graph,
    const uint64 sourceNodeID,
    AnimPoseGraph& targetGraph,
    TArray<uint64>& oldNodeIDs,
    TArray<uint64>& newNodeIDs,
    uint64& outNewNodeID)
{
    outNewNodeID = 0;
    if (sourceNodeID == 0)
        return false;

    for (int32 i = 0; i < oldNodeIDs.Num(); ++i)
    {
        if (oldNodeIDs[i] == sourceNodeID)
        {
            outNewNodeID = newNodeIDs[i];
            return true;
        }
    }

    const AnimGraphNode* sourceNode = graph.FindNode(sourceNodeID);
    if (!sourceNode || sourceNode->Kind == AnimGraphNodeKind::Output || sourceNode->Kind == AnimGraphNodeKind::StateMachine)
        return false;

    AnimGraphNode copied = *sourceNode;
    copied.ID = targetGraph.NextNodeID++;
    copied.StateMachineID = 0;

    const uint64 copiedNodeID = copied.ID;
    oldNodeIDs.Add(sourceNodeID);
    newNodeIDs.Add(copiedNodeID);
    targetGraph.Nodes.Add(copied);

    AnimGraphNode* mutableCopy = FindNodeInPoseGraph(targetGraph, copiedNodeID);
    if (!mutableCopy)
        return false;

    if (mutableCopy->Kind == AnimGraphNodeKind::Blend)
    {
        uint64 inputA = 0;
        uint64 inputB = 0;
        CopyPoseSubtreeToStateGraph(graph, sourceNode->InputA, targetGraph, oldNodeIDs, newNodeIDs, inputA);
        CopyPoseSubtreeToStateGraph(graph, sourceNode->InputB, targetGraph, oldNodeIDs, newNodeIDs, inputB);
        mutableCopy->InputA = inputA;
        mutableCopy->InputB = inputB;
    }
    else
    {
        mutableCopy->InputA = 0;
        mutableCopy->InputB = 0;
        mutableCopy->InputPose = 0;
    }

    outNewNodeID = copiedNodeID;
    return true;
}

void CollectPoseSubtreeNodeIDs(const AnimGraphAsset& graph, const uint64 nodeID, TArray<uint64>& outNodeIDs)
{
    if (nodeID == 0 || ContainsNodeID(outNodeIDs, nodeID))
        return;

    const AnimGraphNode* node = graph.FindNode(nodeID);
    if (!node || node->Kind == AnimGraphNodeKind::Output || node->Kind == AnimGraphNodeKind::StateMachine)
        return;

    outNodeIDs.Add(nodeID);
    if (node->Kind == AnimGraphNodeKind::Blend)
    {
        CollectPoseSubtreeNodeIDs(graph, node->InputA, outNodeIDs);
        CollectPoseSubtreeNodeIDs(graph, node->InputB, outNodeIDs);
    }
}
}

const char* ToString(const AnimGraphNodeKind kind)
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

AnimGraphAsset::AnimGraphAsset()
{
    SerializedVersion = kCurrentVersion;
    m_AnimInstanceClass = LocomotionAnimInstance::StaticType();
}

void AnimGraphAsset::Serialize(BinaryWriter& ar)
{
    SerializedVersion = kCurrentVersion;

    const Rebel::Core::Reflection::TypeInfo* animInstanceType = m_AnimInstanceClass.Get();
    String animInstanceClassName = animInstanceType ? animInstanceType->Name : String();
    ar << animInstanceClassName;
    ar << m_SkeletonID;
    AnimPoseGraph topLevelGraph;
    topLevelGraph.OutputNodeID = m_OutputNodeID;
    topLevelGraph.NextNodeID = m_NextNodeID;
    topLevelGraph.Nodes = m_Nodes;

    SerializePoseGraph(ar, topLevelGraph);
    ar << m_NextStateMachineID;

    const uint32 stateMachineCount = static_cast<uint32>(m_StateMachines.Num());
    ar << stateMachineCount;
    for (const AnimStateMachine& stateMachine : m_StateMachines)
    {
        ar << stateMachine.ID;
        ar << stateMachine.Name;
        ar << stateMachine.EntryStateID;
        ar << stateMachine.NextStateID;
        ar << stateMachine.NextTransitionID;
        ar << stateMachine.NextAliasID;

        const uint32 stateCount = static_cast<uint32>(stateMachine.States.Num());
        ar << stateCount;
        for (const AnimState& state : stateMachine.States)
        {
            ar << state.ID;
            ar << state.Name;
            SerializePoseGraph(ar, state.StateGraph);
            ar << state.EditorX;
            ar << state.EditorY;
            ar << state.bLoop;
        }

        const uint32 transitionCount = static_cast<uint32>(stateMachine.Transitions.Num());
        ar << transitionCount;
        for (const AnimTransition& transition : stateMachine.Transitions)
        {
            ar << transition.ID;
            ar << transition.FromStateID;
            ar << transition.ToStateID;
            ar << transition.BlendDuration;

            const uint32 conditionCount = static_cast<uint32>(transition.Conditions.Num());
            ar << conditionCount;
            for (const AnimTransitionCondition& condition : transition.Conditions)
            {
                ar << condition.PropertyName;
                uint8 op = static_cast<uint8>(condition.Op);
                ar << op;
                ar << condition.FloatValue;
                ar << condition.IntValue;
            }
        }

        const uint32 aliasCount = static_cast<uint32>(stateMachine.Aliases.Num());
        ar << aliasCount;
        for (const AnimStateAlias& alias : stateMachine.Aliases)
        {
            ar << alias.ID;
            ar << alias.Name;
            ar << alias.bGlobalAlias;
            ar << alias.ToStateID;
            const uint32 aliasTargetCount = static_cast<uint32>(alias.TargetStateIDs.Num());
            ar << aliasTargetCount;
            for (const uint64 targetStateID : alias.TargetStateIDs)
                ar << targetStateID;
            ar << alias.BlendDuration;
            ar << alias.EditorX;
            ar << alias.EditorY;

            const uint32 allowedFromCount = static_cast<uint32>(alias.AllowedFromStateIDs.Num());
            ar << allowedFromCount;
            for (const uint64 allowedStateID : alias.AllowedFromStateIDs)
                ar << allowedStateID;

            const uint32 conditionCount = static_cast<uint32>(alias.Conditions.Num());
            ar << conditionCount;
            for (const AnimTransitionCondition& condition : alias.Conditions)
            {
                ar << condition.PropertyName;
                uint8 op = static_cast<uint8>(condition.Op);
                ar << op;
                ar << condition.FloatValue;
                ar << condition.IntValue;
            }

            const uint32 targetTransitionCount = static_cast<uint32>(alias.Targets.Num());
            ar << targetTransitionCount;
            for (const AnimStateAliasTarget& target : alias.Targets)
            {
                ar << target.StateID;
                ar << target.BlendDuration;

                const uint32 targetConditionCount = static_cast<uint32>(target.Conditions.Num());
                ar << targetConditionCount;
                for (const AnimTransitionCondition& condition : target.Conditions)
                {
                    ar << condition.PropertyName;
                    uint8 op = static_cast<uint8>(condition.Op);
                    ar << op;
                    ar << condition.FloatValue;
                    ar << condition.IntValue;
                }
            }
        }
    }
}

void AnimGraphAsset::Deserialize(BinaryReader& ar)
{
    if (SerializedVersion >= 2)
    {
        String animInstanceClassName;
        ar >> animInstanceClassName;

        const Rebel::Core::Reflection::TypeInfo* animInstanceType = nullptr;
        if (animInstanceClassName.length() > 0)
            animInstanceType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(animInstanceClassName);

        m_AnimInstanceClass = animInstanceType ? animInstanceType : LocomotionAnimInstance::StaticType();
    }
    else
    {
        m_AnimInstanceClass = LocomotionAnimInstance::StaticType();
    }

    if (SerializedVersion >= 11)
        ar >> m_SkeletonID;
    else
        m_SkeletonID = 0;

    if (SerializedVersion >= 5)
    {
        AnimPoseGraph topLevelGraph;
        DeserializePoseGraph(ar, topLevelGraph, SerializedVersion);
        m_OutputNodeID = topLevelGraph.OutputNodeID;
        m_NextNodeID = topLevelGraph.NextNodeID;
        m_Nodes = topLevelGraph.Nodes;
    }
    else
    {
        ar >> m_OutputNodeID;
        ar >> m_NextNodeID;
    }

    if (SerializedVersion >= 3)
        ar >> m_NextStateMachineID;
    else
        m_NextStateMachineID = 1;

    if (SerializedVersion < 5)
    {
        uint32 count = 0;
        ar >> count;
        m_Nodes.Resize(count);
        for (uint32 i = 0; i < count; ++i)
        {
            DeserializeNode(ar, m_Nodes[i], SerializedVersion);
            if (SerializedVersion < 3)
                m_Nodes[i].StateMachineID = 0;
            else
                ar >> m_Nodes[i].StateMachineID;
        }
    }

    m_StateMachines.Clear();
    if (SerializedVersion >= 3)
    {
        uint32 stateMachineCount = 0;
        ar >> stateMachineCount;
        m_StateMachines.Resize(stateMachineCount);
        for (uint32 smIndex = 0; smIndex < stateMachineCount; ++smIndex)
        {
            AnimStateMachine& stateMachine = m_StateMachines[smIndex];
            ar >> stateMachine.ID;
            ar >> stateMachine.Name;
            ar >> stateMachine.EntryStateID;
            ar >> stateMachine.NextStateID;
            ar >> stateMachine.NextTransitionID;
            bool bHasAliasData = SerializedVersion >= 4;
            if (SerializedVersion >= 4)
                ar >> stateMachine.NextAliasID;
            else if (SerializedVersion == 3)
            {
                const uint64 afterTransitionIDPos = ar.Tell();
                uint64 possibleNextAliasID = 0;
                uint32 possibleStateCount = 0;
                ar >> possibleNextAliasID;
                ar >> possibleStateCount;
                bHasAliasData = possibleNextAliasID > 0 && possibleNextAliasID < 1000000 && possibleStateCount < 1000000;
                ar.Seek(afterTransitionIDPos);
                if (bHasAliasData)
                    ar >> stateMachine.NextAliasID;
                else
                    stateMachine.NextAliasID = 1;
            }
            else
                stateMachine.NextAliasID = 1;

            uint32 stateCount = 0;
            ar >> stateCount;
            stateMachine.States.Resize(stateCount);
            for (uint32 stateIndex = 0; stateIndex < stateCount; ++stateIndex)
            {
                AnimState& state = stateMachine.States[stateIndex];
                ar >> state.ID;
                ar >> state.Name;
                if (SerializedVersion >= 5)
                    DeserializePoseGraph(ar, state.StateGraph, SerializedVersion);
                else
                    ar >> state.PoseNodeID;
                ar >> state.EditorX;
                ar >> state.EditorY;
                ar >> state.bLoop;
            }

            uint32 transitionCount = 0;
            ar >> transitionCount;
            stateMachine.Transitions.Resize(transitionCount);
            for (uint32 transitionIndex = 0; transitionIndex < transitionCount; ++transitionIndex)
            {
                AnimTransition& transition = stateMachine.Transitions[transitionIndex];
                ar >> transition.ID;
                ar >> transition.FromStateID;
                ar >> transition.ToStateID;
                ar >> transition.BlendDuration;

                uint32 conditionCount = 0;
                ar >> conditionCount;
                transition.Conditions.Resize(conditionCount);
                for (uint32 conditionIndex = 0; conditionIndex < conditionCount; ++conditionIndex)
                {
                    AnimTransitionCondition& condition = transition.Conditions[conditionIndex];
                    ar >> condition.PropertyName;
                    uint8 op = 0;
                    ar >> op;
                    condition.Op = static_cast<AnimConditionOp>(op);
                    ar >> condition.FloatValue;
                    ar >> condition.IntValue;
                }
            }

            stateMachine.Aliases.Clear();
            if (bHasAliasData)
            {
                uint32 aliasCount = 0;
                ar >> aliasCount;
                stateMachine.Aliases.Resize(aliasCount);
                for (uint32 aliasIndex = 0; aliasIndex < aliasCount; ++aliasIndex)
                {
                    AnimStateAlias& alias = stateMachine.Aliases[aliasIndex];
                    ar >> alias.ID;
                    ar >> alias.Name;
                    ar >> alias.bGlobalAlias;
                    ar >> alias.ToStateID;
                    alias.TargetStateIDs.Clear();
                    if (SerializedVersion >= 8)
                    {
                        uint32 targetCount = 0;
                        ar >> targetCount;
                        alias.TargetStateIDs.Resize(targetCount);
                        for (uint32 targetIndex = 0; targetIndex < targetCount; ++targetIndex)
                            ar >> alias.TargetStateIDs[targetIndex];
                    }
                    else if (alias.ToStateID != 0)
                    {
                        alias.TargetStateIDs.Add(alias.ToStateID);
                    }
                    ar >> alias.BlendDuration;
                    ar >> alias.EditorX;
                    ar >> alias.EditorY;

                    uint32 allowedFromCount = 0;
                    ar >> allowedFromCount;
                    alias.AllowedFromStateIDs.Resize(allowedFromCount);
                    for (uint32 allowedIndex = 0; allowedIndex < allowedFromCount; ++allowedIndex)
                        ar >> alias.AllowedFromStateIDs[allowedIndex];

                    uint32 conditionCount = 0;
                    ar >> conditionCount;
                    alias.Conditions.Resize(conditionCount);
                    for (uint32 conditionIndex = 0; conditionIndex < conditionCount; ++conditionIndex)
                    {
                        AnimTransitionCondition& condition = alias.Conditions[conditionIndex];
                        ar >> condition.PropertyName;
                        uint8 op = 0;
                        ar >> op;
                        condition.Op = static_cast<AnimConditionOp>(op);
                        ar >> condition.FloatValue;
                        ar >> condition.IntValue;
                    }

                    alias.Targets.Clear();
                    if (SerializedVersion >= 9)
                    {
                        uint32 targetTransitionCount = 0;
                        ar >> targetTransitionCount;
                        alias.Targets.Resize(targetTransitionCount);
                        for (uint32 targetTransitionIndex = 0; targetTransitionIndex < targetTransitionCount; ++targetTransitionIndex)
                        {
                            AnimStateAliasTarget& target = alias.Targets[targetTransitionIndex];
                            ar >> target.StateID;
                            ar >> target.BlendDuration;

                            uint32 targetConditionCount = 0;
                            ar >> targetConditionCount;
                            target.Conditions.Resize(targetConditionCount);
                            for (uint32 targetConditionIndex = 0; targetConditionIndex < targetConditionCount; ++targetConditionIndex)
                            {
                                AnimTransitionCondition& condition = target.Conditions[targetConditionIndex];
                                ar >> condition.PropertyName;
                                uint8 op = 0;
                                ar >> op;
                                condition.Op = static_cast<AnimConditionOp>(op);
                                ar >> condition.FloatValue;
                                ar >> condition.IntValue;
                            }
                        }
                    }
                    else
                    {
                        for (const uint64 targetStateID : alias.TargetStateIDs)
                        {
                            AnimStateAliasTarget& target = alias.Targets.Emplace();
                            target.StateID = targetStateID;
                            target.BlendDuration = alias.BlendDuration;
                            target.Conditions = alias.Conditions;
                        }
                    }
                }
            }
        }
    }
}

void AnimGraphAsset::PostLoad()
{
    Asset::PostLoad();
    if (!m_AnimInstanceClass.Get())
        m_AnimInstanceClass = LocomotionAnimInstance::StaticType();
    EnsureDefaultGraph();

    TArray<uint64> migratedLegacyNodeIDs;
    if (SerializedVersion < 5)
    {
        for (const AnimStateMachine& stateMachine : m_StateMachines)
        {
            for (const AnimState& state : stateMachine.States)
                CollectPoseSubtreeNodeIDs(*this, state.PoseNodeID, migratedLegacyNodeIDs);
        }
    }

    for (AnimStateMachine& stateMachine : m_StateMachines)
    {
        for (AnimState& state : stateMachine.States)
            EnsureDefaultStateGraph(state);

        for (AnimStateAlias& alias : stateMachine.Aliases)
        {
            if (alias.TargetStateIDs.IsEmpty() && alias.ToStateID != 0)
                alias.TargetStateIDs.Add(alias.ToStateID);
            if (alias.Targets.IsEmpty())
            {
                for (const uint64 targetStateID : alias.TargetStateIDs)
                {
                    AnimStateAliasTarget& target = alias.Targets.Emplace();
                    target.StateID = targetStateID;
                    target.BlendDuration = alias.BlendDuration;
                    target.Conditions = alias.Conditions;
                }
            }
            for (const AnimStateAliasTarget& target : alias.Targets)
            {
                if (target.StateID != 0)
                {
                    bool found = false;
                    for (const uint64 targetStateID : alias.TargetStateIDs)
                    {
                        if (targetStateID == target.StateID)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        alias.TargetStateIDs.Add(target.StateID);
                }
            }
            if (alias.ToStateID == 0 && !alias.TargetStateIDs.IsEmpty())
                alias.ToStateID = alias.TargetStateIDs[0];
        }
    }

    for (int32 i = static_cast<int32>(migratedLegacyNodeIDs.Num()) - 1; i >= 0; --i)
    {
        const uint64 nodeID = migratedLegacyNodeIDs[i];
        if (!IsNodeReachableFromOutput(nodeID))
            RemoveNode(nodeID);
    }
}

AnimGraphNode& AnimGraphAsset::AddNode(AnimGraphNodeKind kind, const String& name)
{
    AnimGraphNode& node = m_Nodes.Emplace();
    node.ID = m_NextNodeID++;
    node.Kind = kind;
    node.Name = name;
    return node;
}

AnimGraphNode& AnimGraphAsset::AddNode(AnimPoseGraph& poseGraph, AnimGraphNodeKind kind, const String& name)
{
    AnimGraphNode& node = poseGraph.Nodes.Emplace();
    node.ID = poseGraph.NextNodeID++;
    node.Kind = kind;
    node.Name = name;
    return node;
}

bool AnimGraphAsset::RemoveNode(uint64 nodeID)
{
    AnimGraphNode* node = FindNode(nodeID);
    if (!node || node->Kind == AnimGraphNodeKind::Output)
        return false;

    for (AnimGraphNode& other : m_Nodes)
    {
        if (other.InputA == nodeID)
            other.InputA = 0;
        if (other.InputB == nodeID)
            other.InputB = 0;
        if (other.InputPose == nodeID)
            other.InputPose = 0;
    }

    for (int32 i = 0; i < m_Nodes.Num(); ++i)
    {
        if (m_Nodes[i].ID == nodeID)
        {
            m_Nodes.RemoveAt(i);
            return true;
        }
    }

    return false;
}

bool AnimGraphAsset::RemoveNode(AnimPoseGraph& poseGraph, uint64 nodeID)
{
    AnimGraphNode* node = FindNode(poseGraph, nodeID);
    if (!node || node->Kind == AnimGraphNodeKind::Output)
        return false;

    for (AnimGraphNode& other : poseGraph.Nodes)
    {
        if (other.InputA == nodeID)
            other.InputA = 0;
        if (other.InputB == nodeID)
            other.InputB = 0;
        if (other.InputPose == nodeID)
            other.InputPose = 0;
    }

    for (int32 i = 0; i < poseGraph.Nodes.Num(); ++i)
    {
        if (poseGraph.Nodes[i].ID == nodeID)
        {
            poseGraph.Nodes.RemoveAt(i);
            return true;
        }
    }

    return false;
}

AnimGraphNode* AnimGraphAsset::FindNode(uint64 nodeID)
{
    for (AnimGraphNode& node : m_Nodes)
    {
        if (node.ID == nodeID)
            return &node;
    }
    return nullptr;
}

const AnimGraphNode* AnimGraphAsset::FindNode(uint64 nodeID) const
{
    for (const AnimGraphNode& node : m_Nodes)
    {
        if (node.ID == nodeID)
            return &node;
    }
    return nullptr;
}

AnimGraphNode* AnimGraphAsset::FindNode(AnimPoseGraph& poseGraph, uint64 nodeID)
{
    return FindNodeInPoseGraph(poseGraph, nodeID);
}

const AnimGraphNode* AnimGraphAsset::FindNode(const AnimPoseGraph& poseGraph, uint64 nodeID) const
{
    return FindNodeInPoseGraph(poseGraph, nodeID);
}

AnimGraphNode* AnimGraphAsset::FindOutputNode()
{
    AnimGraphNode* output = FindNode(m_OutputNodeID);
    if (output && output->Kind == AnimGraphNodeKind::Output)
        return output;

    for (AnimGraphNode& node : m_Nodes)
    {
        if (node.Kind == AnimGraphNodeKind::Output)
            return &node;
    }

    return nullptr;
}

const AnimGraphNode* AnimGraphAsset::FindOutputNode() const
{
    const AnimGraphNode* output = FindNode(m_OutputNodeID);
    if (output && output->Kind == AnimGraphNodeKind::Output)
        return output;

    for (const AnimGraphNode& node : m_Nodes)
    {
        if (node.Kind == AnimGraphNodeKind::Output)
            return &node;
    }

    return nullptr;
}

AnimGraphNode* AnimGraphAsset::FindOutputNode(AnimPoseGraph& poseGraph)
{
    AnimGraphNode* output = FindNode(poseGraph, poseGraph.OutputNodeID);
    if (output && output->Kind == AnimGraphNodeKind::Output)
        return output;

    for (AnimGraphNode& node : poseGraph.Nodes)
    {
        if (node.Kind == AnimGraphNodeKind::Output)
            return &node;
    }

    return nullptr;
}

const AnimGraphNode* AnimGraphAsset::FindOutputNode(const AnimPoseGraph& poseGraph) const
{
    const AnimGraphNode* output = FindNode(poseGraph, poseGraph.OutputNodeID);
    if (output && output->Kind == AnimGraphNodeKind::Output)
        return output;

    for (const AnimGraphNode& node : poseGraph.Nodes)
    {
        if (node.Kind == AnimGraphNodeKind::Output)
            return &node;
    }

    return nullptr;
}

bool AnimGraphAsset::NodeDependsOn(uint64 nodeID, uint64 dependencyNodeID) const
{
    TArray<uint64> visited;

    std::function<bool(uint64)> dependsOn = [&](const uint64 currentNodeID) -> bool
    {
        if (currentNodeID == 0 || dependencyNodeID == 0)
            return false;
        if (currentNodeID == dependencyNodeID)
            return true;
        if (ContainsNodeID(visited, currentNodeID))
            return false;

        visited.Add(currentNodeID);

        const AnimGraphNode* node = FindNode(currentNodeID);
        if (!node)
            return false;

        if (node->Kind == AnimGraphNodeKind::Blend)
            return dependsOn(node->InputA) || dependsOn(node->InputB);
        if (node->Kind == AnimGraphNodeKind::Output)
            return dependsOn(node->InputPose);
        if (node->Kind == AnimGraphNodeKind::StateMachine)
        {
            return false;
        }

        return false;
    };

    return dependsOn(nodeID);
}

bool AnimGraphAsset::NodeDependsOn(const AnimPoseGraph& poseGraph, uint64 nodeID, uint64 dependencyNodeID) const
{
    TArray<uint64> visited;
    return NodeDependsOnInPoseGraph(poseGraph, nodeID, dependencyNodeID, visited);
}

bool AnimGraphAsset::CanConnectPose(uint64 sourceNodeID, uint64 targetNodeID) const
{
    if (sourceNodeID == 0)
        return true;
    if (sourceNodeID == targetNodeID)
        return false;

    const AnimGraphNode* source = FindNode(sourceNodeID);
    const AnimGraphNode* target = FindNode(targetNodeID);
    if (!source || !target || source->Kind == AnimGraphNodeKind::Output)
        return false;

    return !NodeDependsOn(sourceNodeID, targetNodeID);
}

bool AnimGraphAsset::CanConnectPose(const AnimPoseGraph& poseGraph, uint64 sourceNodeID, uint64 targetNodeID) const
{
    if (sourceNodeID == 0)
        return true;
    if (sourceNodeID == targetNodeID)
        return false;

    const AnimGraphNode* source = FindNode(poseGraph, sourceNodeID);
    const AnimGraphNode* target = FindNode(poseGraph, targetNodeID);
    if (!source || !target || source->Kind == AnimGraphNodeKind::Output)
        return false;

    if (source->Kind == AnimGraphNodeKind::StateMachine || target->Kind == AnimGraphNodeKind::StateMachine)
        return false;

    return !NodeDependsOn(poseGraph, sourceNodeID, targetNodeID);
}

bool AnimGraphAsset::IsNodeReachableFromOutput(uint64 nodeID) const
{
    const AnimGraphNode* output = FindOutputNode();
    return output && NodeDependsOn(output->ID, nodeID);
}

bool AnimGraphAsset::IsNodeReachableFromOutput(const AnimPoseGraph& poseGraph, uint64 nodeID) const
{
    const AnimGraphNode* output = FindOutputNode(poseGraph);
    return output && NodeDependsOn(poseGraph, output->ID, nodeID);
}

bool AnimGraphAsset::Validate(TArray<String>& outIssues) const
{
    outIssues.Clear();

    TArray<uint64> seenNodeIDs;
    for (const AnimGraphNode& node : m_Nodes)
    {
        if (node.ID == 0)
            outIssues.Add(String("Node '") + node.Name + "' has an invalid ID.");
        else if (ContainsNodeID(seenNodeIDs, node.ID))
            outIssues.Add(String("Duplicate node ID ") + String(std::to_string(node.ID).c_str()) + ".");
        else
            seenNodeIDs.Add(node.ID);
    }

    const AnimGraphNode* output = FindOutputNode();
    if (!output)
    {
        outIssues.Add("Missing output node.");
        return false;
    }

    if (output->InputPose == 0)
        outIssues.Add("Output node has no input pose.");
    else if (!FindNode(output->InputPose))
        outIssues.Add("Output node references a missing input node.");

    for (const AnimGraphNode& node : m_Nodes)
    {
        if (node.Kind == AnimGraphNodeKind::AnimationClip && !IsValidAssetHandle(node.AnimationClip))
            outIssues.Add(String("Animation clip node '") + node.Name + "' has no clip asset.");
        if (node.Kind == AnimGraphNodeKind::AnimationClip && node.ClipStartTime < 0.0f)
            outIssues.Add(String("Animation clip node '") + node.Name + "' has a negative clip start time.");
        if (node.Kind == AnimGraphNodeKind::AnimationClip && !ClipMatchesGraphSkeleton(m_SkeletonID, node.AnimationClip))
            outIssues.Add(String("Animation clip node '") + node.Name + "' does not match the anim graph skeleton.");

        if (node.Kind == AnimGraphNodeKind::Blend)
        {
            if (node.InputA == 0 || !FindNode(node.InputA))
                outIssues.Add(String("Blend node '") + node.Name + "' has no valid input A.");
            if (node.InputB == 0 || !FindNode(node.InputB))
                outIssues.Add(String("Blend node '") + node.Name + "' has no valid input B.");
            ValidateBlendAlphaSource(
                node,
                m_AnimInstanceClass.Get(),
                String("Blend node '") + node.Name + "'",
                outIssues);
        }

        if (node.Kind == AnimGraphNodeKind::StateMachine && !FindStateMachine(node.StateMachineID))
            outIssues.Add(String("State machine node '") + node.Name + "' has no valid state machine.");

        if (node.Kind != AnimGraphNodeKind::Output && !IsNodeReachableFromOutput(node.ID))
            outIssues.Add(String("Node '") + node.Name + "' is not connected to the output.");
    }

    for (const AnimStateMachine& stateMachine : m_StateMachines)
    {
        for (const AnimState& state : stateMachine.States)
        {
            TArray<uint64> seenStateNodeIDs;
            for (const AnimGraphNode& node : state.StateGraph.Nodes)
            {
                if (node.ID == 0)
                    outIssues.Add(String("State '") + state.Name + "' has a graph node with an invalid ID.");
                else if (ContainsNodeID(seenStateNodeIDs, node.ID))
                    outIssues.Add(String("State '") + state.Name + "' has duplicate graph node ID " + String(std::to_string(node.ID).c_str()) + ".");
                else
                    seenStateNodeIDs.Add(node.ID);
            }

            const AnimGraphNode* stateOutput = FindOutputNode(state.StateGraph);
            if (!stateOutput)
            {
                outIssues.Add(String("State '") + state.Name + "' is missing an output pose node.");
                continue;
            }

            if (stateOutput->InputPose == 0)
                outIssues.Add(String("State '") + state.Name + "' output pose has no input.");
            else if (!FindNode(state.StateGraph, stateOutput->InputPose))
                outIssues.Add(String("State '") + state.Name + "' output pose references a missing node.");

            for (const AnimGraphNode& node : state.StateGraph.Nodes)
            {
                if (node.Kind == AnimGraphNodeKind::AnimationClip && !IsValidAssetHandle(node.AnimationClip))
                    outIssues.Add(String("State '") + state.Name + "' animation clip node '" + node.Name + "' has no clip asset.");
                if (node.Kind == AnimGraphNodeKind::AnimationClip && node.ClipStartTime < 0.0f)
                    outIssues.Add(String("State '") + state.Name + "' animation clip node '" + node.Name + "' has a negative clip start time.");
                if (node.Kind == AnimGraphNodeKind::AnimationClip && !ClipMatchesGraphSkeleton(m_SkeletonID, node.AnimationClip))
                    outIssues.Add(String("State '") + state.Name + "' animation clip node '" + node.Name + "' does not match the anim graph skeleton.");

                if (node.Kind == AnimGraphNodeKind::Blend)
                {
                    if (node.InputA == 0 || !FindNode(state.StateGraph, node.InputA))
                        outIssues.Add(String("State '") + state.Name + "' blend node '" + node.Name + "' has no valid input A.");
                    if (node.InputB == 0 || !FindNode(state.StateGraph, node.InputB))
                        outIssues.Add(String("State '") + state.Name + "' blend node '" + node.Name + "' has no valid input B.");
                    ValidateBlendAlphaSource(
                        node,
                        m_AnimInstanceClass.Get(),
                        String("State '") + state.Name + "' blend node '" + node.Name + "'",
                        outIssues);
                }

                if (node.Kind == AnimGraphNodeKind::StateMachine)
                    outIssues.Add(String("State '") + state.Name + "' contains a nested state machine node, which is not supported yet.");

                if (node.Kind != AnimGraphNodeKind::Output && !IsNodeReachableFromOutput(state.StateGraph, node.ID))
                    outIssues.Add(String("State '") + state.Name + "' node '" + node.Name + "' is not connected to the state output.");
            }
        }
    }

    return outIssues.Num() == 0;
}

void AnimGraphAsset::EnsureDefaultGraph()
{
    const uint64 maxExistingNodeID = FindMaxNodeID(m_Nodes);
    if (m_NextNodeID <= maxExistingNodeID)
        m_NextNodeID = maxExistingNodeID + 1;

    if (m_Nodes.Num() > 0)
    {
        if (AnimGraphNode* output = FindOutputNode())
        {
            m_OutputNodeID = output->ID;
            return;
        }

        AnimGraphNode& output = AddNode(AnimGraphNodeKind::Output, "Output Pose");
        output.EditorX = 420.0f;
        output.EditorY = 120.0f;
        AnimPoseGraph topLevelGraph;
        topLevelGraph.Nodes = m_Nodes;
        if (const AnimGraphNode* firstPoseSource = FindFirstPoseSource(topLevelGraph))
            output.InputPose = firstPoseSource->ID;
        m_OutputNodeID = output.ID;
        return;
    }

    m_Nodes.Clear();
    m_StateMachines.Clear();
    m_NextNodeID = 1;
    m_NextStateMachineID = 1;

    AnimStateMachine& locomotionSM = AddStateMachine("Locomotion");
    AnimState& idleState = AddState(locomotionSM, "Idle");
    AnimState& moveState = AddState(locomotionSM, "Move");
    AnimState& startState = AddState(locomotionSM, "Start");
    AnimState& stopState = AddState(locomotionSM, "Stop");
    AnimState& pivotState = AddState(locomotionSM, "Pivot");
    AnimState& inAirState = AddState(locomotionSM, "InAir");
    AnimState& landState = AddState(locomotionSM, "Land");
    locomotionSM.EntryStateID = idleState.ID;

    auto AddBoolCondition = [](AnimTransition& transition, const String& propertyName, const bool bExpectedValue)
    {
        AnimTransitionCondition& condition = transition.Conditions.Emplace();
        condition.PropertyName = propertyName;
        condition.Op = bExpectedValue ? AnimConditionOp::BoolIsTrue : AnimConditionOp::BoolIsFalse;
    };

    AddBoolCondition(AddTransition(locomotionSM, idleState.ID, startState.ID), "bIsStarting", true);
    AddBoolCondition(AddTransition(locomotionSM, startState.ID, moveState.ID), "bIsMoving", true);
    AddBoolCondition(AddTransition(locomotionSM, moveState.ID, stopState.ID), "bIsStopping", true);
    AddBoolCondition(AddTransition(locomotionSM, moveState.ID, pivotState.ID), "bIsPivoting", true);
    AddBoolCondition(AddTransition(locomotionSM, inAirState.ID, landState.ID), "Landed", true);
    AddBoolCondition(AddTransition(locomotionSM, landState.ID, moveState.ID), "bIsMoving", true);
    AddBoolCondition(AddTransition(locomotionSM, landState.ID, idleState.ID), "bIsMoving", false);
    AddBoolCondition(AddTransition(locomotionSM, stopState.ID, idleState.ID), "bIsMoving", false);
    AddBoolCondition(AddTransition(locomotionSM, stopState.ID, moveState.ID), "bIsMoving", true);
    AddBoolCondition(AddTransition(locomotionSM, pivotState.ID, moveState.ID), "bIsMoving", true);
    AddBoolCondition(AddTransition(locomotionSM, pivotState.ID, idleState.ID), "bIsMoving", false);

    AnimStateAlias& inAirAlias = AddAlias(locomotionSM, "AnyToInAir");
    inAirAlias.bGlobalAlias = true;
    inAirAlias.ToStateID = inAirState.ID;
    inAirAlias.TargetStateIDs.Clear();
    inAirAlias.TargetStateIDs.Add(inAirState.ID);
    inAirAlias.Targets.Clear();
    AnimStateAliasTarget& inAirTarget = inAirAlias.Targets.Emplace();
    inAirTarget.StateID = inAirState.ID;
    inAirTarget.BlendDuration = 0.08f;
    AnimTransitionCondition& inAirCondition = inAirTarget.Conditions.Emplace();
    inAirCondition.PropertyName = "bIsInAir";
    inAirCondition.Op = AnimConditionOp::BoolIsTrue;
    inAirAlias.Conditions = inAirTarget.Conditions;

    AnimGraphNode& locomotionNode = AddNode(AnimGraphNodeKind::StateMachine, "Locomotion State Machine");
    locomotionNode.EditorX = 80.0f;
    locomotionNode.EditorY = 120.0f;
    locomotionNode.StateMachineID = locomotionSM.ID;

    AnimGraphNode& output = AddNode(AnimGraphNodeKind::Output, "Output Pose");
    output.EditorX = 420.0f;
    output.EditorY = 120.0f;
    output.InputPose = locomotionNode.ID;
    m_OutputNodeID = output.ID;
}

void AnimGraphAsset::EnsureDefaultStateGraph(AnimState& state)
{
    const uint64 maxExistingNodeID = FindMaxNodeID(state.StateGraph.Nodes);
    if (state.StateGraph.NextNodeID <= maxExistingNodeID)
        state.StateGraph.NextNodeID = maxExistingNodeID + 1;

    if (state.StateGraph.Nodes.IsEmpty() && state.PoseNodeID != 0)
    {
        TArray<uint64> oldNodeIDs;
        TArray<uint64> newNodeIDs;
        uint64 migratedInputID = 0;
        CopyPoseSubtreeToStateGraph(*this, state.PoseNodeID, state.StateGraph, oldNodeIDs, newNodeIDs, migratedInputID);

        AnimGraphNode& output = AddNode(state.StateGraph, AnimGraphNodeKind::Output, "Output Pose");
        output.EditorX = 420.0f;
        output.EditorY = 120.0f;
        output.InputPose = migratedInputID;
        state.StateGraph.OutputNodeID = output.ID;
        state.PoseNodeID = 0;
        return;
    }

    if (!state.StateGraph.Nodes.IsEmpty())
    {
        if (AnimGraphNode* output = FindOutputNode(state.StateGraph))
        {
            if (output->InputPose == 0)
            {
                if (const AnimGraphNode* firstPoseSource = FindFirstPoseSource(state.StateGraph))
                    output->InputPose = firstPoseSource->ID;
            }
            state.StateGraph.OutputNodeID = output->ID;
            state.PoseNodeID = 0;
            return;
        }

        AnimGraphNode& output = AddNode(state.StateGraph, AnimGraphNodeKind::Output, "Output Pose");
        output.EditorX = 420.0f;
        output.EditorY = 120.0f;
        if (const AnimGraphNode* firstPoseSource = FindFirstPoseSource(state.StateGraph))
            output.InputPose = firstPoseSource->ID;
        state.StateGraph.OutputNodeID = output.ID;
        state.PoseNodeID = 0;
        return;
    }

    AnimGraphNode& clip = AddNode(state.StateGraph, AnimGraphNodeKind::AnimationClip, "Animation Clip");
    clip.EditorX = 80.0f;
    clip.EditorY = 120.0f;

    AnimGraphNode& output = AddNode(state.StateGraph, AnimGraphNodeKind::Output, "Output Pose");
    output.EditorX = 420.0f;
    output.EditorY = 120.0f;
    output.InputPose = clip.ID;
    state.StateGraph.OutputNodeID = output.ID;
    state.PoseNodeID = 0;
}

AnimStateMachine& AnimGraphAsset::AddStateMachine(const String& name)
{
    AnimStateMachine& stateMachine = m_StateMachines.Emplace();
    stateMachine.ID = m_NextStateMachineID++;
    stateMachine.Name = name;
    return stateMachine;
}

AnimStateMachine* AnimGraphAsset::FindStateMachine(uint64 stateMachineID)
{
    for (AnimStateMachine& stateMachine : m_StateMachines)
    {
        if (stateMachine.ID == stateMachineID)
            return &stateMachine;
    }
    return nullptr;
}

const AnimStateMachine* AnimGraphAsset::FindStateMachine(uint64 stateMachineID) const
{
    for (const AnimStateMachine& stateMachine : m_StateMachines)
    {
        if (stateMachine.ID == stateMachineID)
            return &stateMachine;
    }
    return nullptr;
}

AnimState& AnimGraphAsset::AddState(AnimStateMachine& stateMachine, const String& name)
{
    AnimState& state = stateMachine.States.Emplace();
    state.ID = stateMachine.NextStateID++;
    state.Name = name;
    state.EditorX = 120.0f + 180.0f * static_cast<float>(stateMachine.States.Num() - 1);
    state.EditorY = 140.0f;
    if (stateMachine.EntryStateID == 0)
        stateMachine.EntryStateID = state.ID;
    EnsureDefaultStateGraph(state);
    return state;
}

AnimTransition& AnimGraphAsset::AddTransition(AnimStateMachine& stateMachine, uint64 fromStateID, uint64 toStateID)
{
    AnimTransition& transition = stateMachine.Transitions.Emplace();
    transition.ID = stateMachine.NextTransitionID++;
    transition.FromStateID = fromStateID;
    transition.ToStateID = toStateID;
    return transition;
}

AnimStateAlias& AnimGraphAsset::AddAlias(AnimStateMachine& stateMachine, const String& name)
{
    AnimStateAlias& alias = stateMachine.Aliases.Emplace();
    alias.ID = stateMachine.NextAliasID++;
    alias.Name = name;
    alias.bGlobalAlias = true;
    alias.EditorX = 80.0f + 180.0f * static_cast<float>(stateMachine.Aliases.Num() - 1);
    alias.EditorY = 40.0f;
    return alias;
}
