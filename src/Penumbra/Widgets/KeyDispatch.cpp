#include "Penumbra/Widgets/KeyDispatch.h"

#include "Penumbra/Widgets/Length.h"
#include "Penumbra/Widgets/WidgetBase.h"

#include <algorithm>

namespace Penumbra::Widgets {

namespace {

// The pass being dispatched right now, if one is.
struct Pass {
    bool                       Open{false};
    WidgetBase*                Root{nullptr};
    std::vector<Platform::Key>      Keys;
    std::vector<Platform::KeyEvent> Events;
    std::vector<Platform::Key>      Used;
    WidgetBase*                Target{nullptr};
    bool                       Withheld{false};
};
Pass Current;

bool CoversWindow(Rect Area) {
    const Point Viewport = GetLayoutViewportLogical();
    if (Viewport.X <= 0.0f || Viewport.Y <= 0.0f) {
        return false; // the host hasn't said how big the window is
    }
    constexpr float Slack = 0.5f;
    return Area.X <= Slack && Area.Y <= Slack && Area.X + Area.W >= Viewport.X - Slack &&
           Area.Y + Area.H >= Viewport.Y - Slack;
}

} // namespace

KeyDispatch::KeyDispatch(WidgetBase& Root, const Platform::InputState& Input) {
    if (Current.Open) {
        return;
    }
    Outermost = true;
    Current   = Pass{true, &Root, Input.KeysPressedThisFrame, Input.KeyEvents, {}, nullptr, false};
    if (Current.Events.empty()) {
        for (const Platform::Key Key : Current.Keys) {
            Current.Events.push_back(Platform::KeyEvent{Key, true, false, Input.ModifierState});
        }
    }
}

KeyDispatch::~KeyDispatch() {
    if (!Outermost) {
        return;
    }
    Pass Finished = std::move(Current);
    Current       = Pass{};

    // The root is under everything, so a full-window widget holds keys back from it too.
    WidgetBase* Target = Finished.Target;
    if (!Target && (Finished.Root->OnKeyPressed || Finished.Root->OnKeyEvent) && !Finished.Withheld) {
        Target = Finished.Root;
    }
    if (!Target) {
        return;
    }
    std::vector<Platform::Key> UsedForEvents = Finished.Used;
    if (Target->OnKeyPressed) {
        for (const Platform::Key Key : Finished.Keys) {
            // Each use a focused widget marked accounts for one press of that key.
            const auto Use = std::find(Finished.Used.begin(), Finished.Used.end(), Key);
            if (Use != Finished.Used.end()) {
                Finished.Used.erase(Use);
                continue;
            }
            Target->OnKeyPressed(Key);
        }
    }
    if (Target->OnKeyEvent) {
        for (const Platform::KeyEvent& Event : Finished.Events) {
            if (Event.Down) {
                const auto Use = std::find(UsedForEvents.begin(), UsedForEvents.end(), Event.Code);
                if (Use != UsedForEvents.end()) {
                    UsedForEvents.erase(Use);
                    continue;
                }
            }
            Target->OnKeyEvent(Event);
        }
    }
}

void KeyDispatch::MarkUsed(Platform::Key Key) {
    if (Current.Open) {
        Current.Used.push_back(Key);
    }
}

const Platform::InputState& KeyDispatch::ForChild(const Platform::InputState& Input, Platform::InputState& Storage) {
    if (!Current.Withheld ||
        (Input.KeysPressedThisFrame.empty() && Input.KeyEvents.empty() && Input.TextInputThisFrame.empty())) {
        return Input;
    }
    Storage = Input;
    Storage.KeysPressedThisFrame.clear();
    Storage.KeyEvents.clear();
    Storage.TextInputThisFrame.clear();
    return Storage;
}

void KeyDispatch::AfterChild(WidgetBase& Child, const Platform::InputState& Given) {
    if (!Current.Open) {
        return;
    }
    // The first to finish that the keys reached is the topmost. A widget handed its input
    // before a full-window descendant (its backdrop, say) finished still had the keys.
    // A widget claims the frame only if it can act on it: presses for OnKeyPressed,
    // presses or releases for OnKeyEvent. Otherwise a release-only frame would stop at a
    // press-only widget instead of reaching the one under it that listens for releases.
    const bool HasPresses = !Given.KeysPressedThisFrame.empty();
    const bool HasEvents  = HasPresses || !Given.KeyEvents.empty();
    if (!Current.Target && ((Child.OnKeyPressed && HasPresses) || (Child.OnKeyEvent && HasEvents))) {
        Current.Target = &Child;
    }
    if (!Given.PointerPassThrough && CoversWindow(Child.GetArrangedRect())) {
        Current.Withheld = true; // everything after this is painted under it
    }
}

} // namespace Penumbra::Widgets
