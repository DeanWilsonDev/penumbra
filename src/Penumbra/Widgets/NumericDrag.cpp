#include "Penumbra/Widgets/NumericDrag.h"

#include <algorithm>
#include <cstdio>

namespace Penumbra::Widgets {

namespace {
constexpr int LeftButton = 0;
} // namespace

std::string NumericDrag::FormatValue() const {
    char Buffer[32];
    std::snprintf(Buffer, sizeof(Buffer), "%.2f", Value);
    return Buffer;
}

Point NumericDrag::MeasureContent(Point /*AvailableContentSize*/) {
    if (!FontBackend) {
        return {PreferredWidthLogical, 0.0f};
    }
    const Render::TextMetrics Metrics = FontBackend->MeasureText(Font, FormatValue());
    return {std::max(PreferredWidthLogical, Metrics.WidthLogical), Metrics.HeightLogical};
}

bool NumericDrag::UpdateInteractionState(const Platform::InputState& Input) {
    if (!IsEnabled) {
        Dragging = false;
        return false;
    }

    const bool Hovered  = Input.IsPointerOver(ArrangedRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Input.MouseButtonDown[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];

    if (Pressed && Hovered) {
        Dragging = true;
        LastMouseX = Input.MousePosition.X;
    }
    if (Dragging && Down) {
        const float DeltaX = Input.MousePosition.X - LastMouseX;
        if (DeltaX != 0.0f) {
            Value += DeltaX * Sensitivity;
            LastMouseX = Input.MousePosition.X;
            if (OnValueChanged) {
                OnValueChanged(Value);
            }
        }
    }
    if (Released) {
        Dragging = false;
    }

    return Hovered || Dragging;
}

void NumericDrag::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    Renderer.DrawText(Font, FormatValue(), {ContentRect.X, ContentRect.Y}, ColorText);
}

} // namespace Penumbra::Widgets
