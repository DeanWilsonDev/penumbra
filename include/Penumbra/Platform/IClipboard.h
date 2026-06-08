#pragma once

#include <string>

namespace Penumbra::Platform {

// OS clipboard access. Lives in Platform because only Platform calls SDL; widgets
// that need the clipboard (TextInput) hold a pointer to this interface, injected by
// the demo — the same pattern as IFontBackend and FocusState.
class IClipboard {
public:
    virtual ~IClipboard() = default;

    virtual void        SetClipboardText(const std::string& Text) = 0;
    virtual std::string GetClipboardText() const = 0;
};

} // namespace Penumbra::Platform
