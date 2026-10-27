#include "Penumbra/Widgets/Box.h"

#include "Penumbra/Widgets/KeyDispatch.h"
#include "Penumbra/Widgets/LayoutPass.h"
#include "Penumbra/Widgets/Positioning.h"
#include "Penumbra/Widgets/Stacking.h"
#include "Penumbra/Widgets/TextSelection.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Penumbra::Widgets {

namespace {
float NonNegative(float Value) { return Value > 0.0f ? Value : 0.0f; }

// Visible and in its parent's layout: not Absolute or Fixed.
bool InFlow(const WidgetBase& Child) { return Child.GetIsVisible() && !IsOutOfFlow(Child); }

constexpr int LeftButton = 0;

// Inverse of the SRT-around-Pivot composition Renderer::PopTransform applies when
// painting: undoes Translate, then Rotate, then Scale, so a ScreenPoint that landed
// on the transformed visual maps back to the same LocalPoint it would have hit had
// the widget never been transformed -- letting the untouched IsPointerOver(
// ArrangedRect) check downstream work unchanged.
Point InverseTransformPoint(Point ScreenPoint, Point Pivot, const Transform& T) {
    const float OffsetX = ScreenPoint.X - Pivot.X - T.TranslateXLogical;
    const float OffsetY = ScreenPoint.Y - Pivot.Y - T.TranslateYLogical;

    constexpr float Pi = 3.14159265358979323846f;
    const float Radians = -T.RotationDegrees * (Pi / 180.0f);
    const float CosA = std::cos(Radians);
    const float SinA = std::sin(Radians);
    const float RotatedX = OffsetX * CosA - OffsetY * SinA;
    const float RotatedY = OffsetX * SinA + OffsetY * CosA;

    const float ScaleX = (T.ScaleX != 0.0f) ? T.ScaleX : 1.0f;
    const float ScaleY = (T.ScaleY != 0.0f) ? T.ScaleY : 1.0f;

    return {Pivot.X + RotatedX / ScaleX, Pivot.Y + RotatedY / ScaleY};
}

std::vector<float> GrowExtentsFor(const std::vector<std::unique_ptr<WidgetBase>>& Children, bool Vertical,
                                  Point ContentSize, float ChildGap) {
    float TotalGrow = 0.0f;
    for (const auto& Child : Children) {
        if (InFlow(*Child) && Child->GetFlexGrow() > 0.0f) {
            TotalGrow += Child->GetFlexGrow();
        }
    }
    if (TotalGrow <= 0.0f) {
        return {};
    }

    float       FixedMain    = 0.0f;
    std::size_t VisibleCount = 0;
    for (const auto& Child : Children) {
        if (!InFlow(*Child)) {
            continue;
        }
        ++VisibleCount;
        const EdgeInsets Margin = Child->GetMarginLogical();
        FixedMain += Vertical ? Margin.Top + Margin.Bottom : Margin.Left + Margin.Right;
        if (Child->GetFlexGrow() > 0.0f) {
            continue;
        }
        const Point Desired = Child->Measure({NonNegative(ContentSize.X - Margin.Left - Margin.Right),
                                              NonNegative(ContentSize.Y - Margin.Top - Margin.Bottom)});
        FixedMain += Vertical ? Desired.Y : Desired.X;
    }

    const float Gaps      = VisibleCount > 1 ? static_cast<float>(VisibleCount - 1) * ChildGap : 0.0f;
    const float Remaining = NonNegative((Vertical ? ContentSize.Y : ContentSize.X) - FixedMain - Gaps);

    std::vector<float> Extents(Children.size(), -1.0f);
    for (std::size_t Index = 0; Index < Children.size(); ++Index) {
        const WidgetBase* Child = Children[Index].get();
        if (InFlow(*Child) && Child->GetFlexGrow() > 0.0f) {
            Extents[Index] = Remaining * Child->GetFlexGrow() / TotalGrow;
        }
    }
    return Extents;
}

float GrowExtentAt(const std::vector<float>& Extents, std::size_t Index) {
    return Extents.empty() ? -1.0f : Extents[Index];
}

float Lerp(float From, float To, float T) { return From + (To - From) * T; }

// The last of Style's transitions on Property, which wins as in CSS, or null.
const Transition* FindTransition(const BoxStyle& Style, TransitionProperty Property) {
    const Transition* Found = nullptr;
    for (const Transition& Entry : Style.Transitions) {
        if (Entry.Property == Property) {
            Found = &Entry;
        }
    }
    return Found;
}

// One eased value: From is what showed when Target last changed.
template <typename T> struct EasedTrack {
    T            From{};
    T            Target{};
    Anim::Tween  Timeline;
    Anim::Easing Curve{Anim::Easing::Linear};
    bool         Known{false}; // Target has been set at least once

    float Progress() const { return Anim::Ease(Curve, Timeline.Progress()); }
};

// Starts Track toward Target: eased from From under Spec when CanEase, else at once.
template <typename T>
void StartTrack(EasedTrack<T>& Track, const T& From, const T& Target, const Transition* Spec, bool CanEase) {
    Track.Target = Target;
    Track.Known  = true;
    if (CanEase && Spec && Spec->DurationSeconds > 0.0f) {
        Track.From  = From;
        Track.Curve = Spec->Curve;
        Track.Timeline.Start(Spec->DurationSeconds);
    } else {
        Track.From     = Target;
        Track.Timeline = {};
    }
}

// Points Track at Target if that's a change, easing from Current, the value showing.
template <typename T>
void RetargetTrack(EasedTrack<T>& Track, const T& Target, const T& Current, const Transition* Spec, bool CanEase) {
    if (Track.Known && Track.Target == Target) {
        return;
    }
    StartTrack(Track, Current, Target, Spec, Track.Known && CanEase);
}

Point StackChildAvailable(Point ContentSize, EdgeInsets Margin, bool Vertical, float GrowExtent) {
    Point Available{NonNegative(ContentSize.X - Margin.Left - Margin.Right),
                    NonNegative(ContentSize.Y - Margin.Top - Margin.Bottom)};
    if (GrowExtent >= 0.0f) {
        (Vertical ? Available.Y : Available.X) = GrowExtent;
    }
    return Available;
}
} // namespace

struct Box::TransitionState {
    EasedTrack<Length>        Width;
    EasedTrack<Length>        Height;
    EasedTrack<float>         FlexGrow;
    EasedTrack<Render::Color> Background;
    EasedTrack<float>         Opacity;

    // The width and height last laid out, in logical pixels, or -1 when unset: where
    // a size eases from when its target changes mid-flight.
    float LastWidthLogical{-1.0f};
    float LastHeightLogical{-1.0f};

    bool          Entered{false}; // the enter transitions have started since the Box last appeared
    std::uint64_t LastAdvancedFrame{0};
};

Box::Box() = default;
Box::~Box() = default;

TextSelection& Box::EnableTextSelection() {
    if (!Selection) {
        Selection = std::make_unique<TextSelection>();
    }
    return *Selection;
}

WidgetBase* Box::AddChild(std::unique_ptr<WidgetBase> Child) {
    WidgetBase* Raw = Child.get();
    Children.push_back(std::move(Child));
    return Raw;
}

WidgetBase* Box::InsertChildAt(std::size_t Index, std::unique_ptr<WidgetBase> Child) {
    WidgetBase* Raw = Child.get();
    const std::size_t Clamped = std::min(Index, Children.size());
    Children.insert(Children.begin() + static_cast<std::ptrdiff_t>(Clamped), std::move(Child));
    return Raw;
}

void Box::RemoveChild(WidgetBase* Child) {
    auto Iterator = std::find_if(Children.begin(), Children.end(),
                                  [Child](const std::unique_ptr<WidgetBase>& Owned) { return Owned.get() == Child; });
    if (Iterator != Children.end()) {
        Children.erase(Iterator);
    }
}

std::unique_ptr<WidgetBase> Box::ReplaceChild(WidgetBase* Existing, std::unique_ptr<WidgetBase> Replacement) {
    auto Iterator = std::find_if(
        Children.begin(), Children.end(),
        [Existing](const std::unique_ptr<WidgetBase>& Owned) { return Owned.get() == Existing; });
    if (Iterator == Children.end()) {
        return nullptr;
    }
    std::unique_ptr<WidgetBase> Removed = std::move(*Iterator);
    *Iterator                           = std::move(Replacement);
    return Removed;
}

void Box::ClearChildren() { Children.clear(); }

void Box::MoveChild(std::size_t FromIndex, std::size_t ToIndex) {
    if (FromIndex == ToIndex || FromIndex >= Children.size() || ToIndex >= Children.size()) {
        return;
    }
    auto Begin = Children.begin();
    if (FromIndex < ToIndex) {
        std::rotate(Begin + static_cast<std::ptrdiff_t>(FromIndex), Begin + static_cast<std::ptrdiff_t>(FromIndex) + 1,
                    Begin + static_cast<std::ptrdiff_t>(ToIndex) + 1);
    } else {
        std::rotate(Begin + static_cast<std::ptrdiff_t>(ToIndex), Begin + static_cast<std::ptrdiff_t>(FromIndex),
                    Begin + static_cast<std::ptrdiff_t>(FromIndex) + 1);
    }
}

Point Box::FrameSize() const {
    const float Horizontal = Style.Padding.Left + Style.Padding.Right + 2.0f * Style.BorderWidth;
    const float Vertical   = Style.Padding.Top + Style.Padding.Bottom + 2.0f * Style.BorderWidth;
    return {Horizontal, Vertical};
}

Rect Box::ContentRectFrom(Rect OuterRect) const {
    const float B = Style.BorderWidth;
    return {OuterRect.X + B + Style.Padding.Left,
            OuterRect.Y + B + Style.Padding.Top,
            NonNegative(OuterRect.W - 2.0f * B - Style.Padding.Left - Style.Padding.Right),
            NonNegative(OuterRect.H - 2.0f * B - Style.Padding.Top - Style.Padding.Bottom)};
}

Rect Box::PaddingRectFrom(Rect OuterRect) const {
    const float B = Style.BorderWidth;
    return {OuterRect.X + B, OuterRect.Y + B, NonNegative(OuterRect.W - 2.0f * B), NonNegative(OuterRect.H - 2.0f * B)};
}

Point Box::Measure(Point AvailableSizeLogical) {
    const std::uint64_t Pass = LayoutPass::Current();
    if (Pass != 0) {
        for (const MeasureMemo& Memo : MeasureMemos) {
            if (Memo.Pass == Pass && Memo.Available.X == AvailableSizeLogical.X &&
                Memo.Available.Y == AvailableSizeLogical.Y) {
                return Memo.Desired;
            }
        }
    }

    const Point Desired = MeasureUncached(AvailableSizeLogical);
    if (Pass != 0) {
        MeasureMemos[NextMeasureMemo] = {Pass, AvailableSizeLogical, Desired};
        NextMeasureMemo               = (NextMeasureMemo + 1) % MeasureMemos.size();
    }
    return Desired;
}

Point Box::MeasureUncached(Point AvailableSizeLogical) {
    SyncLayoutTransitions();
    const Point Frame = FrameSize();

    const Point EffectiveAvailable = ConstrainAvailable(AvailableSizeLogical);

    const Point ContentAvailable{NonNegative(EffectiveAvailable.X - Frame.X),
                                 NonNegative(EffectiveAvailable.Y - Frame.Y)};

    Point ContentDesired{0.0f, 0.0f};

    if (Layout == LayoutMode::None) {
        ContentDesired = MeasureContent(ContentAvailable);
    } else if (Layout == LayoutMode::FixedLeadingStack) {
        const float LeadingExtent = NonNegative(LeadingExtentLogical);
        const float FillHeight    = NonNegative(ContentAvailable.Y - LeadingExtent - ChildGap);

        bool LeadingSeen = false;
        for (std::size_t Index = 0; Index < Children.size(); ++Index) {
            WidgetBase* Child = Children[Index].get();
            if (!InFlow(*Child)) {
                continue;
            }
            const EdgeInsets Margin     = Child->GetMarginLogical();
            const float      SlotHeight = LeadingSeen ? FillHeight : LeadingExtent;
            const Point ChildAvailable{NonNegative(ContentAvailable.X - Margin.Left - Margin.Right),
                                       NonNegative(SlotHeight - Margin.Top - Margin.Bottom)};
            Child->Measure(ChildAvailable);
            LeadingSeen = true;
        }

        ContentDesired = ContentAvailable;
    } else {
        const bool Vertical = (Layout == LayoutMode::VerticalStack);
        float MainTotal = 0.0f;
        float CrossMax  = 0.0f;
        bool  AnyVisible = false;
        const std::vector<float> GrowExtents = GrowExtentsFor(Children, Vertical, ContentAvailable, ChildGap);

        for (std::size_t Index = 0; Index < Children.size(); ++Index) {
            WidgetBase* Child = Children[Index].get();
            if (!InFlow(*Child)) {
                continue;
            }
            const EdgeInsets Margin = Child->GetMarginLogical();

            const Point Desired = Child->Measure(
                StackChildAvailable(ContentAvailable, Margin, Vertical, GrowExtentAt(GrowExtents, Index)));

            const float ChildMain   = Vertical ? Desired.Y : Desired.X;
            const float ChildCross  = Vertical ? Desired.X : Desired.Y;
            const float MarginMain  = Vertical ? (Margin.Top + Margin.Bottom) : (Margin.Left + Margin.Right);
            const float MarginCross = Vertical ? (Margin.Left + Margin.Right) : (Margin.Top + Margin.Bottom);

            if (AnyVisible) {
                MainTotal += ChildGap;
            }
            MainTotal += ChildMain + MarginMain;
            CrossMax = std::max(CrossMax, ChildCross + MarginCross);
            AnyVisible = true;
        }

        ContentDesired = Vertical ? Point{CrossMax, MainTotal} : Point{MainTotal, CrossMax};
    }

    return ConstrainDesired({ContentDesired.X + Frame.X, ContentDesired.Y + Frame.Y}, AvailableSizeLogical);
}

Point Box::ConstrainAvailable(Point AvailableSizeLogical) const {
    return {ConstrainLength(true, AvailableSizeLogical.X, AvailableSizeLogical.X),
            ConstrainLength(false, AvailableSizeLogical.Y, AvailableSizeLogical.Y)};
}

Point Box::ConstrainDesired(Point DesiredSizeLogical, Point AvailableSizeLogical) const {
    return {ConstrainLength(true, DesiredSizeLogical.X, AvailableSizeLogical.X),
            ConstrainLength(false, DesiredSizeLogical.Y, AvailableSizeLogical.Y)};
}

float Box::ConstrainLength(bool IsWidth, float Value, float AvailableLogical) const {
    const Length& Size   = IsWidth ? Style.Width : Style.Height;
    float         Result = Value;
    if (Size.IsSet()) {
        Result = Size.Resolve(AvailableLogical);
        if (Eased) {
            const EasedTrack<Length>& Track = IsWidth ? Eased->Width : Eased->Height;
            if (Track.Known && Track.Target == Size && Track.From.IsSet()) {
                Result = Lerp(Track.From.Resolve(AvailableLogical), Result, Track.Progress());
            }
        }
    }
    if (Eased) {
        (IsWidth ? Eased->LastWidthLogical : Eased->LastHeightLogical) = Size.IsSet() ? Result : -1.0f;
    }

    if (IsWidth && Style.MaxWidth.IsSet()) {
        Result = std::min(Result, Style.MaxWidth.Resolve(AvailableLogical));
    }
    if (IsWidth && Style.MinWidth.IsSet()) {
        Result = std::max(Result, Style.MinWidth.Resolve(AvailableLogical));
    }
    return Result;
}

float Box::GetFlexGrow() const {
    if (!Eased || !Eased->FlexGrow.Known || !FindTransition(Style, TransitionProperty::FlexGrow)) {
        return Style.FlexGrow;
    }
    // Until this Box's own Measure picks up a new FlexGrow, keep showing the old one:
    // the parent asks before measuring it.
    const EasedTrack<float>& Track = Eased->FlexGrow;
    return Lerp(Track.From, Track.Target, Track.Progress());
}

bool Box::PaintsAsGroup() const { return !Style.Transform.IsIdentity() || DisplayedOpacity() < 1.0f; }

void Box::SyncLayoutTransitions() {
    if (Style.Transitions.empty()) {
        Eased.reset();
        return;
    }
    if (!Eased) {
        Eased = std::make_unique<TransitionState>();
    }
    TransitionState&  State      = *Eased;
    const Transition* WidthSpec  = FindTransition(Style, TransitionProperty::Width);
    const Transition* HeightSpec = FindTransition(Style, TransitionProperty::Height);
    const Transition* GrowSpec   = FindTransition(Style, TransitionProperty::FlexGrow);

    if (!State.Entered) {
        // Appearing: start from StartingStyle where there is one, else as styled.
        // Sizes ease only between set lengths, as in RetargetTrack below.
        State.Entered          = true;
        State.LastWidthLogical = State.LastHeightLogical = -1.0f;
        const BoxStyle* Start  = StartingStyle ? &*StartingStyle : nullptr;
        StartTrack(State.Width, Start ? Start->Width : Length{}, Style.Width, WidthSpec,
                   Start && Start->Width.IsSet() && Style.Width.IsSet());
        StartTrack(State.Height, Start ? Start->Height : Length{}, Style.Height, HeightSpec,
                   Start && Start->Height.IsSet() && Style.Height.IsSet());
        StartTrack(State.FlexGrow, Start ? Start->FlexGrow : 0.0f, Style.FlexGrow, GrowSpec, Start != nullptr);
        StartTrack(State.Background, Start ? Start->ColorBackground : Render::Color{}, BackgroundForState(),
                   FindTransition(Style, TransitionProperty::BackgroundColor), Start != nullptr);
        StartTrack(State.Opacity, Start ? Start->Opacity : 0.0f, Style.Opacity,
                   FindTransition(Style, TransitionProperty::Opacity), Start != nullptr);
        return;
    }

    const Length WidthNow  = State.LastWidthLogical >= 0.0f ? Length::Logical(State.LastWidthLogical) : Length{};
    const Length HeightNow = State.LastHeightLogical >= 0.0f ? Length::Logical(State.LastHeightLogical) : Length{};
    RetargetTrack(State.Width, Style.Width, WidthNow, WidthSpec, Style.Width.IsSet() && WidthNow.IsSet());
    RetargetTrack(State.Height, Style.Height, HeightNow, HeightSpec, Style.Height.IsSet() && HeightNow.IsSet());
    RetargetTrack(State.FlexGrow, Style.FlexGrow, Lerp(State.FlexGrow.From, State.FlexGrow.Target, State.FlexGrow.Progress()),
                  GrowSpec, true);
}

void Box::AdvanceTransitions(Render::Renderer& Renderer) {
    if (!Eased) {
        return;
    }
    TransitionState&    State = *Eased;
    const std::uint64_t Frame = Renderer.GetFrameIndex();
    if (Frame == 0) {
        // No frame clock to move them on by: show every value as it's styled.
        State.Width.Timeline = State.Height.Timeline = State.FlexGrow.Timeline = {};
        State.Background.Timeline = State.Opacity.Timeline = {};
    } else if (Frame != State.LastAdvancedFrame) {
        const float Delta = Renderer.GetFrameDeltaSeconds();
        State.Width.Timeline.Update(Delta);
        State.Height.Timeline.Update(Delta);
        State.FlexGrow.Timeline.Update(Delta);
        State.Background.Timeline.Update(Delta);
        State.Opacity.Timeline.Update(Delta);
    }
    State.LastAdvancedFrame = Frame;

    // The background follows the interaction state, which settles after layout, so it
    // and opacity retarget here rather than in Measure: after advancing, so a change
    // shows its starting value this frame, as a size change does.
    RetargetTrack(State.Background, BackgroundForState(), DisplayedBackground(),
                  FindTransition(Style, TransitionProperty::BackgroundColor), true);
    RetargetTrack(State.Opacity, Style.Opacity, DisplayedOpacity(), FindTransition(Style, TransitionProperty::Opacity),
                  true);
}

Render::Color Box::DisplayedBackground() const {
    if (!Eased || !Eased->Background.Known || !FindTransition(Style, TransitionProperty::BackgroundColor)) {
        return BackgroundForState();
    }
    const EasedTrack<Render::Color>& Track = Eased->Background;
    return Anim::LerpColor(Track.From, Track.Target, Track.Progress());
}

float Box::DisplayedOpacity() const {
    if (!Eased || !Eased->Opacity.Known || !FindTransition(Style, TransitionProperty::Opacity)) {
        return Style.Opacity;
    }
    const EasedTrack<float>& Track = Eased->Opacity;
    return Lerp(Track.From, Track.Target, Track.Progress());
}

void Box::OnShown() {
    if (Eased) {
        Eased->Entered = false;
    }
}

void Box::Arrange(Rect FinalRectLogical) {
    ArrangedRect = FinalRectLogical;

    const ContainingBlockScope Block(*this, PaddingRectFrom(FinalRectLogical));
    const Rect                 Content = ContentRectFrom(FinalRectLogical);

    // An Absolute or Fixed child goes against its containing block. On an axis whose
    // offsets are both auto it stays where it would have been in flow, its margin box
    // starting at At, as CSS's static position does.
    auto PlaceOutOfFlow = [](WidgetBase& Child, Point At) {
        const EdgeInsets Margin = Child.GetMarginLogical();
        ArrangeOutOfFlow(Child, {At.X + Margin.Left, At.Y + Margin.Top});
    };

    if (Layout == LayoutMode::None) {
        // In-flow children, if any, are not laid out (footgun accepted); out-of-flow
        // ones still are, against their containing block.
        for (auto& Child : Children) {
            if (IsOutOfFlow(*Child)) {
                PlaceOutOfFlow(*Child, {Content.X, Content.Y});
            }
        }
        return;
    }

    // Places a child along the CROSS axis, returning its {position, extent}.
    auto PlaceCross = [&](float ContentStart, float ContentExtent, float ChildExtent,
                          float MarginLead, float MarginTrail) -> std::pair<float, float> {
        const float Available = NonNegative(ContentExtent - MarginLead - MarginTrail);
        switch (CrossAlignment) {
        case CrossAlign::Start:
            return {ContentStart + MarginLead, ChildExtent};
        case CrossAlign::Center:
            return {ContentStart + MarginLead + (Available - ChildExtent) / 2.0f, ChildExtent};
        case CrossAlign::End:
            return {ContentStart + ContentExtent - MarginTrail - ChildExtent, ChildExtent};
        case CrossAlign::Stretch:
            return {ContentStart + MarginLead, Available};
        }
        return {ContentStart + MarginLead, ChildExtent};
    };

    if (Layout == LayoutMode::FixedLeadingStack) {
        // Mirrors FixedLeadingStrip::Arrange: Leading (children[0]) gets exactly
        // LeadingExtentLogical of the main axis (clamped to Content.H so it can't
        // overflow a too-small container), Fill (every child after it) gets
        // whatever's left once Leading + ChildGap are subtracted -- neither slot is
        // sized by its own measured main-axis extent the way an ordinary
        // VerticalStack child is. Only the cross axis (width) still goes through
        // PlaceCross/CrossAlignment as usual, which needs each child's own
        // measured cross-axis size -- hence the Measure() call below, whose Y
        // component is otherwise discarded in favour of the slot's own height.
        const float LeadingExtent = std::min(NonNegative(LeadingExtentLogical), Content.H);
        const float FillTop       = std::min(Content.H, LeadingExtent + ChildGap);
        const float FillHeight    = NonNegative(Content.H - FillTop);

        bool LeadingSeen = false;
        for (std::size_t Index = 0; Index < Children.size(); ++Index) {
            WidgetBase* Child = Children[Index].get();
            if (IsOutOfFlow(*Child)) {
                PlaceOutOfFlow(*Child, {Content.X, LeadingSeen ? Content.Y + FillTop : Content.Y});
                continue;
            }
            if (!Child->GetIsVisible()) {
                continue;
            }
            const EdgeInsets Margin     = Child->GetMarginLogical();
            const float      SlotTop    = LeadingSeen ? (Content.Y + FillTop) : Content.Y;
            const float      SlotHeight = LeadingSeen ? FillHeight : LeadingExtent;

            const Point ChildAvailable{NonNegative(Content.W - Margin.Left - Margin.Right),
                                       NonNegative(SlotHeight - Margin.Top - Margin.Bottom)};
            const Point Desired = Child->Measure(ChildAvailable);

            auto [CrossPos, CrossExtent] = PlaceCross(Content.X, Content.W, Desired.X, Margin.Left, Margin.Right);
            Child->Arrange(OffsetInFlow(
                *Child,
                {CrossPos, SlotTop + Margin.Top, CrossExtent, NonNegative(SlotHeight - Margin.Top - Margin.Bottom)},
                Content));
            LeadingSeen = true;
        }
        return;
    }

    const bool Vertical = (Layout == LayoutMode::VerticalStack);
    const std::size_t Count = Children.size();
    const Point ContentSize{Content.W, Content.H};
    const std::vector<float> GrowExtents = GrowExtentsFor(Children, Vertical, ContentSize, ChildGap);

    float StartOffset   = 0.0f;
    float ExtraGapPerGap = 0.0f;
    if (JustifyContentMode != Justify::Start && GrowExtents.empty()) {
        float       TotalChildrenMain = 0.0f;
        std::size_t VisibleCount      = 0;
        for (std::size_t Index = 0; Index < Count; ++Index) {
            WidgetBase* Child = Children[Index].get();
            if (!InFlow(*Child)) {
                continue;
            }
            const EdgeInsets Margin = Child->GetMarginLogical();
            const Point ChildAvailable{NonNegative(Content.W - Margin.Left - Margin.Right),
                                       NonNegative(Content.H - Margin.Top - Margin.Bottom)};
            const Point Desired = Child->Measure(ChildAvailable);
            TotalChildrenMain += (Vertical ? Desired.Y + Margin.Top + Margin.Bottom
                                            : Desired.X + Margin.Left + Margin.Right);
            ++VisibleCount;
        }

        const float ContentMain = Vertical ? Content.H : Content.W;
        const float TotalGaps =
            VisibleCount > 1 ? static_cast<float>(VisibleCount - 1) * ChildGap : 0.0f;
        const float FreeSpace = NonNegative(ContentMain - TotalChildrenMain - TotalGaps);

        switch (JustifyContentMode) {
        case Justify::Start:
            break; // unreachable (guarded above); keeps the switch exhaustive
        case Justify::Center:
            StartOffset = FreeSpace / 2.0f;
            break;
        case Justify::End:
            StartOffset = FreeSpace;
            break;
        case Justify::SpaceBetween:
            ExtraGapPerGap = VisibleCount > 1 ? FreeSpace / static_cast<float>(VisibleCount - 1) : 0.0f;
            break;
        }
    }

    float Cursor = (Vertical ? Content.Y : Content.X) + StartOffset;
    bool  AnyVisible = false;

    for (std::size_t Index = 0; Index < Count; ++Index) {
        WidgetBase* Child = Children[Index].get();
        if (IsOutOfFlow(*Child)) {
            const float Next = AnyVisible ? Cursor + ChildGap + ExtraGapPerGap : Cursor;
            PlaceOutOfFlow(*Child, Vertical ? Point{Content.X, Next} : Point{Next, Content.Y});
            continue;
        }
        if (!Child->GetIsVisible()) {
            continue;
        }
        const EdgeInsets Margin = Child->GetMarginLogical();

        const float GrowExtent = GrowExtentAt(GrowExtents, Index);
        const Point Desired = Child->Measure(StackChildAvailable(ContentSize, Margin, Vertical, GrowExtent));

        if (AnyVisible) {
            Cursor += ChildGap + ExtraGapPerGap;
        }

        if (Vertical) {
            auto [CrossPos, CrossExtent] =
                PlaceCross(Content.X, Content.W, Desired.X, Margin.Left, Margin.Right);
            const float Top = Cursor + Margin.Top;
            const float Height = GrowExtent >= 0.0f ? GrowExtent : Desired.Y;
            Child->Arrange(OffsetInFlow(*Child, {CrossPos, Top, CrossExtent, Height}, Content));
            Cursor = Top + Height + Margin.Bottom;
        } else {
            auto [CrossPos, CrossExtent] =
                PlaceCross(Content.Y, Content.H, Desired.Y, Margin.Top, Margin.Bottom);
            const float Left = Cursor + Margin.Left;
            const float Width = GrowExtent >= 0.0f ? GrowExtent : Desired.X;
            Child->Arrange(OffsetInFlow(*Child, {Left, CrossPos, Width, CrossExtent}, Content));
            Cursor = Left + Width + Margin.Right;
        }

        AnyVisible = true;
    }
}

const Platform::InputState& InputForPointerEvents(PointerEvents Mode, const Platform::InputState& Input,
                                                  Platform::InputState& Storage) {
    const bool PassThrough = Mode == PointerEvents::None || (Mode == PointerEvents::Inherit && Input.PointerPassThrough);
    if (PassThrough == Input.PointerPassThrough) {
        return Input;
    }
    Storage                    = Input;
    Storage.PointerPassThrough = PassThrough;
    return Storage;
}

bool DispatchToChild(WidgetBase& Child, const Platform::InputState& Input) {
    Platform::InputState        PointerEventsInput;
    Platform::InputState        KeysInput;
    const BoxStyle*             ChildStyle = Child.GetBoxStyle();
    const Platform::InputState& WithPointerEvents =
        ChildStyle ? InputForPointerEvents(ChildStyle->PointerEventsMode, Input, PointerEventsInput) : Input;
    const Platform::InputState& Given = KeyDispatch::ForChild(WithPointerEvents, KeysInput);
    const bool                  Took  = Child.UpdateInteractionState(Given);
    KeyDispatch::AfterChild(Child, Given);
    return Took;
}

bool Box::UpdateInteractionState(const Platform::InputState& Input) {
    if (StackingContext::IsLayeredOut(*this)) {
        return false; // its stacking context hands it input itself, in paint order
    }

    // Only pay for an InputState copy when this Box is actually transformed -- the
    // overwhelmingly common case is Style.Transform.IsIdentity(), where Input is
    // used (and passed to children) completely unchanged.
    Platform::InputState TransformedInput;
    const Platform::InputState* Effective = &Input;
    if (!Style.Transform.IsIdentity()) {
        TransformedInput = Input;
        const Point Pivot{ArrangedRect.X + ArrangedRect.W * 0.5f, ArrangedRect.Y + ArrangedRect.H * 0.5f};
        TransformedInput.MousePosition = InverseTransformPoint(Input.MousePosition, Pivot, Style.Transform);
        Effective = &TransformedInput;
    }
    Platform::InputState PointerEventsInput;
    Effective = &InputForPointerEvents(Style.PointerEventsMode, *Effective, PointerEventsInput);

    if (Selection) {
        Selection->Update(*this, ArrangedRect, *Effective);
    }

    // Reverse paint order: the layered descendants over the ordinary children, the
    // children, then the layered descendants under them.
    const KeyDispatch       Keys(*this, *Effective);
    StackingContext         Context(*this, StackingContext::Pass::Input);
    Platform::OccludedInput Occluded(*Effective);
    bool                    ChildConsumed = Context.DispatchAbove(*Effective);
    for (auto Iterator = Children.rbegin(); Iterator != Children.rend(); ++Iterator) {
        if (!(*Iterator)->GetIsVisible() || StackingContext::IsLayeredOut(**Iterator)) {
            continue; // a hidden subtree can't consume input
        }
        if (DispatchToChild(**Iterator, ChildConsumed ? Occluded.Get() : *Effective)) {
            ChildConsumed = true;
        }
    }
    if (Context.DispatchBelow(*Effective, ChildConsumed)) {
        ChildConsumed = true;
    }

    // A layered descendant that took the pointer is still over this Box, as a hovered
    // element is over its ancestors in CSS: this Box keeps its hover, though the press
    // is the descendant's.
    Platform::InputState Uncovered;
    if (Effective->PointerOccluded && StackingContext::IsOnPointerPath(*this)) {
        Uncovered                 = *Effective;
        Uncovered.PointerOccluded = false;
        Effective                 = &Uncovered;
        ChildConsumed             = true;
    }

    if (!IsEnabled) {
        CurrentState  = InteractionState::Disabled;
        PressedInside = false;
        if (WasHovered && OnUnhovered) {
            OnUnhovered(); // a widget that goes inert while hovered still ends its hover
        }
        WasHovered = false;
        return ChildConsumed;
    }

    // A Box with none of the generic callbacks AND no interaction-state colours set
    // is an inert layout container -- exactly as before this API existed -- so skip
    // hit-testing it rather than paying for a hit test on every plain Box
    // in the tree, every frame. A Box that only sets :hover-equivalent colours (no
    // onPress) still needs hit-testing to know when to use them.
    const bool HasCallbacks = OnPressed || OnReleased || OnHovered || OnUnhovered || OnFocused || OnChanged;
    const bool HasInteractionColors =
        Style.ColorBackgroundHovered.A != 0 || Style.ColorBackgroundPressed.A != 0 ||
        Style.ColorBorderHovered.A != 0 || Style.ColorBorderPressed.A != 0;
    if (!HasCallbacks && !HasInteractionColors) {
        CurrentState  = InteractionState::Default;
        PressedInside = false;
        WasHovered    = false;
        return ChildConsumed;
    }

    // Hovered whenever the pointer is over this Box, even while a child has it, as
    // CSS's :hover covers ancestors; but a press a child took isn't also this Box's.
    const bool Hovered  = Effective->IsPointerOver(ArrangedRect);
    const bool Pressed  = Effective->MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Effective->MouseButtonDown[LeftButton];
    const bool Released = Effective->MouseButtonReleasedThisFrame[LeftButton];

    if (Hovered && OnHovered) {
        OnHovered();
    }
    if (!Hovered && WasHovered && OnUnhovered) {
        OnUnhovered();
    }
    WasHovered = Hovered;
    if (Pressed && Hovered && !ChildConsumed) {
        PressedInside = true;
        if (OnPressed) {
            OnPressed();
        }
    }
    if (Released) {
        if (PressedInside && OnReleased) {
            OnReleased();
        }
        PressedInside = false;
    }

    if (PressedInside && Down && Hovered) {
        CurrentState = InteractionState::Pressed;
    } else if (Hovered) {
        CurrentState = InteractionState::Hovered;
    } else {
        CurrentState = InteractionState::Default;
    }

    return ChildConsumed || Hovered || (PressedInside && Down);
}

Render::Color Box::BackgroundForState() const {
    switch (CurrentState) {
    case InteractionState::Hovered:
        return Style.ColorBackgroundHovered.A != 0 ? Style.ColorBackgroundHovered : Style.ColorBackground;
    case InteractionState::Pressed:
        return Style.ColorBackgroundPressed.A != 0 ? Style.ColorBackgroundPressed : Style.ColorBackground;
    case InteractionState::Disabled:
        return Style.ColorBackgroundDisabled.A != 0 ? Style.ColorBackgroundDisabled : Style.ColorBackground;
    case InteractionState::Default:
    default:
        return Style.ColorBackground;
    }
}

Render::Color Box::GradientTopForState() const {
    switch (CurrentState) {
    case InteractionState::Hovered:
        return Style.GradientTopHovered.A != 0 ? Style.GradientTopHovered : Style.GradientTop;
    case InteractionState::Pressed:
        return Style.GradientTopPressed.A != 0 ? Style.GradientTopPressed : Style.GradientTop;
    case InteractionState::Disabled:
    case InteractionState::Default:
    default:
        return Style.GradientTop;
    }
}

Render::Color Box::GradientBottomForState() const {
    switch (CurrentState) {
    case InteractionState::Hovered:
        return Style.GradientBottomHovered.A != 0 ? Style.GradientBottomHovered : Style.GradientBottom;
    case InteractionState::Pressed:
        return Style.GradientBottomPressed.A != 0 ? Style.GradientBottomPressed : Style.GradientBottom;
    case InteractionState::Disabled:
    case InteractionState::Default:
    default:
        return Style.GradientBottom;
    }
}

Render::Color Box::BorderForState() const {
    switch (CurrentState) {
    case InteractionState::Hovered:
        return Style.ColorBorderHovered.A != 0 ? Style.ColorBorderHovered : Style.ColorBorder;
    case InteractionState::Pressed:
        return Style.ColorBorderPressed.A != 0 ? Style.ColorBorderPressed : Style.ColorBorder;
    case InteractionState::Disabled:
        return Style.ColorBorderDisabled.A != 0 ? Style.ColorBorderDisabled : Style.ColorBorder;
    case InteractionState::Default:
    default:
        return Style.ColorBorder;
    }
}

void Box::Draw(Render::Renderer& Renderer) {
    if (StackingContext::IsLayeredOut(*this)) {
        return; // its stacking context paints it later, in z order
    }
    AdvanceTransitions(Renderer);
    const float Opacity = DisplayedOpacity();
    if (Opacity <= 0.0f) {
        return;
    }

    // A non-identity Transform, or an Opacity below 1, redirects this Box's own paint
    // plus every descendant's into an offscreen texture, composited back as one
    // scaled/rotated/translated, translucent blit -- see Renderer::PushTransform.
    // Layout (ArrangedRect, children's rects) is untouched either way, matching CSS:
    // transform never reflows.
    const bool Transformed = !Style.Transform.IsIdentity() || Opacity < 1.0f;
    if (Transformed) {
        Renderer.PushTransform(ArrangedRect, Style.Transform, Opacity);
    }
    StackingContext Context(*this, StackingContext::Pass::Paint);

    // A shadow is drawn just before this Box's own fill, so it reads as sitting
    // behind it -- matching pharos-proto's GradientButton::Draw() order (shadow,
    // then gradient, then border).
    if (Style.ShadowBlurRadiusLogical > 0.0f) {
        Renderer.DrawDropShadow(ArrangedRect, Style.ShadowColor, Style.ShadowBlurRadiusLogical, Style.BorderRadius);
    }

    // GradientTop set (alpha != 0) wins over the flat ColorBackground fill --
    // gradient takes priority, avoiding two fills when a consumer sets both.
    const Render::Color Background = DisplayedBackground();
    const Render::Color GradientTop = GradientTopForState();
    if (GradientTop.A != 0) {
        Renderer.DrawGradientRect(ArrangedRect, GradientTop, GradientBottomForState(), Style.BorderRadius);
    } else if (Background.A != 0) {
        Renderer.DrawFilledRect(ArrangedRect, Background, Style.BorderRadius);
    }
    const Render::Color Border = BorderForState();
    if (Style.BorderWidth > 0.0f && Border.A != 0) {
        Renderer.DrawRectOutline(ArrangedRect, Border, Style.BorderWidth, Style.BorderRadius);
    }

    DrawContent(Renderer, ContentRectFrom(ArrangedRect));
    if (Selection) {
        Selection->DrawGaps(Renderer);
    }

    // CSS's paint order: layered descendants with a negative z-index, the ordinary
    // children in tree order, then the other layered descendants.
    Context.PaintBelow(Renderer);
    for (auto& Child : Children) {
        if (!Child->GetIsVisible() || StackingContext::IsLayeredOut(*Child)) {
            continue;
        }
        Child->Draw(Renderer);
    }
    Context.PaintAbove(Renderer);

    if (Transformed) {
        Renderer.PopTransform();
    }
}

Box::Builder::Builder() : Owned(std::make_unique<Box>()) {}

Box::Builder& Box::Builder::className(std::string Value) {
    Owned->ClassName = std::move(Value);
    return *this;
}

Box::Builder& Box::Builder::child(std::unique_ptr<WidgetBase> Child) {
    Owned->AddChild(std::move(Child));
    return *this;
}

Box::Builder& Box::Builder::children(std::vector<std::unique_ptr<WidgetBase>> Kids) {
    for (auto& Kid : Kids) {
        Owned->AddChild(std::move(Kid));
    }
    return *this;
}

Box::Builder& Box::Builder::onPress(std::function<void()> Handler) {
    Owned->OnPressed = std::move(Handler);
    return *this;
}

Box::Builder& Box::Builder::onRelease(std::function<void()> Handler) {
    Owned->OnReleased = std::move(Handler);
    return *this;
}

Box::Builder& Box::Builder::onHover(std::function<void()> Handler) {
    Owned->OnHovered = std::move(Handler);
    return *this;
}

Box::Builder& Box::Builder::onFocus(std::function<void()> Handler) {
    Owned->OnFocused = std::move(Handler);
    return *this;
}

Box::Builder& Box::Builder::onChange(std::function<void()> Handler) {
    Owned->OnChanged = std::move(Handler);
    return *this;
}

Box::Builder& Box::Builder::onKeyPressed(std::function<void(Platform::Key)> Handler) {
    Owned->OnKeyPressed = std::move(Handler);
    return *this;
}

std::unique_ptr<Box> Box::Builder::build() { return std::move(Owned); }

} // namespace Penumbra::Widgets
