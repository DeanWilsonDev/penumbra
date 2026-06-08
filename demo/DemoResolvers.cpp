#include "DemoResolvers.h"

namespace Demo {

Penumbra::Widgets::BoxStyle ResolvePanelStyle(const Theme& Theme) {
    Penumbra::Widgets::BoxStyle Style;
    Style.ColorBackground = Theme.ColorSurfaceRaised;
    Style.ColorBorder     = Theme.ColorBorderDefault;
    Style.BorderWidth     = Theme.BorderWidthDefault;
    Style.BorderRadius    = Theme.BorderRadiusSmall;
    Style.Padding         = {Theme.SpacingLarge, Theme.SpacingLarge,
                             Theme.SpacingLarge, Theme.SpacingLarge};
    return Style;
}

Penumbra::Widgets::ButtonStyle ResolvePrimaryButtonStyle(const Theme& Theme) {
    Penumbra::Widgets::ButtonStyle Style;
    Style.ColorBackground         = Theme.ColorAccent;
    Style.ColorBackgroundHovered  = Theme.ColorAccentHovered;
    Style.ColorBackgroundPressed  = Theme.ColorAccentPressed;
    Style.ColorBackgroundDisabled = Theme.ColorControlDisabled;
    Style.ColorLabel              = Theme.ColorTextPrimary; // for a future Label child
    Style.ColorBorder             = Theme.ColorBorderDefault;
    Style.BorderWidth             = Theme.BorderWidthDefault;
    Style.BorderRadius            = Theme.BorderRadiusSmall;
    // Until labels (Milestone 5) give buttons their natural content size, padding
    // alone gives them a clickable footprint.
    Style.Padding                 = {40.0f, 16.0f, 40.0f, 16.0f};
    return Style;
}

Penumbra::Widgets::CheckboxStyle ResolveCheckboxStyle(const Theme& Theme) {
    Penumbra::Widgets::CheckboxStyle Style;
    Style.ColorBackground  = Theme.ColorBackgroundPrimary; // empty box fill
    Style.ColorBorder      = Theme.ColorBorderDefault;
    Style.BorderWidth      = Theme.BorderWidthDefault;
    Style.BorderRadius     = Theme.BorderRadiusSmall;
    Style.ColorBoxChecked  = Theme.ColorAccent;
    Style.ColorCheckMark   = Theme.ColorTextPrimary;
    return Style;
}

Penumbra::Widgets::BoxStyle ResolveInputFieldStyle(const Theme& Theme) {
    Penumbra::Widgets::BoxStyle Style;
    Style.ColorBackground = Theme.ColorBackgroundPrimary;
    Style.ColorBorder     = Theme.ColorBorderDefault;
    Style.BorderWidth     = Theme.BorderWidthDefault;
    Style.BorderRadius    = Theme.BorderRadiusSmall;
    Style.Padding         = {Theme.SpacingSmall, Theme.SpacingSmall,
                             Theme.SpacingSmall, Theme.SpacingSmall};
    return Style;
}

} // namespace Demo
