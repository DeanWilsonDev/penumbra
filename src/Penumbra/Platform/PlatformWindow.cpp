#include "Penumbra/Platform/PlatformWindow.h"

#include <algorithm>

namespace Penumbra::Platform {

namespace {
// SDL mouse buttons are 1-based (left=1, middle=2, right=3); InputState indexes 0-based.
constexpr Uint32 ButtonMask[3] = {SDL_BUTTON_LMASK, SDL_BUTTON_MMASK, SDL_BUTTON_RMASK};

// Maps SDL's keycode space onto Penumbra's Key vocabulary. Anything else comes
// through as Key::Unknown.
Key MapKey(SDL_Keycode Code) {
    switch (Code) {
    case SDLK_LEFT: return Key::Left;
    case SDLK_RIGHT: return Key::Right;
    case SDLK_UP: return Key::Up;
    case SDLK_DOWN: return Key::Down;
    case SDLK_HOME: return Key::Home;
    case SDLK_END: return Key::End;
    case SDLK_PAGEUP: return Key::PageUp;
    case SDLK_PAGEDOWN: return Key::PageDown;
    case SDLK_INSERT: return Key::Insert;
    case SDLK_BACKSPACE: return Key::Backspace;
    case SDLK_DELETE: return Key::Delete;
    case SDLK_ESCAPE: return Key::Escape;
    case SDLK_RETURN: return Key::Enter;
    case SDLK_TAB: return Key::Tab;
    case SDLK_SPACE: return Key::Space;
    case SDLK_A: return Key::A;
    case SDLK_B: return Key::B;
    case SDLK_C: return Key::C;
    case SDLK_D: return Key::D;
    case SDLK_E: return Key::E;
    case SDLK_F: return Key::F;
    case SDLK_G: return Key::G;
    case SDLK_H: return Key::H;
    case SDLK_I: return Key::I;
    case SDLK_J: return Key::J;
    case SDLK_K: return Key::K;
    case SDLK_L: return Key::L;
    case SDLK_M: return Key::M;
    case SDLK_N: return Key::N;
    case SDLK_O: return Key::O;
    case SDLK_P: return Key::P;
    case SDLK_Q: return Key::Q;
    case SDLK_R: return Key::R;
    case SDLK_S: return Key::S;
    case SDLK_T: return Key::T;
    case SDLK_U: return Key::U;
    case SDLK_V: return Key::V;
    case SDLK_W: return Key::W;
    case SDLK_X: return Key::X;
    case SDLK_Y: return Key::Y;
    case SDLK_Z: return Key::Z;
    case SDLK_0: return Key::Digit0;
    case SDLK_1: return Key::Digit1;
    case SDLK_2: return Key::Digit2;
    case SDLK_3: return Key::Digit3;
    case SDLK_4: return Key::Digit4;
    case SDLK_5: return Key::Digit5;
    case SDLK_6: return Key::Digit6;
    case SDLK_7: return Key::Digit7;
    case SDLK_8: return Key::Digit8;
    case SDLK_9: return Key::Digit9;
    case SDLK_F1: return Key::F1;
    case SDLK_F2: return Key::F2;
    case SDLK_F3: return Key::F3;
    case SDLK_F4: return Key::F4;
    case SDLK_F5: return Key::F5;
    case SDLK_F6: return Key::F6;
    case SDLK_F7: return Key::F7;
    case SDLK_F8: return Key::F8;
    case SDLK_F9: return Key::F9;
    case SDLK_F10: return Key::F10;
    case SDLK_F11: return Key::F11;
    case SDLK_F12: return Key::F12;
    case SDLK_MINUS: return Key::Minus;
    case SDLK_EQUALS: return Key::Equals;
    case SDLK_LEFTBRACKET: return Key::LeftBracket;
    case SDLK_RIGHTBRACKET: return Key::RightBracket;
    case SDLK_BACKSLASH: return Key::Backslash;
    case SDLK_SEMICOLON: return Key::Semicolon;
    case SDLK_APOSTROPHE: return Key::Apostrophe;
    case SDLK_GRAVE: return Key::Grave;
    case SDLK_COMMA: return Key::Comma;
    case SDLK_PERIOD: return Key::Period;
    case SDLK_SLASH: return Key::Slash;
    case SDLK_LSHIFT: return Key::LeftShift;
    case SDLK_RSHIFT: return Key::RightShift;
    case SDLK_LCTRL: return Key::LeftControl;
    case SDLK_RCTRL: return Key::RightControl;
    case SDLK_LALT: return Key::LeftAlt;
    case SDLK_RALT: return Key::RightAlt;
    case SDLK_LGUI: return Key::LeftSuper;
    case SDLK_RGUI: return Key::RightSuper;
    case SDLK_CAPSLOCK: return Key::CapsLock;
    case SDLK_PRINTSCREEN: return Key::PrintScreen;
    case SDLK_PAUSE: return Key::Pause;
    case SDLK_KP_0: return Key::Numpad0;
    case SDLK_KP_1: return Key::Numpad1;
    case SDLK_KP_2: return Key::Numpad2;
    case SDLK_KP_3: return Key::Numpad3;
    case SDLK_KP_4: return Key::Numpad4;
    case SDLK_KP_5: return Key::Numpad5;
    case SDLK_KP_6: return Key::Numpad6;
    case SDLK_KP_7: return Key::Numpad7;
    case SDLK_KP_8: return Key::Numpad8;
    case SDLK_KP_9: return Key::Numpad9;
    case SDLK_KP_PLUS: return Key::NumpadAdd;
    case SDLK_KP_MINUS: return Key::NumpadSubtract;
    case SDLK_KP_MULTIPLY: return Key::NumpadMultiply;
    case SDLK_KP_DIVIDE: return Key::NumpadDivide;
    case SDLK_KP_PERIOD: return Key::NumpadDecimal;
    case SDLK_KP_ENTER: return Key::NumpadEnter;
    default: return Key::Unknown;
    }
}

Modifiers MapModifiers(SDL_Keymod Mod) {
    Modifiers Result{(Mod & SDL_KMOD_SHIFT) != 0, (Mod & SDL_KMOD_CTRL) != 0,
                     (Mod & SDL_KMOD_ALT) != 0, (Mod & SDL_KMOD_GUI) != 0};
#if defined(__APPLE__)
    Result.Shortcut = Result.Super;
#else
    Result.Shortcut = Result.Ctrl;
#endif
    return Result;
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
    OutInputState.KeyEvents.clear();
    OutInputState.MouseWheelDelta = 0.0f;
    bool PressedByEvent[3]{};
    bool ReleasedByEvent[3]{};
    OutInputState.MouseWheelDeltaX = 0.0f;

    bool KeepRunning = true;

    SDL_Event Event;
    while (SDL_PollEvent(&Event)) {
        switch (Event.type) {
        case SDL_EVENT_QUIT:
            KeepRunning = false;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            OutInputState.MouseWheelDelta += Event.wheel.y;
            OutInputState.MouseWheelDeltaX += Event.wheel.x;
            break;
        case SDL_EVENT_TEXT_INPUT:
            OutInputState.TextInputThisFrame += Event.text.text;
            break;
        case SDL_EVENT_KEY_DOWN: {
            const Key Code = MapKey(Event.key.key);
            OutInputState.KeysPressedThisFrame.push_back(Code);
            OutInputState.KeyEvents.push_back({Code, true, Event.key.repeat, MapModifiers(Event.key.mod)});
            if (!Event.key.repeat && Code != Key::Unknown && !OutInputState.IsKeyHeld(Code)) {
                OutInputState.KeysHeld.push_back(Code);
            }
            break;
        }
        case SDL_EVENT_KEY_UP: {
            const Key Code = MapKey(Event.key.key);
            OutInputState.KeyEvents.push_back({Code, false, false, MapModifiers(Event.key.mod)});
            std::erase(OutInputState.KeysHeld, Code);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            // Folded into the per-frame edges below, so a press and release inside one
            // frame still count as a click instead of vanishing between two samples.
            if (Event.button.button >= 1 && Event.button.button <= 3) {
                (Event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PressedByEvent : ReleasedByEvent)[Event.button.button - 1] = true;
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            OutInputState.KeysHeld.clear();
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
        OutInputState.MouseButtonPressedThisFrame[Index] = (Down && !WasDown) || PressedByEvent[Index];
        OutInputState.MouseButtonReleasedThisFrame[Index] = (!Down && WasDown) || (ReleasedByEvent[Index] && !Down);

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
