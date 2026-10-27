#pragma once

#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Widgets/Box.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

struct DropdownState;

struct DropdownItem {
    std::string Label;
    std::function<void(Render::Renderer&, Rect, Render::Color)> DrawLeadingVisual;
};

// An icon-sized select trigger. Its list opens as ordinary positioned boxes: a Fixed,
// full-window, transparent backdrop at DropdownStyle::MenuZIndex, with the menu placed
// absolutely over it at the trigger's right edge. A press on the backdrop or Escape
// closes it, as does choosing a row, and while it's open the backdrop keeps the pointer
// and keys from everything under it.
class Dropdown : public Box {
public:
    ~Dropdown() override;

    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    std::vector<DropdownItem> Items;
    std::size_t           SelectedIndex{0};
    std::function<void(std::size_t)> OnSelectionChanged;

    void ApplyStyle(const DropdownStyle& Style);
    bool GetIsOpen() const;
    void Dismiss();

    bool UpdateInteractionState(const Platform::InputState&) override;

protected:
    // A trigger whose selected item has no leading visual shows that item's label and a
    // chevron, sized to the widest label.
    static constexpr float TriggerChevronWidthLogical{16.0f};
    bool  DrawsSelectedLabel() const;

    Point MeasureContent(Point AvailableContentSize) override;
    void  DrawContent(Render::Renderer&, Rect ContentRect) override;

private:
    void Open();

    DropdownStyle                  DropdownVisualStyle{};
    std::shared_ptr<DropdownState> State;
    bool                           PressedInside{false};
    Box*                           Layer{nullptr}; // the backdrop and menu, a child; hidden while closed
};

} // namespace Penumbra::Widgets
