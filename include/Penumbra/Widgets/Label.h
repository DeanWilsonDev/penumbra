#pragma once

#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Widgets/Box.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

// Intrinsic text content; a leaf. The font backend is injected (the Measure pass
// has no Renderer, yet text size must be known there) and identifies the font the
// Renderer was initialised with.
class Label : public Box {
public:
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    std::string           Text;
    Render::Color         ColorText{0, 0, 0, 0};

    // Lustre's `max-width`/`text-overflow` (docs/
    // penumbra_iris_lustre_componentization_gaps_requirements.md's
    // InspectorRow migration finding -- no truncation concept existed
    // before this). Unset MaxWidthLogical means "no constraint," the
    // current unbounded-intrinsic-size behavior. Set with
    // TruncateWithEllipsis false means "clip, no dots" (a Renderer clip
    // rect); true means "truncate and append .." -- both plain fields
    // rather than Builder methods, same "set directly post-build" pattern
    // FontBackend/Font above already use.
    std::optional<float> MaxWidthLogical;
    bool                  TruncateWithEllipsis{false};

    // Fluent, chainable construction — see Box::Builder for the naming-convention
    // rationale (method names match Iris prop names exactly, className() aside).
    // Adds text() for Iris <Text>'s own content prop on top of the shared set.
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
};

} // namespace Penumbra::Widgets
