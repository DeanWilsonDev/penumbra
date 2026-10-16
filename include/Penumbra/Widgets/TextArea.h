#pragma once

#include "Penumbra/Platform/IClipboard.h"
#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Render/TextWrap.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/FocusState.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Penumbra::Widgets {

struct TextAreaSpan {
    std::size_t                       Start{0};
    std::size_t                       End{0};
    std::string                       ClassName;
    std::optional<Render::FontHandle> Font;
    std::optional<Render::Color>      ColorText;
    Render::Color                     ColorBackground{0, 0, 0, 0};
};

class TextArea : public Box {
public:
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    Render::Color         ColorText{0, 0, 0, 0};
    Render::Color         ColorCaret{0, 0, 0, 0};
    Render::Color         ColorSelection{0, 0, 0, 0};
    Render::Color         ColorScrollbarThumb{0, 0, 0, 0};
    float                 CaretWidthLogical{0.0f};
    float                 PreferredWidthLogical{0.0f};
    float                 PreferredHeightLogical{0.0f};
    float                 WheelStepLogical{0.0f};
    float                 ScrollbarWidthLogical{0.0f};

    FocusState*           Focus{nullptr};
    Platform::IClipboard* Clipboard{nullptr};
    std::string           Text;

    std::function<void(const std::string&)> OnTextChanged;

    void                             SetSpans(std::vector<TextAreaSpan> NewSpans);
    const std::vector<TextAreaSpan>& GetSpans() const { return Spans; }

    bool UpdateInteractionState(const Platform::InputState&) override;
    bool ConsumedWheelThisFrame() const override { return WheelHandledThisFrame; }

protected:
    Point MeasureContent(Point AvailableContentSize) override;
    void  DrawContent(Render::Renderer&, Rect ContentRect) override;

private:
    using Line = Render::TextLine;

    struct LineBox {
        float Top{0.0f};
        float Height{0.0f};
        float Ascent{0.0f};
    };

    struct Run {
        std::size_t        Start{0};
        std::size_t        End{0};
        Render::FontHandle Font{0};
        Render::Color      ColorText{0, 0, 0, 0};
        Render::Color      ColorBackground{0, 0, 0, 0};
    };

    bool IsFocused() const { return Focus != nullptr && Focus->Focused == this; }

    bool        HasSelection() const { return SelectionAnchor != CaretIndex; }
    std::size_t SelectionStart() const { return std::min(SelectionAnchor, CaretIndex); }
    std::size_t SelectionEnd() const { return std::max(SelectionAnchor, CaretIndex); }
    void        DeleteSelection();
    void        InsertText(std::string_view Inserted);
    void        EraseText(std::size_t Start, std::size_t Length);

    template <typename Visitor>
    void                ForEachRun(std::size_t Start, std::size_t End, Visitor&& Visit) const;
    float               MeasureRange(std::size_t Start, std::size_t End) const;
    Render::TextMetrics FontMetrics(Render::FontHandle) const;

    float       DefaultLineHeight() const;
    float       TotalHeight() const;
    float       WrapWidthFor(Rect ContentRect) const;
    void        RebuildLinesIfNeeded(float WrapWidthLogical);
    std::size_t LineIndexForCaret(std::size_t Index) const;
    std::size_t LineIndexAtY(float ContentY) const;
    std::size_t LineOwnershipEnd(std::size_t LineIndex) const;
    float       LineLocalX(const Line&, std::size_t Index) const;
    std::size_t IndexAtLineX(const Line&, float LocalX) const;
    std::size_t IndexAtLocalPoint(float LocalX, float LocalY) const;

    void MoveCaretVertically(int LineDelta, bool Shift);
    void ScrollCaretIntoView(float ViewportHeightLogical);

    std::size_t CaretIndex{0};
    std::size_t SelectionAnchor{0};
    bool        Dragging{false};

    std::vector<TextAreaSpan> Spans;

    std::vector<Line>    Lines{{0, 0}};
    std::vector<LineBox> LineBoxes{{}};
    float                LinesWrapWidth{-1.0f};
    bool                 LinesDirty{true};

    mutable std::vector<std::pair<Render::FontHandle, Render::TextMetrics>> MetricsCache;

    float DesiredCaretXLogical{-1.0f};
    float ScrollOffsetY{0.0f};
    bool  WheelHandledThisFrame{false};
};

} // namespace Penumbra::Widgets
