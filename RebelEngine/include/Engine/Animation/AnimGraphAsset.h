#pragma once

#include "Engine/Animation/AnimInstance.h"
#include "Engine/Assets/BaseAsset.h"
#include "Engine/Framework/TSubclassOf.h"

enum class AnimGraphNodeKind : uint8
{
    AnimationClip = 0,
    Blend = 1,
    Output = 2,
    StateMachine = 3
};

const char* ToString(AnimGraphNodeKind kind);

enum class AnimBlendAlphaMode : uint8
{
    Fixed = 0,
    FloatProperty,
    BoolProperty
};

struct AnimGraphNode
{
    uint64 ID = 0;
    AnimGraphNodeKind Kind = AnimGraphNodeKind::AnimationClip;
    String Name;
    float EditorX = 0.0f;
    float EditorY = 0.0f;
    AssetHandle AnimationClip = 0;
    float BlendAlpha = 0.5f;
    AnimBlendAlphaMode BlendAlphaMode = AnimBlendAlphaMode::Fixed;
    String BlendParameterName;
    float BlendInputMin = 0.0f;
    float BlendInputMax = 1.0f;
    float BlendTime = 0.0f;
    float ClipStartTime = 0.0f;
    bool bBlendInvertBool = false;
    uint64 InputA = 0;
    uint64 InputB = 0;
    uint64 InputPose = 0;
    uint64 StateMachineID = 0;
};

struct AnimPoseGraph
{
    uint64 OutputNodeID = 0;
    uint64 NextNodeID = 1;
    TArray<AnimGraphNode> Nodes;
};

enum class AnimConditionOp : uint8
{
    BoolIsTrue = 0,
    BoolIsFalse,
    FloatGreater,
    FloatLess,
    IntGreater,
    IntLess,
    StateTimeGreater,
    StateTimeLess,
    AnimTimeRemainingLess,
    AnimTimeRemainingRatioLess,
    EnumEquals,
    EnumNotEquals
};

struct AnimTransitionCondition
{
    String PropertyName;
    AnimConditionOp Op = AnimConditionOp::BoolIsTrue;
    float FloatValue = 0.0f;
    int32 IntValue = 0;
};

struct AnimState
{
    uint64 ID = 0;
    String Name;
    // Legacy v4 and older assets stored a direct pose node reference here.
    // New assets evaluate StateGraph.OutputNodeID instead.
    uint64 PoseNodeID = 0;
    AnimPoseGraph StateGraph;
    float EditorX = 0.0f;
    float EditorY = 0.0f;
    bool bLoop = true;
};

struct AnimTransition
{
    uint64 ID = 0;
    uint64 FromStateID = 0;
    uint64 ToStateID = 0;
    float BlendDuration = 0.15f;
    TArray<AnimTransitionCondition> Conditions;
};

struct AnimStateAliasTarget
{
    uint64 StateID = 0;
    float BlendDuration = 0.15f;
    TArray<AnimTransitionCondition> Conditions;
};

struct AnimStateAlias
{
    uint64 ID = 0;
    String Name;
    bool bGlobalAlias = true;
    uint64 ToStateID = 0;
    TArray<uint64> TargetStateIDs;
    TArray<AnimStateAliasTarget> Targets;
    float BlendDuration = 0.15f;
    float EditorX = 0.0f;
    float EditorY = 0.0f;
    TArray<uint64> AllowedFromStateIDs;
    TArray<AnimTransitionCondition> Conditions;
};

struct AnimStateMachine
{
    uint64 ID = 0;
    String Name;
    uint64 EntryStateID = 0;
    uint64 NextStateID = 1;
    uint64 NextTransitionID = 1;
    uint64 NextAliasID = 1;
    TArray<AnimState> States;
    TArray<AnimTransition> Transitions;
    TArray<AnimStateAlias> Aliases;
};

struct AnimGraphAsset : Asset
{
    REFLECTABLE_CLASS(AnimGraphAsset, Asset)

    static constexpr uint32 kCurrentVersion = 11;

    AnimGraphAsset();

    void Serialize(BinaryWriter& ar) override;
    void Deserialize(BinaryReader& ar) override;
    void PostLoad() override;

    AssetDisplayColor GetDisplayColor() const override { return GetStaticDisplayColor(); }

    static constexpr AssetDisplayColor GetStaticDisplayColor()
    {
        return { 255, 175, 90, 255 };
    }

    AnimGraphNode& AddNode(AnimGraphNodeKind kind, const String& name);
    AnimGraphNode& AddNode(AnimPoseGraph& poseGraph, AnimGraphNodeKind kind, const String& name);
    bool RemoveNode(uint64 nodeID);
    bool RemoveNode(AnimPoseGraph& poseGraph, uint64 nodeID);
    AnimGraphNode* FindNode(uint64 nodeID);
    const AnimGraphNode* FindNode(uint64 nodeID) const;
    AnimGraphNode* FindNode(AnimPoseGraph& poseGraph, uint64 nodeID);
    const AnimGraphNode* FindNode(const AnimPoseGraph& poseGraph, uint64 nodeID) const;
    AnimGraphNode* FindOutputNode();
    const AnimGraphNode* FindOutputNode() const;
    AnimGraphNode* FindOutputNode(AnimPoseGraph& poseGraph);
    const AnimGraphNode* FindOutputNode(const AnimPoseGraph& poseGraph) const;
    bool NodeDependsOn(uint64 nodeID, uint64 dependencyNodeID) const;
    bool NodeDependsOn(const AnimPoseGraph& poseGraph, uint64 nodeID, uint64 dependencyNodeID) const;
    bool CanConnectPose(uint64 sourceNodeID, uint64 targetNodeID) const;
    bool CanConnectPose(const AnimPoseGraph& poseGraph, uint64 sourceNodeID, uint64 targetNodeID) const;
    bool IsNodeReachableFromOutput(uint64 nodeID) const;
    bool IsNodeReachableFromOutput(const AnimPoseGraph& poseGraph, uint64 nodeID) const;
    bool Validate(TArray<String>& outIssues) const;
    void EnsureDefaultGraph();
    void EnsureDefaultStateGraph(AnimState& state);
    AnimStateMachine& AddStateMachine(const String& name);
    AnimStateMachine* FindStateMachine(uint64 stateMachineID);
    const AnimStateMachine* FindStateMachine(uint64 stateMachineID) const;
    AnimState& AddState(AnimStateMachine& stateMachine, const String& name);
    AnimTransition& AddTransition(AnimStateMachine& stateMachine, uint64 fromStateID, uint64 toStateID);
    AnimStateAlias& AddAlias(AnimStateMachine& stateMachine, const String& name);

    TArray<AnimGraphNode> m_Nodes;
    TArray<AnimStateMachine> m_StateMachines;
    AssetHandle m_SkeletonID = 0;
    Rebel::TSubclassOf<AnimInstance> m_AnimInstanceClass;
    uint64 m_OutputNodeID = 0;
    uint64 m_NextNodeID = 1;
    uint64 m_NextStateMachineID = 1;
};

REFLECT_CLASS(AnimGraphAsset, Asset)
{
    REFLECT_PROPERTY(AnimGraphAsset, m_OutputNodeID,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(AnimGraphAsset, m_NextNodeID,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(AnimGraphAsset, m_NextStateMachineID,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(AnimGraphAsset, m_SkeletonID,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(AnimGraphAsset, m_AnimInstanceClass,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
}
END_REFLECT_CLASS(AnimGraphAsset)
