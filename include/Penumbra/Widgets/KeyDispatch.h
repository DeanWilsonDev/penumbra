#pragma once

#include "Penumbra/Platform/InputState.h"

#include <vector>

namespace Penumbra::Widgets {

class WidgetBase;

// Routes each key no focused widget used to the topmost widget, in paint order, with an
// OnKeyPressed or OnKeyEvent handler: a menu over a modal gets Escape before the modal does, and a
// modal gets it while a field inside it has focus, since a field doesn't use Escape.
//
// The outermost input pass opens one (Box, ScrollablePanel, SplitPanel and
// ViewportWidget do, as the root). Every visible widget gets UpdateInteractionState each
// frame, in reverse paint order, so the first with a handler to finish its pass is the
// topmost; when the pass ends it gets every key no focused widget marked used. With no
// other handler the root gets them.
//
// A widget that covers the whole window, and doesn't let the pointer through, withholds
// keys and typed text from everything painted under it, so a field focused behind a
// modal doesn't type into it.
class KeyDispatch {
public:
    // Begins a pass at Root when none is running, with Input's keys; otherwise does
    // nothing. The outermost one delivers the unused keys when it ends.
    KeyDispatch(WidgetBase& Root, const Platform::InputState& Input);
    ~KeyDispatch();

    KeyDispatch(const KeyDispatch&)            = delete;
    KeyDispatch& operator=(const KeyDispatch&) = delete;

    // A focused widget took Key this frame, so no handler gets it.
    static void MarkUsed(Platform::Key Key);
    // A focused widget took this press: OnKeyEvent skips the press with the same modifiers.
    static void MarkUsed(const Platform::KeyEvent& Press);

    // The input to hand a child: Input itself, or once a full-window widget above it has
    // had its turn, Input without keys or typed text, filled into Storage.
    static const Platform::InputState& ForChild(const Platform::InputState& Input, Platform::InputState& Storage);

    // Call when Child's UpdateInteractionState returns, with the input it was given.
    static void AfterChild(WidgetBase& Child, const Platform::InputState& Given);

private:
    bool Outermost{false};
};

} // namespace Penumbra::Widgets
