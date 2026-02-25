#pragma once
#include <memory>
#include <vector>

#include "OpenGLBuffers.h"
#include <cstdint>
#include "Buffers.h"

struct Material
{
	uint32 AlbedoTex = 0;      // GL texture id
	Vector3 AlbedoColor{1.0f}; // tint
	// later: NormalTex, RoughnessTex, Metallic, etc.
};

struct MaterialHandle
{
	uint32 Id = 0; // index into some material array
};

struct MeshHandle {
	uint32 firstIndex = 0;
	uint32 indexCount = 0;
	int32  baseVertex = 0;

	BOOL isValid() const { return indexCount != 0; }
};

/*
class Mesh
{
public:
	Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	: m_RawVertices(vertices), m_RawIndices(indices) 
	{
		m_VertexArray = std::make_shared<OpenGLVertexArray>();

		m_VertexBuffer = std::make_shared<OpenGLVertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
		m_IndexBuffer  = std::make_shared<OpenGLIndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size()));

		std::vector<VertexAttribute> attributes = {
			{ 0, 3, GL_FLOAT, false, offsetof(Vertex, Position) },
			{ 1, 3, GL_FLOAT, false, offsetof(Vertex, Color) }
		};

		m_VertexArray->SetVertexBuffer(m_VertexBuffer, sizeof(Vertex), attributes);
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);
	}

	void Bind() const { m_VertexArray->Bind(); }
	void Unbind() const { m_VertexArray->Unbind(); }

	void Draw() const
	{
		m_VertexArray->Bind();
		glDrawElements(GL_TRIANGLES, m_IndexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);
	}
	// ✅ Add this getter for the abstract VAO
	std::shared_ptr<VertexArrayBase> GetVAO() const { return m_VertexArray; }

	// ✅ New getters for batch rendering
	const std::vector<Vertex>& GetVertices() const { return m_RawVertices; }
	const std::vector<uint32_t>& GetIndices() const { return m_RawIndices; }

private:
	std::shared_ptr<VertexArrayBase> m_VertexArray;
	std::shared_ptr<VertexBufferBase> m_VertexBuffer;
	std::shared_ptr<IndexBufferBase>  m_IndexBuffer;

	// ✅ Store raw CPU-side data
	std::vector<Vertex> m_RawVertices;
	std::vector<uint32_t> m_RawIndices;
};
*/
