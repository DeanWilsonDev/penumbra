#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace Penumbra::Render {

inline bool IsUtf8Continuation(char Byte) { return (static_cast<unsigned char>(Byte) & 0xC0) == 0x80; }

inline std::size_t PreviousCodePoint(std::string_view Text, std::size_t Index) {
    std::size_t Previous = std::min(Index, Text.size());
    if (Previous == 0) {
        return 0;
    }
    --Previous;
    while (Previous > 0 && IsUtf8Continuation(Text[Previous])) {
        --Previous;
    }
    return Previous;
}

inline std::size_t NextCodePoint(std::string_view Text, std::size_t Index) {
    if (Index >= Text.size()) {
        return Text.size();
    }
    std::size_t Next = Index + 1;
    while (Next < Text.size() && IsUtf8Continuation(Text[Next])) {
        ++Next;
    }
    return Next;
}

} // namespace Penumbra::Render
