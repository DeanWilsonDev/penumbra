#pragma once

#include "Penumbra/Anim/Animation.h"
#include "Penumbra/Widgets/Box.h"

#include <functional>

namespace Penumbra::Widgets {

// A Button is a Box plus click handling and a state-driven background fill. It has
// no built-in label: a text button is a Button with one Label child (Milestone 5).
class Button : public Box {
public:
    std::function<void()> OnClicked;

    // Time constant (seconds) for easing the background between state colours. 0 (the
    // default) means instant — Penumbra holds no opinion; the demo supplies the value.
    float BackgroundTransitionSeconds{0.0f};

    // Pours a resolved ButtonStyle into this widget: the whole BoxStyle slice (box
    // model, default background, and interaction-state colours) into Box::Style.
    // ColorLabel is intentionally ignored — the resolver applies it to a Label child.
    void ApplyStyle(const ButtonStyle& Style);

    bool UpdateInteractionState(const Platform::InputState&) override;
    void Draw(Render::Renderer&) override;

    InteractionState GetInteractionState() const { return CurrentState; }

private:
    Render::Color BackgroundForState() const;

    InteractionState    CurrentState{InteractionState::Default};
    bool                PressedInside{false}; // a press began on this button and is held
    Anim::AnimatedColor BackgroundAnim;       // eases toward the current state's colour
};

} // namespace Penumbra::Widgets
