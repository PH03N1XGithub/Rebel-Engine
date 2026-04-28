#pragma once

#include <yaml-cpp/yaml.h>

#include "Core/Serialization/YamlSerializer.h"

class Actor;
class Scene;

namespace ActorTemplateSerializer
{
struct SerializeOptions
{
    bool bSerializeIdentityComponents = false;
};

void SerializeActorTemplate(
    Rebel::Core::Serialization::YamlSerializer& serializer,
    Actor& actor,
    const SerializeOptions& options = {});

Actor* DeserializeActorTemplate(
    Scene& scene,
    Rebel::Core::Serialization::YamlSerializer& serializer,
    const YAML::Node& actorNode);

bool SerializeActorTemplateToString(
    Actor& actor,
    String& outYaml,
    const SerializeOptions& options = {});

Actor* SpawnActorTemplateFromString(Scene& scene, const String& yamlText);
}
