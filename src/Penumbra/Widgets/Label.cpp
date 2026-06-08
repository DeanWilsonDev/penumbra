#include "Penumbra/Widgets/Label.h"

namespace Penumbra::Widgets {

SDL_FPoint Label::MeasureContent(SDL_FPoint /*AvailableContentSize*/) {
    if (!FontBackend) {
        return {0.0f, 0.0f};
    }
    const Render::TextMetrics Metrics = FontBackend->MeasureText(Font, Text);
    return {Metrics.WidthLogical, Metrics.HeightLogical};
}

void Label::DrawContent(Render::Renderer& Renderer, SDL_FRect ContentRect) {
    Renderer.DrawText(Font, Text, {ContentRect.x, ContentRect.y}, ColorText);
}

} // namespace Penumbra::Widgets
