#pragma once
#include "Core.h"
#include <memory>
#include <vector>
#include <cstdint>

// FBX-ready canonical layout.
// If a mesh doesn't have some attributes, fill sensible defaults.
struct Vertex
{
	Vector3 	Position;								// layout(location=0)
	Vector3 	Color;									// kept for debug / vertex-color FBX (we'll move this to location=4 in VAO)
	// --- new fields for real assets ---
	Vector3		Normal   {0,1,0};					// layout(location=1)
	Vector2		UV       {0,0};						// layout(location=2)
	Vector4		Tangent  {1,0,0,1};			// layout(location=3) (xyz tangent, w = handedness)

	// optional skinning (leave zeros for static meshes)
	uint8		BoneIndex[4] {0,0,0,0};	// layout(location=5) as integer attrib
	Float		BoneWeight[4] {1,0,0,0};	// layout(location=6)
};
static_assert(sizeof(Vertex) == 80, "Vertex should be 80 bytes");

// The rest of your interfaces unchanged…
struct VertexAttribute
{
	uint32 		Index;
	uint32 		Size;
	uint32 		Type;
	Bool   		Normalized;
	uint32 		Offset;
};

/*// --- Base Interfaces ---

class VertexBufferBase
{
public:
	virtual ~VertexBufferBase() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;
};

class IndexBufferBase
{
public:
	virtual ~IndexBufferBase() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual uint32 GetCount() const = 0;
};

class VertexArrayBase
{
public:
	virtual ~VertexArrayBase() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void SetVertexBuffer(const std::shared_ptr<VertexBufferBase>& vertexBuffer,
		uint32 stride,
		const std::vector<VertexAttribute>& attributes) = 0;

	virtual void SetIndexBuffer(const std::shared_ptr<IndexBufferBase>& indexBuffer) = 0;

	virtual const std::shared_ptr<IndexBufferBase>& GetIndexBuffer() const = 0;
};*/
