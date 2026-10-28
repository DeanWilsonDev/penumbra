#pragma once

#include "Penumbra/Geometry.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Penumbra::Platform {

// Penumbra's keycode vocabulary: every key an editor's shortcuts, a viewport's camera
// controls or a text field can need. Anything SDL reports that isn't here comes through
// as Key::Unknown. KeyName gives each key its web `KeyboardEvent.key` name.
enum class Key {
    Unknown,
    Left, Right, Up, Down,
    Home, End, PageUp, PageDown, Insert, Backspace, Delete, Escape, Enter, Tab, Space,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Apostrophe, Grave, Comma, Period, Slash,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt, LeftSuper, RightSuper,
    CapsLock, PrintScreen, Pause,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide, NumpadDecimal, NumpadEnter,
};

// The key's name as the web's `KeyboardEvent.key` gives it: "a", "0", "Escape", "ArrowLeft",
// "F2", " " for Space. Letters are lower case; Unknown is "Unidentified".
std::string_view KeyName(Key Code);

struct Modifiers {
    bool Shift{false};
    bool Ctrl{false};
    bool Alt{false};
    bool Super{false};
    bool Shortcut{false};
};

// One key transition this frame, in the order SDL reported them. Down is true for a
// press (Repeat when the key was already held and the OS repeated it) and false for a
// release. ModifierState is the modifier set at that moment, not at the end of the frame.
// TextInputBefore is how many bytes of TextInputThisFrame were typed before it.
struct KeyEvent {
    Key         Code{Key::Unknown};
    bool        Down{true};
    bool        Repeat{false};
    Modifiers   ModifierState{};
    std::size_t TextInputBefore{0};
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
    std::vector<Key>  KeysPressedThisFrame;            // every press, repeats included
    std::vector<KeyEvent> KeyEvents;                   // every press and release, in order
    std::vector<Key>  KeysHeld;                        // down at the end of the frame
    Modifiers         ModifierState{};

    // Every press this frame, in order, with the modifiers held at the time. A hand-built
    // state with no KeyEvents gets its KeysPressedThisFrame, after its typed text.
    std::vector<KeyEvent> KeyPressesInOrder() const {
        std::vector<KeyEvent> Presses;
        if (KeyEvents.empty()) {
            for (const Key Pressed : KeysPressedThisFrame) {
                Presses.push_back(KeyEvent{Pressed, true, false, ModifierState, TextInputThisFrame.size()});
            }
            return Presses;
        }
        for (const KeyEvent& Event : KeyEvents) {
            if (Event.Down) {
                Presses.push_back(Event);
            }
        }
        return Presses;
    }

    bool IsKeyHeld(Key Code) const {
        for (const Key Held : KeysHeld) {
            if (Held == Code) {
                return true;
            }
        }
        return false;
    }

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

    // Set inside a subtree whose widgets let the pointer through (BoxStyle's
    // PointerEventsMode None): like PointerOccluded, nothing there hovers or starts a
    // press, but unlike it, a descendant set to Auto clears it again, and a widget the
    // pointer passes through doesn't hide what's under it.
    bool PointerPassThrough{false};

    // Whether the pointer is over Area, nothing above has taken it, and this widget
    // doesn't let it through. Use this, not a raw MousePosition test, for hover and for
    // starting a press.
    bool IsPointerOver(Rect Area) const {
        return !PointerOccluded && !PointerPassThrough && MousePosition.X >= Area.X &&
               MousePosition.X < Area.X + Area.W && MousePosition.Y >= Area.Y && MousePosition.Y < Area.Y + Area.H;
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
