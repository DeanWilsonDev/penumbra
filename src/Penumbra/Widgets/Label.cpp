#include "Penumbra/Widgets/Label.h"

#include "Penumbra/Render/Utf8.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace Penumbra::Widgets {

float Label::LineHeight() const {
    return FontBackend ? FontBackend->MeasureText(Font, "Ag").HeightLogical : 0.0f;
}

const std::vector<Render::TextLine>& Label::WrappedLinesFor(float WidthLogical) {
    if (WrapWidth != WidthLogical || WrapFontBackend != FontBackend || WrapFont != Font || WrappedText != Text) {
        const float Tolerance = WidthLogical > 0.0f ? 0.01f : 0.0f;
        WrapLines       = Render::WrapText(FontBackend, Font, Text, WidthLogical + Tolerance);
        WrapLineHeight  = LineHeight();
        WrapWidth       = WidthLogical;
        WrapFontBackend = FontBackend;
        WrapFont        = Font;
        WrappedText     = Text;
    }
    return WrapLines;
}

std::vector<Render::TextLine> Label::LinesFor(float WidthLogical) {
    if (Wrap) {
        return WrappedLinesFor(WidthLogical);
    }
    return {{0, Text.size()}};
}

std::size_t Label::LineIndexOf(const std::vector<Render::TextLine>& Lines, std::size_t Index) const {
    for (std::size_t Line = 0; Line + 1 < Lines.size(); ++Line) {
        if (Index < Lines[Line + 1].ContentStart) {
            return Line;
        }
    }
    return Lines.size() - 1;
}

float Label::WidthOf(std::size_t Start, std::size_t End) const {
    if (!FontBackend || End <= Start) {
        return 0.0f;
    }
    return FontBackend->MeasureTextWidth(Font, std::string_view(Text).substr(Start, End - Start));
}

std::size_t Label::IndexAtPoint(Point PointLogical) {
    if (!FontBackend || Text.empty()) {
        return 0;
    }
    const Rect                          Content = ContentRectFrom(ArrangedRect);
    const std::vector<Render::TextLine> Lines = LinesFor(Content.W);
    const float                         LH = LineHeight();
    std::size_t                         LineIndex = 0;
    if (LH > 0.0f && PointLogical.Y > Content.Y) {
        LineIndex = std::min(static_cast<std::size_t>((PointLogical.Y - Content.Y) / LH), Lines.size() - 1);
    }
    const Render::TextLine& L = Lines[LineIndex];
    const float             LocalX = PointLogical.X - Content.X;
    std::size_t             Best = L.ContentStart;
    float                   BestDistance = std::fabs(LocalX);
    for (std::size_t Index = L.ContentStart; Index < L.ContentEnd;) {
        Index = std::min(Render::NextCodePoint(Text, Index), L.ContentEnd);
        const float Distance = std::fabs(LocalX - WidthOf(L.ContentStart, Index));
        if (Distance < BestDistance) {
            BestDistance = Distance;
            Best = Index;
        }
    }
    return Best;
}

Rect Label::CaretRectAt(std::size_t Index) {
    const Rect                          Content = ContentRectFrom(ArrangedRect);
    const std::vector<Render::TextLine> Lines = LinesFor(Content.W);
    const std::size_t                   LineIndex = LineIndexOf(Lines, Index);
    const Render::TextLine&             L = Lines[LineIndex];
    const float                         LH = LineHeight();
    return {Content.X + WidthOf(L.ContentStart, std::clamp(Index, L.ContentStart, L.ContentEnd)),
            Content.Y + static_cast<float>(LineIndex) * LH, 0.0f, LH};
}

void Label::SetSelection(std::size_t Start, std::size_t End, Render::Color Color) {
    SelectionStart = std::min(Start, Text.size());
    SelectionEnd = std::clamp(End, SelectionStart, Text.size());
    ColorSelection = Color;
}

void Label::ClearSelection() {
    SelectionStart = 0;
    SelectionEnd = 0;
}

Point Label::MeasureContent(Point AvailableContentSize) {
    if (!FontBackend) {
        return {0.0f, 0.0f};
    }

    if (Wrap) {
        const std::vector<Render::TextLine>& WrappedLines = WrappedLinesFor(AvailableContentSize.X);
        const std::string_view               View(Text);
        float                                Widest = 0.0f;
        for (const Render::TextLine& L : WrappedLines) {
            const std::size_t End = (L.ContentEnd < View.size() && View[L.ContentEnd] == ' ') ? L.ContentEnd + 1 : L.ContentEnd;
            Widest = std::max(Widest, FontBackend->MeasureTextWidth(Font, View.substr(L.ContentStart, End - L.ContentStart)));
        }
        const float ReportedWidth = AvailableContentSize.X > 0.0f ? std::min(Widest, AvailableContentSize.X) : Widest;
        return {ReportedWidth, WrapLineHeight * static_cast<float>(WrappedLines.size())};
    }

    const Render::TextMetrics Metrics = FontBackend->MeasureText(Font, Text);
    float Width = Metrics.WidthLogical;
    if (MaxWidthLogical && Width > *MaxWidthLogical) {
        Width = *MaxWidthLogical;
    }
    return {Width, Metrics.HeightLogical};
}

void Label::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    if (SelectionEnd > SelectionStart && FontBackend) {
        const std::vector<Render::TextLine> Lines = LinesFor(ContentRect.W);
        const float                         LH = LineHeight();
        const float Right = MaxWidthLogical && !Wrap ? ContentRect.X + *MaxWidthLogical : ContentRect.X + ContentRect.W;
        for (std::size_t Index = 0; Index < Lines.size(); ++Index) {
            const Render::TextLine& L = Lines[Index];
            const std::size_t       From = std::max(L.ContentStart, SelectionStart);
            const std::size_t       To = std::min(L.ContentEnd, SelectionEnd);
            if (From >= To) {
                continue;
            }
            const float X0 = ContentRect.X + WidthOf(L.ContentStart, From);
            const float X1 = std::min(X0 + WidthOf(From, To), std::max(Right, X0));
            Renderer.DrawFilledRect({X0, ContentRect.Y + static_cast<float>(Index) * LH, X1 - X0, LH}, ColorSelection);
        }
    }

    if (Wrap) {
        const std::vector<Render::TextLine>& WrappedLines = WrappedLinesFor(ContentRect.W);
        const float LH = WrapLineHeight;
        for (std::size_t Index = 0; Index < WrappedLines.size(); ++Index) {
            const Render::TextLine& L = WrappedLines[Index];
            if (L.ContentEnd > L.ContentStart) {
                Renderer.DrawText(Font, std::string_view(Text).substr(L.ContentStart, L.ContentEnd - L.ContentStart),
                                   {ContentRect.X, ContentRect.Y + static_cast<float>(Index) * LH}, ColorText);
            }
        }
        return;
    }

    if (!MaxWidthLogical || Renderer.MeasureTextWidth(Font, Text) <= *MaxWidthLogical) {
        Renderer.DrawText(Font, Text, {ContentRect.X, ContentRect.Y}, ColorText);
        return;
    }

    if (!TruncateWithEllipsis) {
        Renderer.PushClipRect({ContentRect.X, ContentRect.Y, *MaxWidthLogical, ContentRect.H});
        Renderer.DrawText(Font, Text, {ContentRect.X, ContentRect.Y}, ColorText);
        Renderer.PopClipRect();
        return;
    }

    std::string ToDraw = Text;
    while (!ToDraw.empty() && Renderer.MeasureTextWidth(Font, ToDraw) > *MaxWidthLogical) {
        ToDraw.pop_back();
    }
    if (ToDraw.size() >= 2) {
        ToDraw[ToDraw.size() - 1] = '.';
        if (ToDraw.size() >= 3) {
            ToDraw[ToDraw.size() - 2] = '.';
        }
    }
    Renderer.DrawText(Font, ToDraw, {ContentRect.X, ContentRect.Y}, ColorText);
}

Label::Builder::Builder() : Owned(std::make_unique<Label>()) {}

Label::Builder& Label::Builder::className(std::string Value) {
    Owned->ClassName = std::move(Value);
    return *this;
}

Label::Builder& Label::Builder::text(std::string Value) {
    Owned->Text = std::move(Value);
    return *this;
}

Label::Builder& Label::Builder::child(std::unique_ptr<WidgetBase> Child) {
    Owned->AddChild(std::move(Child));
    return *this;
}

Label::Builder& Label::Builder::children(std::vector<std::unique_ptr<WidgetBase>> Kids) {
    for (auto& Kid : Kids) {
        Owned->AddChild(std::move(Kid));
    }
    return *this;
}

Label::Builder& Label::Builder::onPress(std::function<void()> Handler) {
    Owned->OnPressed = std::move(Handler);
    return *this;
}

Label::Builder& Label::Builder::onRelease(std::function<void()> Handler) {
    Owned->OnReleased = std::move(Handler);
    return *this;
}

Label::Builder& Label::Builder::onHover(std::function<void()> Handler) {
    Owned->OnHovered = std::move(Handler);
    return *this;
}

Label::Builder& Label::Builder::onFocus(std::function<void()> Handler) {
    Owned->OnFocused = std::move(Handler);
    return *this;
}

Label::Builder& Label::Builder::onChange(std::function<void()> Handler) {
    Owned->OnChanged = std::move(Handler);
    return *this;
}

std::unique_ptr<Label> Label::Builder::build() { return std::move(Owned); }

} // namespace Penumbra::Widgets
