#include "DemoResolvers.h"
#include "DemoTheme.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Render/SdlTtfFontBackend.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/Checkbox.h"
#include "Penumbra/Widgets/FocusState.h"
#include "Penumbra/Widgets/Label.h"
#include "Penumbra/Widgets/NumericDrag.h"
#include "Penumbra/Widgets/TextInput.h"

#include <cstdio>
#include <memory>
#include <string>

namespace {

// Logical window size for the demo. A demo concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 960;
constexpr int WindowLogicalHeight = 640;

constexpr const char* FontFileName = "JetBrainsMonoNerdFontMono-Regular.ttf";

using namespace Penumbra::Widgets;

} // namespace

int main() {
    Demo::Theme Theme;

    Penumbra::Platform::PlatformWindow Window;
    if (!Window.Initialise("Penumbra Demo", WindowLogicalWidth, WindowLogicalHeight)) {
        std::fprintf(stderr, "Failed to initialise platform window: %s\n", SDL_GetError());
        return 1;
    }

    std::printf("DPI scale factor: %.3f\n", Window.GetDpiScaleFactor());
    std::printf("Toggle the checkbox, drag the number, click the field and type.\n");
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

        auto MakeLabel = [&](const std::string& Text) {
            auto Widget = std::make_unique<Label>();
            Widget->FontBackend = &FontBackend;
            Widget->Font        = BodyFont;
            Widget->Text        = Text;
            Widget->ColorText   = Theme.ColorTextPrimary;
            return Widget;
        };
        auto MakeRow = [&]() {
            auto Row = std::make_unique<Box>();
            Row->Layout         = LayoutMode::HorizontalStack;
            Row->ChildGap       = Theme.SpacingMedium;
            Row->CrossAlignment = CrossAlign::Center;
            return Row;
        };

        auto Root = std::make_unique<Box>();
        Root->Style    = Demo::ResolvePanelStyle(Theme);
        Root->Layout   = LayoutMode::VerticalStack;
        Root->ChildGap = Theme.SpacingMedium;

        Root->AddChild(MakeLabel("Milestone 5 widgets"));

        // Checkbox + label.
        {
            auto Row = MakeRow();
            auto Check = std::make_unique<Checkbox>();
            Check->ApplyStyle(Demo::ResolveCheckboxStyle(Theme));
            Check->GlyphSizeLogical = Theme.CheckboxGlyphSize;
            Check->OnChanged = [](bool On) {
                std::printf("Checkbox: %s\n", On ? "checked" : "unchecked");
                std::fflush(stdout);
            };
            Row->AddChild(std::move(Check));
            Row->AddChild(MakeLabel("Enable feature"));
            Root->AddChild(std::move(Row));
        }

        // Label + numeric drag.
        {
            auto Row = MakeRow();
            Row->AddChild(MakeLabel("Value:"));
            auto Drag = std::make_unique<NumericDrag>();
            Drag->Style                 = Demo::ResolveInputFieldStyle(Theme);
            Drag->FontBackend           = &FontBackend;
            Drag->Font                  = BodyFont;
            Drag->ColorText             = Theme.ColorTextPrimary;
            Drag->Value                 = 1.0f;
            Drag->Sensitivity           = Theme.DragSensitivity;
            Drag->PreferredWidthLogical = Theme.FieldWidthSmall;
            Drag->OnValueChanged = [](float Value) {
                std::printf("Value = %.2f\n", Value);
                std::fflush(stdout);
            };
            Row->AddChild(std::move(Drag));
            Root->AddChild(std::move(Row));
        }

        // Label + text input.
        {
            auto Row = MakeRow();
            Row->AddChild(MakeLabel("Name:"));
            auto Field = std::make_unique<TextInput>();
            Field->Style                 = Demo::ResolveInputFieldStyle(Theme);
            Field->FontBackend           = &FontBackend;
            Field->Font                  = BodyFont;
            Field->ColorText             = Theme.ColorTextPrimary;
            Field->ColorCaret            = Theme.ColorTextPrimary;
            Field->CaretWidthLogical     = Theme.BorderWidthDefault;
            Field->PreferredWidthLogical = Theme.FieldWidthSmall;
            Field->Focus                 = &Focus;
            Field->OnTextChanged = [](const std::string& Text) {
                std::printf("Text = \"%s\"\n", Text.c_str());
                std::fflush(stdout);
            };
            Row->AddChild(std::move(Field));
            Root->AddChild(std::move(Row));
        }

        bool TextInputActive = false;

        Penumbra::Platform::InputState Input;
        bool KeepRunning = true;
        while (KeepRunning) {
            KeepRunning = Window.PumpEventsAndBuildInput(Input);

            const SDL_FPoint Available = Window.GetLogicalWindowSize();
            const SDL_FPoint Desired = Root->Measure(Available);
            Root->Arrange({Theme.SpacingLarge, Theme.SpacingLarge, Desired.x, Desired.y});

            // A press clears focus first; if it lands on a TextInput that field
            // re-claims it during the update, otherwise focus drops (click-away).
            if (Input.MouseButtonPressedThisFrame[0]) {
                Focus.Focused = nullptr;
            }
            Root->UpdateInteractionState(Input);

            // Toggle the platform's text-input mode when focus appears/disappears.
            const bool WantTextInput = (Focus.Focused != nullptr);
            if (WantTextInput != TextInputActive) {
                Window.SetTextInputActive(WantTextInput);
                TextInputActive = WantTextInput;
            }

            Renderer.BeginFrame(Theme.ColorBackgroundPrimary);
            Root->Draw(Renderer);
            Renderer.EndFrameAndPresent();
        }
    }

    Window.Shutdown();
    return 0;
}
