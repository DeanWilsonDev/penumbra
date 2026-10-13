#include "Penumbra/Widgets/Checkbox.h"

namespace Penumbra::Widgets {

namespace {
constexpr int LeftButton = 0;
constexpr float MarkInsetRatio = 0.25f; // proportion of the glyph, not a pixel value
} // namespace

void Checkbox::ApplyStyle(const CheckboxStyle& Style) {
    this->Style    = static_cast<const BoxStyle&>(Style);
    ColorCheckMark = Style.ColorCheckMark;
    ColorBoxChecked = Style.ColorBoxChecked;
}

Point Checkbox::MeasureContent(Point /*AvailableContentSize*/) {
    return {GlyphSizeLogical, GlyphSizeLogical};
}

bool Checkbox::UpdateInteractionState(const Platform::InputState& Input) {
    if (!IsEnabled) {
        PressedInside = false;
        return false;
    }

    const bool Hovered  = Input.IsPointerOver(ArrangedRect);
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

void Checkbox::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    if (!Checked) {
        return; // unchecked: Box already drew the empty box background + border
    }
    Renderer.DrawFilledRect(ContentRect, ColorBoxChecked, Style.BorderRadius);

    const float InsetX = ContentRect.W * MarkInsetRatio;
    const float InsetY = ContentRect.H * MarkInsetRatio;
    const Rect Mark{ContentRect.X + InsetX, ContentRect.Y + InsetY,
                    ContentRect.W - 2.0f * InsetX, ContentRect.H - 2.0f * InsetY};
    Renderer.DrawFilledRect(Mark, ColorCheckMark, Style.BorderRadius * 0.6f);
}

} // namespace Penumbra::Widgets
