#pragma once

#include "Penumbra/Render/Color.h"

namespace Demo {

// The app owns the vocabulary AND the values. Penumbra never sees these names.
// This is the ONLY place in the project where values and aesthetics live.
struct Theme {
    float SpacingSmall  = 8.0f;
    float SpacingMedium = 12.0f;
    float SpacingLarge  = 16.0f;

    Penumbra::Render::Color ColorBackgroundPrimary = {26, 26, 26, 255};
    Penumbra::Render::Color ColorSurfaceRaised     = {58, 58, 65, 255};
    Penumbra::Render::Color ColorAccent            = {77, 127, 235, 255};
    Penumbra::Render::Color ColorAccentHovered     = {97, 147, 255, 255};
    Penumbra::Render::Color ColorAccentPressed     = {57, 107, 215, 255};
    Penumbra::Render::Color ColorTextPrimary       = {235, 235, 235, 255};
    Penumbra::Render::Color ColorTextDisabled      = {120, 120, 128, 255};
    Penumbra::Render::Color ColorBorderDefault     = {70, 70, 75, 255};
    Penumbra::Render::Color ColorControlDisabled   = {45, 45, 50, 255};
    Penumbra::Render::Color ColorSelection         = {77, 127, 235, 120}; // translucent accent

    // Chart series colours, and the translucent wash behind a hovered category.
    Penumbra::Render::Color ColorChartBlue   = {97, 147, 255, 255};
    Penumbra::Render::Color ColorChartOrange = {240, 160, 70, 255};
    Penumbra::Render::Color ColorChartGreen  = {90, 200, 140, 255};
    Penumbra::Render::Color ColorChartHover  = {255, 255, 255, 28};
    Penumbra::Render::Color ColorChartGrid   = {70, 70, 78, 255};

    float BorderRadiusSmall  = 20.0f;
    float BorderWidthDefault = 1.0f;

    float FontSizeBody  = 16.0f;
    float FontSizeSmall = 11.0f; // chart labels

    float CheckboxGlyphSize  = 18.0f;  // edge length of the checkbox square
    float DragSensitivity    = 0.05f;  // value change per logical px dragged
    float FieldWidthSmall    = 160.0f; // min width for numeric/text fields
    float FieldWidthLarge    = 240.0f; // base width for the multi-line text area (it also
                                        // stretches to fill Root's column via CrossAlign::Stretch)
    float FieldHeightLarge   = 96.0f;  // height for the multi-line text area
    float TextAreaWheelStep  = 24.0f;  // logical px scrolled per wheel notch in a text area
    float ScrollbarWidth    = 4.0f;    // gutter width for the text area's scroll indicator
    float PanelScrollbarWidth = 6.0f;  // the settings column's draggable scrollbar
    float SeparatorThickness = 2.0f;   // height of a separator Box
    float IconSize            = 20.0f;
    float IconStrokeThickness = 1.5f;
    float CanvasHeight        = 64.0f; // content height of the settings column's wave canvas
    float CanvasLineThickness = 2.0f;

    float ChartHeight           = 150.0f; // content height of a full-size chart
    float ChartSparklineHeight  = 32.0f;
    float ChartTickSpacing      = 28.0f;
    float ChartLabelGap         = 6.0f;
    float ChartLineThickness    = 2.0f;
    float ChartPointRadius      = 2.5f;
    float ChartHoverPointRadius = 4.5f;
    float ChartAreaOpacity      = 0.25f;
    float ChartBarGapFraction   = 0.3f;
    float ChartBarCornerRadius  = 2.0f;
    float ChartDonutThickness   = 18.0f;
    float ChartSegmentGap       = 3.0f;
    float ChartAppearSeconds    = 0.6f;
    float ChartTransitionSeconds = 0.4f;
    float ProgressHeight         = 8.0f; // the Box-built progress bar under the charts

    float AnimColorSeconds   = 0.09f;  // easing time constant for hover/press colours

    // Positioning: the badge on the card that shows it, and the fixed panel that card's
    // button opens, which sits over both panes in the window's corner.
    float BadgeSize           = 12.0f;
    float BadgeOffset         = -4.0f; // pokes past the card's top-right corner
    float FloatingPanelWidth  = 240.0f;
    int   ZIndexFloatingPanel = 10;

    // The modal the same card opens: over everything, including the fixed panel.
    int                     ZIndexModal        = 20;
    float                   ModalWidth         = 320.0f;
    Penumbra::Render::Color ColorModalBackdrop = {0, 0, 0, 150};
};

} // namespace Demo
