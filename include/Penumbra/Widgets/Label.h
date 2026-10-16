#pragma once

#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Render/TextWrap.h"
#include "Penumbra/Widgets/Box.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

class Label : public Box {
public:
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    std::string           Text;
    Render::Color         ColorText{0, 0, 0, 0};

    std::optional<float> MaxWidthLogical;
    bool                  TruncateWithEllipsis{false};
    bool                  Wrap{false};

    std::size_t IndexAtPoint(Point PointLogical);
    Rect        CaretRectAt(std::size_t Index);
    void        SetSelection(std::size_t Start, std::size_t End, Render::Color Color);
    void        ClearSelection();

    class Builder {
    public:
        Builder();

        Builder& className(std::string Value);
        Builder& text(std::string Value);
        Builder& child(std::unique_ptr<WidgetBase> Child);
        Builder& children(std::vector<std::unique_ptr<WidgetBase>> Kids);
        Builder& onPress(std::function<void()> Handler);
        Builder& onRelease(std::function<void()> Handler);
        Builder& onHover(std::function<void()> Handler);
        Builder& onFocus(std::function<void()> Handler);
        Builder& onChange(std::function<void()> Handler);

        std::unique_ptr<Label> build();

    private:
        std::unique_ptr<Label> Owned;
    };

protected:
    Point MeasureContent(Point AvailableContentSize) override;
    void  DrawContent(Render::Renderer&, Rect ContentRect) override;

private:
    float LineHeight() const;
    const std::vector<Render::TextLine>& WrappedLinesFor(float WidthLogical);
    std::vector<Render::TextLine>        LinesFor(float WidthLogical);
    std::size_t                          LineIndexOf(const std::vector<Render::TextLine>& Lines, std::size_t Index) const;
    float                                WidthOf(std::size_t Start, std::size_t End) const;

    std::size_t   SelectionStart{0};
    std::size_t   SelectionEnd{0};
    Render::Color ColorSelection{0, 0, 0, 0};

    std::vector<Render::TextLine> WrapLines;
    float                         WrapLineHeight{0.0f};
    float                         WrapWidth{-1.0f};
    Render::IFontBackend*         WrapFontBackend{nullptr};
    Render::FontHandle            WrapFont{0};
    std::string                   WrappedText;
};

} // namespace Penumbra::Widgets
