#include "Penumbra/Widgets/LayoutPass.h"

namespace Penumbra::Widgets {

namespace {
std::uint64_t LastPassId    = 0;
std::uint64_t RunningPassId = 0;
} // namespace

LayoutPass::LayoutPass() {
    if (RunningPassId == 0) {
        Outermost     = true;
        RunningPassId = ++LastPassId;
    }
}

LayoutPass::~LayoutPass() {
    if (Outermost) {
        RunningPassId = 0;
    }
}

std::uint64_t LayoutPass::Current() { return RunningPassId; }

} // namespace Penumbra::Widgets
