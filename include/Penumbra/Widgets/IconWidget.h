#pragma once

#include "Penumbra/Backends/IIconBackend.h"
#include "Penumbra/Widgets/WidgetBase.h"

#include <memory>
#include <string>

namespace Penumbra::Widgets {

// A leaf widget that draws a single named vector glyph through an app-supplied
// IIconBackend. A WidgetBase subclass directly, same as ImageWidget: no box model
// of its own. Unlike
// ImageWidget there is no separate load step — IconBackend is a plain public field
// (mirroring how Walker.cpp already sets Label::FontBackend/Font directly post-build,
// not through the Builder), and Draw() calls it fresh every frame since an icon is
// redrawn procedurally, never cached as a texture.
class IconWidget : public WidgetBase {
public:
    std::string       IconName;
    Backends::IIconBackend* IconBackend{nullptr};

    // No true intrinsic size exists for a procedurally-drawn glyph (no asset to
    // measure, unlike ImageWidget's decoded texture) — a fixed logical square,
    // overridable directly, same "reasonable default, not a real measurement"
    // treatment as everywhere else in this PoC that has no better answer.
    float SizeLogical{16.0f};

    // Per-call icon tint, threaded through to IIconBackend::DrawIcon -- plain public
    // fields (mirroring IconBackend above), not Builder methods, since <Icon>'s
    // resolved color is set post-build by the resolver same as Label::FontBackend/
    // Font. ColorLogical is the Default-state color; the Hovered/Pressed/Disabled
    // overrides follow BoxStyle's own "alpha != 0 means set" fallback convention
    // (see Box::BorderForState) -- unset means "use ColorLogical unchanged". Leaving
    // every field default-constructed (all-zero-alpha) reproduces the pre-IconColor
    // behaviour exactly: DrawIcon receives a zero-alpha color and the backend falls
    // back to its own hardcoded default.
    Render::Color ColorLogical{0, 0, 0, 0};
    Render::Color ColorLogicalHovered{0, 0, 0, 0};
    Render::Color ColorLogicalPressed{0, 0, 0, 0};
    Render::Color ColorLogicalDisabled{0, 0, 0, 0};

    Point Measure(Point AvailableSizeLogical) override;
    void  Arrange(Rect FinalRectLogical) override;
    bool  UpdateInteractionState(const Platform::InputState&) override;
    void  Draw(Render::Renderer&) override;

    InteractionState GetInteractionState() const { return CurrentState; }

    // Fluent, chainable construction. Deliberately narrow: icon(), size(), and
    // className(); no child()/children() because this is a leaf.
    class Builder {
    public:
        Builder();

        Builder& icon(std::string Value);
        // Overrides SizeLogical's 16px default; unset means "use the default".
        Builder& size(float Value);
        Builder& className(std::string Value);

        std::unique_ptr<IconWidget> build();

    private:
        std::unique_ptr<IconWidget> Owned;
    };

private:
    // The color Draw should use for CurrentState -- ColorLogicalHovered/Pressed/
    // Disabled when set (non-zero alpha), else ColorLogical unchanged. Mirrors
    // Box::BorderForState's fallback convention exactly.
    Render::Color ColorForState() const;

    bool             PressedInside{false};
    bool             WasHovered{false};
    InteractionState CurrentState{InteractionState::Default};
};

} // namespace Penumbra::Widgets
