#pragma once

#include "BaseModule.h"
#include "Mesh.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "OpenGLRenderAPI.h"
#include "RenderAPI.h"

class Camera;

class RenderModule : public IModule
{
    REFLECTABLE_CLASS(RenderModule, IModule)

public:
    struct DirectionalLightSettings
    {
        Vector3 Direction = Vector3(0.0f, -1.0f, 0.0f);
        Vector3 Color = Vector3(1.0f, 1.0f, 1.0f);
        float Intensity = 0.0f;
        float SpecularIntensity = 0.f;
        float SpecularPower = 1.0f;

        // Shadow-ready placeholders for future integration.
        bool CastShadows = false;
        float ShadowBias = 0.001f;
    };

    struct SkyAmbientSettings
    {
        Vector3 SkyColor = Vector3(1.f, 1.f, 1.f);
        Vector3 GroundColor = Vector3(0.25f, 0.25f, 0.25f);
        float Intensity = 1.f;
    };

    struct EnvironmentLightingSettings
    {
        // Future HDR/IBL extension points.
        bool UseEnvironmentMap = false;
        uint32 EnvironmentCubemap = 0;
        float DiffuseIBLIntensity = 1.0f;
        float SpecularIBLIntensity = 1.0f;
    };

    struct EditorGridSettings
    {
        bool Enabled = true;
        float GridExtent = 10.0f; // <= 0 means infinite extent
        float CellSpacing = 1.0f;
        float MajorLineSpacing = 1.0f;
        float FadeDistance = 200.0f;
        float MinorLineWidth = 1.0f;
        float MajorLineWidth = 1.00f;
        Vector4 MinorColor = Vector4(1.f, 1.f, 1.f, 1.0f);
        Vector4 MajorColor = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        Vector4 XAxisColor = Vector4(1.f, 0.f, 0.f, 1.f);
        Vector4 YAxisColor = Vector4(0.f, 1.f, 0.f, 1.f); 
    };

    RenderModule()
    {
        E_TickType = TickType::Render;
    }
    ~RenderModule() override{}

    // --- IModule interface ---
    void Init() override;
    void Tick(float dt) override;
    void OnEvent(const Event& e) override;
    void Shutdown() override;

    // --- Public API used by the rest of the engine ---

    // Texture that the editor/game viewport will display
    [[nodiscard]] GLuint GetViewportTexture() const { return m_ColorTexture; }

    // Viewport size control (e.g., editor panel resize)
    void SetViewportSize(uint32 width, uint32 height);
    [[nodiscard]] uint32 GetViewportWidth()  const { return m_ViewportWidth;  }
    [[nodiscard]] uint32 GetViewportHeight() const { return m_ViewportHeight; }

    // Material management
    MaterialHandle CreateMaterial(const String& albedoPath,
                                  const Vector3& tint = Vector3(1.0f));

    [[nodiscard]] const Material& GetMaterial(MaterialHandle h) const
    {
        return m_Materials[h.Id];
    }

    Material& GetMaterial(MaterialHandle h)
    {
        return m_Materials[h.Id];
    }

    // Global light direction (static because it affects all draws)
    static void SetLightDirection(const Vector3& dir) { s_LightDirection = dir; }
    static Vector3& GetLightDirection() { return s_LightDirection; }
    void SetDirectionalLight(const DirectionalLightSettings& settings) { m_DirectionalLight = settings; s_LightDirection = settings.Direction; }
    DirectionalLightSettings& GetDirectionalLight() { return m_DirectionalLight; }
    const DirectionalLightSettings& GetDirectionalLight() const { return m_DirectionalLight; }

    void SetSkyAmbient(const SkyAmbientSettings& settings) { m_SkyAmbient = settings; }
    SkyAmbientSettings& GetSkyAmbient() { return m_SkyAmbient; }
    const SkyAmbientSettings& GetSkyAmbient() const { return m_SkyAmbient; }

    void SetEnvironmentLighting(const EnvironmentLightingSettings& settings) { m_EnvironmentLighting = settings; }
    EnvironmentLightingSettings& GetEnvironmentLighting() { return m_EnvironmentLighting; }
    const EnvironmentLightingSettings& GetEnvironmentLighting() const { return m_EnvironmentLighting; }

    RenderAPI* GetRendererAPI() { return m_RendererAPI.Get(); }

    TArray<Material> GetMaterials() const { return m_Materials; }

    // --- Mouse picking ---
    // Coordinates are in viewport pixels (origin top-left, like ImGui).
    // Returns 0 if nothing is under the cursor.
    // If id > 0, the entt::entity is (id - 1).
    uint32 ReadPickID(uint32 viewportX, uint32 viewportY) const;

    // --- Editor Grid ---
    void SetEditorGridEnabled(bool bEnabled) { m_GridSettings.Enabled = bEnabled; }
    [[nodiscard]] bool IsEditorGridEnabled() const { return m_GridSettings.Enabled; }
    void SetEditorGridSettings(const EditorGridSettings& settings) { m_GridSettings = settings; }
    EditorGridSettings& GetEditorGridSettings() { return m_GridSettings; }
    const EditorGridSettings& GetEditorGridSettings() const { return m_GridSettings; }


private:
    // Internal helpers
    uint32 CreateWhiteTexture();

    // GL resources
    uint32 m_FBO          = 0;
    uint32 m_ColorTexture = 0;
    uint32 m_RBO          = 0;
    uint32 m_ShaderProgram = 0;

    // Viewport state
    uint32 m_ViewportWidth  = 1280;
    uint32 m_ViewportHeight = 720;
    uint32 Width = 1280;
    uint32 Height = 720;

    // Scene-independent render state
    Float  m_Rotation = 0.0f;

    // Uniform locations
    int32 m_uViewProjLoc     = -1;
    int32 m_uLightDirLoc     = -1;
    int32 m_uLightIntensityLoc = -1;
    int32 m_uSpecularIntensityLoc = -1;
    int32 m_uAlbedoLoc       = -1;
    int32 m_uCamPosLoc       = -1;
    int32 m_uSpecColorLoc    = -1;
    int32 m_uShininessLoc    = -1;
    int32 m_uMetallicLoc     = -1;
    int32 m_uRoughnessLoc    = -1;
    int32 m_uLightColorLoc   = -1;
    int32 m_uSkyColorLoc     = -1;
    int32 m_uGroundColorLoc  = -1;
    int32 m_uAmbientIntensityLoc = -1;

    // Mesh handles (internal to render module)
    MeshHandle* m_CubeHandle    = nullptr;
    MeshHandle* m_PyramidHandle = nullptr;
    MeshHandle* m_MyModel       = nullptr;
    MeshHandle* m_MyModel2      = nullptr;

    // Materials & textures
    TArray<Material> m_Materials;

    uint32 m_AlbedoTex  = 0;
    uint32 m_AlbedoTex2 = 0;
    uint32 m_WhiteTex   = 0; // 1x1 white

    // Renderer backend
    RUniquePtr<RenderAPI> m_RendererAPI;
    MaterialHandle m_MatMyModel2;
    MaterialHandle m_MatMyModel;

    void InitPickingTargets(uint32 width, uint32 height);

    // Picking (integer id buffer)
    uint32 m_PickFBO           = 0;
    uint32 m_PickTexture       = 0; // GL_R32UI
    uint32 m_PickShaderProgram = 0;
    int32  m_uPickViewProjLoc  = -1;
    

    // Physics debug line renderer
    uint32 m_DebugLineShader = 0;
    uint32 m_DebugLineVAO = 0;
    uint32 m_DebugLineVBO = 0;
    int32  m_uDebugLineViewProjLoc = -1;

    // Editor grid renderer
    uint32 m_GridShader = 0;
    uint32 m_GridVAO = 0;
    int32  m_uGridInvViewProjLoc = -1;
    int32  m_uGridViewProjLoc = -1;
    int32  m_uGridCameraPosLoc = -1;
    int32  m_uGridViewportSizeLoc = -1;
    int32  m_uGridExtentLoc = -1;
    int32  m_uGridCellSpacingLoc = -1;
    int32  m_uGridMajorSpacingLoc = -1;
    int32  m_uGridFadeDistanceLoc = -1;
    int32  m_uGridMinorWidthLoc = -1;
    int32  m_uGridMajorWidthLoc = -1;
    int32  m_uGridMinorColorLoc = -1;
    int32  m_uGridMajorColorLoc = -1;
    int32  m_uGridXAxisColorLoc = -1;
    int32  m_uGridZAxisColorLoc = -1;
    EditorGridSettings m_GridSettings;
    void DrawEditorGrid(const Mat4& viewProj, const Vector3& cameraPos);

    DirectionalLightSettings m_DirectionalLight;
    SkyAmbientSettings m_SkyAmbient;
    EnvironmentLightingSettings m_EnvironmentLighting;


    std::vector<Mat4> m_FrameBoneData;
    std::vector<uint32> m_FrameBoneBase;





    // Global light direction shared by all RenderModule instances
    static Vector3 s_LightDirection;
};

REFLECT_CLASS(RenderModule, IModule)
END_REFLECT_CLASS(RenderModule)
