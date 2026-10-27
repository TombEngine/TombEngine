#pragma once

#include "Renderer/RendererEnums.h"

// 2D layer data that the frontend resolves and the scene renderer draws. The frontend records one UiFrame per
// presented frame; SceneRenderer::FinishFrame draws it over the scene (SceneRendererOverlay.cpp).

struct ImDrawData;

namespace TEN::Renderer::Structures
{
    struct RendererMesh;
    struct RendererSprite;
}

namespace TEN::Renderer::Backend
{
    // Resolved string of the 2D layer: frontend screen position (interpolated), premultiplied color and scale.
    struct OverlayText
    {
        std::string Text     = {};
        Vector2     Position = Vector2::Zero;
        Vector4     Color    = Vector4::Zero;
        float       Scale    = 1.0f;
        Vector4     Scissor  = DISPLAY_SCISSOR_NONE; // Normalized screen rectangle the text is clipped to.
    };

    // Status bar of the HUD, in the 800x600 display space with its vertical layout position.
    struct OverlayBar
    {
        Vector2                           Position         = Vector2::Zero;
        Vector2                           VerticalPosition = Vector2::Zero;
        Vector2                           Size             = Vector2::Zero;
        float                             Percent          = 0.0f;
        const Structures::RendererSprite* Border           = nullptr;
        const Structures::RendererSprite* Inner            = nullptr;
        bool                              IsPoisoned       = false;
        int                               Frame            = 0;
        std::string                       BorderPath       = {}; // Image files used instead of the sprites, as by the loading bar.
        std::string                       InnerPath        = {};
    };

    // Display sprite of the 2D layer, with its corners transformed into the 800x600 display space.
    struct OverlaySprite
    {
        const Structures::RendererSprite* Sprite     = nullptr;
        Vector2                           Corners[4] = {};
        Vector4                           Color      = Vector4::Zero;
        BlendMode                         BlendMode  = BlendMode::AlphaBlend;
        Vector4                           Scissor    = DISPLAY_SCISSOR_NONE; // Normalized screen rectangle the sprite is clipped to.
    };

    // Image path of the background video frame, drawn full screen in exclusive video playback.
    constexpr auto OVERLAY_VIDEO_PATH = "<video>";

    // Image file drawn by the 2D layer (title logo, loading screen, intro image), in normalized screen space.
    struct OverlayImage
    {
        std::string Path      = {};
        Vector2     Min       = Vector2::Zero;
        Vector2     Max       = Vector2::One;
        Vector2     UVMin     = Vector2::Zero;
        Vector2     UVMax     = Vector2::One;
        Vector4     Color     = Vector4::One;
        BlendMode   BlendMode = BlendMode::Opaque;
    };

    // Moveable mesh of an overlay object; skin meshes use bind pose corrected bones from UiFrame::ObjectBones.
    struct OverlayObjectMesh
    {
        const Structures::RendererMesh* Mesh      = nullptr;
        Matrix                          World     = Matrix::Identity;
        int                             FirstBone = 0;
        int                             BoneCount = 0;
    };

    // Moveable drawn by the 2D layer with the Inventory shader: inventory, examine, pickups and display items.
    struct OverlayObject
    {
        Matrix       ViewProjection    = Matrix::Identity;
        Vector4      Color             = Vector4::One;
        Vector4      AmbientLight      = Vector4::One;
        unsigned int Frame             = 0;
        float        InterpolatedFrame = 0.0f;
        bool         IsAlphaTested     = false; // Alpha tested buckets keep their alpha test, as in the 2D space draws.
        int          FirstMesh         = 0;
        int          MeshCount         = 0;
        Vector4      Scissor           = DISPLAY_SCISSOR_NONE; // Normalized screen rectangle the object is clipped to.
    };

    // Element list of UiFrame a layer indexes; Objects index UiFrame::Objects.
    enum class OverlayLayerType
    {
        Texts,
        Bars,
        Sprites,
        Images,
        ScreenOverlays,
        Objects
    };

    // Run of consecutive overlay elements of one type, in the frontend draw order.
    struct OverlayLayer
    {
        OverlayLayerType Type  = OverlayLayerType::Texts;
        int              First = 0;
        int              Count = 0;
    };

    // 2D layer of one presented frame, recorded by the frontend in its draw order, for game and menu frames.
    struct UiFrame
    {
        int   ScreenWidth    = 0; // Frontend screen resolution of the string positions.
        int   ScreenHeight   = 0;
        float BackgroundFade = 1.0f; // Brightness of the last rendered scene behind the 2D layer, 0 for none.

        std::vector<OverlayText>   Texts   = {};
        std::vector<OverlayBar>    Bars    = {};
        std::vector<OverlaySprite> Sprites = {};
        std::vector<OverlayImage>  Images  = {};
        std::vector<OverlayLayer>  Layers  = {};

        std::vector<OverlayObject>     Objects      = {};
        std::vector<OverlayObjectMesh> ObjectMeshes = {};
        std::vector<Matrix>            ObjectBones  = {};
        float                          Gamma        = 1.0f; // Gamma of the overlay objects.

        // Menu frames get the glow of the object emissive and antialiasing over the whole image.
        bool IsMenu           = false;
        int  AntialiasingMode = 0;

        const ImDrawData* DebugDrawData = nullptr; // ImGui debug overlay rendered this frame, null when hidden.

        // Video frame of the video handler (BGRA8), null when no video plays.
        const unsigned char* VideoFrame  = nullptr;
        int                  VideoWidth  = 0;
        int                  VideoHeight = 0;

        // Screen overlays of the frontend DrawOverlays: lightning flash and binocular or laser sight sprite.
        Vector3                           FlashColor   = Vector3::Zero;
        const Structures::RendererSprite* OpticSprite  = nullptr;
        bool                              IsLaserSight = false;
    };
}
