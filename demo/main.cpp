#include "DemoResolvers.h"
#include "DemoTheme.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Render/SdlTtfFontBackend.h"
#include "Penumbra/Widgets/BarChart.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/Button.h"
#include "Penumbra/Widgets/CanvasWidget.h"
#include "Penumbra/Widgets/Checkbox.h"
#include "Penumbra/Widgets/DonutChart.h"
#include "Penumbra/Widgets/FocusState.h"
#include "Penumbra/Widgets/IconWidget.h"
#include "Penumbra/Widgets/Label.h"
#include "Penumbra/Widgets/LayoutPass.h"
#include "Penumbra/Widgets/LineChart.h"
#include "Penumbra/Widgets/NumericDrag.h"
#include "Penumbra/Widgets/ScrollablePanel.h"
#include "Penumbra/Widgets/SplitPanel.h"
#include "Penumbra/Widgets/TextArea.h"
#include "Penumbra/Widgets/TextInput.h"
#include "Penumbra/Widgets/ViewportWidget.h"
#include "PenumbraIcons/StandardIconBackend.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iterator>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace {

// Logical window size for the demo. A demo concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 720;
constexpr int WindowLogicalHeight = 560;

constexpr const char* FontFileName = "JetBrainsMonoNerdFontMono-Regular.ttf";

constexpr int ExtraSettingRows = 16; // filler rows so the column overflows and scrolls
constexpr int StickySectionRows = 8; // rows that scroll under the sticky section header

constexpr std::size_t IconsPerRow = 7;

using namespace Penumbra::Widgets;

std::string FormatFloat(float Value) {
    char Buffer[32];
    std::snprintf(Buffer, sizeof(Buffer), "%.2f", Value);
    return Buffer;
}

// The shortest form of a value, for chart readouts: "11", "7.8".
std::string FormatShort(float Value) {
    char Buffer[32];
    std::snprintf(Buffer, sizeof(Buffer), "%g", Value);
    return Buffer;
}

} // namespace

int main() {
    Demo::Theme Theme;

    Penumbra::Platform::PlatformWindow Window;
    if (!Window.Initialise("Penumbra Demo", WindowLogicalWidth, WindowLogicalHeight)) {
        std::fprintf(stderr, "Failed to initialise platform window: %s\n", Window.GetLastError().c_str());
        return 1;
    }

    std::printf("DPI scale factor: %.3f\n", Window.GetDpiScaleFactor());
    std::printf("Click Increment, toggle the checkbox, drag the value, type a name, "
                "scroll, and resize.\n");
    std::fflush(stdout);

    {
        Penumbra::Render::SdlTtfFontBackend FontBackend;
        const std::string FontPath = std::string(DEMO_ASSET_DIR) + "/" + FontFileName;
        float LastKnownDpiScaleFactor = Window.GetDpiScaleFactor();
        Penumbra::Render::FontHandle BodyFont =
            FontBackend.LoadFont(FontPath.c_str(), Theme.FontSizeBody, LastKnownDpiScaleFactor);
        Penumbra::Render::FontHandle ChartFont =
            FontBackend.LoadFont(FontPath.c_str(), Theme.FontSizeSmall, LastKnownDpiScaleFactor);

        Penumbra::Render::Renderer Renderer;
        if (!Renderer.Initialise(Window.GetSdlRenderer(), LastKnownDpiScaleFactor, &FontBackend)) {
            std::fprintf(stderr, "Failed to initialise renderer\n");
            Window.Shutdown();
            return 1;
        }

        FocusState Focus;

        PenumbraIcons::StandardIconBackend Icons(Theme.IconStrokeThickness, Theme.ColorTextPrimary);

        // Every widget that was handed BodyFont, so a DPI-driven font reload (see the
        // frame loop below) has somewhere to deliver the new FontHandle. Penumbra has
        // no widget-tree walk of its own -- a consumer with more than a couple of
        // fonts/widgets would want its own registry; this demo stands in for one.
        std::vector<Label*> LabelsUsingBodyFont;

        // ---- composition helpers (a stand-in for what UmbraComponentLibrary owns) ----
        auto MakeLabel = [&](const std::string& Text, Penumbra::Render::Color Color) {
            auto Widget = std::make_unique<Label>();
            Widget->FontBackend = &FontBackend;
            Widget->Font        = BodyFont;
            Widget->Text        = Text;
            Widget->ColorText   = Color;
            LabelsUsingBodyFont.push_back(Widget.get());
            return Widget;
        };
        auto MakeRow = [&]() {
            auto Row = std::make_unique<Box>();
            Row->Layout         = LayoutMode::HorizontalStack;
            Row->ChildGap       = Theme.SpacingMedium;
            Row->CrossAlignment = CrossAlign::Center;
            return Row;
        };
        auto MakeSeparator = [&]() {
            auto Line = std::make_unique<Box>();
            Line->Style.ColorBackground = Theme.ColorBorderDefault;
            Line->Style.Padding         = {0.0f, Theme.SeparatorThickness, 0.0f, 0.0f};
            return Line;
        };
        auto MakeTextButton = [&](const std::string& Text, std::function<void()> OnClick) {
            const auto Style = Demo::ResolvePrimaryButtonStyle(Theme);
            auto Btn = std::make_unique<Button>();
            Btn->ApplyStyle(Style);
            Btn->Layout         = LayoutMode::HorizontalStack; // measure + place the label child
            Btn->CrossAlignment = CrossAlign::Center;
            Btn->OnClicked      = std::move(OnClick);
            Btn->BackgroundTransitionSeconds = Theme.AnimColorSeconds;
            Btn->AddChild(MakeLabel(Text, Style.ColorLabel)); // resolver decides the label colour
            return Btn;
        };

        // ---- the settings panel, inside a scrollable viewport ----
        auto Root = std::make_unique<ScrollablePanel>();
        Root->Style            = Demo::ResolvePanelStyle(Theme);
        // Floating-card gap around the pane, exercising SplitPanel's Margin support --
        // without it this Margin was silently ignored and the pane tiled edge-to-edge
        // with its sibling.
        Root->Style.Margin      = {Theme.SpacingLarge, Theme.SpacingLarge,
                                   Theme.SpacingLarge, Theme.SpacingLarge};
        Root->ChildGap         = Theme.SpacingMedium;
        Root->CrossAlignment   = CrossAlign::Stretch;
        Root->WheelStepLogical = Theme.SpacingLarge * 3.0f;
        // In the right padding; drag the thumb, or press the track to jump there.
        Root->ScrollbarWidthLogical      = Theme.PanelScrollbarWidth;
        Root->ColorScrollbarThumb        = Theme.ColorBorderDefault;
        Root->ColorScrollbarThumbHovered = Theme.ColorTextDisabled;

        Root->AddChild(MakeLabel("Penumbra - Settings (Milestone 6)", Theme.ColorTextPrimary));
        Root->AddChild(MakeSeparator());

        int Counter = 0;

        // Counter button + live count label.
        Label* CountLabel = nullptr;
        Button* IncrementButton = nullptr;
        {
            auto Row = MakeRow();
            auto Button = MakeTextButton("Increment", [&Counter]() { ++Counter; });
            IncrementButton = Button.get();
            Row->AddChild(std::move(Button));
            auto Count = MakeLabel("Count: 0", Theme.ColorTextPrimary);
            CountLabel = Count.get();
            Row->AddChild(std::move(Count));
            Root->AddChild(std::move(Row));
        }

        // Checkbox toggling the increment button's enabled state. Both start enabled
        // so Increment works immediately; unticking the box disables it.
        {
            auto Row = MakeRow();
            auto Check = std::make_unique<Checkbox>();
            Check->ApplyStyle(Demo::ResolveCheckboxStyle(Theme));
            Check->GlyphSizeLogical = Theme.CheckboxGlyphSize;
            Check->Checked = true;
            Check->OnChanged = [IncrementButton](bool On) { IncrementButton->SetIsEnabled(On); };
            Row->AddChild(std::move(Check));
            Row->AddChild(MakeLabel("Enable the Increment button", Theme.ColorTextPrimary));
            Root->AddChild(std::move(Row));
        }

        Root->AddChild(MakeSeparator());

        // Numeric drag + text input, with a live readout label below.
        NumericDrag* Drag = nullptr;
        TextInput* Field = nullptr;
        {
            auto Row = MakeRow();
            Row->AddChild(MakeLabel("Value:", Theme.ColorTextPrimary));
            auto DragWidget = std::make_unique<NumericDrag>();
            DragWidget->Style                 = Demo::ResolveInputFieldStyle(Theme);
            DragWidget->FontBackend           = &FontBackend;
            DragWidget->Font                  = BodyFont;
            DragWidget->ColorText             = Theme.ColorTextPrimary;
            DragWidget->Value                 = 1.0f;
            DragWidget->Sensitivity           = Theme.DragSensitivity;
            DragWidget->PreferredWidthLogical = Theme.FieldWidthSmall;
            Drag = DragWidget.get();
            Row->AddChild(std::move(DragWidget));
            Root->AddChild(std::move(Row));
        }
        {
            auto Row = MakeRow();
            Row->AddChild(MakeLabel("Name:", Theme.ColorTextPrimary));
            auto FieldWidget = std::make_unique<TextInput>();
            FieldWidget->Style                 = Demo::ResolveInputFieldStyle(Theme);
            FieldWidget->FontBackend           = &FontBackend;
            FieldWidget->Font                  = BodyFont;
            FieldWidget->ColorText             = Theme.ColorTextPrimary;
            FieldWidget->ColorCaret            = Theme.ColorTextPrimary;
            FieldWidget->ColorSelection        = Theme.ColorSelection;
            FieldWidget->CaretWidthLogical     = Theme.BorderWidthDefault;
            FieldWidget->PreferredWidthLogical = Theme.FieldWidthSmall;
            FieldWidget->Focus                 = &Focus;
            FieldWidget->Clipboard             = &Window;
            Field = FieldWidget.get();
            Row->AddChild(std::move(FieldWidget));
            Root->AddChild(std::move(Row));
        }

        TextArea* Notes = nullptr;
        {
            // Label above the field, not beside it (unlike the single-line rows above) --
            // a multi-line field wants the column's full width, which Root's own
            // CrossAlign::Stretch already gives a direct child; wedging it into a
            // MakeRow() alongside a label would size it independently of that column
            // width and just get clipped by Root's own viewport once it overflowed.
            Root->AddChild(MakeLabel("Notes:", Theme.ColorTextPrimary));
            auto AreaWidget = std::make_unique<TextArea>();
            AreaWidget->Style                  = Demo::ResolveInputFieldStyle(Theme);
            AreaWidget->FontBackend            = &FontBackend;
            AreaWidget->Font                   = BodyFont;
            AreaWidget->ColorText              = Theme.ColorTextPrimary;
            AreaWidget->ColorCaret             = Theme.ColorTextPrimary;
            AreaWidget->ColorSelection         = Theme.ColorSelection;
            AreaWidget->ColorScrollbarThumb    = Theme.ColorTextDisabled;
            AreaWidget->CaretWidthLogical      = Theme.BorderWidthDefault;
            AreaWidget->PreferredWidthLogical  = Theme.FieldWidthLarge;
            AreaWidget->PreferredHeightLogical = Theme.FieldHeightLarge;
            AreaWidget->WheelStepLogical       = Theme.TextAreaWheelStep;
            AreaWidget->ScrollbarWidthLogical  = Theme.ScrollbarWidth;
            AreaWidget->Focus                  = &Focus;
            AreaWidget->Clipboard              = &Window;
            AreaWidget->Text                   = "Multi-line notes. Word-wraps automatically, "
                                                  "and scrolls (with a scrollbar) once the text "
                                                  "runs past the field's height.\n\nType, drag to "
                                                  "select, and try the arrow keys -- Up/Down move "
                                                  "by visual line and Home/End jump to the start/"
                                                  "end of the current line.";
            Notes = AreaWidget.get();
            Root->AddChild(std::move(AreaWidget));
        }

        Label* StatusLabel = nullptr;
        {
            auto Status = MakeLabel("", Theme.ColorTextDisabled);
            StatusLabel = Status.get();
            Root->AddChild(std::move(Status));
        }

        // A CanvasWidget drawing a sine wave whose amplitude follows the drag value. It
        // redraws every frame, so dragging the value reshapes it live; past a value of
        // 2 the wave runs off the canvas and the content-rect clip cuts it off.
        {
            auto Wave = std::make_unique<CanvasWidget>();
            Wave->Style                  = Demo::ResolveInputFieldStyle(Theme);
            Wave->PreferredHeightLogical = Theme.CanvasHeight;
            Wave->OnDraw = [&Theme, Drag](Penumbra::Render::Renderer& CanvasRenderer, Penumbra::Rect Content) {
                const float MidY = Content.Y + Content.H * 0.5f;
                CanvasRenderer.DrawLine({Content.X, MidY}, {Content.X + Content.W, MidY}, Theme.ColorBorderDefault,
                                        Theme.BorderWidthDefault);

                constexpr int   Samples = 64;
                constexpr float Cycles  = 2.0f;
                const float     Amplitude = Content.H * 0.25f * Drag->Value;
                std::vector<Penumbra::Point> WavePoints;
                WavePoints.reserve(Samples + 1);
                for (int Index = 0; Index <= Samples; ++Index) {
                    const float T = static_cast<float>(Index) / Samples;
                    WavePoints.push_back({Content.X + Content.W * T,
                                          MidY - Amplitude * std::sin(T * Cycles * 2.0f * std::numbers::pi_v<float>)});
                }
                CanvasRenderer.DrawPolyline(WavePoints, Theme.ColorAccentHovered, Theme.CanvasLineThickness,
                                            Penumbra::Render::LineJoin::Round, Penumbra::Render::LineCap::Round);
            };
            Root->AddChild(std::move(Wave));
        }

        // A menu, built from positioned boxes: the button is a normal child deep in the
        // scrolled column, but the menu it opens is a Fixed, z-indexed, transparent
        // backdrop over the whole window, with the panel placed absolutely under the
        // button. A press on the backdrop or Escape closes it, as does picking an option.
        Label* MenuStatusLabel = nullptr;
        {
            auto Row = MakeRow();
            auto MenuBtn = MakeTextButton("Show menu", nullptr);
            Button* MenuButton = MenuBtn.get();
            Row->AddChild(std::move(MenuBtn));
            auto MenuStatus = MakeLabel("", Theme.ColorTextDisabled);
            MenuStatusLabel = MenuStatus.get();
            Row->AddChild(std::move(MenuStatus));

            auto Layer = std::make_unique<Box>();
            Layer->Style.PositionMode = Position::Fixed;
            Layer->Style.Top = Layer->Style.Right = Layer->Style.Bottom = Layer->Style.Left = Length::Logical(0.0f);
            Layer->Style.ZIndex = Theme.ZIndexMenu;
            Box* LayerPtr       = Layer.get();
            Layer->OnPressed    = [LayerPtr]() { LayerPtr->SetIsVisible(false); };
            Layer->OnKeyPressed = [LayerPtr](Penumbra::Platform::Key Key) {
                if (Key == Penumbra::Platform::Key::Escape) {
                    LayerPtr->SetIsVisible(false);
                }
            };

            auto Menu = std::make_unique<Box>();
            Menu->Style              = Demo::ResolvePanelStyle(Theme);
            Menu->Style.Margin       = {0.0f, 0.0f, 0.0f, 0.0f};
            Menu->Style.PositionMode = Position::Absolute;
            Menu->Style.Width        = Length::Logical(Theme.MenuWidth);
            Menu->Layout             = LayoutMode::VerticalStack;
            Menu->ChildGap           = Theme.SpacingSmall;
            Menu->CrossAlignment     = CrossAlign::Stretch;
            Box* MenuPtr             = Menu.get();
            for (const std::string Text : {"Option A", "Option B"}) {
                Menu->AddChild(MakeTextButton(Text, [LayerPtr, Text, MenuStatusLabel]() {
                    MenuStatusLabel->Text = "picked: " + Text;
                    LayerPtr->SetIsVisible(false);
                }));
            }
            Layer->AddChild(std::move(Menu));
            Layer->SetIsVisible(false);
            Row->AddChild(std::move(Layer));

            // Anchored just under the button, wherever the column has scrolled it to.
            MenuButton->OnClicked = [MenuButton, MenuPtr, LayerPtr, &Theme]() {
                const Penumbra::Rect Anchor = MenuButton->GetArrangedRect();
                MenuPtr->Style.Left         = Length::Logical(Anchor.X);
                MenuPtr->Style.Top          = Length::Logical(Anchor.Y + Anchor.H + Theme.SpacingSmall);
                LayerPtr->SetIsVisible(true);
            };
            Root->AddChild(std::move(Row));
        }

        Root->AddChild(MakeSeparator());

        {
            const auto Names = PenumbraIcons::StandardIconBackend::Names();
            for (std::size_t First = 0; First < Names.size(); First += IconsPerRow) {
                auto Row = MakeRow();
                const std::size_t Last = std::min(First + IconsPerRow, Names.size());
                for (std::size_t Index = First; Index < Last; ++Index) {
                    auto Glyph = IconWidget::Builder().icon(std::string(Names[Index])).size(Theme.IconSize).build();
                    Glyph->IconBackend         = &Icons;
                    Glyph->ColorLogicalHovered = Theme.ColorAccentHovered;
                    Row->AddChild(std::move(Glyph));
                }
                Root->AddChild(std::move(Row));
            }
        }

        Root->AddChild(MakeSeparator());

        // ---- charts, fed plain C++ data ----
        // "Next data set" cycles three data sets and every chart eases to the new values,
        // "Replay" grows them in again, and the label below them names whatever chart
        // entry the mouse is over.
        struct ChartDataSet {
            std::vector<float> Done, Added;         // per weekday, the column chart
            float              Todo, Doing, Finished; // the donut
            std::vector<float> ProjectDone, ProjectOpen;
            std::vector<float> Velocity, Bugs;      // per week, the line chart
            std::vector<float> Trend;               // the sparkline
        };
        const std::vector<ChartDataSet> ChartDataSets = {
            {{3, 5, 2, 8, 6, 0, 1}, {4, 2, 6, 3, 7, 1, 0}, 12, 5, 21, {14, 9, 4, 22}, {6, 11, 3, 8},
             {3.5f, 5, 4.2f, 7, 6.2f, 8.5f, 7.8f, 9, 6, 7.5f, 10, 11}, {1, 2, 1.5f, 3, 2, 1, -1.5f, -2, 0.5f, 1, 2, 1},
             {5, 7, 6, 9, 8, 12, 10, 14, 13, 16}},
            {{6, 9, 4, 14, 11, 2, 3}, {2, 1, 3, 2, 4, 1, 0}, 4, 9, 30, {18, 12, 9, 27}, {2, 6, 1, 4},
             {5, 6, 8, 7, 9, 11, 10, 12, 14, 13, 15, 17}, {2, 1, 0, -1, 1, 2, 3, 1, 0, -2, -1, 0},
             {9, 8, 10, 11, 9, 13, 15, 14, 17, 19}},
            {{1, 2, 1, 3, 2, 0, 0}, {5, 6, 4, 7, 5, 2, 1}, 25, 3, 8, {5, 3, 1, 10}, {14, 16, 9, 20},
             {2, 3, 2.5f, 4, 3, 2, 3.5f, 4, 5, 4.5f, 6, 5}, {3, 4, 5, 3, 6, 4, 5, 7, 6, 5, 4, 6},
             {16, 14, 15, 12, 11, 12, 9, 8, 9, 6}},
        };
        std::size_t ChartDataIndex = 0;

        std::vector<ChartBase*> Charts; // repointed at the reloaded font on a DPI change
        auto SetUpChart = [&](ChartBase& Chart, float Height) {
            Chart.ApplyStyle(Demo::ResolveChartStyle(Theme));
            Chart.FontBackend            = &FontBackend;
            Chart.Font                   = ChartFont;
            Chart.PreferredHeightLogical = Height;
            Charts.push_back(&Chart);
        };

        // The hovered entry's description, owned by the chart that set it, so one chart
        // clearing its hover doesn't wipe another's.
        std::string      ChartHoverText;
        const ChartBase* ChartHoverOwner = nullptr;
        auto ReportHover = [&ChartHoverText, &ChartHoverOwner](const ChartBase* Chart, int Index, std::string Text) {
            if (Index >= 0) {
                ChartHoverText  = std::move(Text);
                ChartHoverOwner = Chart;
            } else if (ChartHoverOwner == Chart) {
                ChartHoverText.clear();
                ChartHoverOwner = nullptr;
            }
        };

        BarChart*   Columns   = nullptr;
        DonutChart* Donut     = nullptr;
        BarChart*   Projects  = nullptr;
        LineChart*  Velocity  = nullptr;
        LineChart*  Sparkline = nullptr;
        {
            auto ColumnsWidget = std::make_unique<BarChart>();
            SetUpChart(*ColumnsWidget, Theme.ChartHeight);
            ColumnsWidget->Labels = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            Columns                 = ColumnsWidget.get();
            Columns->OnHoverChanged = [&ReportHover, Columns](int Index) {
                if (Index < 0) {
                    ReportHover(Columns, Index, {});
                    return;
                }
                const auto Day = static_cast<std::size_t>(Index);
                ReportHover(Columns, Index,
                            Columns->Labels[Day] + ": " + FormatShort(Columns->Series[0].Values[Day]) + " done, " +
                                FormatShort(Columns->Series[1].Values[Day]) + " added");
            };
            Root->AddChild(std::move(ColumnsWidget));
        }
        {
            // As wide as the column, and the ring takes the biggest circle that fits.
            auto DonutWidget = std::make_unique<DonutChart>();
            SetUpChart(*DonutWidget, Theme.ChartHeight);
            DonutWidget->CentreFont    = BodyFont;
            DonutWidget->CentreCaption = "tasks";
            Donut                      = DonutWidget.get();
            Donut->OnHoverChanged      = [&ReportHover, Donut](int Index) {
                if (Index < 0) {
                    ReportHover(Donut, Index, {});
                    return;
                }
                const ChartSlice& Slice = Donut->Slices[static_cast<std::size_t>(Index)];
                ReportHover(Donut, Index, Slice.Label + ": " + FormatShort(Slice.Value) + " of " + Donut->CentreText);
            };
            Root->AddChild(std::move(DonutWidget));
        }
        {
            auto ProjectsWidget = std::make_unique<BarChart>();
            SetUpChart(*ProjectsWidget, Theme.ChartHeight * 0.7f);
            ProjectsWidget->Orientation = BarOrientation::Horizontal;
            ProjectsWidget->Stacked     = true;
            ProjectsWidget->Labels      = {"Penumbra", "Chaos", "Cosmos", "Cairn"};
            Projects                    = ProjectsWidget.get();
            Projects->OnHoverChanged    = [&ReportHover, Projects](int Index) {
                if (Index < 0) {
                    ReportHover(Projects, Index, {});
                    return;
                }
                const auto Project = static_cast<std::size_t>(Index);
                ReportHover(Projects, Index,
                            Projects->Labels[Project] + ": " + FormatShort(Projects->Series[0].Values[Project]) +
                                " done, " + FormatShort(Projects->Series[1].Values[Project]) + " open");
            };
            Root->AddChild(std::move(ProjectsWidget));
        }
        {
            auto VelocityWidget = std::make_unique<LineChart>();
            SetUpChart(*VelocityWidget, Theme.ChartHeight);
            for (int Week = 1; Week <= 12; ++Week) {
                VelocityWidget->Labels.push_back("W" + std::to_string(Week));
            }
            Velocity                 = VelocityWidget.get();
            Velocity->OnHoverChanged = [&ReportHover, Velocity](int Index) {
                if (Index < 0) {
                    ReportHover(Velocity, Index, {});
                    return;
                }
                const auto Week = static_cast<std::size_t>(Index);
                ReportHover(Velocity, Index,
                            Velocity->Labels[Week] + ": velocity " + FormatShort(Velocity->Series[0].Values[Week]) +
                                ", bugs " + FormatShort(Velocity->Series[1].Values[Week]));
            };
            Root->AddChild(std::move(VelocityWidget));
        }
        {
            auto SparklineWidget = std::make_unique<LineChart>();
            SetUpChart(*SparklineWidget, Theme.ChartSparklineHeight);
            SparklineWidget->ShowAxes = false;
            Sparkline                 = SparklineWidget.get();
            Root->AddChild(std::move(SparklineWidget));
        }

        // A progress bar built from plain Boxes, the way Cairn builds its bars: a fill
        // and a spacer whose FlexGrow split is the share of tasks done. A FlexGrow
        // transition eases each change, and StartingStyle grows the fill in from 0 when
        // it first appears.
        Box* ProgressFill   = nullptr;
        Box* ProgressSpacer = nullptr;
        {
            Root->AddChild(MakeLabel("Share of tasks done", Theme.ColorTextDisabled));
            const float Radius = Theme.ProgressHeight * 0.5f;
            const std::vector<Transition> EaseGrow = {
                {TransitionProperty::FlexGrow, Theme.ChartAppearSeconds, Penumbra::Anim::Easing::EaseOut}};

            auto Track                   = std::make_unique<Box>();
            Track->Layout                = LayoutMode::HorizontalStack;
            Track->CrossAlignment        = CrossAlign::Stretch;
            Track->Style.Height          = Length::Logical(Theme.ProgressHeight);
            Track->Style.ColorBackground = Theme.ColorBackgroundPrimary;
            Track->Style.BorderRadius    = Radius;

            auto Fill                   = std::make_unique<Box>();
            Fill->Style.ColorBackground = Theme.ColorChartGreen;
            Fill->Style.BorderRadius    = Radius;
            Fill->Style.Transitions     = EaseGrow;
            Fill->StartingStyle         = Fill->Style; // FlexGrow 0
            ProgressFill                = static_cast<Box*>(Track->AddChild(std::move(Fill)));

            auto Spacer               = std::make_unique<Box>();
            Spacer->Style.Transitions = EaseGrow;
            ProgressSpacer            = static_cast<Box*>(Track->AddChild(std::move(Spacer)));
            Root->AddChild(std::move(Track));
        }

        auto ApplyChartData = [&](const ChartDataSet& Data) {
            Columns->Series   = {{"Done", Data.Done, Theme.ColorChartBlue}, {"Added", Data.Added, Theme.ColorChartOrange}};
            Donut->Slices     = {{"Todo", Data.Todo, Theme.ColorChartOrange},
                                 {"Doing", Data.Doing, Theme.ColorChartBlue},
                                 {"Done", Data.Finished, Theme.ColorChartGreen}};
            Donut->CentreText = FormatShort(Data.Todo + Data.Doing + Data.Finished);
            Projects->Series  = {{"Done", Data.ProjectDone, Theme.ColorChartGreen},
                                 {"Open", Data.ProjectOpen, Theme.ColorChartOrange}};
            Velocity->Series  = {{"Velocity", Data.Velocity, Theme.ColorChartBlue},
                                 {"Bugs", Data.Bugs, Theme.ColorChartOrange}};
            Sparkline->Series = {{"Trend", Data.Trend, Theme.ColorChartGreen}};

            const float DoneShare          = Data.Finished / (Data.Todo + Data.Doing + Data.Finished);
            ProgressFill->Style.FlexGrow   = DoneShare;
            ProgressSpacer->Style.FlexGrow = 1.0f - DoneShare;
        };
        ApplyChartData(ChartDataSets[ChartDataIndex]);

        Label* ChartStatusLabel = nullptr;
        {
            auto Row = MakeRow();
            Row->AddChild(MakeTextButton("Next data set", [&]() {
                ChartDataIndex = (ChartDataIndex + 1) % ChartDataSets.size();
                ApplyChartData(ChartDataSets[ChartDataIndex]);
            }));
            // The charts grew in at startup, below the fold; this shows it again.
            Row->AddChild(MakeTextButton("Replay", [&Charts]() {
                for (ChartBase* Chart : Charts) {
                    Chart->ReplayAppear();
                }
            }));
            Root->AddChild(std::move(Row));
            auto Status      = MakeLabel("", Theme.ColorTextDisabled);
            ChartStatusLabel = Status.get();
            Root->AddChild(std::move(Status));
        }

        Root->AddChild(MakeSeparator());

        // Filler rows so the column is taller than the viewport (proves scrolling).
        for (int Index = 1; Index <= ExtraSettingRows; ++Index) {
            Root->AddChild(MakeLabel("Additional setting entry " + std::to_string(Index),
                                     Theme.ColorTextPrimary));
        }

        // ---- the scene viewport, alongside the settings panel ----
        // A ViewportWidget proves the render-target seam: its OnRenderScene draws a
        // checkerboard through the ordinary Renderer interface while Penumbra (not the
        // callback) handles the off-screen texture and the blit back into the UI pass.
        auto ScenePane = std::make_unique<ViewportWidget>();
        ScenePane->Style          = Demo::ResolvePanelStyle(Theme);
        ScenePane->Style.Margin   = {Theme.SpacingLarge, Theme.SpacingLarge,
                                     Theme.SpacingLarge, Theme.SpacingLarge};
        ScenePane->SceneClearColor = Theme.ColorBackgroundPrimary; // odd cells show this
        ScenePane->OnRenderScene =
            [&Theme](Penumbra::Render::Renderer& SceneRenderer, Penumbra::Point SceneSize) {
                // Showcase for the Renderer's shape primitives: a drop shadow sitting
                // behind a gradient-filled "card", plus a chevron built from two DrawLine
                // calls and a caret built from one DrawTriangleFilled -- the kind of thing
                // Pharos would use in place of TreeRow's ASCII '-'/'+' glyph.
                const Penumbra::Rect Card{SceneSize.X * 0.1f, SceneSize.Y * 0.1f,
                                          SceneSize.X * 0.8f, SceneSize.Y * 0.35f};
                SceneRenderer.DrawDropShadow(Card, {0, 0, 0, 140}, 18.0f, Theme.BorderRadiusSmall);
                SceneRenderer.DrawGradientRect(Card, Theme.ColorAccentHovered, Theme.ColorAccentPressed,
                                               Theme.BorderRadiusSmall);

                const float ChevronCx = SceneSize.X * 0.3f;
                const float ChevronCy = SceneSize.Y * 0.62f;
                const float ChevronR  = 12.0f;
                SceneRenderer.DrawLine({ChevronCx - ChevronR, ChevronCy - ChevronR},
                                       {ChevronCx, ChevronCy + ChevronR * 0.4f},
                                       Theme.ColorTextPrimary, 3.0f);
                SceneRenderer.DrawLine({ChevronCx, ChevronCy + ChevronR * 0.4f},
                                       {ChevronCx + ChevronR, ChevronCy - ChevronR},
                                       Theme.ColorTextPrimary, 3.0f);

                const float CaretCx = SceneSize.X * 0.55f;
                const float CaretCy = SceneSize.Y * 0.62f;
                const float CaretR  = 12.0f;
                SceneRenderer.DrawTriangleFilled({CaretCx - CaretR * 0.6f, CaretCy - CaretR},
                                                 {CaretCx - CaretR * 0.6f, CaretCy + CaretR},
                                                 {CaretCx + CaretR * 0.9f, CaretCy},
                                                 Theme.ColorTextPrimary);

                // Original checkerboard, shifted below the showcase -- still exercises the
                // off-screen scene texture render-target seam ViewportWidget exists to test.
                constexpr int Cells = 6;
                const float Top   = SceneSize.Y * 0.78f;
                const float CellW = SceneSize.X / static_cast<float>(Cells);
                const float CellH = (SceneSize.Y - Top) * 0.5f;
                for (int Row = 0; Row < 2; ++Row) {
                    for (int Col = 0; Col < Cells; ++Col) {
                        if ((Row + Col) % 2 == 0) {
                            SceneRenderer.DrawFilledRect(
                                {Col * CellW, Top + Row * CellH, CellW, CellH}, Theme.ColorAccent);
                        }
                    }
                }
            };

        // Split the two panes with a draggable handle whose margin-driven gap around
        // each side is only possible now that SplitPanel::Arrange honours Style.Margin.
        auto Split = std::make_unique<SplitPanel>();
        SplitPanelStyle SplitStyle;
        SplitStyle.ColorHandle        = Theme.ColorBorderDefault;
        SplitStyle.ColorHandleHovered = Theme.ColorAccentHovered;
        SplitStyle.ColorHandleDragged = Theme.ColorAccentPressed;
        Split->ApplyStyle(SplitStyle);
        Split->Axis                   = SplitAxis::Horizontal;
        Split->HandleThicknessLogical = Theme.SpacingLarge;
        Split->MinPaneSizeLogical     = 200.0f;
        Split->SplitRatio             = 0.48f;
        Split->SetFirst(std::move(Root));
        Split->SetSecond(std::move(ScenePane));

        bool TextInputActive = false;

        Penumbra::Platform::InputState Input;
        bool KeepRunning = true;
        while (KeepRunning) {
            KeepRunning = Window.PumpEventsAndBuildInput(Input);

            // Pick up a DPI scale change (e.g. the window was dragged to a different
            // display) before this frame draws anything with it.
            const float CurrentDpiScaleFactor = Window.GetDpiScaleFactor();
            Renderer.SetDpiScaleFactor(CurrentDpiScaleFactor);
            if (CurrentDpiScaleFactor != LastKnownDpiScaleFactor) {
                // Existing FontHandles stay rasterised at the old DPI (Penumbra doesn't
                // reload them itself -- that's each consumer's own job), so the demo
                // reloads its one font and repoints every widget at the fresh handle,
                // the same way Pharos would for its own fonts.
                BodyFont = FontBackend.LoadFont(FontPath.c_str(), Theme.FontSizeBody, CurrentDpiScaleFactor);
                for (Label* L : LabelsUsingBodyFont) {
                    L->Font = BodyFont;
                }
                Drag->Font  = BodyFont;
                Field->Font = BodyFont;
                Notes->Font = BodyFont;
                ChartFont = FontBackend.LoadFont(FontPath.c_str(), Theme.FontSizeSmall, CurrentDpiScaleFactor);
                for (ChartBase* Chart : Charts) {
                    Chart->Font = ChartFont;
                }
                Donut->CentreFont = BodyFont;
                LastKnownDpiScaleFactor = CurrentDpiScaleFactor;
            }

            // Live values flow demo state → widget text every frame.
            CountLabel->Text = "Count: " + std::to_string(Counter);
            StatusLabel->Text =
                "live: value=" + FormatFloat(Drag->Value) + "  name=\"" + Field->Text + "\"";
            ChartStatusLabel->Text = ChartHoverText;

            // Settings on the left, the scene viewport on the right, sized by the
            // SplitPanel; each pane's own Style.Margin (set above) is what produces the
            // floating-card gap now that SplitPanel::Arrange honours it.
            const Penumbra::Point WindowSize = Window.GetLogicalWindowSize();
            const Penumbra::Rect  FullRect{0.0f, 0.0f, WindowSize.X, WindowSize.Y};

            SetLayoutViewportLogical(WindowSize); // vw/vh, and the window that fixed widgets sit in
            {
                const LayoutPass Pass; // repeated measures within the layout reuse their results
                Split->Measure({FullRect.W, FullRect.H});
                Split->Arrange(FullRect);
            }

            if (Input.MouseButtonPressedThisFrame[0]) {
                Focus.Focused = nullptr;
            }
            Split->UpdateInteractionState(Input);

            const bool WantTextInput = (Focus.Focused != nullptr);
            if (WantTextInput != TextInputActive) {
                Window.SetTextInputActive(WantTextInput);
                TextInputActive = WantTextInput;
            }

            Renderer.BeginFrame(Theme.ColorBackgroundPrimary, Input.DeltaTimeSeconds);
            Split->Draw(Renderer);
            Renderer.EndFrameAndPresent();
        }
    }

    Window.Shutdown();
    return 0;
}
