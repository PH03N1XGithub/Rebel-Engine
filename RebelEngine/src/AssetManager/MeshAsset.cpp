#include "EnginePch.h"
#include "AssetManager/MeshAsset.h"

//#include "BaseEngine.h"

void MeshAsset::Serialize(BinaryWriter& ar)
{
	ar << Vertices;
	ar << Indices;
}

void MeshAsset::Deserialize(BinaryReader& ar)
{
	ar >> Vertices;
	ar >> Indices;
}

void MeshAsset::PostLoad()
{
	/*auto ogl = static_cast<OpenGLRenderAPI*>(GEngine->GetModuleManager().GetModule<RenderModule>()->GetRendererAPI());
	Handle = ogl->AddStaticMesh(Vertices, Indices);
	Vertices.Clear();
	Indices.Clear();*/
}