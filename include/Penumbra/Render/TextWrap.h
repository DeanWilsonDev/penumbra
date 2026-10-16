#pragma once

#include "Penumbra/Render/IFontBackend.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Penumbra::Render {

// One visual (wrapped) line within a larger string: the rendered slice is
// Text[ContentStart, ContentEnd). Shared by TextArea (which additionally tracks
// caret/selection state per line, see its own Line struct) and Label (Wrap ==
// true, no editing state at all) so both widgets word-wrap identically instead
// of each carrying its own copy of the algorithm.
struct TextLine {
    std::size_t ContentStart{0};
    std::size_t ContentEnd{0};
};

using MeasureTextRangeFn = std::function<float(std::size_t Start, std::size_t End)>;

std::vector<TextLine> WrapText(IFontBackend* FontBackend, FontHandle Font, const std::string& Text,
                                float WrapWidthLogical);

std::vector<TextLine> WrapText(const std::string& Text, float WrapWidthLogical, const MeasureTextRangeFn& MeasureRange);

} // namespace Penumbra::Render
