#include "Engine/Framework/EnginePch.h"
#include "Engine/Assets/PrefabAsset.h"

void PrefabAsset::Serialize(BinaryWriter& ar)
{
    ar << m_ActorTypeName;
    ar << m_TemplateYaml;
}

void PrefabAsset::Deserialize(BinaryReader& ar)
{
    ar >> m_ActorTypeName;
    ar >> m_TemplateYaml;
}
