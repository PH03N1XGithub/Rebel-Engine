#pragma once
#include "Buffers.h"
#include <memory>
#include <vector>

class RenderAPI
{
public:
    virtual ~RenderAPI() = default;

    // Clear screen
    virtual void Clear(float r, float g, float b, float a) = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    virtual void EnableDepthTest(bool enable) = 0;
    virtual void EnableBackfaceCull(bool enable) = 0;

    // Shader
    virtual uint32_t CreateShader(const char* vertexSrc, const char* fragmentSrc) = 0;
    virtual void UseShader(uint32_t shaderID) = 0;
    virtual void SetUniformMat4(uint32_t shaderID, const char* name, const float* value) = 0;
    virtual int GetUniformLocation(uint32_t shaderID, const char* name) = 0;

    // Framebuffer
    virtual void InitFBO(int width, int height, uint32_t& fbo, uint32_t& colorTex, uint32_t& rbo) = 0;
    virtual void BindFBO(uint32_t fbo) = 0;

    // Draw
    //virtual void DrawIndexed(const std::shared_ptr<VertexArrayBase>& vao) = 0;

    //Delet Buffers
    virtual void Shutdown(uint32_t fbo, uint32_t colorTex, uint32_t rbo, uint32_t shaderProgram) = 0;
};
