#include "Penumbra/Widgets/CanvasWidget.h"

namespace Penumbra::Widgets {

Point CanvasWidget::MeasureContent(Point /*AvailableContentSize*/) {
    return {PreferredWidthLogical, PreferredHeightLogical};
}

void CanvasWidget::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    if (!OnDraw || ContentRect.W <= 0.0f || ContentRect.H <= 0.0f) {
        return;
    }

    Renderer.PushClipRect(ContentRect);
    OnDraw(Renderer, ContentRect);
    Renderer.PopClipRect();
}

} // namespace Penumbra::Widgets
