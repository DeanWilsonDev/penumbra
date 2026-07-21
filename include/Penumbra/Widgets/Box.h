#pragma once

#include "Penumbra/Widgets/WidgetBase.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

// The universal primitive — the "div". Everything is a Box. A Box owns the box
// model, an optional list of children, and a layout mode for arranging them.
// "Container-ness" is just a Box that has children.
class Box : public WidgetBase {
public:
    BoxStyle                                 Style{};
    std::vector<std::unique_ptr<WidgetBase>> Children;
    LayoutMode                               Layout{LayoutMode::None};
    float                                    ChildGap{0.0f};
    CrossAlign                               CrossAlignment{CrossAlign::Start};

    WidgetBase* AddChild(std::unique_ptr<WidgetBase> Child);
    WidgetBase* InsertChildAt(std::size_t Index, std::unique_ptr<WidgetBase> Child);

    // Removes Child if it's found among Children; no-op otherwise. The removed
    // widget is destroyed.
    void RemoveChild(WidgetBase* Child);

    // Swaps Existing out for Replacement in place (same index), returning the
    // widget that was removed so the caller can decide its fate. Existing must be
    // one of Children; behaves like a no-op returning nullptr otherwise.
    std::unique_ptr<WidgetBase> ReplaceChild(WidgetBase* Existing, std::unique_ptr<WidgetBase> Replacement);

    void ClearChildren();

    // Repositions an existing child without losing its place at the end of the
    // vector the way remove-then-add would. Out-of-range indices are a no-op.
    void MoveChild(std::size_t FromIndex, std::size_t ToIndex);

    Point Measure(Point AvailableSizeLogical) override;
    void  Arrange(Rect FinalRectLogical) override;
    bool  UpdateInteractionState(const Platform::InputState&) override;
    void  Draw(Render::Renderer&) override;

    EdgeInsets GetMarginLogical() const override { return Style.Margin; }

    InteractionState GetInteractionState() const { return CurrentState; }

    std::size_t GetChildCount() const override { return Children.size(); }
    WidgetBase* GetChildAt(std::size_t Index) const override { return Children[Index].get(); }

    // Fluent, chainable construction for Iris's Penumbra backend codegen: method
    // names match Iris prop names exactly (the Iris preprocessor does a
    // mechanical prop-name -> builder-method-name translation), except
    // className() — class is a reserved word, so Iris's codegen maps its
    // `class` prop to this name specifically; there is no other divergence.
    // Owns the widget being built until build() transfers it out; not meant to
    // outlive that call.
    class Builder {
    public:
        Builder();

        Builder& className(std::string Value);
        Builder& child(std::unique_ptr<WidgetBase> Child);
        Builder& children(std::vector<std::unique_ptr<WidgetBase>> Kids);
        Builder& onPress(std::function<void()> Handler);
        Builder& onRelease(std::function<void()> Handler);
        Builder& onHover(std::function<void()> Handler);
        Builder& onFocus(std::function<void()> Handler);
        Builder& onChange(std::function<void()> Handler);

        std::unique_ptr<Box> build();

    private:
        std::unique_ptr<Box> Owned;
    };

protected:
    // Subclasses override these for their own intrinsic visuals (text, glyphs).
    virtual Point MeasureContent(Point AvailableContentSize) { return {0.0f, 0.0f}; }
    virtual void  DrawContent(Render::Renderer&, Rect ContentRect) {}

    // The box-model overhead (border + padding) on each axis, and the content rect
    // an outer rect collapses to once border and padding are removed.
    Point FrameSize() const;
    Rect  ContentRectFrom(Rect OuterRect) const;

private:
    // The background Draw should use for CurrentState -- Style.ColorBackgroundHovered/
    // Pressed/Disabled when set (non-zero alpha), else Style.ColorBackground unchanged.
    // Mirrors Button::BackgroundForState, minus Button's unconditional (no-fallback)
    // override: most Boxes with onPress set never define interaction colours, and
    // those must keep rendering their plain ColorBackground rather than going
    // transparent on hover (docs/lustre_style_gaps_requirements.md #1).
    Render::Color BackgroundForState() const;

    // Tracks whether a press began inside this Box, so OnReleased pairs with the
    // OnPressed that started it rather than firing on an unrelated mouse-up.
    bool              PressedInside{false};
    InteractionState  CurrentState{InteractionState::Default};
};

} // namespace Penumbra::Widgets
