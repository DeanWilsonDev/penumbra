#pragma once

#include "Penumbra/Platform/IClipboard.h"
#include "Penumbra/Platform/InputState.h"

#include <SDL3/SDL.h>

#include <string>

namespace Penumbra::Platform {

// The single primary window and the only code in the project that talks to SDL
// for window / event / DPI concerns. Multi-window is a non-goal. Also serves as
// the OS clipboard provider.
class PlatformWindow : public IClipboard {
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

    void        SetClipboardText(const std::string& Text) override;
    std::string GetClipboardText() const override;

private:
    SDL_Window*   Window{nullptr};
    SDL_Renderer* SdlRenderer{nullptr};
    float         DpiScaleFactor{1.0f};
    Uint64        LastFrameTimeNs{0}; // for the per-frame delta time

    // Previous-frame button state, used to derive pressed / released edges.
    bool PreviousMouseButtonDown[3]{};
};

} // namespace Penumbra::Platform
