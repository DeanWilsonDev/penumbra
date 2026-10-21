#include "Penumbra/Widgets/TextArea.h"

#include "Penumbra/Widgets/KeyDispatch.h"

#include "Penumbra/Render/Utf8.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace Penumbra::Widgets {

namespace {
constexpr int LeftButton = 0;
} // namespace

void TextArea::SetSpans(std::vector<TextAreaSpan> NewSpans) {
    std::stable_sort(NewSpans.begin(), NewSpans.end(),
                     [](const TextAreaSpan& A, const TextAreaSpan& B) { return A.Start < B.Start; });
    std::size_t PreviousEnd = 0;
    for (TextAreaSpan& Span : NewSpans) {
        Span.Start = std::min(std::max(Span.Start, PreviousEnd), Text.size());
        Span.End = std::max(std::min(Span.End, Text.size()), Span.Start);
        PreviousEnd = Span.End;
    }
    Spans = std::move(NewSpans);
    LinesDirty = true;
}

template <typename Visitor>
void TextArea::ForEachRun(std::size_t Start, std::size_t End, Visitor&& Visit) const {
    std::size_t Cursor = Start;
    auto It = std::upper_bound(Spans.begin(), Spans.end(), Start,
                               [](std::size_t At, const TextAreaSpan& Span) { return At < Span.End; });
    for (; It != Spans.end() && Cursor < End && It->Start < End; ++It) {
        if (It->Start > Cursor) {
            Visit(Run{Cursor, It->Start, Font, ColorText, {}});
            Cursor = It->Start;
        }
        const std::size_t PieceEnd = std::min(It->End, End);
        if (PieceEnd > Cursor) {
            Visit(Run{Cursor, PieceEnd, It->Font.value_or(Font), It->ColorText.value_or(ColorText), It->ColorBackground});
            Cursor = PieceEnd;
        }
    }
    if (Cursor < End) {
        Visit(Run{Cursor, End, Font, ColorText, {}});
    }
}

float TextArea::MeasureRange(std::size_t Start, std::size_t End) const {
    if (!FontBackend || End <= Start) {
        return 0.0f;
    }
    const std::string_view View(Text);
    float Width = 0.0f;
    ForEachRun(Start, End, [&](const Run& Piece) {
        Width += FontBackend->MeasureTextWidth(Piece.Font, View.substr(Piece.Start, Piece.End - Piece.Start));
    });
    return Width;
}

Render::TextMetrics TextArea::FontMetrics(Render::FontHandle Handle) const {
    if (!FontBackend) {
        return {0.0f, 0.0f, 0.0f};
    }
    for (const auto& [Cached, Metrics] : MetricsCache) {
        if (Cached == Handle) {
            return Metrics;
        }
    }
    const Render::TextMetrics Metrics = FontBackend->MeasureText(Handle, "Ag");
    MetricsCache.emplace_back(Handle, Metrics);
    return Metrics;
}

float TextArea::DefaultLineHeight() const { return FontMetrics(Font).HeightLogical; }

float TextArea::TotalHeight() const { return LineBoxes.back().Top + LineBoxes.back().Height; }

float TextArea::WrapWidthFor(Rect ContentRect) const {
    return ScrollbarWidthLogical > 0.0f ? std::max(0.0f, ContentRect.W - ScrollbarWidthLogical)
                                         : ContentRect.W;
}

void TextArea::RebuildLinesIfNeeded(float WrapWidthLogical) {
    if (!LinesDirty && LinesWrapWidth == WrapWidthLogical) {
        return;
    }

    Render::MeasureTextRangeFn Measure;
    if (FontBackend) {
        Measure = [this](std::size_t Start, std::size_t End) { return MeasureRange(Start, End); };
    }
    Lines = Render::WrapText(Text, WrapWidthLogical, Measure);

    LineBoxes.clear();
    LineBoxes.reserve(Lines.size());
    float Top = 0.0f;
    for (const Line& L : Lines) {
        float Ascent = 0.0f;
        float Descent = 0.0f;
        const auto Include = [&](Render::FontHandle Handle) {
            const Render::TextMetrics Metrics = FontMetrics(Handle);
            Ascent = std::max(Ascent, Metrics.AscentLogical);
            Descent = std::max(Descent, Metrics.HeightLogical - Metrics.AscentLogical);
        };
        if (L.ContentEnd > L.ContentStart) {
            ForEachRun(L.ContentStart, L.ContentEnd, [&](const Run& Piece) { Include(Piece.Font); });
        } else {
            Include(Font);
        }
        LineBoxes.push_back({Top, Ascent + Descent, Ascent});
        Top += Ascent + Descent;
    }

    LinesWrapWidth = WrapWidthLogical;
    LinesDirty = false;
}

std::size_t TextArea::LineOwnershipEnd(std::size_t LineIndex) const {
    if (LineIndex + 1 < Lines.size()) {
        return Lines[LineIndex + 1].ContentStart;
    }
    return Text.size() + 1;
}

std::size_t TextArea::LineIndexForCaret(std::size_t Index) const {
    for (std::size_t I = 0; I < Lines.size(); ++I) {
        if (Index >= Lines[I].ContentStart && Index < LineOwnershipEnd(I)) {
            return I;
        }
    }
    return Lines.size() - 1;
}

std::size_t TextArea::LineIndexAtY(float ContentY) const {
    if (ContentY <= 0.0f) {
        return 0;
    }
    const auto It = std::upper_bound(LineBoxes.begin(), LineBoxes.end(), ContentY,
                                     [](float Y, const LineBox& Box) { return Y < Box.Top + Box.Height; });
    return It == LineBoxes.end() ? LineBoxes.size() - 1 : static_cast<std::size_t>(It - LineBoxes.begin());
}

float TextArea::LineLocalX(const Line& L, std::size_t Index) const {
    const std::size_t Clamped = std::min(std::max(Index, L.ContentStart), L.ContentEnd);
    return MeasureRange(L.ContentStart, Clamped);
}

std::size_t TextArea::IndexAtLineX(const Line& L, float LocalX) const {
    const std::size_t Length = L.ContentEnd - L.ContentStart;
    if (!FontBackend || Length == 0 || LocalX <= 0.0f) {
        return L.ContentStart;
    }
    std::size_t Best = L.ContentStart;
    float BestDistance = std::fabs(LocalX);
    for (std::size_t Index = L.ContentStart; Index < L.ContentEnd;) {
        Index = std::min(Render::NextCodePoint(Text, Index), L.ContentEnd);
        const float Distance = std::fabs(LocalX - MeasureRange(L.ContentStart, Index));
        if (Distance < BestDistance) {
            BestDistance = Distance;
            Best = Index;
        }
    }
    return Best;
}

std::size_t TextArea::IndexAtLocalPoint(float LocalX, float LocalY) const {
    const std::size_t LineIdx = std::min(LineIndexAtY(LocalY + ScrollOffsetY), Lines.size() - 1);
    return IndexAtLineX(Lines[LineIdx], LocalX);
}

void TextArea::InsertText(std::string_view Inserted) {
    const std::size_t At = CaretIndex;
    Text.insert(At, Inserted);
    for (TextAreaSpan& Span : Spans) {
        if (Span.Start >= At) {
            Span.Start += Inserted.size();
            Span.End += Inserted.size();
        } else if (Span.End >= At) {
            Span.End += Inserted.size();
        }
    }
    CaretIndex = At + Inserted.size();
    SelectionAnchor = CaretIndex;
    DesiredCaretXLogical = -1.0f;
    LinesDirty = true;
}

void TextArea::EraseText(std::size_t Start, std::size_t Length) {
    Text.erase(Start, Length);
    const auto Shift = [Start, Length](std::size_t Index) {
        if (Index <= Start) {
            return Index;
        }
        return Index >= Start + Length ? Index - Length : Start;
    };
    for (TextAreaSpan& Span : Spans) {
        Span.Start = Shift(Span.Start);
        Span.End = Shift(Span.End);
    }
    std::erase_if(Spans, [](const TextAreaSpan& Span) { return Span.Start >= Span.End; });
    LinesDirty = true;
}

void TextArea::DeleteSelection() {
    const std::size_t Start = SelectionStart();
    EraseText(Start, SelectionEnd() - Start);
    CaretIndex = Start;
    SelectionAnchor = Start;
}

void TextArea::MoveCaretVertically(int LineDelta, bool Shift) {
    const std::size_t CurrentLine = LineIndexForCaret(CaretIndex);
    if (DesiredCaretXLogical < 0.0f) {
        DesiredCaretXLogical = LineLocalX(Lines[CurrentLine], CaretIndex);
    }

    if (LineDelta < 0) {
        if (CurrentLine == 0) {
            return;
        }
        CaretIndex = IndexAtLineX(Lines[CurrentLine - 1], DesiredCaretXLogical);
    } else if (LineDelta > 0) {
        if (CurrentLine + 1 >= Lines.size()) {
            return;
        }
        CaretIndex = IndexAtLineX(Lines[CurrentLine + 1], DesiredCaretXLogical);
    }

    if (!Shift) {
        SelectionAnchor = CaretIndex;
    }
}

void TextArea::ScrollCaretIntoView(float ViewportHeightLogical) {
    if (DefaultLineHeight() <= 0.0f) {
        return;
    }
    const LineBox& Box = LineBoxes[LineIndexForCaret(CaretIndex)];
    const float LineBottom = Box.Top + Box.Height;
    if (Box.Top < ScrollOffsetY) {
        ScrollOffsetY = Box.Top;
    } else if (LineBottom > ScrollOffsetY + ViewportHeightLogical) {
        ScrollOffsetY = LineBottom - ViewportHeightLogical;
    }
    const float MaxScroll = std::max(0.0f, TotalHeight() - ViewportHeightLogical);
    ScrollOffsetY = std::min(std::max(ScrollOffsetY, 0.0f), MaxScroll);
}

Point TextArea::MeasureContent(Point /*AvailableContentSize*/) {
    return {PreferredWidthLogical, PreferredHeightLogical};
}

TextArea::~TextArea() {
    if (IsFocused()) {
        Focus->Focused = nullptr;
    }
}

bool TextArea::UpdateInteractionState(const Platform::InputState& Input) {
    const bool Hovered  = Input.IsPointerOver(ArrangedRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Input.MouseButtonDown[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];
    const Rect Content  = ContentRectFrom(ArrangedRect);
    const float WrapWidth = WrapWidthFor(Content);

    RebuildLinesIfNeeded(WrapWidth);

    if (Pressed && Hovered && Focus) {
        Focus->Focused = this;
        const std::size_t Index =
            IndexAtLocalPoint(Input.MousePosition.X - Content.X, Input.MousePosition.Y - Content.Y);
        CaretIndex = Index;
        SelectionAnchor = Index;
        DesiredCaretXLogical = -1.0f;
        Dragging = true;
    }
    if (Dragging && Down && IsFocused()) {
        CaretIndex = IndexAtLocalPoint(Input.MousePosition.X - Content.X, Input.MousePosition.Y - Content.Y);
        DesiredCaretXLogical = -1.0f;
    }
    if (Released) {
        Dragging = false;
    }

    bool Consumed = false;
    WheelHandledThisFrame = false;
    if (Hovered && WheelStepLogical != 0.0f && Input.MouseWheelDelta != 0.0f) {
        const float MaxScroll = std::max(0.0f, TotalHeight() - Content.H);
        if (MaxScroll > 0.0f) {
            ScrollOffsetY = std::min(std::max(ScrollOffsetY - Input.MouseWheelDelta * WheelStepLogical, 0.0f), MaxScroll);
            Consumed = true;
            WheelHandledThisFrame = true;
        }
    }

    if (!IsFocused()) {
        return Hovered || Consumed;
    }

    const bool Shortcut = Input.ModifierState.Shortcut;
    const bool Shift    = Input.ModifierState.Shift;
    bool Changed = false;

    if (!Input.TextInputThisFrame.empty()) {
        if (HasSelection()) {
            DeleteSelection();
        }
        InsertText(Input.TextInputThisFrame);
        Changed = true;
    }

    for (const Platform::Key Key : Input.KeysPressedThisFrame) {
        // A multi-line field types, edits and moves, but has no use for Escape or Tab, nor
        // any shortcut but select-all, copy, cut and paste: those go on to whatever
        // handles keys (KeyDispatch).
        const bool Uses = Shortcut ? (Key == Platform::Key::A || Key == Platform::Key::C ||
                                      Key == Platform::Key::X || Key == Platform::Key::V)
                                   : (Key != Platform::Key::Escape && Key != Platform::Key::Tab);
        if (Uses) {
            KeyDispatch::MarkUsed(Key);
        }
        RebuildLinesIfNeeded(WrapWidth);

        if (Shortcut) {
            switch (Key) {
            case Platform::Key::A:
                SelectionAnchor = 0;
                CaretIndex = Text.size();
                DesiredCaretXLogical = -1.0f;
                break;
            case Platform::Key::C:
                if (HasSelection() && Clipboard) {
                    Clipboard->SetClipboardText(Text.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                }
                break;
            case Platform::Key::X:
                if (HasSelection() && Clipboard) {
                    Clipboard->SetClipboardText(Text.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                    DeleteSelection();
                    DesiredCaretXLogical = -1.0f;
                    Changed = true;
                }
                break;
            case Platform::Key::V:
                if (Clipboard) {
                    const std::string Pasted = Clipboard->GetClipboardText();
                    if (!Pasted.empty()) {
                        if (HasSelection()) {
                            DeleteSelection();
                        }
                        InsertText(Pasted);
                        Changed = true;
                    }
                }
                break;
            default:
                break;
            }
            continue;
        }

        switch (Key) {
        case Platform::Key::Backspace:
            if (HasSelection()) {
                DeleteSelection();
                Changed = true;
            } else if (CaretIndex > 0) {
                const std::size_t Previous = Render::PreviousCodePoint(Text, CaretIndex);
                EraseText(Previous, CaretIndex - Previous);
                CaretIndex = Previous;
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            DesiredCaretXLogical = -1.0f;
            break;
        case Platform::Key::Delete:
            if (HasSelection()) {
                DeleteSelection();
                Changed = true;
            } else if (CaretIndex < Text.size()) {
                EraseText(CaretIndex, Render::NextCodePoint(Text, CaretIndex) - CaretIndex);
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            DesiredCaretXLogical = -1.0f;
            break;
        case Platform::Key::Enter:
            if (HasSelection()) {
                DeleteSelection();
            }
            InsertText("\n");
            Changed = true;
            break;
        case Platform::Key::Left:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionStart();
            } else {
                CaretIndex = Render::PreviousCodePoint(Text, CaretIndex);
            }
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            DesiredCaretXLogical = -1.0f;
            break;
        case Platform::Key::Right:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionEnd();
            } else {
                CaretIndex = Render::NextCodePoint(Text, CaretIndex);
            }
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            DesiredCaretXLogical = -1.0f;
            break;
        case Platform::Key::Up:
            MoveCaretVertically(-1, Shift);
            break;
        case Platform::Key::Down:
            MoveCaretVertically(1, Shift);
            break;
        case Platform::Key::Home: {
            const std::size_t Line = LineIndexForCaret(CaretIndex);
            CaretIndex = Lines[Line].ContentStart;
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            DesiredCaretXLogical = -1.0f;
            break;
        }
        case Platform::Key::End: {
            const std::size_t Line = LineIndexForCaret(CaretIndex);
            CaretIndex = Lines[Line].ContentEnd;
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            DesiredCaretXLogical = -1.0f;
            break;
        }
        default:
            break;
        }
    }

    RebuildLinesIfNeeded(WrapWidth);
    ScrollCaretIntoView(Content.H);

    if (Changed && OnTextChanged) {
        OnTextChanged(Text);
    }
    return true;
}

void TextArea::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    const float WrapWidth = WrapWidthFor(ContentRect);
    RebuildLinesIfNeeded(WrapWidth);

    Renderer.PushClipRect(ContentRect);

    if (DefaultLineHeight() <= 0.0f) {
        Renderer.PopClipRect();
        return;
    }

    const bool Selected = IsFocused() && HasSelection();
    const std::size_t SelStart = Selected ? SelectionStart() : 0;
    const std::size_t SelEnd   = Selected ? SelectionEnd() : 0;
    const std::string_view View(Text);
    const float ViewportBottom = ScrollOffsetY + ContentRect.H;

    struct Piece {
        Run   Source;
        float X{0.0f};
        float Width{0.0f};
    };
    std::vector<Piece> Pieces;

    for (std::size_t I = LineIndexAtY(ScrollOffsetY); I < Lines.size() && LineBoxes[I].Top < ViewportBottom; ++I) {
        const Line& L = Lines[I];
        const LineBox& Box = LineBoxes[I];
        const float LineY = ContentRect.Y + Box.Top - ScrollOffsetY;

        Pieces.clear();
        float X = ContentRect.X;
        ForEachRun(L.ContentStart, L.ContentEnd, [&](const Run& Source) {
            const float Width =
                FontBackend->MeasureTextWidth(Source.Font, View.substr(Source.Start, Source.End - Source.Start));
            Pieces.push_back({Source, X, Width});
            X += Width;
        });

        for (const Piece& P : Pieces) {
            if (P.Source.ColorBackground.A > 0) {
                const Render::TextMetrics Metrics = FontMetrics(P.Source.Font);
                Renderer.DrawFilledRect({P.X, LineY + Box.Ascent - Metrics.AscentLogical, P.Width, Metrics.HeightLogical},
                                        P.Source.ColorBackground);
            }
        }

        if (Selected) {
            const std::size_t OwnershipEnd = LineOwnershipEnd(I);
            const std::size_t OverlapStart = std::max(L.ContentStart, SelStart);
            const std::size_t OverlapEnd   = std::min(OwnershipEnd, SelEnd);
            if (OverlapStart < OverlapEnd) {
                const bool FullLine = OverlapStart <= L.ContentStart && SelEnd >= OwnershipEnd &&
                                      OwnershipEnd > L.ContentEnd;
                const float X0 = ContentRect.X + LineLocalX(L, OverlapStart);
                const float X1 = FullLine
                    ? ContentRect.X + WrapWidth
                    : ContentRect.X + LineLocalX(L, std::min(OverlapEnd, L.ContentEnd));
                Renderer.DrawFilledRect({X0, LineY, X1 - X0, Box.Height}, ColorSelection);
            }
        }

        for (const Piece& P : Pieces) {
            const Render::TextMetrics Metrics = FontMetrics(P.Source.Font);
            Renderer.DrawText(P.Source.Font, View.substr(P.Source.Start, P.Source.End - P.Source.Start),
                              {P.X, LineY + Box.Ascent - Metrics.AscentLogical}, P.Source.ColorText);
        }
    }

    if (IsFocused()) {
        const std::size_t CaretLine = LineIndexForCaret(CaretIndex);
        const LineBox& Box = LineBoxes[CaretLine];
        if (Box.Top + Box.Height > ScrollOffsetY && Box.Top < ViewportBottom) {
            const float CaretY = ContentRect.Y + Box.Top - ScrollOffsetY;
            const float CaretX = ContentRect.X + LineLocalX(Lines[CaretLine], CaretIndex);
            Renderer.DrawFilledRect({CaretX, CaretY, CaretWidthLogical, Box.Height}, ColorCaret);
        }
    }

    const float Total = TotalHeight();
    if (ScrollbarWidthLogical > 0.0f && Total > ContentRect.H) {
        const float MaxScroll   = Total - ContentRect.H;
        const float ThumbHeight = std::max(DefaultLineHeight(), ContentRect.H * (ContentRect.H / Total));
        const float ThumbTravel = ContentRect.H - ThumbHeight;
        const float ThumbY = ContentRect.Y + (MaxScroll > 0.0f ? (ScrollOffsetY / MaxScroll) * ThumbTravel : 0.0f);
        const float TrackX = ContentRect.X + ContentRect.W - ScrollbarWidthLogical;
        Renderer.DrawFilledRect({TrackX, ThumbY, ScrollbarWidthLogical, ThumbHeight}, ColorScrollbarThumb);
    }

    Renderer.PopClipRect();
}

} // namespace Penumbra::Widgets
