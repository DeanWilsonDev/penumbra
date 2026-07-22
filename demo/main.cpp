#include "DemoResolvers.h"
#include "DemoTheme.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Render/SdlTtfFontBackend.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/Button.h"
#include "Penumbra/Widgets/Checkbox.h"
#include "Penumbra/Widgets/FocusState.h"
#include "Penumbra/Widgets/Label.h"
#include "Penumbra/Widgets/NumericDrag.h"
#include "Penumbra/Widgets/OverlayHost.h"
#include "Penumbra/Widgets/ScrollablePanel.h"
#include "Penumbra/Widgets/SplitPanel.h"
#include "Penumbra/Widgets/TextInput.h"
#include "Penumbra/Widgets/ViewportWidget.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace {

// Logical window size for the demo. A demo concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 720;
constexpr int WindowLogicalHeight = 560;

constexpr const char* FontFileName = "JetBrainsMonoNerdFontMono-Regular.ttf";

constexpr int ExtraSettingRows = 16; // filler rows so the column overflows and scrolls

using namespace Penumbra::Widgets;

std::string FormatFloat(float Value) {
    char Buffer[32];
    std::snprintf(Buffer, sizeof(Buffer), "%.2f", Value);
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

        Penumbra::Render::Renderer Renderer;
        if (!Renderer.Initialise(Window.GetSdlRenderer(), LastKnownDpiScaleFactor, &FontBackend)) {
            std::fprintf(stderr, "Failed to initialise renderer\n");
            Window.Shutdown();
            return 1;
        }

        FocusState Focus;

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

        Label* StatusLabel = nullptr;
        {
            auto Status = MakeLabel("", Theme.ColorTextDisabled);
            StatusLabel = Status.get();
            Root->AddChild(std::move(Status));
        }

        // Dropdown menu, proving OverlayHost: the button is a normal child deep in
        // Root's tree, but the menu it opens paints above the whole SplitPanel and
        // gets first refusal on input, anchored to the button's own arranged rect.
        Button* MenuButton = nullptr;
        Label*  MenuStatusLabel = nullptr;
        {
            auto Row = MakeRow();
            auto MenuBtn = MakeTextButton("Show menu", nullptr);
            MenuButton = MenuBtn.get();
            Row->AddChild(std::move(MenuBtn));
            auto MenuStatus = MakeLabel("", Theme.ColorTextDisabled);
            MenuStatusLabel = MenuStatus.get();
            Row->AddChild(std::move(MenuStatus));
            Root->AddChild(std::move(Row));
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

        // OverlayHost wraps the whole app tree so the dropdown menu below can paint
        // above it and claim input first, however deep the button that opens it is.
        auto Host = std::make_unique<OverlayHost>();
        OverlayHost* HostPtr = Host.get();
        Host->SetRoot(std::move(Split));

        MenuButton->OnClicked = [HostPtr, MenuButton, MenuStatusLabel, &Theme, &MakeTextButton]() {
            auto Menu = std::make_unique<Box>();
            Menu->Style          = Demo::ResolvePanelStyle(Theme);
            Menu->Style.Margin   = {0.0f, 0.0f, 0.0f, 0.0f};
            Menu->Layout         = LayoutMode::VerticalStack;
            Menu->ChildGap       = Theme.SpacingSmall;
            Menu->CrossAlignment = CrossAlign::Stretch;

            auto AddOption = [&](const std::string& Text) {
                Menu->AddChild(MakeTextButton(Text, [HostPtr, MenuStatusLabel, Text]() {
                    MenuStatusLabel->Text = "picked: " + Text;
                    HostPtr->DismissAll();
                }));
            };
            AddOption("Option A");
            AddOption("Option B");

            // Sized to its own content, anchored just below the button that opened it.
            constexpr float MenuWidth = 180.0f;
            const Penumbra::Point Desired = Menu->Measure({MenuWidth, 1000.0f});
            const Penumbra::Rect  Anchor  = MenuButton->GetArrangedRect();
            const Penumbra::Rect  Placement{Anchor.X, Anchor.Y + Anchor.H + 4.0f, MenuWidth,
                                            Desired.Y};

            HostPtr->ShowOverlay(std::move(Menu), Placement);
        };

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
                LastKnownDpiScaleFactor = CurrentDpiScaleFactor;
            }

            // Live values flow demo state → widget text every frame.
            CountLabel->Text = "Count: " + std::to_string(Counter);
            StatusLabel->Text =
                "live: value=" + FormatFloat(Drag->Value) + "  name=\"" + Field->Text + "\"";

            // Settings on the left, the scene viewport on the right, sized by the
            // SplitPanel; each pane's own Style.Margin (set above) is what produces the
            // floating-card gap now that SplitPanel::Arrange honours it.
            const Penumbra::Point WindowSize = Window.GetLogicalWindowSize();
            const Penumbra::Rect  FullRect{0.0f, 0.0f, WindowSize.X, WindowSize.Y};

            Host->Measure({FullRect.W, FullRect.H});
            Host->Arrange(FullRect);

            if (Input.MouseButtonPressedThisFrame[0]) {
                Focus.Focused = nullptr;
            }
            Host->UpdateInteractionState(Input);

            const bool WantTextInput = (Focus.Focused != nullptr);
            if (WantTextInput != TextInputActive) {
                Window.SetTextInputActive(WantTextInput);
                TextInputActive = WantTextInput;
            }

            Renderer.BeginFrame(Theme.ColorBackgroundPrimary);
            Host->Draw(Renderer);
            Renderer.EndFrameAndPresent();
        }
    }

    Window.Shutdown();
    return 0;
}
