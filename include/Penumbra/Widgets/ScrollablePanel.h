#pragma once

#include "Penumbra/Widgets/Box.h"

namespace Penumbra::Widgets {

class ScrollablePanel : public Box {
public:
    enum class ScrollDirection { Vertical, Horizontal };

    ScrollDirection Direction{ScrollDirection::Vertical};

    float WheelStepLogical{0.0f};
    float HorizontalWheelStepLogical{0.0f};

    // The scrollbar, drawn when the content overflows and ScrollbarWidthLogical is
    // set. Dragging the thumb scrolls; pressing the track elsewhere jumps the thumb's
    // centre there and carries on as a drag. The hovered thumb colour shows while the
    // pointer is over the panel or the thumb is being dragged.
    float         ScrollbarWidthLogical{0.0f};
    Render::Color ColorScrollbarThumb{0, 0, 0, 0};
    Render::Color ColorScrollbarThumbHovered{0, 0, 0, 0};
    Render::Color ColorScrollbarTrack{0, 0, 0, 0};

    float GetScrollOffset() const { return ScrollOffsetY; }
    float GetScrollOffsetX() const { return ScrollOffsetX; }

    Point Measure(Point AvailableSizeLogical) override;
    void  Arrange(Rect FinalRectLogical) override;
    bool  UpdateInteractionState(const Platform::InputState&) override;
    void  Draw(Render::Renderer&) override;

    bool ConsumedWheelThisFrame() const override { return WheelHandledThisFrame; }

private:
    struct ScrollbarLayout {
        bool  Shown{false};
        Rect  Track{};
        Rect  Thumb{};
        float Travel{0.0f};    // how far the thumb can move along the track
        float MaxScroll{0.0f}; // the scroll offset at the end of that travel
    };
    ScrollbarLayout LayOutScrollbar(Rect Content) const;

    // Starts, continues or ends a thumb drag; returns whether the scrollbar has the
    // pointer this frame.
    bool UpdateScrollbar(const Platform::InputState&, Rect Content);
    void DrawScrollbar(Render::Renderer&, Rect Content) const;

    float ScrollOffsetY{0.0f};
    float ScrollOffsetX{0.0f};
    float ContentHeight{0.0f};
    float ContentWidth{0.0f};
    bool  WheelHandledThisFrame{false};
    bool  PointerOver{false};
    bool  DraggingThumb{false};
    float ThumbGrabOffset{0.0f}; // where along the thumb the drag took hold
};

} // namespace Penumbra::Widgets
