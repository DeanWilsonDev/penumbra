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
#include "Penumbra/Widgets/ScrollablePanel.h"
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
        std::fprintf(stderr, "Failed to initialise platform window: %s\n", SDL_GetError());
        return 1;
    }

    std::printf("DPI scale factor: %.3f\n", Window.GetDpiScaleFactor());
    std::printf("Click Increment, toggle the checkbox, drag the value, type a name, "
                "scroll, and resize.\n");
    std::fflush(stdout);

    {
        Penumbra::Render::SdlTtfFontBackend FontBackend;
        const std::string FontPath = std::string(DEMO_ASSET_DIR) + "/" + FontFileName;
        const Penumbra::Render::FontHandle BodyFont =
            FontBackend.LoadFont(FontPath.c_str(), Theme.FontSizeBody, Window.GetDpiScaleFactor());

        Penumbra::Render::Renderer Renderer;
        if (!Renderer.Initialise(Window.GetSdlRenderer(), Window.GetDpiScaleFactor(), &FontBackend)) {
            std::fprintf(stderr, "Failed to initialise renderer\n");
            Window.Shutdown();
            return 1;
        }

        FocusState Focus;

        // ---- composition helpers (a stand-in for what UmbraComponentLibrary owns) ----
        auto MakeLabel = [&](const std::string& Text, SDL_Color Color) {
            auto Widget = std::make_unique<Label>();
            Widget->FontBackend = &FontBackend;
            Widget->Font        = BodyFont;
            Widget->Text        = Text;
            Widget->ColorText   = Color;
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
        ScenePane->SceneClearColor = Theme.ColorBackgroundPrimary; // odd cells show this
        ScenePane->OnRenderScene =
            [&Theme](Penumbra::Render::Renderer& SceneRenderer, SDL_FPoint SceneSize) {
                constexpr int Cells = 8;
                const float CellW = SceneSize.x / static_cast<float>(Cells);
                const float CellH = SceneSize.y / static_cast<float>(Cells);
                for (int Row = 0; Row < Cells; ++Row) {
                    for (int Col = 0; Col < Cells; ++Col) {
                        if ((Row + Col) % 2 == 0) {
                            SceneRenderer.DrawFilledRect(
                                {Col * CellW, Row * CellH, CellW, CellH}, Theme.ColorAccent);
                        }
                    }
                }
            };

        bool TextInputActive = false;

        Penumbra::Platform::InputState Input;
        bool KeepRunning = true;
        while (KeepRunning) {
            KeepRunning = Window.PumpEventsAndBuildInput(Input);

            // Live values flow demo state → widget text every frame.
            CountLabel->Text = "Count: " + std::to_string(Counter);
            StatusLabel->Text =
                "live: value=" + FormatFloat(Drag->Value) + "  name=\"" + Field->Text + "\"";

            // Two panes: settings on the left, the scene viewport on the right.
            const SDL_FPoint WindowSize = Window.GetLogicalWindowSize();
            const float Margin = Theme.SpacingLarge;
            const float Gap    = Theme.SpacingLarge;
            const float FullW  = WindowSize.x - 2.0f * Margin;
            const float FullH  = WindowSize.y - 2.0f * Margin;
            const float LeftW  = (FullW - Gap) * 0.48f;
            const SDL_FRect LeftRect{Margin, Margin, LeftW, FullH};
            const SDL_FRect RightRect{Margin + LeftW + Gap, Margin, FullW - LeftW - Gap, FullH};

            Root->Measure({LeftRect.w, LeftRect.h});
            Root->Arrange(LeftRect);
            ScenePane->Measure({RightRect.w, RightRect.h});
            ScenePane->Arrange(RightRect);

            if (Input.MouseButtonPressedThisFrame[0]) {
                Focus.Focused = nullptr;
            }
            if (!Root->UpdateInteractionState(Input)) {
                ScenePane->UpdateInteractionState(Input);
            }

            const bool WantTextInput = (Focus.Focused != nullptr);
            if (WantTextInput != TextInputActive) {
                Window.SetTextInputActive(WantTextInput);
                TextInputActive = WantTextInput;
            }

            Renderer.BeginFrame(Theme.ColorBackgroundPrimary);
            Root->Draw(Renderer);
            ScenePane->Draw(Renderer);
            Renderer.EndFrameAndPresent();
        }
    }

    Window.Shutdown();
    return 0;
}
