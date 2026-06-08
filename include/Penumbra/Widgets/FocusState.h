#pragma once

namespace Penumbra::Widgets {

class WidgetBase;

// The single keyboard-focus slot. The demo/root owns one of these and hands a
// pointer to each focus-taking widget (only TextInput, in this PoC). Text and key
// events route to whatever widget this points at.
struct FocusState {
    WidgetBase* Focused{nullptr};
};

} // namespace Penumbra::Widgets
