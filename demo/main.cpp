#include "DemoTheme.h"

#include "Penumbra/Platform/PlatformWindow.h"
#include "Penumbra/Render/Renderer.h"

#include <cstdio>

namespace {

// Logical window size for the demo. A demo concern, never a Penumbra concern.
constexpr int WindowLogicalWidth  = 960;
constexpr int WindowLogicalHeight = 640;

} // namespace

int main() {
    Demo::Theme Theme;

    Penumbra::Platform::PlatformWindow Window;
    if (!Window.Initialise("Penumbra Demo", WindowLogicalWidth, WindowLogicalHeight)) {
        std::fprintf(stderr, "Failed to initialise platform window: %s\n", SDL_GetError());
        return 1;
    }

    Penumbra::Render::Renderer Renderer;
    if (!Renderer.Initialise(Window.GetSdlRenderer(), Window.GetDpiScaleFactor(), nullptr)) {
        std::fprintf(stderr, "Failed to initialise renderer\n");
        Window.Shutdown();
        return 1;
    }

    // Confirm the Milestone 0 "DPI scale queried" criterion.
    std::printf("DPI scale factor: %.3f\n", Window.GetDpiScaleFactor());
    std::fflush(stdout);

    Penumbra::Platform::InputState Input;
    bool KeepRunning = true;
    while (KeepRunning) {
        KeepRunning = Window.PumpEventsAndBuildInput(Input);

        Renderer.BeginFrame(Theme.ColorBackgroundPrimary);
        // (Milestone 1 onward draws here.)
        Renderer.EndFrameAndPresent();
    }

    Window.Shutdown();
    return 0;
}
