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
    Style.Padding                 = {Theme.SpacingLarge, Theme.SpacingMedium,
                                     Theme.SpacingLarge, Theme.SpacingMedium};
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

Penumbra::Widgets::ChartStyle ResolveChartStyle(const Theme& Theme) {
    Penumbra::Widgets::ChartStyle Style;
    static_cast<Penumbra::Widgets::BoxStyle&>(Style) = ResolveInputFieldStyle(Theme);
    Style.ColorAxis                = Theme.ColorTextDisabled;
    Style.ColorGridLine            = Theme.ColorChartGrid;
    Style.ColorLabel               = Theme.ColorTextDisabled;
    Style.ColorHover               = Theme.ColorChartHover;
    Style.AxisThicknessLogical     = Theme.BorderWidthDefault;
    Style.GridLineThicknessLogical = Theme.BorderWidthDefault;
    Style.TickSpacingLogical       = Theme.ChartTickSpacing;
    Style.LabelGapLogical          = Theme.ChartLabelGap;
    Style.LineThicknessLogical     = Theme.ChartLineThickness;
    Style.PointRadiusLogical       = Theme.ChartPointRadius;
    Style.HoverPointRadiusLogical  = Theme.ChartHoverPointRadius;
    Style.AreaOpacity              = Theme.ChartAreaOpacity;
    Style.BarGapFraction           = Theme.ChartBarGapFraction;
    Style.BarCornerRadiusLogical   = Theme.ChartBarCornerRadius;
    Style.DonutThicknessLogical    = Theme.ChartDonutThickness;
    Style.SegmentGapDegrees        = Theme.ChartSegmentGap;
    Style.AppearSeconds            = Theme.ChartAppearSeconds;
    Style.TransitionSeconds        = Theme.ChartTransitionSeconds;
    return Style;
}

} // namespace Demo
