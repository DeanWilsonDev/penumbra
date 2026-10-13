#pragma once

#include <cstdint>

namespace Penumbra::Widgets {

// Brackets one frame's layout: construct it before the root's Measure and let it go
// after the root's Arrange (Application::RunOneFrame does). Within a pass a Box
// measured again with the same available size returns its first result instead of
// walking its subtree again, so a widget deep in the tree is measured once per
// distinct size rather than once per ancestor. That's sound because nothing changes a
// widget between the root's Measure and Arrange. Outside a pass -- app code measuring
// a widget from a callback, say -- Measure always works it out afresh. Passes don't
// nest; an inner one is ignored.
class LayoutPass {
public:
    LayoutPass();
    ~LayoutPass();

    LayoutPass(const LayoutPass&)            = delete;
    LayoutPass& operator=(const LayoutPass&) = delete;

    // The running pass's id, or 0 when no pass is running.
    static std::uint64_t Current();

private:
    bool Outermost{false};
};

} // namespace Penumbra::Widgets
