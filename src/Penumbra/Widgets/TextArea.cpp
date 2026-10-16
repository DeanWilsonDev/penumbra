#include "Penumbra/Widgets/TextArea.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace Penumbra::Widgets {

namespace {
constexpr int LeftButton = 0;
} // namespace

float TextArea::LineHeight() const {
    return FontBackend ? FontBackend->MeasureText(Font, "Ag").HeightLogical : 0.0f;
}

float TextArea::WrapWidthFor(Rect ContentRect) const {
    // Reserved unconditionally (not only once content actually overflows) so wrapping
    // doesn't reflow out from under the caret the moment a scrollbar thumb appears.
    return ScrollbarWidthLogical > 0.0f ? std::max(0.0f, ContentRect.W - ScrollbarWidthLogical)
                                         : ContentRect.W;
}

void TextArea::RebuildLinesIfNeeded(float WrapWidthLogical) {
    if (!LinesDirty && LinesWrapWidth == WrapWidthLogical) {
        return;
    }

    Lines = Render::WrapText(FontBackend, Font, Text, WrapWidthLogical);

    LinesWrapWidth = WrapWidthLogical;
    LinesDirty = false;
}

std::size_t TextArea::LineOwnershipEnd(std::size_t LineIndex) const {
    if (LineIndex + 1 < Lines.size()) {
        return Lines[LineIndex + 1].ContentStart;
    }
    return Text.size() + 1; // the last line owns up through (and including) Text.size()
}

std::size_t TextArea::LineIndexForCaret(std::size_t Index) const {
    for (std::size_t I = 0; I < Lines.size(); ++I) {
        if (Index >= Lines[I].ContentStart && Index < LineOwnershipEnd(I)) {
            return I;
        }
    }
    return Lines.size() - 1;
}

float TextArea::LineLocalX(const Line& L, std::size_t Index) const {
    if (!FontBackend) {
        return 0.0f;
    }
    const std::size_t Clamped = std::min(std::max(Index, L.ContentStart), L.ContentEnd);
    if (Clamped == L.ContentStart) {
        return 0.0f;
    }
    return FontBackend->MeasureTextWidth(Font, std::string_view(Text).substr(L.ContentStart, Clamped - L.ContentStart));
}

std::size_t TextArea::IndexAtLineX(const Line& L, float LocalX) const {
    const std::size_t Length = L.ContentEnd - L.ContentStart;
    if (!FontBackend || Length == 0 || LocalX <= 0.0f) {
        return L.ContentStart;
    }
    std::size_t Best = 0;
    float BestDistance = std::fabs(LocalX);
    for (std::size_t Offset = 1; Offset <= Length; ++Offset) {
        const float X = FontBackend->MeasureTextWidth(Font, std::string_view(Text).substr(L.ContentStart, Offset));
        const float Distance = std::fabs(LocalX - X);
        if (Distance < BestDistance) {
            BestDistance = Distance;
            Best = Offset;
        }
    }
    return L.ContentStart + Best;
}

std::size_t TextArea::IndexAtLocalPoint(float LocalX, float LocalY) const {
    const float LH = LineHeight();
    std::size_t LineIdx = 0;
    if (LH > 0.0f) {
        const float Y = LocalY + ScrollOffsetY;
        LineIdx = Y > 0.0f ? static_cast<std::size_t>(std::floor(Y / LH)) : 0;
    }
    LineIdx = std::min(LineIdx, Lines.size() - 1);
    return IndexAtLineX(Lines[LineIdx], LocalX);
}

void TextArea::DeleteSelection() {
    const std::size_t Start = SelectionStart();
    Text.erase(Start, SelectionEnd() - Start);
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
    const float LH = LineHeight();
    if (LH <= 0.0f) {
        return;
    }
    const std::size_t CurrentLine = LineIndexForCaret(CaretIndex);
    const float LineTop = static_cast<float>(CurrentLine) * LH;
    const float LineBottom = LineTop + LH;
    if (LineTop < ScrollOffsetY) {
        ScrollOffsetY = LineTop;
    } else if (LineBottom > ScrollOffsetY + ViewportHeightLogical) {
        ScrollOffsetY = LineBottom - ViewportHeightLogical;
    }
    const float MaxScroll = std::max(0.0f, static_cast<float>(Lines.size()) * LH - ViewportHeightLogical);
    ScrollOffsetY = std::min(std::max(ScrollOffsetY, 0.0f), MaxScroll);
}

Point TextArea::MeasureContent(Point /*AvailableContentSize*/) {
    return {PreferredWidthLogical, PreferredHeightLogical};
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
        SelectionAnchor = Index; // fresh selection at the click point
        DesiredCaretXLogical = -1.0f;
        Dragging = true;
    }
    if (Dragging && Down && IsFocused()) {
        CaretIndex = IndexAtLocalPoint(Input.MousePosition.X - Content.X, Input.MousePosition.Y - Content.Y);
        DesiredCaretXLogical = -1.0f; // drag extends selection
    }
    if (Released) {
        Dragging = false;
    }

    bool Consumed = false;
    WheelHandledThisFrame = false;
    if (Hovered && WheelStepLogical != 0.0f && Input.MouseWheelDelta != 0.0f) {
        const float LH = LineHeight();
        const float MaxScroll = std::max(0.0f, static_cast<float>(Lines.size()) * LH - Content.H);
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

    // Typed text replaces any selection.
    if (!Input.TextInputThisFrame.empty()) {
        if (HasSelection()) {
            DeleteSelection();
        }
        Text.insert(CaretIndex, Input.TextInputThisFrame);
        CaretIndex += Input.TextInputThisFrame.size();
        SelectionAnchor = CaretIndex;
        DesiredCaretXLogical = -1.0f;
        LinesDirty = true;
        Changed = true;
    }

    for (const Platform::Key Key : Input.KeysPressedThisFrame) {
        RebuildLinesIfNeeded(WrapWidth); // keep Lines current if an earlier key this frame edited Text

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
                    LinesDirty = true;
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
                        Text.insert(CaretIndex, Pasted);
                        CaretIndex += Pasted.size();
                        SelectionAnchor = CaretIndex;
                        DesiredCaretXLogical = -1.0f;
                        LinesDirty = true;
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
                Text.erase(CaretIndex - 1, 1);
                --CaretIndex;
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            DesiredCaretXLogical = -1.0f;
            LinesDirty = true;
            break;
        case Platform::Key::Delete:
            if (HasSelection()) {
                DeleteSelection();
                Changed = true;
            } else if (CaretIndex < Text.size()) {
                Text.erase(CaretIndex, 1);
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            DesiredCaretXLogical = -1.0f;
            LinesDirty = true;
            break;
        case Platform::Key::Enter:
            if (HasSelection()) {
                DeleteSelection();
            }
            Text.insert(CaretIndex, 1, '\n');
            ++CaretIndex;
            SelectionAnchor = CaretIndex;
            DesiredCaretXLogical = -1.0f;
            LinesDirty = true;
            Changed = true;
            break;
        case Platform::Key::Left:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionStart();
            } else if (CaretIndex > 0) {
                --CaretIndex;
            }
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            DesiredCaretXLogical = -1.0f;
            break;
        case Platform::Key::Right:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionEnd();
            } else if (CaretIndex < Text.size()) {
                ++CaretIndex;
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

    RebuildLinesIfNeeded(WrapWidth); // pick up an edit made by the last processed key
    ScrollCaretIntoView(Content.H);

    if (Changed && OnTextChanged) {
        OnTextChanged(Text);
    }
    return true; // a focused field consumes input
}

void TextArea::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    const float WrapWidth = WrapWidthFor(ContentRect);
    RebuildLinesIfNeeded(WrapWidth);

    Renderer.PushClipRect(ContentRect);

    const float LH = LineHeight();
    if (LH <= 0.0f) {
        Renderer.PopClipRect();
        return;
    }

    const bool Selected = IsFocused() && HasSelection();
    const std::size_t SelStart = Selected ? SelectionStart() : 0;
    const std::size_t SelEnd   = Selected ? SelectionEnd() : 0;

    const std::size_t LastLineIndex = Lines.size() - 1;
    std::size_t FirstVisible = ScrollOffsetY > 0.0f
        ? static_cast<std::size_t>(std::floor(ScrollOffsetY / LH))
        : 0;
    std::size_t LastVisible = static_cast<std::size_t>(std::ceil((ScrollOffsetY + ContentRect.H) / LH));
    FirstVisible = std::min(FirstVisible, LastLineIndex);
    LastVisible  = std::min(LastVisible, LastLineIndex);

    for (std::size_t I = FirstVisible; I <= LastVisible; ++I) {
        const Line& L = Lines[I];
        const float LineY = ContentRect.Y + static_cast<float>(I) * LH - ScrollOffsetY;

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
                Renderer.DrawFilledRect({X0, LineY, X1 - X0, LH}, ColorSelection);
            }
        }

        if (L.ContentEnd > L.ContentStart) {
            Renderer.DrawText(Font, std::string_view(Text).substr(L.ContentStart, L.ContentEnd - L.ContentStart),
                               {ContentRect.X, LineY}, ColorText);
        }
    }

    if (IsFocused()) {
        const std::size_t CaretLine = LineIndexForCaret(CaretIndex);
        if (CaretLine >= FirstVisible && CaretLine <= LastVisible) {
            const float CaretY = ContentRect.Y + static_cast<float>(CaretLine) * LH - ScrollOffsetY;
            const float CaretX = ContentRect.X + LineLocalX(Lines[CaretLine], CaretIndex);
            Renderer.DrawFilledRect({CaretX, CaretY, CaretWidthLogical, LH}, ColorCaret);
        }
    }

    const float TotalHeight = static_cast<float>(Lines.size()) * LH;
    if (ScrollbarWidthLogical > 0.0f && TotalHeight > ContentRect.H) {
        const float MaxScroll     = TotalHeight - ContentRect.H;
        const float ThumbHeight   = std::max(LH, ContentRect.H * (ContentRect.H / TotalHeight));
        const float ThumbTravel   = ContentRect.H - ThumbHeight;
        const float ThumbY = ContentRect.Y + (MaxScroll > 0.0f ? (ScrollOffsetY / MaxScroll) * ThumbTravel : 0.0f);
        const float TrackX = ContentRect.X + ContentRect.W - ScrollbarWidthLogical;
        Renderer.DrawFilledRect({TrackX, ThumbY, ScrollbarWidthLogical, ThumbHeight}, ColorScrollbarThumb);
    }

    Renderer.PopClipRect();
}

} // namespace Penumbra::Widgets
