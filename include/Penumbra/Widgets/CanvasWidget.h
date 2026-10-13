#pragma once

#include "Penumbra/Widgets/Box.h"

#include <functional>

namespace Penumbra::Widgets {

// A Box whose content is whatever OnDraw draws. Background, border, padding, margin,
// Width/Height and FlexGrow all come from BoxStyle as for any Box; OnDraw then draws
// straight into the content rect through the ordinary Renderer, clipped to it.
// Unlike ViewportWidget there is no off-screen texture: the drawing lands in the UI
// pass in place, so it composites with ancestors' clips and transforms like any
// other widget's paint. Children, if any, draw on top of it.
class CanvasWidget : public Box {
public:
    // Called every frame the canvas is drawn, with the content rect in logical
    // pixels. Draw in absolute coordinates inside ContentRectLogical; anything
    // outside it is clipped. Not called while the content rect is empty.
    std::function<void(Render::Renderer&, Rect ContentRectLogical)> OnDraw;

    // The content size the canvas asks for when Style.Width/Height don't set one --
    // a drawing has no intrinsic size of its own. Both 0 by default, so an unsized
    // canvas collapses unless its parent stretches or grows it.
    float PreferredWidthLogical{0.0f};
    float PreferredHeightLogical{0.0f};

protected:
    Point MeasureContent(Point AvailableContentSize) override;
    void  DrawContent(Render::Renderer&, Rect ContentRect) override;
};

} // namespace Penumbra::Widgets
