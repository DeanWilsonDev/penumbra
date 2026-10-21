#pragma once

#include "Penumbra/Widgets/WidgetBase.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

class TextSelection;

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
    // Main-axis distribution for VerticalStack/HorizontalStack only (see Justify's own
    // doc comment, Styles.h) -- Start (the default) is sequential packing from Content's
    // start, identical to every Box's behavior before this field existed. Not consulted
    // by FixedLeadingStack or None.
    Justify                                  JustifyContentMode{Justify::Start};

    // Only meaningful when Layout == LayoutMode::FixedLeadingStack: the exact
    // main-axis (Y) extent children[0] ("Leading") is arranged at, regardless of
    // its own measured size. -1 (the default) behaves as 0 -- an unset extent
    // collapses Leading rather than giving it the whole Box, matching
    // WidthLogical/HeightLogical's own "explicit non-negative override" shape.
    float                                    LeadingExtentLogical{-1.0f};

    // Where the values Style.Transitions names start from when this Box first
    // appears, and again each time it's shown after SetIsVisible(false) on it or an
    // ancestor: CSS's @starting-style. Only the transitioned values are read, all of
    // them, so build it from Style and change just the ones that should ease in (a
    // bar's FlexGrow from 0, say). Unset, the Box appears with its style as it stands.
    std::optional<BoxStyle> StartingStyle;

    Box();
    ~Box() override;

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

    EdgeInsets      GetMarginLogical() const override { return Style.Margin; }
    float           GetFlexGrow() const override; // eased, under a FlexGrow transition
    const BoxStyle* GetBoxStyle() const override { return &Style; }
    bool            PaintsAsGroup() const override; // a transform, or an opacity below 1 as displayed

    InteractionState GetInteractionState() const { return CurrentState; }

    TextSelection& EnableTextSelection();
    TextSelection* GetTextSelection() const { return Selection.get(); }

    std::size_t GetChildCount() const override { return Children.size(); }
    WidgetBase* GetChildAt(std::size_t Index) const override { return Children[Index].get(); }

    // Fluent, chainable construction. Owns the widget until build() transfers
    // it out; not meant to outlive that call.
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
    std::unique_ptr<TextSelection> Selection;

    virtual Point MeasureContent(Point AvailableContentSize) { return {0.0f, 0.0f}; }
    virtual void  DrawContent(Render::Renderer&, Rect ContentRect) {}

    Point FrameSize() const;
    Rect  ContentRectFrom(Rect OuterRect) const;
    Rect  PaddingRectFrom(Rect OuterRect) const; // inside the border: a containing block's area
    Point ConstrainAvailable(Point AvailableSizeLogical) const;
    Point ConstrainDesired(Point DesiredSizeLogical, Point AvailableSizeLogical) const;

    // Style.Transitions. A subclass that overrides Measure or Draw without calling
    // Box's calls these instead: SyncLayoutTransitions at the start of Measure, to
    // catch changes to Width, Height and FlexGrow, and AdvanceTransitions at the start
    // of Draw, which moves every eased value on by the Renderer's frame clock. The
    // Displayed values are what to draw with.
    void          SyncLayoutTransitions();
    void          AdvanceTransitions(Render::Renderer&);
    Render::Color DisplayedBackground() const;
    float         DisplayedOpacity() const;

    void OnShown() override;

private:
    // The background Draw should use for CurrentState -- Style.ColorBackgroundHovered/
    // Pressed/Disabled when set (non-zero alpha), else Style.ColorBackground unchanged.
    // Mirrors Button::BackgroundForState, minus Button's unconditional (no-fallback)
    // override: most Boxes with onPress set never define interaction colours, and
    // those must keep rendering their plain ColorBackground rather than going
    // transparent on hover.
    Render::Color BackgroundForState() const;

    // The gradient stops Draw should use for CurrentState -- Style.GradientTopHovered/
    // Pressed (resp. Bottom) when set (non-zero alpha), else Style.GradientTop/
    // GradientBottom unchanged. Mirrors BackgroundForState's fallback convention;
    // no Disabled case, per BoxStyle's own comment on the Hovered/Pressed fields.
    Render::Color GradientTopForState() const;
    Render::Color GradientBottomForState() const;

    // The border color Draw should use for CurrentState -- Style.ColorBorderHovered/
    // Pressed/Disabled when set (non-zero alpha), else Style.ColorBorder unchanged.
    // Mirrors BackgroundForState's fallback convention exactly.
    Render::Color BorderForState() const;

    // Tracks whether a press began inside this Box, so OnReleased pairs with the
    // OnPressed that started it rather than firing on an unrelated mouse-up.
    bool              PressedInside{false};
    InteractionState  CurrentState{InteractionState::Default};

    // The eased values, created once Style.Transitions isn't empty, so a plain Box
    // carries nothing extra.
    struct TransitionState;
    std::unique_ptr<TransitionState> Eased;

    // Measure's results in the running LayoutPass, by available size. A Box is
    // measured with a few distinct sizes a pass (its parent's Measure, any FlexGrow
    // sharing, its parent's Arrange), so a handful of slots, reused in turn, holds
    // them all.
    struct MeasureMemo {
        std::uint64_t Pass{0};
        Point         Available{};
        Point         Desired{};
    };
    std::array<MeasureMemo, 4> MeasureMemos{};
    std::size_t                NextMeasureMemo{0};

    float ConstrainLength(bool IsWidth, float Value, float AvailableLogical) const;
    Point MeasureUncached(Point AvailableSizeLogical);
};

// The input a Box's subtree takes, given what its parent handed it: Input, with
// PointerPassThrough set or cleared by Mode, the way CSS inherits pointer-events.
// Returns Input itself when that's unchanged, else Storage, filled in.
const Platform::InputState& InputForPointerEvents(PointerEvents Mode, const Platform::InputState& Input,
                                                  Platform::InputState& Storage);

// The input a container hands Child: Input with Child's own pointer-events applied, so
// a widget that handles its input itself (a Button, a TextInput) honours its
// BoxStyle's PointerEventsMode too. Returns Input itself when that's unchanged.
const Platform::InputState& InputForChild(const WidgetBase& Child, const Platform::InputState& Input,
                                          Platform::InputState& Storage);

} // namespace Penumbra::Widgets
