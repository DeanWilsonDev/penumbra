#pragma once

namespace Penumbra {

struct Point {
    float X{0.0f};
    float Y{0.0f};
};

struct Rect {
    float X{0.0f};
    float Y{0.0f};
    float W{0.0f};
    float H{0.0f};
};

// A scale/rotate/translate transform, always applied around a shape's own centre
// (transform-origin is unconditionally centre -- no per-call override, keeping this
// a three-knob primitive rather than a general affine one). Lives alongside Point/Rect
// rather than under
// Widgets/ or Render/ because both layers need it: Render applies it when painting
// (Renderer::PushTransform/PopTransform) and hit-testing inverse-transforms the mouse
// point through it, neither of which is a Widgets-only concern. Identity by default:
// a Transform nobody touches draws and hit-tests exactly as if it didn't exist.
struct Transform {
    float ScaleX{1.0f};
    float ScaleY{1.0f};
    float TranslateXLogical{0.0f};
    float TranslateYLogical{0.0f};
    float RotationDegrees{0.0f};

    bool IsIdentity() const {
        return ScaleX == 1.0f && ScaleY == 1.0f && TranslateXLogical == 0.0f &&
               TranslateYLogical == 0.0f && RotationDegrees == 0.0f;
    }
};

} // namespace Penumbra
