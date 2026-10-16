#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Platform/IClipboard.h"
#include "Penumbra/Platform/InputState.h"
#include "Penumbra/Render/Color.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Widgets/FocusState.h"

#include <compare>
#include <cstddef>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

class Label;
class WidgetBase;

class TextSelection {
public:
    FocusState*           Focus{nullptr};
    Platform::IClipboard* Clipboard{nullptr};
    Render::Color         ColorSelection{0x6E, 0x8E, 0xFF, 0x55};

    void Update(WidgetBase& Scope, Rect Region, const Platform::InputState& Input);

    bool        HasSelection() const;
    std::string SelectedText(WidgetBase& Scope) const;
    void        SelectAll(WidgetBase& Scope);
    void        Clear();
    void        DrawGaps(Render::Renderer& Renderer) const;

private:
    struct Position {
        std::size_t Label{0};
        std::size_t Index{0};

        friend bool operator==(const Position&, const Position&) = default;
        friend auto operator<=>(const Position&, const Position&) = default;
    };

    struct Entry {
        Widgets::Label*   Target{nullptr};
        const WidgetBase* Paragraph{nullptr};
    };

    static std::vector<Entry> CollectLabels(WidgetBase& Scope);
    static Position           PositionAt(const std::vector<Entry>& Labels, Point PointLogical);
    std::string               TextOf(const std::vector<Entry>& Labels) const;
    void                      Paint(const std::vector<Entry>& Labels);

    Position          Anchor;
    Position          Caret;
    std::vector<Rect> Gaps;
    bool              Anchored{false};
    bool              Dragging{false};
    bool              Painted{false};
};

} // namespace Penumbra::Widgets
