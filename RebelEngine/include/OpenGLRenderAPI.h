#pragma once
#include "RenderAPI.h"
#include "glad/glad.h"
#include <vector>
#include <cstdint>
#include <iostream>
#include <algorithm>    // for std::sort

// assumes Vertex is FBX-ready:
// struct Vertex {
//   glm::vec3 Position; glm::vec3 Color;
//   glm::vec3 Normal; glm::vec2 UV; glm::vec4 Tangent;
//   uint8_t BoneIndex[4]; Float BoneWeight[4];
// };

class OpenGLRenderAPI : public RenderAPI
{
public:
    struct DrawElementsIndirectCommand {
        uint32 count;
        uint32 instanceCount;
        uint32 firstIndex;
        uint32 baseVertex;
        uint32 baseInstance;
    };
    
    OpenGLRenderAPI() {
        // global VAO + packed static buffers
        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);

        glGenBuffers(1, &m_VBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, StaticVBSize, nullptr, GL_STATIC_DRAW);

        glGenBuffers(1, &m_IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, StaticIBSize, nullptr, GL_STATIC_DRAW);

        // --- FBX-ready vertex layout ---
        glEnableVertexAttribArray(0); // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, Position));

        glEnableVertexAttribArray(1); // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, Normal));

        glEnableVertexAttribArray(2); // UV
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, UV));

        glEnableVertexAttribArray(3); // Tangent
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, Tangent));

        glEnableVertexAttribArray(4); // Color (for your current demo / vertex colors)
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, Color));

        glEnableVertexAttribArray(5); // BoneIndex (u8vec4) as integer attrib
        glVertexAttribIPointer(5, 4, GL_UNSIGNED_BYTE, sizeof(Vertex), (const void*)offsetof(Vertex, BoneIndex));

        glEnableVertexAttribArray(6); // BoneWeight
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, BoneWeight));

        glBindVertexArray(0);

        // per-frame SSBO (model matrices) & indirect command buffer
        glGenBuffers(1, &m_ModelSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ModelSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, PerFrameDrawCap * sizeof(Float) * 16, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ModelSSBO);

        glGenBuffers(1, &m_IndirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_IndirectBuffer);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, PerFrameDrawCap * sizeof(DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

        // per-frame SSBO (object ids) for picking
        glGenBuffers(1, &m_ObjectIdSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ObjectIdSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, PerFrameDrawCap * sizeof(uint32), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_ObjectIdSSBO);

        // per-frame SSBO (bone matrices, binding=2)
        glGenBuffers(1, &m_BoneSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BoneSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Mat4), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_BoneSSBO);

        // per-batch SSBO (bone base per draw, binding=3)
        glGenBuffers(1, &m_BoneBaseSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BoneBaseSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, PerFrameDrawCap * sizeof(uint32), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BoneBaseSSBO);


        EnableDebugOutput();
    }

    ~OpenGLRenderAPI() override {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_IBO);
        glDeleteBuffers(1, &m_ModelSSBO);
        glDeleteBuffers(1, &m_IndirectBuffer);
        glDeleteBuffers(1, &m_ObjectIdSSBO);
        glDeleteBuffers(1, &m_BoneSSBO);
        glDeleteBuffers(1, &m_BoneBaseSSBO);

    }

    // ----- core api -----
    void Clear(Float r, Float g, Float b, Float a) override {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void SetViewport(int32 x, int32 y, int32 width, int32 height) override { glViewport(x, y, width, height); }

    void EnableDepthTest(Bool enable) override { if (enable) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); }

    void EnableBackfaceCull(Bool enable) override { if (enable) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK);} else glDisable(GL_CULL_FACE); }

    uint32 CreateShader(const char* vertexSrc, const char* fragmentSrc) override {
        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert, 1, &vertexSrc, nullptr);
        glCompileShader(vert);
        CheckShader(vert, false);

        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &fragmentSrc, nullptr);
        glCompileShader(frag);
        CheckShader(frag, false);

        GLuint program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);
        CheckShader(program, true);

        glDeleteShader(vert);
        glDeleteShader(frag);
        return program;
    }

    void UseShader(uint32 shaderID) override { glUseProgram(shaderID); }

    int32 GetUniformLocation(uint32 shaderID, const char* name) override { return glGetUniformLocation(shaderID, name); }

    void SetUniformMat4(uint32 shaderID, const char* name, const Float* value) override {
        const GLint loc = glGetUniformLocation(shaderID, name);
        glUniformMatrix4fv(loc, 1, GL_FALSE, value);
    }

    void SetUniformMat4Cached(uint32, int32 cachedLoc, const Float* value) { glUniformMatrix4fv(cachedLoc, 1, GL_FALSE, value); }

    void InitFBO(int32 width, int32 height, uint32& fbo, uint32& colorTex, uint32& rbo) override {
        if (fbo) { glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &colorTex); glDeleteRenderbuffers(1, &rbo); }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "FBO not complete!\n";

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void BindFBO(uint32 fbo) override { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }

    void BlitToDefault(uint32 srcFBO, int32 w, int32 h) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }

    /*void DrawIndexed(const std::shared_ptr<VertexArrayBase>& vao) override {
        vao->Bind();
        glDrawElements(GL_TRIANGLES, vao->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
        vao->Unbind();
    }*/

    void Shutdown(uint32 fbo, uint32 colorTex, uint32 rbo, uint32 shaderProgram) override {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (colorTex) glDeleteTextures(1, &colorTex);
        if (rbo) glDeleteRenderbuffers(1, &rbo);
        if (shaderProgram) glDeleteProgram(shaderProgram);
    }

    // ----- MDI path -----
    MeshHandle AddStaticMesh(const TArray<Vertex>& verts, const TArray<uint32>& indices) {
        MeshHandle h{};
        h.baseVertex = static_cast<int32_t>(m_VertCount);
        h.firstIndex = static_cast<uint32>(m_IndexCount);
        h.indexCount = static_cast<uint32>(indices.Num());

        MeshAssetEntry entry;
        entry.Name   = std::to_string(m_MeshAssets.Num()).c_str();
        entry.Handle = h;
        m_MeshAssets.Add(entry);

        const GLsizeiptr vBytes = (GLsizeiptr)(verts.Num()   * sizeof(Vertex));
        const GLsizeiptr iBytes = (GLsizeiptr)(indices.Num() * sizeof(uint32));

        if ((m_VertCount + verts.Num()) * sizeof(Vertex) > StaticVBSize ||
            (m_IndexCount + indices.Num()) * sizeof(uint32) > StaticIBSize)
        {
            std::cerr << "Static buffer overflow. Increase StaticVBSize/StaticIBSize.\n";
            return h;
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, m_VertCount * sizeof(Vertex), vBytes, verts.Data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, m_IndexCount * sizeof(uint32), iBytes, indices.Data());

        m_VertCount  += (uint32)verts.Num();
        m_IndexCount += (uint32)indices.Num();
        return h;
    }

    //MeshHandle AddStaticMesh(const Mesh& mesh) { return AddStaticMesh(mesh.GetVertices(), mesh.GetIndices()); }

    void BeginFrame() {
        m_PendingDraws.Clear();
        m_PendingDraws.Reserve(4096);
        m_FrameBones.clear();
        m_FrameBones.emplace_back(1.0f); // index 0: identity for static meshes
    }

    void SetFrameBones(const std::vector<Mat4>& bones)
    {
        m_FrameBones = bones;
        if (m_FrameBones.empty())
            m_FrameBones.emplace_back(1.0f);
    }

    // NEW: per-draw material id
    // NEW: per-draw material id + object id
    void SubmitDraw(const MeshHandle& h, const Mat4& model, uint32 materialId, uint32 objectId, uint32 boneBase = 0) {
        if (m_PendingDraws.Num() >= PerFrameDrawCap) return;
        PendingDraw d;
        d.mesh       = h;
        d.model      = model;
        d.materialId = materialId;
        d.objectId   = objectId;
        d.boneBase   = boneBase;
        m_PendingDraws.Add(d);
    }


    // materials: array indexed by materialId (matches what you pass to SubmitDraw)
    void EndFrameAndDraw(uint32 shaderProgram, const TArray<Material>& materials) {
        if (m_PendingDraws.IsEmpty()) return;

        const size_t total = (size_t)m_PendingDraws.Num();
        PendingDraw* data  = m_PendingDraws.Data();

        // sort draws by material so we can batch per material
        std::sort(data, data + total, [](const PendingDraw& a, const PendingDraw& b) {
            return a.materialId < b.materialId;
        });

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);

        // Upload frame bone palette once (binding=2)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BoneSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     m_FrameBones.size() * sizeof(Mat4),
                     nullptr,
                     GL_DYNAMIC_DRAW);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        0,
                        m_FrameBones.size() * sizeof(Mat4),
                        m_FrameBones.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_BoneSSBO);

        const GLint uAlbedoTexLoc = glGetUniformLocation(shaderProgram, "u_AlbedoTex");
        const GLint uAlbedoColLoc = glGetUniformLocation(shaderProgram, "u_AlbedoColor");

        size_t i = 0;
        while (i < total)
        {
            const uint32 matId = data[i].materialId;
            if (matId >= (uint32)materials.Num()) {
                // invalid material index, skip this batch
                size_t j = i + 1;
                while (j < total && data[j].materialId == matId) ++j;
                i = j;
                continue;
            }

            const Material& mat = materials[matId];

            // bind material
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mat.AlbedoTex);
            glUniform1i(uAlbedoTexLoc, 0);
            glUniform3fv(uAlbedoColLoc, 1, &mat.AlbedoColor[0]);

            // gather all draws that share this material
            m_Draws.Clear();
            m_ModelMats.Clear();
            m_BoneBases.Clear();

            size_t j = i;
            while (j < total && data[j].materialId == matId)
            {
                const PendingDraw& d = data[j];

                if (m_Draws.Num() >= PerFrameDrawCap)
                    break;

                DrawElementsIndirectCommand cmd{};
                cmd.count         = d.mesh.indexCount;
                cmd.instanceCount = 1;
                cmd.firstIndex    = d.mesh.firstIndex;
                cmd.baseVertex    = (uint32)d.mesh.baseVertex;
                cmd.baseInstance  = (uint32)m_ModelMats.Num();

                m_Draws.Add(cmd);
                m_ModelMats.Add(d.model);
                m_BoneBases.Add(d.boneBase);

                ++j;
            }

            // upload model matrices
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ModelSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         m_ModelMats.Num() * sizeof(Mat4),
                         nullptr,
                         GL_DYNAMIC_DRAW); // orphan
            glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                            0,
                            m_ModelMats.Num() * sizeof(Mat4),
                            m_ModelMats.Data());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ModelSSBO);

            // upload bone base per draw (binding=3), aligned with DrawID in this batch
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BoneBaseSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         m_BoneBases.Num() * sizeof(uint32),
                         nullptr,
                         GL_DYNAMIC_DRAW);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                            0,
                            m_BoneBases.Num() * sizeof(uint32),
                            m_BoneBases.Data());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BoneBaseSSBO);

            // upload indirect commands
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_IndirectBuffer);
            glBufferData(GL_DRAW_INDIRECT_BUFFER,
                         m_Draws.Num() * sizeof(DrawElementsIndirectCommand),
                         nullptr,
                         GL_DYNAMIC_DRAW); // orphan
            glBufferSubData(GL_DRAW_INDIRECT_BUFFER,
                            0,
                            m_Draws.Num() * sizeof(DrawElementsIndirectCommand),
                            m_Draws.Data());

            glMultiDrawElementsIndirect(GL_TRIANGLES,
                                        GL_UNSIGNED_INT,
                                        nullptr,
                                        (GLsizei)m_Draws.Num(),
                                        0);

            i = j;
        }

        glBindVertexArray(0);
    }

    // Draw an object-id buffer for mouse picking.
    // Requirements:
    //  - Caller must bind the desired FBO (with an integer color attachment) before calling.
    //  - Caller must bind/use pickShaderProgram (expects u_ViewProj, and SSBO bindings 0 (Models) and 1 (ObjectIDs)).
    void DrawPicking(uint32 pickShaderProgram)
    {
        (void)pickShaderProgram; // call-site clarity
        if (m_PendingDraws.IsEmpty()) return;
    
        const size_t total = (size_t)m_PendingDraws.Num();
        PendingDraw* data  = m_PendingDraws.Data();
    
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
    
        m_Draws.Clear();
        m_ModelMats.Clear();
        m_ObjectIDs.Clear();
    
        // Build one big MDI list. No material sorting needed for picking.
        for (size_t i = 0; i < total; ++i)
        {
            const PendingDraw& d = data[i];
            if (m_Draws.Num() >= PerFrameDrawCap)
                break;
    
            DrawElementsIndirectCommand cmd{};
            cmd.count         = d.mesh.indexCount;
            cmd.instanceCount = 1;
            cmd.firstIndex    = d.mesh.firstIndex;
            cmd.baseVertex    = (uint32)d.mesh.baseVertex;
            cmd.baseInstance  = (uint32)m_ModelMats.Num(); // still using DrawID indexing
    
            m_Draws.Add(cmd);
            m_ModelMats.Add(d.model);
            m_ObjectIDs.Add(d.objectId);
        }
    
        // upload model matrices (binding=0)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ModelSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_ModelMats.Num() * sizeof(Mat4), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_ModelMats.Num() * sizeof(Mat4), m_ModelMats.Data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ModelSSBO);
    
        // upload object ids (binding=1)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ObjectIdSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_ObjectIDs.Num() * sizeof(uint32), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_ObjectIDs.Num() * sizeof(uint32), m_ObjectIDs.Data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_ObjectIdSSBO);
    
        // upload indirect commands
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_IndirectBuffer);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, m_Draws.Num() * sizeof(DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, m_Draws.Num() * sizeof(DrawElementsIndirectCommand), m_Draws.Data());
    
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, (GLsizei)m_Draws.Num(), 0);
    
        glBindVertexArray(0);
    }


    struct MeshAssetEntry
    {
        String    Name;
        MeshHandle Handle;
    };
    TArray<MeshAssetEntry> m_MeshAssets;

private:
    struct PendingDraw {
        MeshHandle mesh;
        Mat4  model;
        uint32 materialId;
        uint32 objectId;
        uint32 boneBase = 0;
    };

    
    static constexpr GLsizeiptr StaticVBSize    = 128 * 1024 * 1024; // 128 MB vertices
    static constexpr GLsizeiptr StaticIBSize    = 64  * 1024 * 1024; // 64 MB indices
    static constexpr size_t     PerFrameDrawCap = 65536;

    uint32 m_VAO = 0, m_VBO = 0, m_IBO = 0;
    uint32 m_ModelSSBO = 0, m_IndirectBuffer = 0;
    uint32 m_BoneSSBO = 0, m_BoneBaseSSBO = 0;

    uint32 m_VertCount  = 0;
    uint32 m_IndexCount = 0;

    // reused as temporary per-material buffers
    TArray<DrawElementsIndirectCommand> m_Draws;
    TArray<Mat4>                   m_ModelMats;
    TArray<uint32>                 m_BoneBases;
    std::vector<Mat4>              m_FrameBones;

    // NEW: all pending draws for this frame (with material info)
    TArray<PendingDraw>                 m_PendingDraws;

    uint32  m_ObjectIdSSBO = 0;
    TArray<uint32> m_ObjectIDs;

    

private:
    static void CheckShader(GLuint obj, Bool program) {
        GLint ok = 0;
        if (program) glGetProgramiv(obj, GL_LINK_STATUS, &ok);
        else         glGetShaderiv(obj,  GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            if (program) glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
            else         glGetShaderiv(obj,  GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            if (program) glGetProgramInfoLog(obj, len, nullptr, log.data());
            else         glGetShaderInfoLog (obj, len, nullptr, log.data());
            std::cerr << log << "\n";
        }
    }

    static void APIENTRY GLDebugCallback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* message, const void*) {
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
        std::cerr << "GL: " << message << "\n";
    }
    void EnableDebugOutput() {
        if (glDebugMessageCallback) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(GLDebugCallback, nullptr);
        }
    }
    
};
