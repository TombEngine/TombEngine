#pragma once

// 2D layer data that the legacy renderer resolves and the next renderer draws.

namespace TEN::Renderer::Structures
{
    struct RendererSprite;
}

namespace TEN::Renderer::Next
{
    // Resolved string of the legacy 2D layer: legacy screen position (interpolated), premultiplied color and scale.
    struct OverlayText
    {
        std::string Text     = {};
        Vector2     Position = Vector2::Zero;
        Vector4     Color    = Vector4::Zero;
        float       Scale    = 1.0f;
    };

    // Status bar of the legacy HUD, in the 800x600 display space with its vertical layout position.
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
    };
}
