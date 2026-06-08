#pragma once

#include "Penumbra/Platform/InputState.h"

#include <SDL3/SDL.h>

namespace Penumbra::Platform {

// The single primary window and the only code in the project that talks to SDL
// for window / event / DPI concerns. Multi-window is a non-goal.
class PlatformWindow {
public:
    bool Initialise(const char* Title, int LogicalWidth, int LogicalHeight);
    void Shutdown();

    // Polls all pending SDL events and fills OutInputState for this frame.
    // Returns false when the OS has asked the application to quit.
    bool PumpEventsAndBuildInput(InputState& OutInputState);

    SDL_FPoint GetLogicalWindowSize() const;
    float      GetDpiScaleFactor() const;

    void          SetTextInputActive(bool Active);
    SDL_Renderer* GetSdlRenderer() const; // handed to the Render layer only

private:
    SDL_Window*   Window{nullptr};
    SDL_Renderer* SdlRenderer{nullptr};
    float         DpiScaleFactor{1.0f};

    // Previous-frame button state, used to derive pressed / released edges.
    bool PreviousMouseButtonDown[3]{};
};

} // namespace Penumbra::Platform
