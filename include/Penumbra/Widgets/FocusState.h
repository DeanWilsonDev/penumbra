#pragma once

namespace Penumbra::Widgets {

class WidgetBase;

// The single keyboard-focus slot. The demo/root owns one of these and hands a
// pointer to each focus-taking widget (TextInput and TextArea, in this PoC). Text
// and key events route to whatever widget this points at. A focused widget clears it
// as it's destroyed, so it must outlive every widget that points at it.
struct FocusState {
    WidgetBase* Focused{nullptr};
};

} // namespace Penumbra::Widgets
