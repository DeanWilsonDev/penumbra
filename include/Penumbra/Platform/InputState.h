#pragma once

#include "Penumbra/Geometry.h"

#include <optional>
#include <string>
#include <vector>

namespace Penumbra::Platform {

// Penumbra's own small keycode vocabulary. Only covers what Penumbra's own widgets
// and documented use cases need — TextInput's editing/navigation/clipboard keys, plus
// Backspace/Escape for consumers implementing zoom-out-style interactions. Anything not
// mapped comes through as Key::Unknown rather than growing this to match SDL's keyspace.
enum class Key {
    Unknown,
    Left, Right, Up, Down,
    Home, End, Backspace, Delete, Escape, Enter, Tab,
    A, C, V, X,
};

struct Modifiers {
    bool Shift{false};
    bool Ctrl{false};
    bool Alt{false};
    bool Super{false};
    bool Shortcut{false};
};

// A snapshot of all OS input for a single frame, built by PlatformWindow.
struct InputState {
    Point MousePosition{};                            // logical pixels
    bool  MouseButtonDown[3]{};                        // left, middle, right — current
    bool  MouseButtonPressedThisFrame[3]{};
    bool  MouseButtonReleasedThisFrame[3]{};
    float MouseWheelDelta{0.0f};                       // accumulated this frame (vertical)
    // Horizontal wheel/trackpad delta, accumulated this frame -- SDL's own Event.wheel.x
    // (a trackpad's own horizontal swipe on most platforms). Needed by
    // ScrollablePanel::ScrollDirection::Horizontal (give-code-blocks-their-own-
    // horizontally-scrollable-non-wrapping-frame-in-the-card-modal, Cairn) -- a
    // horizontally-scrolling viewport wants this instead of MouseWheelDelta above.
    float MouseWheelDeltaX{0.0f};

    std::string       TextInputThisFrame;              // from SDL text-input events
    std::vector<Key>  KeysPressedThisFrame;            // arrows, backspace, delete, etc.
    Modifiers         ModifierState{};

    // Frame timing carried alongside input, so per-frame widget updates (e.g.
    // animations) can advance without changing the WidgetBase interface.
    float DeltaTimeSeconds{0.0f};

    // Set when something drawn above has already taken the pointer this frame. Every
    // widget still gets its UpdateInteractionState each frame: a container hands the
    // children it reaches after one of them consumed the input this input with the
    // pointer occluded (and does the same for all of them when the pointer isn't over
    // it), instead of skipping them. An occluded widget hovers nothing and starts no
    // press, so a hover it showed clears, but a drag already under way carries on
    // with the real MousePosition, and keys still reach whatever has focus.
    bool PointerOccluded{false};

    // Whether the pointer is over Area and nothing above has taken it. Use this, not
    // a raw MousePosition test, for hover and for starting a press.
    bool IsPointerOver(Rect Area) const {
        return !PointerOccluded && MousePosition.X >= Area.X && MousePosition.X < Area.X + Area.W &&
               MousePosition.Y >= Area.Y && MousePosition.Y < Area.Y + Area.H;
    }
};

// The input a container hands the children it reaches after one of them took the
// pointer: Source with PointerOccluded set. It copies Source at most once, and not at
// all when Source is occluded already.
class OccludedInput {
public:
    explicit OccludedInput(const InputState& InSource) : Source(InSource) {}

    const InputState& Get() {
        if (Source.PointerOccluded) {
            return Source;
        }
        if (!Copy) {
            Copy = Source;
            Copy->PointerOccluded = true;
        }
        return *Copy;
    }

private:
    const InputState&         Source;
    std::optional<InputState> Copy;
};

} // namespace Penumbra::Platform
