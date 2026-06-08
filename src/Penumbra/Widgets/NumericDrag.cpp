#include "Penumbra/Widgets/NumericDrag.h"

#include <algorithm>
#include <cstdio>

namespace Penumbra::Widgets {

namespace {
bool PointInRect(SDL_FPoint Point, SDL_FRect Rect) {
    return Point.x >= Rect.x && Point.x < Rect.x + Rect.w &&
           Point.y >= Rect.y && Point.y < Rect.y + Rect.h;
}
constexpr int LeftButton = 0;
} // namespace

std::string NumericDrag::FormatValue() const {
    char Buffer[32];
    std::snprintf(Buffer, sizeof(Buffer), "%.2f", Value);
    return Buffer;
}

SDL_FPoint NumericDrag::MeasureContent(SDL_FPoint /*AvailableContentSize*/) {
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

    const bool Hovered  = PointInRect(Input.MousePosition, ArrangedRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Input.MouseButtonDown[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];

    if (Pressed && Hovered) {
        Dragging = true;
        LastMouseX = Input.MousePosition.x;
    }
    if (Dragging && Down) {
        const float DeltaX = Input.MousePosition.x - LastMouseX;
        if (DeltaX != 0.0f) {
            Value += DeltaX * Sensitivity;
            LastMouseX = Input.MousePosition.x;
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

void NumericDrag::DrawContent(Render::Renderer& Renderer, SDL_FRect ContentRect) {
    Renderer.DrawText(Font, FormatValue(), {ContentRect.x, ContentRect.y}, ColorText);
}

} // namespace Penumbra::Widgets
