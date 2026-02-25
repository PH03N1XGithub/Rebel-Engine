#include "EnginePch.h"
#include "RenderModule.h"

#include <filesystem>

#include "BaseEngine.h"
#include "Window.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <functional>

#include "Camera.h"
#include "MeshLoader.h"
#include "OpenGLRenderAPI.h"
// --- stb_image ---
#define STB_IMAGE_IMPLEMENTATION
#include "Components.h"
#include "AssetManager/AssetManagerModule.h"
#include "ThirdParty/stb_image.h"
#include "PhysicsDebugDraw.h"


DEFINE_LOG_CATEGORY(RenderLOG)

// MDI shaders (FBX-ready layout; still uses aColor for your demo)
static const char* kVertexShaderMDI = R"(
#version 430 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in vec3 aColor;

layout(location = 5) in uvec4 aBoneIndex;
layout(location = 6) in vec4  aBoneWeight;

layout(std430, binding = 0) readonly buffer Models   { mat4 u_Model[]; };
layout(std430, binding = 2) readonly buffer Bones    { mat4 u_Bones[]; };
layout(std430, binding = 3) readonly buffer BoneBase { uint u_BoneBase[]; };

uniform mat4 u_ViewProj;

out vec3 vNormalWS;
out vec2 vUV;
out vec3 vWorldPos;

void main()
{
    uint drawID = gl_DrawIDARB;
    mat4 model  = u_Model[drawID];
    uint base   = u_BoneBase[drawID];

    vec4 w = aBoneWeight;
    float sum = w.x + w.y + w.z + w.w;
    
    mat4 skin = mat4(1.0);
    
    if (sum > 1e-8)
    {
        w /= sum;
    
        skin =
            w.x * u_Bones[base + aBoneIndex.x] +
            w.y * u_Bones[base + aBoneIndex.y] +
            w.z * u_Bones[base + aBoneIndex.z] +
            w.w * u_Bones[base + aBoneIndex.w];
    }


    vec4 skinnedPos = skin * vec4(aPos, 1.0);
    vec4 worldPos   = model * skinnedPos;

    gl_Position = u_ViewProj * worldPos;

    mat3 skin3 = mat3(skin);
    vec3 skinnedNormal = normalize(skin3 * aNormal);
    mat3 normalMat = transpose(inverse(mat3(model)));
    vNormalWS = normalize(normalMat * skinnedNormal);
    vUV = aUV;
    vWorldPos = worldPos.xyz;
}
)";



static const char* kFragmentShader = R"(
#version 430 core

in vec3 vNormalWS;
in vec2 vUV;
in vec3 vWorldPos;
out vec4 FragColor;

uniform vec3      u_LightDir;
uniform vec3      u_LightColor;
uniform float     u_LightIntensity;
uniform float     u_SpecularIntensity;
uniform float     u_SpecularPower;
uniform vec3      u_CamPos;
uniform vec3      u_SkyColor;
uniform vec3      u_GroundColor;
uniform float     u_AmbientIntensity;
uniform sampler2D u_AlbedoTex;   // texture
uniform vec3      u_AlbedoColor; // optional tint

void main()
{
    vec3 N = normalize(vNormalWS);
    vec3 L = normalize(-u_LightDir);
    vec3 V = normalize(u_CamPos - vWorldPos);
    vec3 H = normalize(L + V);

    vec3 albedo = texture(u_AlbedoTex, vUV).rgb * u_AlbedoColor;

    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * u_LightColor * (NdotL * u_LightIntensity);

    float spec = 0.0;
    if (NdotL > 0.0)
    {
        spec = pow(max(dot(N, H), 0.0), max(u_SpecularPower, 1.0)) * u_SpecularIntensity;
    }
    vec3 specular = u_LightColor * spec;

    // Hemispheric ambient: up-facing normals receive more sky, down-facing get ground bounce.
    float hemi = N.y * 0.5 + 0.5;
    vec3 ambient = albedo * mix(u_GroundColor, u_SkyColor, hemi) * u_AmbientIntensity;

    vec3 lit = ambient + diffuse + specular;

    FragColor = vec4(lit, 1.0);
}
)";

// --- Picking shaders (writes an unsigned integer object id into an R32UI render target) ---
static const char* kVertexShaderPick = R"(
#version 430 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in vec3 aColor;

layout(std430, binding = 0) readonly buffer Models    { mat4 u_Model[]; };
layout(std430, binding = 1) readonly buffer ObjectIDs { uint u_ObjectID[]; };

uniform mat4 u_ViewProj;

flat out uint vObjectID;

void main()
{
    uint drawID = gl_DrawIDARB;
    mat4 model  = u_Model[drawID];

    gl_Position = u_ViewProj * (model * vec4(aPos, 1.0));
    vObjectID   = u_ObjectID[drawID];
}
)";

static const char* kFragmentShaderPick = R"(
#version 430 core

flat in uint vObjectID;
layout(location = 0) out uint OutObjectID;

void main()
{
    OutObjectID = vObjectID;
}
)";

static const char* kVertexShaderDebugLines = R"(
#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 u_ViewProj;
out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = u_ViewProj * vec4(aPos, 1.0);
}
)";

static const char* kFragmentShaderDebugLines = R"(
#version 430 core
in vec3 vColor;
out vec4 FragColor;
void main()
{
    FragColor = vec4(vColor, 1.0);
}
)";

static const char* kVertexShaderEditorGrid = R"(
#version 430 core
const vec2 kVerts[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

void main()
{
    gl_Position = vec4(kVerts[gl_VertexID], 0.0, 1.0);
}
)";

static const char* kFragmentShaderEditorGrid = R"(
#version 430 core

layout(location = 0) out vec4 FragColor;

uniform mat4 u_InvViewProj;
uniform mat4 u_ViewProj;
uniform vec3 u_CameraPos;
uniform vec2 u_ViewportSize;
uniform float u_GridExtent;
uniform float u_CellSpacing;
uniform float u_MajorSpacing;
uniform float u_FadeDistance;
uniform float u_MinorLineWidth;
uniform float u_MajorLineWidth;
uniform vec4 u_MinorColor;
uniform vec4 u_MajorColor;
uniform vec4 u_XAxisColor;
uniform vec4 u_ZAxisColor;

float GridLineFactor(vec2 coord, float thicknessPx)
{
    vec2 deriv = max(fwidth(coord), vec2(1e-6));
    vec2 a = abs(fract(coord - 0.5) - 0.5) / deriv;
    float line = min(a.x, a.y);
    return 1.0 - clamp(line / max(thicknessPx, 1.0), 0.0, 1.0);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / max(u_ViewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;

    vec4 nearH = u_InvViewProj * vec4(ndc, -1.0, 1.0);
    vec4 farH  = u_InvViewProj * vec4(ndc,  1.0, 1.0);
    vec3 nearP = nearH.xyz / nearH.w;
    vec3 farP  = farH.xyz / farH.w;
    vec3 rayDir = normalize(farP - nearP);

    if (abs(rayDir.z) < 1e-6)
        discard;

    float t = -nearP.z / rayDir.z;
    if (t < 0.0)
        discard;

    vec3 worldPos = nearP + rayDir * t;

    if (u_GridExtent > 0.0 && (abs(worldPos.x) > u_GridExtent || abs(worldPos.y) > u_GridExtent))
        discard;

    vec4 clipPos = u_ViewProj * vec4(worldPos, 1.0);
    if (clipPos.w <= 0.0)
        discard;
    float ndcDepth = clipPos.z / clipPos.w;
    if (ndcDepth < -1.0 || ndcDepth > 1.0)
        discard;

    float minor = GridLineFactor(worldPos.xy / max(u_CellSpacing, 1e-4), u_MinorLineWidth);
    float major = GridLineFactor(worldPos.xy / max(u_MajorSpacing, 1e-4), u_MajorLineWidth);

    float axisX = 1.0 - clamp(abs(worldPos.y) / (fwidth(worldPos.y) * 2.25), 0.0, 1.0); // X axis line (y=0)
    float axisZ = 1.0 - clamp(abs(worldPos.x) / (fwidth(worldPos.x) * 2.25), 0.0, 1.0); // Y axis line (x=0)

    float dist = length(worldPos.xy - u_CameraPos.xy);
    float fade = 1.0 - smoothstep(u_FadeDistance * 0.35, u_FadeDistance, dist);

    vec3 color = mix(u_MinorColor.rgb, u_MajorColor.rgb, major);
    float alpha = max(minor * u_MinorColor.a, major * u_MajorColor.a);

    color = mix(color, u_XAxisColor.rgb, axisX);
    alpha = max(alpha, axisX * u_XAxisColor.a);

    color = mix(color, u_ZAxisColor.rgb, axisZ);
    alpha = max(alpha, axisZ * u_ZAxisColor.a);

    alpha *= fade;
    if (alpha <= 1e-3)
        discard;

    gl_FragDepth = ndcDepth * 0.5 + 0.5;
    FragColor = vec4(color, alpha);
}
)";


static void RenderSkeletalDebug(const SkeletonAsset* skeleton, const Mat4& model, const TArray<Mat4>& globalPose)
{
    if (!skeleton)
        return;

    const int32 boneCount = static_cast<int32>(skeleton->m_InvBind.Num());
    if (boneCount == 0 || skeleton->m_Parent.Num() != boneCount || globalPose.Num() != boneCount)
        return;

    const Vector3 yellow(1.0f, 1.0f, 0.0f);

    for (int32 i = 0; i < boneCount; ++i)
    {
        const int32 parent = skeleton->m_Parent[i];
        if (parent < 0 || parent >= boneCount)
            continue;

        const Vector3 parentLocalPos = Vector3(globalPose[parent][3]);
        const Vector3 childLocalPos  = Vector3(globalPose[i][3]);

        const Vector3 parentPos = Vector3(model * Vector4(parentLocalPos, 1.0f));
        const Vector3 childPos  = Vector3(model * Vector4(childLocalPos, 1.0f));

        PhysicsDebug::DrawLine(parentPos, childPos, yellow);
    }
}

void RenderModule::InitPickingTargets(uint32 width, uint32 height)
{
    if (m_PickFBO) { glDeleteFramebuffers(1, &m_PickFBO); m_PickFBO = 0; }
    if (m_PickTexture) { glDeleteTextures(1, &m_PickTexture); m_PickTexture = 0; }

    glGenFramebuffers(1, &m_PickFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_PickFBO);

    glGenTextures(1, &m_PickTexture);
    glBindTexture(GL_TEXTURE_2D, m_PickTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, (GLsizei)width, (GLsizei)height, 0,
                 GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_PickTexture, 0);

    // reuse main depth-stencil renderbuffer
    if (m_RBO)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Pick FBO not complete!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

uint32 RenderModule::ReadPickID(uint32 viewportX, uint32 viewportY) const
{
    if (!m_PickFBO || viewportX >= Width || viewportY >= Height)
        return 0;

    uint32 id = 0;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_PickFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // OpenGL origin is bottom-left; UI coords usually top-left
    const GLint x = (GLint)viewportX;
    const GLint y = (GLint)(/*(Height - 1u) - */viewportY);

    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    return id;
}



// --------------------------------------------------------------------------------------------------


static uint32 LoadTexture2D(const std::string& path)
{
    int32 w, h, channels;
    stbi_set_flip_vertically_on_load(1); // Blender exports usually need flipping
    int8* data = reinterpret_cast<int8*>(stbi_load(path.c_str(), &w, &h, &channels, 4)); // force RGBA

    if (!data) {
        std::cerr << "Failed to load texture: " << path
                  << " Error: " << stbi_failure_reason() << "\n";
        return 0;
    }

    uint32 texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return texID;
}

uint32_t RenderModule::CreateWhiteTexture()
{
    uint32 tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    uint32 pixel = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

void RenderModule::Init() {
    PROFILE_SCOPE("Render MOdule Ä°nit")
    m_RendererAPI = RMakeUnique<OpenGLRenderAPI>();

    Window* window = GEngine->GetWindow();
    m_ViewportWidth  = window->GetWidth();
    m_ViewportHeight = window->GetHeight();

    m_RendererAPI->InitFBO(m_ViewportWidth, m_ViewportHeight, m_FBO, m_ColorTexture, m_RBO);
    InitPickingTargets(m_ViewportWidth, m_ViewportHeight);
    m_RendererAPI->EnableDepthTest(true);
    m_RendererAPI->EnableBackfaceCull(false);

    m_ShaderProgram = m_RendererAPI->CreateShader(kVertexShaderMDI, kFragmentShader);
    m_PickShaderProgram = m_RendererAPI->CreateShader(kVertexShaderPick, kFragmentShaderPick);
    m_uPickViewProjLoc = m_RendererAPI->GetUniformLocation(m_PickShaderProgram, "u_ViewProj");

    m_uViewProjLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_ViewProj");
    m_uLightDirLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_LightDir");
    m_uLightColorLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_LightColor");
    m_uLightIntensityLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_LightIntensity");
    m_uSpecularIntensityLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_SpecularIntensity");
    m_uShininessLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_SpecularPower");
    m_uCamPosLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_CamPos");
    m_uSkyColorLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_SkyColor");
    m_uGroundColorLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_GroundColor");
    m_uAmbientIntensityLoc = m_RendererAPI->GetUniformLocation(m_ShaderProgram, "u_AmbientIntensity");
    



    // --- Materials ---
    m_WhiteTex = CreateWhiteTexture();
    Material defaultMat;
    defaultMat.AlbedoTex   = m_WhiteTex;
    defaultMat.AlbedoColor = Vector3(1.0f); // pure white
    m_Materials.Add(defaultMat);

    // Material for myModel.fbx
    m_MatMyModel = CreateMaterial("assets/models/myModel_albedo.png",
                                  glm::vec3(1.0f, 1.0f, 1.0f)); // bone-ish white

    // Material for myModel.obj (use same or a different albedo)
    m_MatMyModel2 = CreateMaterial("assets/models/smyModel_albedo - Kopya2.png",
                                   Vector3(1.0f, 1.0f, 1.0f));
    
    

    
    
    
    // --- Debug line pipeline (physics / gizmos) ---
    m_DebugLineShader = m_RendererAPI->CreateShader(kVertexShaderDebugLines, kFragmentShaderDebugLines);
    m_uDebugLineViewProjLoc = m_RendererAPI->GetUniformLocation(m_DebugLineShader, "u_ViewProj");

    glGenVertexArrays(1, &m_DebugLineVAO);
    glGenBuffers(1, &m_DebugLineVBO);

    glBindVertexArray(m_DebugLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_DebugLineVBO);

    // dynamic buffer (we upload every frame)
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PhysicsDebug::DebugLineVertex), (void*)offsetof(PhysicsDebug::DebugLineVertex, Pos));
    // Color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PhysicsDebug::DebugLineVertex), (void*)offsetof(PhysicsDebug::DebugLineVertex, Color));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // --- Editor grid pipeline ---
    m_GridShader = m_RendererAPI->CreateShader(kVertexShaderEditorGrid, kFragmentShaderEditorGrid);
    m_uGridInvViewProjLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_InvViewProj");
    m_uGridViewProjLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_ViewProj");
    m_uGridCameraPosLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_CameraPos");
    m_uGridViewportSizeLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_ViewportSize");
    m_uGridExtentLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_GridExtent");
    m_uGridCellSpacingLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_CellSpacing");
    m_uGridMajorSpacingLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_MajorSpacing");
    m_uGridFadeDistanceLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_FadeDistance");
    m_uGridMinorWidthLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_MinorLineWidth");
    m_uGridMajorWidthLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_MajorLineWidth");
    m_uGridMinorColorLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_MinorColor");
    m_uGridMajorColorLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_MajorColor");
    m_uGridXAxisColorLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_XAxisColor");
    m_uGridZAxisColorLoc = m_RendererAPI->GetUniformLocation(m_GridShader, "u_ZAxisColor");

    glGenVertexArrays(1, &m_GridVAO);
    glBindVertexArray(m_GridVAO);
    glBindVertexArray(0);



}

MaterialHandle RenderModule::CreateMaterial(const String& albedoPath,
                                            const Vector3& tint)
{
    Material mat;
    mat.AlbedoTex   = LoadTexture2D(albedoPath.c_str());
    if (!mat.AlbedoTex)
        mat.AlbedoTex = m_WhiteTex;
    mat.AlbedoColor = tint;

    MaterialHandle h;
    h.Id = static_cast<uint32_t>(m_Materials.Num());
    m_Materials.Add(mat);
    return h;
}



Vector3 RenderModule::s_LightDirection = Vector3(0.0f, -1.0f, 0.0f);

void RenderModule::DrawEditorGrid(const Mat4& viewProj, const Vector3& cameraPos)
{
    if (!m_GridShader || !m_GridVAO || !m_GridSettings.Enabled)
        return;

    EditorGridSettings settings = m_GridSettings;
    settings.CellSpacing = FMath::max(settings.CellSpacing, 0.01f);
    settings.MajorLineSpacing = FMath::max(settings.MajorLineSpacing, settings.CellSpacing);
    settings.FadeDistance = FMath::max(settings.FadeDistance, 1.0f);
    settings.GridExtent = FMath::max(settings.GridExtent, 0.0f);

    const Mat4 invViewProj = FMath::inverse(viewProj);

    const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    GLint previousDepthMask = GL_TRUE;
    GLint previousDepthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glUseProgram(m_GridShader);
    glUniformMatrix4fv(m_uGridInvViewProjLoc, 1, GL_FALSE, &invViewProj[0][0]);
    glUniformMatrix4fv(m_uGridViewProjLoc, 1, GL_FALSE, &viewProj[0][0]);
    glUniform3fv(m_uGridCameraPosLoc, 1, &cameraPos[0]);
    glUniform2f(m_uGridViewportSizeLoc, (float)Width, (float)Height);
    glUniform1f(m_uGridExtentLoc, settings.GridExtent);
    glUniform1f(m_uGridCellSpacingLoc, settings.CellSpacing);
    glUniform1f(m_uGridMajorSpacingLoc, settings.MajorLineSpacing);
    glUniform1f(m_uGridFadeDistanceLoc, settings.FadeDistance);
    glUniform1f(m_uGridMinorWidthLoc, settings.MinorLineWidth);
    glUniform1f(m_uGridMajorWidthLoc, settings.MajorLineWidth);
    glUniform4fv(m_uGridMinorColorLoc, 1, &settings.MinorColor[0]);
    glUniform4fv(m_uGridMajorColorLoc, 1, &settings.MajorColor[0]);
    glUniform4fv(m_uGridXAxisColorLoc, 1, &settings.XAxisColor[0]);
    glUniform4fv(m_uGridZAxisColorLoc, 1, &settings.YAxisColor[0]);

    glBindVertexArray(m_GridVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(previousDepthMask == GL_TRUE ? GL_TRUE : GL_FALSE);
    glDepthFunc(previousDepthFunc);
    if (!wasDepthTestEnabled) glDisable(GL_DEPTH_TEST);
    if (!wasCullEnabled) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
    if (!wasBlendEnabled) glDisable(GL_BLEND);
}

void RenderModule::Tick(float dt) {
    if (Width == 0 || Height == 0) return;

    if (Width != m_ViewportWidth || Height != m_ViewportHeight) {
        m_ViewportWidth  = Width;
        m_ViewportHeight = Height;
        m_RendererAPI->InitFBO(Width, Height, m_FBO, m_ColorTexture, m_RBO);
        InitPickingTargets(Width, Height);
    }

    m_RendererAPI->BindFBO(m_FBO);
    m_RendererAPI->SetViewport(0, 0, Width, Height);
    m_RendererAPI->Clear(0.1f, 0.1f, 0.1f, 1.0f);

    CameraView camera = GEngine->GetActiveCamera(float(Width) / Height);
    Mat4 proj = camera.Projection;
    proj[1][1] *= -1.0f; // keep if you need GL/VK clip fix
    Mat4 view = camera.View;
    Mat4 viewProj = proj * view;

    m_RendererAPI->UseShader(m_ShaderProgram);
    auto* ogl = static_cast<OpenGLRenderAPI*>(m_RendererAPI.Get());
    ogl->SetUniformMat4Cached(m_ShaderProgram, m_uViewProjLoc, &viewProj[0][0]);

    // Keep legacy static light direction API in sync with configurable light settings.
    m_DirectionalLight.Direction = s_LightDirection;
    Vector3 lightDir = FMath::normalize(m_DirectionalLight.Direction);
    glUniform3fv(m_uLightDirLoc, 1, &lightDir[0]);
    glUniform3fv(m_uLightColorLoc, 1, &m_DirectionalLight.Color[0]);
    glUniform1f(m_uLightIntensityLoc, FMath::max(m_DirectionalLight.Intensity, 0.0f));
    glUniform1f(m_uSpecularIntensityLoc, FMath::max(m_DirectionalLight.SpecularIntensity, 0.0f));
    glUniform1f(m_uShininessLoc, FMath::max(m_DirectionalLight.SpecularPower, 1.0f));
    glUniform3fv(m_uCamPosLoc, 1, &camera.Position[0]);
    glUniform3fv(m_uSkyColorLoc, 1, &m_SkyAmbient.SkyColor[0]);
    glUniform3fv(m_uGroundColorLoc, 1, &m_SkyAmbient.GroundColor[0]);
    glUniform1f(m_uAmbientIntensityLoc, FMath::max(m_SkyAmbient.Intensity, 0.0f));

    m_Rotation += dt;

    {
        //PROFILE_SCOPE("BeginRenderFrame")
        ogl->BeginFrame();

        // index 0 is reserved identity bone for non-skinned draws
        m_FrameBoneData.clear();
        m_FrameBoneData.emplace_back(1.0f);
        m_FrameBoneBase.clear();
    }

    {
        //PROFILE_SCOPE("RenderLoop")
        

        auto& reg = GEngine->GetActiveScene()->GetRegistry();
        auto view = reg.view<StaticMeshComponent*>();
       
        auto skelView = reg.view<SkeletalMeshComponent*>();
        AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
        if (!assetModule)
            return;
        AssetManager& assetManager = assetModule->GetManager();

        for (auto e : skelView)
        {
            auto* skComp = skelView.get<SkeletalMeshComponent*>(e);

            if (!skComp->bIsVisible || !skComp->IsValid())
                continue;

            // Load skeletal mesh asset
            SkeletalMeshAsset* skAsset =
                dynamic_cast<SkeletalMeshAsset*>(assetManager.Load(skComp->Mesh.GetHandle()));

            if (!skAsset)
                continue;

            // Ensure GPU mesh uploaded
            if (!skAsset->Handle.isValid())
            {
                skAsset->Handle = ogl->AddStaticMesh(skAsset->Vertices, skAsset->Indices);
            }

            Mat4 model = skComp->GetWorldTransform();

            const uint32 objectId =
                (uint32)skComp->Owner->GetHandle() + 1u;

            if (skComp->bDrawSkeleton)
            {
                SkeletonAsset* skeleton =
                    dynamic_cast<SkeletonAsset*>(assetManager.Load(skAsset->m_Skeleton.GetHandle()));
                if (skeleton && !skComp->GlobalPose.IsEmpty())
                    RenderSkeletalDebug(skeleton, model, skComp->GlobalPose);
            }

            if (skComp->FinalPalette.IsEmpty())
                continue;

            const uint32 boneBase = static_cast<uint32>(m_FrameBoneData.size());
            for (int32 i = 0; i < skComp->FinalPalette.Num(); ++i)
                m_FrameBoneData.push_back(skComp->FinalPalette[i]);
            m_FrameBoneBase.push_back(boneBase);

            // Submit draw
            ogl->SubmitDraw(skAsset->Handle, model, skComp->Material.Id, objectId, boneBase);
        }


        for (auto e : view)
        {
            auto& mc = view.get<StaticMeshComponent*>(e);

            if (!mc->bIsVisible || !mc->IsValid())
                continue;

            
            Mat4 model = mc->GetWorldTransform();
            const uint32 objectId = (uint32)mc->Owner->GetHandle() + 1u;


            if ((uint64)mc->Mesh.GetHandle() == 0)
            {
               continue;
            }
            MeshAsset* mesh;
            mesh = dynamic_cast<MeshAsset*>(assetManager.Load(mc->Mesh.GetHandle()));
            if (mesh)
            {
                auto handle = mesh->Handle;
                if (!handle.isValid())
                {
                    mesh->Handle = ogl->AddStaticMesh(mesh->Vertices, mesh->Indices);
                }
            }

            if(!mesh)
            {
                continue;
            }
            ogl->SubmitDraw(mesh->Handle, model, mc->Material.Id, objectId, 0);
        }

    }
    {

        glClearColor(0.45f, 0.65f, 0.95f, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //PROFILE_SCOPE("End and Draw")

        // Pass all materials and frame bone palette to the renderer
        ogl->SetFrameBones(m_FrameBoneData);
        ogl->EndFrameAndDraw(m_ShaderProgram, m_Materials);

        // --- Editor-only ground grid pass ---
        if (GEngine->GetMode() == EngineMode::Editor)
            DrawEditorGrid(viewProj, camera.Position);

        // --- Physics debug lines pass ---
        {
            const auto& verts = PhysicsDebug::GetLineVertices();
            if (!verts.empty())
            {
                // Separate skeleton (yellow) lines to draw without depth; keep physics lines depth-tested.
                std::vector<PhysicsDebug::DebugLineVertex> xray;
                std::vector<PhysicsDebug::DebugLineVertex> normal;
                xray.reserve(verts.size());
                normal.reserve(verts.size());

                auto isYellow = [](const Vector3& c)
                {
                    const float eps = 1e-3f;
                    return std::fabs(c.x - 1.0f) < eps &&
                           std::fabs(c.y - 1.0f) < eps &&
                           std::fabs(c.z - 0.0f) < eps;
                };

                for (const auto& v : verts)
                {
                    (isYellow(v.Color) ? xray : normal).push_back(v);
                }

                auto drawList = [&](const std::vector<PhysicsDebug::DebugLineVertex>& list, bool disableDepth)
                {
                    if (list.empty()) return;
                    if (disableDepth) glDisable(GL_DEPTH_TEST);
                    glUseProgram(m_DebugLineShader);
                    glUniformMatrix4fv(m_uDebugLineViewProjLoc, 1, GL_FALSE, &viewProj[0][0]);

                    glBindVertexArray(m_DebugLineVAO);
                    glBindBuffer(GL_ARRAY_BUFFER, m_DebugLineVBO);
                    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(list.size() * sizeof(PhysicsDebug::DebugLineVertex)),
                                 list.data(), GL_DYNAMIC_DRAW);

                    glLineWidth(2.0f);
                    glDrawArrays(GL_LINES, 0, (GLsizei)list.size());

                    glBindVertexArray(0);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    if (disableDepth) glEnable(GL_DEPTH_TEST);
                };

                drawList(xray, true);    // skeleton debug (yellow) always visible
                drawList(normal, false); // physics debug respects depth
            }
        }

        // clear after rendering so next frame starts clean
        PhysicsDebug::Clear();


        

        // Picking pass (writes object ids into m_PickTexture)
        glBindFramebuffer(GL_FRAMEBUFFER, m_PickFBO);
        glViewport(0, 0, (GLsizei)Width, (GLsizei)Height);

        const GLuint clearID = 0;
        glClearBufferuiv(GL_COLOR, 0, &clearID);
        glClear(GL_DEPTH_BUFFER_BIT);

        m_RendererAPI->UseShader(m_PickShaderProgram);
        ogl->SetUniformMat4Cached(m_PickShaderProgram, m_uPickViewProjLoc, &viewProj[0][0]);
        ogl->DrawPicking(m_PickShaderProgram);

        // restore for consistency
        m_RendererAPI->BindFBO(m_FBO);

    }

    m_RendererAPI->BindFBO(0);
    ogl->BlitToDefault(m_FBO, m_ViewportWidth, m_ViewportHeight);
}

void RenderModule::OnEvent(const Event& e) { IModule::OnEvent(e); }

void RenderModule::Shutdown() {
    if (m_RendererAPI)
        m_RendererAPI->Shutdown(m_FBO, m_ColorTexture, m_RBO, m_ShaderProgram);
    if (m_PickShaderProgram) { glDeleteProgram(m_PickShaderProgram); m_PickShaderProgram = 0; }
    if (m_PickTexture) { glDeleteTextures(1, &m_PickTexture); m_PickTexture = 0; }
    if (m_PickFBO) { glDeleteFramebuffers(1, &m_PickFBO); m_PickFBO = 0; }

    if (m_DebugLineShader) { glDeleteProgram(m_DebugLineShader); m_DebugLineShader = 0; }
    if (m_DebugLineVBO) { glDeleteBuffers(1, &m_DebugLineVBO); m_DebugLineVBO = 0; }
    if (m_DebugLineVAO) { glDeleteVertexArrays(1, &m_DebugLineVAO); m_DebugLineVAO = 0; }
    if (m_GridShader) { glDeleteProgram(m_GridShader); m_GridShader = 0; }
    if (m_GridVAO) { glDeleteVertexArrays(1, &m_GridVAO); m_GridVAO = 0; }


    delete m_CubeHandle;
    delete m_PyramidHandle;
    delete m_MyModel;
    delete m_MyModel2;
}

void RenderModule::SetViewportSize(uint32 width, uint32 height)
{
    Width = width;
    Height = height;
}


