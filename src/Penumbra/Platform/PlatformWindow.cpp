#include "Penumbra/Platform/PlatformWindow.h"

namespace Penumbra::Platform {

namespace {
// SDL mouse buttons are 1-based (left=1, middle=2, right=3); InputState indexes 0-based.
constexpr Uint32 ButtonMask[3] = {SDL_BUTTON_LMASK, SDL_BUTTON_MMASK, SDL_BUTTON_RMASK};

// Maps SDL's keycode space onto Penumbra's small, deliberately incomplete Key
// vocabulary (spec item 5) — only the keys Penumbra's own widgets and documented use
// cases care about. Anything else comes through as Key::Unknown.
Key MapKey(SDL_Keycode Code) {
    switch (Code) {
    case SDLK_LEFT:      return Key::Left;
    case SDLK_RIGHT:     return Key::Right;
    case SDLK_UP:        return Key::Up;
    case SDLK_DOWN:      return Key::Down;
    case SDLK_HOME:      return Key::Home;
    case SDLK_END:       return Key::End;
    case SDLK_BACKSPACE: return Key::Backspace;
    case SDLK_DELETE:    return Key::Delete;
    case SDLK_ESCAPE:    return Key::Escape;
    case SDLK_RETURN:    return Key::Enter;
    case SDLK_TAB:       return Key::Tab;
    case SDLK_A:         return Key::A;
    case SDLK_C:         return Key::C;
    case SDLK_V:         return Key::V;
    case SDLK_X:         return Key::X;
    default:             return Key::Unknown;
    }
}

Modifiers MapModifiers(SDL_Keymod Mod) {
    return {(Mod & SDL_KMOD_SHIFT) != 0, (Mod & SDL_KMOD_CTRL) != 0,
            (Mod & SDL_KMOD_ALT) != 0, (Mod & SDL_KMOD_GUI) != 0};
}

// The scale actually baked into Window's real drawable, derived from its
// pixel size vs its logical size, rather than trusted from
// SDL_GetWindowDisplayScale alone (docs/penumbra_x11_driver_dpi_requirements.md,
// in the `pharos-proto` repo). SDL_WINDOW_HIGH_PIXEL_DENSITY only actually
// grows the drawable under drivers that support per-window content scale
// (Wayland); under X11 the drawable stays at exactly the requested logical
// size regardless of the flag, while SDL_GetWindowDisplayScale can still
// report the desktop's real (XWayland-inherited) compositor scale -- a
// mismatch that previously left DpiScaleFactor overstated, causing
// Renderer to draw as if the canvas were bigger than the real drawable
// backing it. Comparing SDL_GetWindowSizeInPixels against the window's own
// current logical size sidesteps the query and reflects what actually got
// built, whichever driver built it. Returns 0.0f (not a valid scale) if
// Window is null or its logical width is degenerate, so callers can fall
// back to the display-scale query the same way a failed query already did.
float ObservedDrawableScale(SDL_Window* Window) {
    if (!Window) {
        return 0.0f;
    }
    int LogicalWidth = 0, LogicalHeight = 0;
    SDL_GetWindowSize(Window, &LogicalWidth, &LogicalHeight);
    if (LogicalWidth <= 0) {
        return 0.0f;
    }
    int PixelWidth = 0, PixelHeight = 0;
    SDL_GetWindowSizeInPixels(Window, &PixelWidth, &PixelHeight);
    return static_cast<float>(PixelWidth) / static_cast<float>(LogicalWidth);
}
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

    // Sync presentation to the display refresh. Without this the loop renders
    // thousands of uncapped frames per second, pegging the GPU — which shows up as
    // coil whine and needless power draw. One frame per refresh is plenty for a UI.
    SDL_SetRenderVSync(SdlRenderer, 1);

    const float Observed = ObservedDrawableScale(Window);
    DpiScaleFactor = (Observed > 0.0f) ? Observed : SDL_GetWindowDisplayScale(Window);
    if (DpiScaleFactor <= 0.0f) {
        DpiScaleFactor = 1.0f;
    }

    LastFrameTimeNs = SDL_GetTicksNS();
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
    const Uint64 Now = SDL_GetTicksNS();
    float Delta = static_cast<float>(static_cast<double>(Now - LastFrameTimeNs) / 1.0e9);
    LastFrameTimeNs = Now;
    if (Delta > 0.1f) {
        Delta = 0.1f; // clamp pauses (e.g. a stalled frame) so animations don't jump
    }
    OutInputState.DeltaTimeSeconds = Delta;

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
            OutInputState.KeysPressedThisFrame.push_back(MapKey(Event.key.key));
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

    OutInputState.ModifierState = MapModifiers(SDL_GetModState());

    return KeepRunning;
}

Point PlatformWindow::GetLogicalWindowSize() const {
    int Width = 0;
    int Height = 0;
    if (Window) {
        SDL_GetWindowSize(Window, &Width, &Height);
    }
    return {static_cast<float>(Width), static_cast<float>(Height)};
}

float PlatformWindow::GetDpiScaleFactor() const {
    const float Observed = ObservedDrawableScale(Window);
    if (Observed > 0.0f) {
        return Observed;
    }
    const float Live = SDL_GetWindowDisplayScale(Window);
    return (Live > 0.0f) ? Live : DpiScaleFactor; // fall back to the cached value if both queries fail
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

void PlatformWindow::SetTitle(const std::string& Title) {
    SDL_SetWindowTitle(Window, Title.c_str());
}

std::string PlatformWindow::GetLastError() const {
    return SDL_GetError();
}

void PlatformWindow::SetClipboardText(const std::string& Text) {
    SDL_SetClipboardText(Text.c_str());
}

std::string PlatformWindow::GetClipboardText() const {
    if (!SDL_HasClipboardText()) {
        return {};
    }
    char* Raw = SDL_GetClipboardText(); // SDL allocates; we own it
    std::string Result = (Raw != nullptr) ? Raw : "";
    SDL_free(Raw);
    return Result;
}

} // namespace Penumbra::Platform
