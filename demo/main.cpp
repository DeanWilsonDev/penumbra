#include "DemoTheme.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Render/SdlTtfFontBackend.h"

#include <cstdio>
#include <string>

namespace {

// Logical window size for the demo. A demo concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 960;
constexpr int WindowLogicalHeight = 640;

constexpr const char* FontFileName = "JetBrainsMonoNerdFontMono-Regular.ttf";

} // namespace

int main() {
    Demo::Theme Theme;

    Penumbra::Platform::PlatformWindow Window;
    if (!Window.Initialise("Penumbra Demo", WindowLogicalWidth, WindowLogicalHeight)) {
        std::fprintf(stderr, "Failed to initialise platform window: %s\n", SDL_GetError());
        return 1;
    }

    std::printf("DPI scale factor: %.3f\n", Window.GetDpiScaleFactor());
    std::fflush(stdout);

    // The font backend and renderer live in an inner scope so the backend's
    // texture cache is torn down while the SDL_Renderer is still alive.
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

        Penumbra::Platform::InputState Input;
        bool KeepRunning = true;
        while (KeepRunning) {
            KeepRunning = Window.PumpEventsAndBuildInput(Input);

            Renderer.BeginFrame(Theme.ColorBackgroundPrimary);

            // A filled rect.
            Renderer.DrawFilledRect({Theme.SpacingLarge, Theme.SpacingLarge, 240.0f, 120.0f},
                                    Theme.ColorSurfaceRaised);

            // An outline.
            Renderer.DrawRectOutline({280.0f, Theme.SpacingLarge, 240.0f, 120.0f},
                                     Theme.ColorBorderDefault, Theme.BorderWidthDefault);

            // A line of crisp text.
            Renderer.DrawText(BodyFont, "Penumbra - Milestone 1: crisp text at DPI",
                              {Theme.SpacingLarge, 168.0f}, Theme.ColorTextPrimary);

            // Clip test: the accent rect and its label are far larger than the clip
            // region, so only the clipped window of each should be visible.
            const SDL_FRect ClipRegion{Theme.SpacingLarge, 220.0f, 240.0f, 80.0f};
            Renderer.PushClipRect(ClipRegion);
            Renderer.DrawFilledRect({Theme.SpacingLarge, 220.0f, 640.0f, 400.0f}, Theme.ColorAccent);
            Renderer.DrawText(BodyFont, "this text is clipped to the accent box on the left",
                              {Theme.SpacingLarge + Theme.SpacingMedium, 252.0f},
                              Theme.ColorTextPrimary);
            Renderer.PopClipRect();

            // Drawn after the pop, outside the clip, to prove the clip was released.
            Renderer.DrawText(BodyFont, "(this line is outside the clip)",
                              {Theme.SpacingLarge, 320.0f}, Theme.ColorTextPrimary);

            Renderer.EndFrameAndPresent();
        }
    }

    Window.Shutdown();
    return 0;
}
