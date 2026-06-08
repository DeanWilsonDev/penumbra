#include "Penumbra/Platform/PlatformWindow.h"

namespace Penumbra::Platform {

namespace {
// SDL mouse buttons are 1-based (left=1, middle=2, right=3); InputState indexes 0-based.
constexpr Uint32 ButtonMask[3] = {SDL_BUTTON_LMASK, SDL_BUTTON_MMASK, SDL_BUTTON_RMASK};
} // namespace

bool PlatformWindow::Initialise(const char* Title, int LogicalWidth, int LogicalHeight) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return false;
    }

    const SDL_WindowFlags Flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!SDL_CreateWindowAndRenderer(Title, LogicalWidth, LogicalHeight, Flags,
                                     &Window, &SdlRenderer)) {
        SDL_Quit();
        return false;
    }

    DpiScaleFactor = SDL_GetWindowDisplayScale(Window);
    if (DpiScaleFactor <= 0.0f) {
        DpiScaleFactor = 1.0f;
    }

    return true;
}

void PlatformWindow::Shutdown() {
    if (SdlRenderer) {
        SDL_DestroyRenderer(SdlRenderer);
        SdlRenderer = nullptr;
    }
    if (Window) {
        SDL_DestroyWindow(Window);
        Window = nullptr;
    }
    SDL_Quit();
}

bool PlatformWindow::PumpEventsAndBuildInput(InputState& OutInputState) {
    OutInputState.TextInputThisFrame.clear();
    OutInputState.KeysPressedThisFrame.clear();
    OutInputState.MouseWheelDelta = 0.0f;

    bool KeepRunning = true;

    SDL_Event Event;
    while (SDL_PollEvent(&Event)) {
        switch (Event.type) {
        case SDL_EVENT_QUIT:
            KeepRunning = false;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            OutInputState.MouseWheelDelta += Event.wheel.y;
            break;
        case SDL_EVENT_TEXT_INPUT:
            OutInputState.TextInputThisFrame += Event.text.text;
            break;
        case SDL_EVENT_KEY_DOWN:
            OutInputState.KeysPressedThisFrame.push_back(Event.key.key);
            break;
        default:
            break;
        }
    }

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    const SDL_MouseButtonFlags Buttons = SDL_GetMouseState(&MouseX, &MouseY);
    OutInputState.MousePosition = {MouseX, MouseY};

    for (int Index = 0; Index < 3; ++Index) {
        const bool Down = (Buttons & ButtonMask[Index]) != 0;
        const bool WasDown = PreviousMouseButtonDown[Index];

        OutInputState.MouseButtonDown[Index] = Down;
        OutInputState.MouseButtonPressedThisFrame[Index] = Down && !WasDown;
        OutInputState.MouseButtonReleasedThisFrame[Index] = !Down && WasDown;

        PreviousMouseButtonDown[Index] = Down;
    }

    OutInputState.ModifierState = SDL_GetModState();

    return KeepRunning;
}

SDL_FPoint PlatformWindow::GetLogicalWindowSize() const {
    int Width = 0;
    int Height = 0;
    if (Window) {
        SDL_GetWindowSize(Window, &Width, &Height);
    }
    return {static_cast<float>(Width), static_cast<float>(Height)};
}

float PlatformWindow::GetDpiScaleFactor() const {
    return DpiScaleFactor;
}

void PlatformWindow::SetTextInputActive(bool Active) {
    if (!Window) {
        return;
    }
    if (Active) {
        SDL_StartTextInput(Window);
    } else {
        SDL_StopTextInput(Window);
    }
}

SDL_Renderer* PlatformWindow::GetSdlRenderer() const {
    return SdlRenderer;
}

} // namespace Penumbra::Platform
