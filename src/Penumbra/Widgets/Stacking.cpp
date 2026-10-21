#include "Penumbra/Widgets/Stacking.h"

#include "Penumbra/Widgets/Box.h"

#include <algorithm>
#include <cassert>

namespace Penumbra::Widgets {

namespace {

// The contexts being painted or dispatched right now, innermost last.
std::vector<StackingContext*> ActiveContexts;
std::uint64_t                 NextSerial{1};

std::optional<Rect> Intersect(std::optional<Rect> A, std::optional<Rect> B) {
    if (!A) {
        return B;
    }
    if (!B) {
        return A;
    }
    const float Left   = std::max(A->X, B->X);
    const float Top    = std::max(A->Y, B->Y);
    const float Right  = std::min(A->X + A->W, B->X + B->W);
    const float Bottom = std::min(A->Y + A->H, B->Y + B->H);
    return Rect{Left, Top, std::max(0.0f, Right - Left), std::max(0.0f, Bottom - Top)};
}

bool Contains(Rect Area, Point P) { return P.X >= Area.X && P.X < Area.X + Area.W && P.Y >= Area.Y && P.Y < Area.Y + Area.H; }

Position PositionOf(const WidgetBase& Widget) {
    const BoxStyle* Style = Widget.GetBoxStyle();
    return Style ? Style->PositionMode : Position::Static;
}

std::optional<int> ZIndexOf(const WidgetBase& Widget) {
    const BoxStyle* Style = Widget.GetBoxStyle();
    return Style ? Style->ZIndex : std::nullopt;
}

PointerEvents PointerEventsOf(const WidgetBase& Widget) {
    const BoxStyle* Style = Widget.GetBoxStyle();
    return Style ? Style->PointerEventsMode : PointerEvents::Inherit;
}

// The pointer-events a widget hands its subtree: its own, or, inheriting, its parent's
// (nullopt being whatever the context's root was handed).
std::optional<bool> PassThroughFor(PointerEvents Mode, std::optional<bool> Inherited) {
    switch (Mode) {
    case PointerEvents::None: return true;
    case PointerEvents::Auto: return false;
    case PointerEvents::Inherit: break;
    }
    return Inherited;
}

} // namespace

struct StackingContext::Walk {
    Clip                           ForChildren;   // how the current widget's in-flow children are clipped
    std::optional<Clip>            BlockChildren; // how the nearest positioned ancestor's children are, if one is inside
    std::optional<bool>            PassThrough;   // the pointer-events the current widget hands down
    std::vector<const WidgetBase*> Ancestors;
    std::size_t                    Order{0};
};

bool StackingContext::FormsContext(const WidgetBase& Widget) {
    const Position Mode = PositionOf(Widget);
    return ZIndexOf(Widget).has_value() || Mode == Position::Fixed || Mode == Position::Sticky ||
           Widget.PaintsAsGroup();
}

StackingContext::StackingContext(WidgetBase& InRoot, Pass InKind) : Kind(InKind), Root(&InRoot) {
    if (!ActiveContexts.empty()) {
        // Inside another context, only where that one found a nested context, so that
        // it and this agree on whose widgets are whose.
        const StackingContext& Enclosing = *ActiveContexts.back();
        if (InRoot.LayeredBy != Enclosing.Serial || !InRoot.LayeredAsContext) {
            return;
        }
    }
    Active = true;
    Serial = NextSerial++;

    Walk State;
    State.ForChildren = Clip{false, InRoot.GetChildClipRect()};
    if (PositionOf(InRoot) != Position::Static) {
        State.BlockChildren = State.ForChildren;
    }
    State.PassThrough = PassThroughFor(PointerEventsOf(InRoot), std::nullopt);
    Collect(InRoot, State);

    // Tree order within each z-index, as collected.
    std::stable_sort(Entries.begin(), Entries.end(), [](const Entry& A, const Entry& B) { return A.Z < B.Z; });
    ActiveContexts.push_back(this);
}

StackingContext::~StackingContext() {
    if (!Active) {
        return;
    }
    assert(!ActiveContexts.empty() && ActiveContexts.back() == this && "stacking contexts must end in reverse order");
    ActiveContexts.pop_back();
}

void StackingContext::Collect(const WidgetBase& Parent, Walk& State) {
    for (std::size_t Index = 0; Index < Parent.GetChildCount(); ++Index) {
        WidgetBase* Child = Parent.GetChildAt(Index);
        if (!Child || !Child->GetIsVisible()) {
            continue; // a hidden subtree paints nothing and takes no input, as in CSS
        }

        const Position           Mode = PositionOf(*Child);
        const std::optional<int> Z    = ZIndexOf(*Child);

        // How the child itself is clipped: by its containing block's ancestors.
        Clip Own = State.ForChildren;
        if (Mode == Position::Fixed) {
            Own = Clip{true, std::nullopt};
        } else if (Mode == Position::Absolute) {
            Own = State.BlockChildren.value_or(Clip{false, std::nullopt});
        }

        if (Mode != Position::Static || Z || Child->PaintsAsGroup()) {
            Entries.push_back({Child, Z.value_or(0), State.Order++, Own, State.PassThrough, State.Ancestors});
            Child->LayeredBy        = Serial;
            Child->LayeredAsContext = FormsContext(*Child);
            if (Child->LayeredAsContext) {
                continue; // its own context paints its subtree
            }
        }

        const Clip                Saved      = State.ForChildren;
        const std::optional<Clip> SavedBlock = State.BlockChildren;
        const std::optional<bool> SavedPass  = State.PassThrough;

        State.ForChildren = Clip{Own.Replace, Intersect(Own.Area, Child->GetChildClipRect())};
        if (Mode != Position::Static) {
            State.BlockChildren = State.ForChildren;
        }
        State.PassThrough = PassThroughFor(PointerEventsOf(*Child), State.PassThrough);
        State.Ancestors.push_back(Child);

        Collect(*Child, State);

        State.Ancestors.pop_back();
        State.ForChildren   = Saved;
        State.BlockChildren = SavedBlock;
        State.PassThrough   = SavedPass;
    }
}

void StackingContext::Paint(Render::Renderer& Renderer, const Entry& Layered) {
    bool Pushed = true;
    if (Layered.Clipping.Replace) {
        Renderer.PushClipRectUnbounded(Layered.Clipping.Area);
    } else if (Layered.Clipping.Area) {
        Renderer.PushClipRect(*Layered.Clipping.Area);
    } else {
        Pushed = false;
    }

    const WidgetBase* Previous = Current;
    Current                    = Layered.Widget;
    Layered.Widget->Draw(Renderer);
    Current = Previous;

    if (Pushed) {
        Renderer.PopClipRect();
    }
}

void StackingContext::PaintBelow(Render::Renderer& Renderer) {
    for (const Entry& Layered : Entries) {
        if (Layered.Z < 0) {
            Paint(Renderer, Layered);
        }
    }
}

void StackingContext::PaintAbove(Render::Renderer& Renderer) {
    for (const Entry& Layered : Entries) {
        if (Layered.Z >= 0) {
            Paint(Renderer, Layered);
        }
    }
}

bool StackingContext::Dispatch(const Entry& Layered, const Platform::InputState& Input, bool PointerTaken) {
    Platform::InputState Given = Input;
    if (PointerTaken || (Layered.Clipping.Area && !Contains(*Layered.Clipping.Area, Input.MousePosition))) {
        Given.PointerOccluded = true;
    }
    Given.PointerPassThrough = Layered.PassThroughAbove.value_or(Input.PointerPassThrough);
    Given.PointerPassThrough = PassThroughFor(PointerEventsOf(*Layered.Widget), Given.PointerPassThrough).value_or(false);

    const WidgetBase* Previous = Current;
    Current                    = Layered.Widget;
    bool Took                  = Layered.Widget->UpdateInteractionState(Given);
    Current                    = Previous;

    // Opaque over its whole rect, as CSS hit-tests a box, unless it lets the pointer
    // through: whatever is painted under it doesn't see the pointer there.
    if (Given.IsPointerOver(Layered.Widget->GetArrangedRect())) {
        Took = true;
    }

    if (Took && !Given.PointerOccluded && PointerPath.empty()) {
        PointerPath    = Layered.Ancestors;
        TakerUsedWheel = Layered.Widget->ConsumedWheelThisFrame();
    }
    return Took;
}

bool StackingContext::DispatchAbove(const Platform::InputState& Input) {
    bool Taken = false;
    for (auto Iterator = Entries.rbegin(); Iterator != Entries.rend(); ++Iterator) {
        if (Iterator->Z >= 0 && Dispatch(*Iterator, Input, Taken)) {
            Taken = true;
        }
    }
    return Taken;
}

bool StackingContext::DispatchBelow(const Platform::InputState& Input, bool PointerTaken) {
    bool Taken = false;
    for (auto Iterator = Entries.rbegin(); Iterator != Entries.rend(); ++Iterator) {
        if (Iterator->Z < 0 && Dispatch(*Iterator, Input, PointerTaken || Taken)) {
            Taken = true;
        }
    }
    return Taken;
}

bool StackingContext::IsLayeredOut(const WidgetBase& Widget) {
    if (ActiveContexts.empty()) {
        return false;
    }
    const StackingContext& Innermost = *ActiveContexts.back();
    return Widget.LayeredBy == Innermost.Serial && Innermost.Current != &Widget;
}

bool StackingContext::IsOnPointerPath(const WidgetBase& Widget) {
    if (ActiveContexts.empty()) {
        return false;
    }
    const std::vector<const WidgetBase*>& Path = ActiveContexts.back()->PointerPath;
    return std::find(Path.begin(), Path.end(), &Widget) != Path.end();
}

bool StackingContext::PointerTakerUsedWheel() {
    return !ActiveContexts.empty() && ActiveContexts.back()->TakerUsedWheel;
}

} // namespace Penumbra::Widgets
