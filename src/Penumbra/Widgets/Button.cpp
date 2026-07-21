#include "Penumbra/Widgets/Button.h"

namespace Penumbra::Widgets {

namespace {
bool PointInRect(Point Point, Rect Rect) {
    return Point.X >= Rect.X && Point.X < Rect.X + Rect.W &&
           Point.Y >= Rect.Y && Point.Y < Rect.Y + Rect.H;
}
constexpr int LeftButton = 0;
} // namespace

void Button::ApplyStyle(const ButtonStyle& InStyle) {
    // Box model + default background + interaction-state colours all live on
    // BoxStyle now (docs/lustre_style_gaps_requirements.md #1).
    Style = static_cast<const BoxStyle&>(InStyle);
}

bool Button::UpdateInteractionState(const Platform::InputState& Input) {
    bool Consumed = false;

    if (!IsEnabled) {
        CurrentState = InteractionState::Disabled;
        PressedInside = false;
    } else {
        const bool Hovered = PointInRect(Input.MousePosition, ArrangedRect);
        const bool Down     = Input.MouseButtonDown[LeftButton];
        const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
        const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];

        if (Pressed && Hovered) {
            PressedInside = true;
        }

        // Click fires on release, but only if the press both began and ended on the
        // button — dragging off and releasing cancels it.
        bool Clicked = false;
        if (Released) {
            Clicked = PressedInside && Hovered;
            PressedInside = false;
        }

        if (PressedInside && Down && Hovered) {
            CurrentState = InteractionState::Pressed;
        } else if (Hovered) {
            CurrentState = InteractionState::Hovered;
        } else {
            CurrentState = InteractionState::Default;
        }

        if (Clicked && OnClicked) {
            OnClicked();
        }

        Consumed = Hovered || (PressedInside && Down);
    }

    // Advance the background easing every frame, in every state, so transitions
    // (including enable/disable) animate smoothly.
    BackgroundAnim.Animate(BackgroundForState(), Input.DeltaTimeSeconds, BackgroundTransitionSeconds);
    return Consumed;
}

Render::Color Button::BackgroundForState() const {
    switch (CurrentState) {
    case InteractionState::Hovered:  return Style.ColorBackgroundHovered;
    case InteractionState::Pressed:  return Style.ColorBackgroundPressed;
    case InteractionState::Disabled: return Style.ColorBackgroundDisabled;
    case InteractionState::Default:
    default:                         return Style.ColorBackground;
    }
}

void Button::Draw(Render::Renderer& Renderer) {
    // Swap in the eased state background, reuse Box's drawing, then restore.
    const Render::Color Chosen = BackgroundAnim.Initialised ? BackgroundAnim.Value() : BackgroundForState();
    const Render::Color Saved = Style.ColorBackground;
    Style.ColorBackground = Chosen;
    Box::Draw(Renderer);
    Style.ColorBackground = Saved;
}

} // namespace Penumbra::Widgets
