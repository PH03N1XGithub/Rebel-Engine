/*
#pragma once
#include "Buffers.h"
#include "glad/glad.h"

// ----------------------
// OpenGL Vertex Buffer
// ----------------------
class OpenGLVertexBuffer : public VertexBufferBase
{
public:
	OpenGLVertexBuffer(const void* data, uint32_t size)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
	}

	~OpenGLVertexBuffer() override
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void Bind() const override { glBindBuffer(GL_ARRAY_BUFFER, m_RendererID); }
	void Unbind() const override { glBindBuffer(GL_ARRAY_BUFFER, 0); }

private:
	uint32_t m_RendererID = 0;
};

// ----------------------
// OpenGL Index Buffer
// ----------------------
class OpenGLIndexBuffer : public IndexBufferBase
{
public:
	OpenGLIndexBuffer(const uint32_t* data, uint32_t count)
		: m_Count(count)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), data, GL_STATIC_DRAW);
	}

	~OpenGLIndexBuffer() override
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void Bind() const override { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID); }
	void Unbind() const override { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }

	uint32_t GetCount() const override { return m_Count; }

private:
	uint32_t m_RendererID = 0;
	uint32_t m_Count = 0;
};

// ----------------------
// OpenGL Vertex Array
// ----------------------
class OpenGLVertexArray : public VertexArrayBase
{
public:
	OpenGLVertexArray()
	{
		glGenVertexArrays(1, &m_RendererID);
	}

	~OpenGLVertexArray() override
	{
		glDeleteVertexArrays(1, &m_RendererID);
	}

	void Bind() const override { glBindVertexArray(m_RendererID); }
	void Unbind() const override { glBindVertexArray(0); }

	void SetVertexBuffer(const std::shared_ptr<VertexBufferBase>& vertexBuffer,
		uint32_t stride,
		const std::vector<VertexAttribute>& attributes) override
	{
		m_VertexBuffer = vertexBuffer;
		Bind();
		vertexBuffer->Bind();

		for (const auto& attr : attributes)
		{
			glEnableVertexAttribArray(attr.Index);
			glVertexAttribPointer(
				attr.Index,
				attr.Size,
				attr.Type,
				attr.Normalized ? GL_TRUE : GL_FALSE,
				stride,
				(const void*)(uintptr_t)attr.Offset
			);
		}

		Unbind();
	}

	void SetIndexBuffer(const std::shared_ptr<IndexBufferBase>& indexBuffer) override
	{
		m_IndexBuffer = indexBuffer;
		Bind();
		indexBuffer->Bind();
		Unbind();
	}

	const std::shared_ptr<IndexBufferBase>& GetIndexBuffer() const override
	{
		return m_IndexBuffer;
	}

private:
	uint32_t m_RendererID = 0;
	std::shared_ptr<VertexBufferBase> m_VertexBuffer;
	std::shared_ptr<IndexBufferBase> m_IndexBuffer;
};
*/
