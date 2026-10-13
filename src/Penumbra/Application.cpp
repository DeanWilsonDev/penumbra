#include "Penumbra/Application.h"
#include "Penumbra/Widgets/LayoutPass.h"
#include "Penumbra/Widgets/Length.h"

#include <algorithm>
#include <utility>

namespace Penumbra {

Application::Application() = default;
Application::~Application() = default;

bool Application::Initialize() {
    Configure(Config);

    if (!Window.Initialise(Config.Title.c_str(), Config.WindowLogicalWidth, Config.WindowLogicalHeight)) {
        return false;
    }

    LastKnownDpiScaleFactor = Window.GetDpiScaleFactor();
    if (!Renderer.Initialise(Window.GetSdlRenderer(), LastKnownDpiScaleFactor, &FontBackend)) {
        Window.Shutdown();
        return false;
    }

    if (!OnStart()) {
        OnShutdown();
        Window.Shutdown();
        return false;
    }

    return true;
}

bool Application::RunOneFrame() {
    if (QuitRequested) {
        return false;
    }
    if (!Window.PumpEventsAndBuildInput(Input)) {
        return false;
    }
    if (InputFilterFn) {
        InputFilterFn(Input);
    }

    const float CurrentDpiScaleFactor = Window.GetDpiScaleFactor();
    Renderer.SetDpiScaleFactor(CurrentDpiScaleFactor);
    if (CurrentDpiScaleFactor != LastKnownDpiScaleFactor) {
        LastKnownDpiScaleFactor = CurrentDpiScaleFactor;
        OnDpiScaleChanged(CurrentDpiScaleFactor);
    }

    Tick(Input.DeltaTimeSeconds);
    if (HasUpdateHook()) {
        OnUpdateHookFn(Input.DeltaTimeSeconds, Input);
    } else {
        OnUpdate(Input.DeltaTimeSeconds);
    }

    if (RootWidget) {
        const Point WindowSize = Window.GetLogicalWindowSize();
        const Rect  WindowRect{0.0f, 0.0f, WindowSize.X, WindowSize.Y};
        Widgets::SetLayoutViewportLogical(WindowSize);
        {
            const Widgets::LayoutPass Pass;
            RootWidget->Measure(WindowSize);
            RootWidget->Arrange(WindowRect);
        }
        RootWidgetConsumedInputThisFrame = RootWidget->UpdateInteractionState(Input);
    } else {
        RootWidgetConsumedInputThisFrame = false;
    }

    Renderer.BeginFrame(Config.ClearColor, Input.DeltaTimeSeconds);
    if (RootWidget) {
        RootWidget->Draw(Renderer);
    }
    if (HasRenderHook()) {
        OnRenderHookFn(Renderer);
    } else {
        OnRender(Renderer);
    }
    Renderer.EndFrameAndPresent();

    return !QuitRequested;
}

void Application::Shutdown() {
    OnShutdown();
    Window.Shutdown();
}

int Application::Run() {
    if (!Initialize()) {
        return 1;
    }
    while (RunOneFrame()) {
    }
    Shutdown();
    return 0;
}

void Application::RequestQuit() {
    QuitRequested = true;
}

void Application::SetOnRenderHook(RenderHook Hook) {
    OnRenderHookFn = std::move(Hook);
}

bool Application::HasRenderHook() const {
    return static_cast<bool>(OnRenderHookFn);
}

void Application::SetOnUpdateHook(UpdateHook Hook) {
    OnUpdateHookFn = std::move(Hook);
}

bool Application::HasUpdateHook() const {
    return static_cast<bool>(OnUpdateHookFn);
}

void Application::SetInputFilter(InputFilter Filter) {
    InputFilterFn = std::move(Filter);
}

void Application::RegisterLifecycle(IWidgetLifecycle* Lifecycle) {
    Lifecycles.RegisterLifecycle(Lifecycle);
}

void Application::UnregisterLifecycle(IWidgetLifecycle* Lifecycle) {
    Lifecycles.UnregisterLifecycle(Lifecycle);
}

LifecycleRegistry& Application::GetLifecycleRegistry() {
    return Lifecycles;
}

void Application::Tick(float DeltaSeconds) {
    Lifecycles.Tick(DeltaSeconds);
}

Platform::PlatformWindow& Application::GetWindow() {
    return Window;
}

Render::Renderer& Application::GetRenderer() {
    return Renderer;
}

Render::IFontBackend& Application::GetFontBackend() {
    return FontBackend;
}

float Application::GetDpiScaleFactor() const {
    return Renderer.GetDpiScaleFactor();
}

void Application::SetTextInputActive(bool Active) {
    Window.SetTextInputActive(Active);
}

Point Application::GetWindowLogicalSize() const {
    return Window.GetLogicalWindowSize();
}

void Application::SetRootWidget(std::unique_ptr<Widgets::WidgetBase> Root) {
    RootWidget = std::move(Root);
}

Widgets::WidgetBase* Application::GetRootWidget() const {
    return RootWidget.get();
}

bool Application::GetRootWidgetConsumedInputThisFrame() const {
    return RootWidgetConsumedInputThisFrame;
}

const Platform::InputState& Application::GetInput() const {
    return Input;
}

const ApplicationConfig& Application::GetConfig() const {
    return Config;
}

} // namespace Penumbra
