#include "Penumbra/Render/TextWrap.h"

#include "Penumbra/Render/Utf8.h"

#include <string_view>

namespace Penumbra::Render {

std::vector<TextLine> WrapText(IFontBackend* FontBackend, FontHandle Font, const std::string& Text,
                                float WrapWidthLogical) {
    if (!FontBackend) {
        return WrapText(Text, WrapWidthLogical, {});
    }
    const std::string_view View(Text);
    return WrapText(Text, WrapWidthLogical, [FontBackend, Font, View](std::size_t Start, std::size_t End) {
        return FontBackend->MeasureTextWidth(Font, View.substr(Start, End - Start));
    });
}

std::vector<TextLine> WrapText(const std::string& Text, float WrapWidthLogical, const MeasureTextRangeFn& MeasureRange) {
    std::vector<TextLine> Lines;
    const std::size_t N = Text.size();

    if (!MeasureRange || WrapWidthLogical <= 0.0f) {
        std::size_t LineStart = 0;
        for (std::size_t Index = 0; Index <= N; ++Index) {
            if (Index == N || Text[Index] == '\n') {
                Lines.push_back({LineStart, Index});
                LineStart = Index + 1;
            }
        }
    } else {
        std::size_t LineStart = 0;
        const auto Overflows = [&](std::size_t Cursor) {
            return MeasureRange(LineStart, Cursor + 1) > WrapWidthLogical;
        };

        while (true) {
            const std::size_t NewlineAt  = Text.find('\n', LineStart);
            const std::size_t SegmentEnd = NewlineAt == std::string::npos ? N : NewlineAt;

            while (true) {
                std::size_t Fits = LineStart;
                std::size_t Over = std::string::npos;
                for (std::size_t Probe = LineStart + 1; Probe < SegmentEnd;) {
                    const std::size_t NextSpace = Text.find(' ', Probe);
                    const std::size_t Candidate = NextSpace < SegmentEnd ? NextSpace : SegmentEnd - 1;
                    if (Overflows(Candidate)) {
                        Over = Candidate;
                        break;
                    }
                    Fits  = Candidate;
                    Probe = Candidate + 1;
                }
                if (Over == std::string::npos) {
                    break;
                }

                if (Fits > LineStart || Text[LineStart] == ' ') {
                    Lines.push_back({LineStart, Fits});
                    LineStart = Fits + 1;
                } else {
                    std::size_t Cursor = NextCodePoint(Text, LineStart);
                    std::size_t Next = NextCodePoint(Text, Cursor);
                    while (Cursor < Over && MeasureRange(LineStart, Next) <= WrapWidthLogical) {
                        Cursor = Next;
                        Next = NextCodePoint(Text, Cursor);
                    }
                    Lines.push_back({LineStart, Cursor});
                    LineStart = Cursor;
                }
            }

            Lines.push_back({LineStart, SegmentEnd});
            if (SegmentEnd == N) {
                break;
            }
            LineStart = SegmentEnd + 1;
        }
    }

    if (Lines.empty()) {
        Lines.push_back({0, 0});
    }

    return Lines;
}

} // namespace Penumbra::Render
