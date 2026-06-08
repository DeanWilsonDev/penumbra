#include "Penumbra/Widgets/Checkbox.h"

namespace Penumbra::Widgets {

namespace {
bool PointInRect(SDL_FPoint Point, SDL_FRect Rect) {
    return Point.x >= Rect.x && Point.x < Rect.x + Rect.w &&
           Point.y >= Rect.y && Point.y < Rect.y + Rect.h;
}
constexpr int LeftButton = 0;
constexpr float MarkInsetRatio = 0.25f; // proportion of the glyph, not a pixel value
} // namespace

void Checkbox::ApplyStyle(const CheckboxStyle& Style) {
    this->Style    = static_cast<const BoxStyle&>(Style);
    ColorCheckMark = Style.ColorCheckMark;
    ColorBoxChecked = Style.ColorBoxChecked;
}

SDL_FPoint Checkbox::MeasureContent(SDL_FPoint /*AvailableContentSize*/) {
    return {GlyphSizeLogical, GlyphSizeLogical};
}

bool Checkbox::UpdateInteractionState(const Platform::InputState& Input) {
    if (!IsEnabled) {
        PressedInside = false;
        return false;
    }

    const bool Hovered  = PointInRect(Input.MousePosition, ArrangedRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];

    if (Pressed && Hovered) {
        PressedInside = true;
    }
    if (Released) {
        if (PressedInside && Hovered) {
            Checked = !Checked;
            if (OnChanged) {
                OnChanged(Checked);
            }
        }
        PressedInside = false;
    }

    return Hovered;
}

void Checkbox::DrawContent(Render::Renderer& Renderer, SDL_FRect ContentRect) {
    if (!Checked) {
        return; // unchecked: Box already drew the empty box background + border
    }
    Renderer.DrawFilledRect(ContentRect, ColorBoxChecked);

    const float InsetX = ContentRect.w * MarkInsetRatio;
    const float InsetY = ContentRect.h * MarkInsetRatio;
    const SDL_FRect Mark{ContentRect.x + InsetX, ContentRect.y + InsetY,
                         ContentRect.w - 2.0f * InsetX, ContentRect.h - 2.0f * InsetY};
    Renderer.DrawFilledRect(Mark, ColorCheckMark);
}

} // namespace Penumbra::Widgets
