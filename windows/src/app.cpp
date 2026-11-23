#include "app.h"

#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <shellapi.h>  // NIN_SELECT, NIN_KEYSELECT

#include "install.h"
#include "log.h"
#include "profile.h"
#include "strings.h"

namespace {

// The app's own window messages, in one place so they can't collide.
constexpr UINT kTrayMessage = WM_APP + 1;          // clicks on the tray icon
constexpr UINT kWorkerResultMessage = WM_APP + 2;  // wParam: 1 = sidetone off, 0 = every attempt failed

// What the worker applies on each connection. This version: sidetone off, nothing else.
const Profile kProfile{.sidetone = std::uint8_t{0}};

}  // namespace

std::unique_ptr<App> App::Create(HINSTANCE instance, bool showNotice) {
    const WNDCLASSEXW windowClass{
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = WindowProc,
        .hInstance = instance,
        .lpszClassName = ids::kWindowClass,
    };
    if (!RegisterClassExW(&windowClass)) {
        Log(L"can't register the window class (error %lu)", GetLastError());
        return nullptr;
    }

    std::unique_ptr<App> app(new App());
    // A hidden top-level window, never shown. Not HWND_MESSAGE: message-only windows don't get broadcasts,
    // and the tray needs the TaskbarCreated broadcast to come back after Explorer restarts. The last
    // argument hands `app` to WindowProc (WM_NCCREATE), which links the window to it.
    if (!CreateWindowExW(0, ids::kWindowClass, ui::kAppName, WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, instance,
                         app.get())) {
        Log(L"can't create the window (error %lu)", GetLastError());
        return nullptr;
    }
    app->taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");

    app->tray_ = std::make_unique<Tray>(app->window_, kTrayMessage);
    app->logWindow_ = std::make_unique<LogWindow>(instance);
    app->worker_ = Worker::Start(app->window_, kWorkerResultMessage, kProfile);
    app->devices_ = DeviceWatcher::Create(app->window_);
    if (!app->worker_ || !app->devices_) return nullptr;  // logged by them; ~App cleans up

    // "SidetoneOff is running": on every start you make, not at sign-in (decision M14).
    if (showNotice) {
        if (app->tray_->ShowNotice(ui::kNoticeTitle, ui::kNoticeText)) Log(L"notice shown");
        else Log(L"can't show the notice (error %lu)", GetLastError());
    }

    if (app->devices_->IsConnected()) app->TurnOffSidetone();
    return app;
}

App::~App() {
    if (window_) DestroyWindow(window_);  // only if Create failed half-way; normally it is gone already
}

int App::Run() {
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {  // the first message: store the App passed to CreateWindowExW
        auto* app = static_cast<App*>(reinterpret_cast<const CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return app ? app->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DEVICECHANGE:
        OnDeviceChange(wParam, lParam);
        return TRUE;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC && devices_ && devices_->IsConnected()) {
            Log(L"resumed from sleep");
            TurnOffSidetone();
        }
        return TRUE;
    case kWorkerResultMessage:
        OnWorkerResult(wParam != 0);
        return 0;
    case kTrayMessage:
        // NOTIFYICON_VERSION_4: the event is in the low word of lParam, the anchor point in wParam.
        OnTrayEvent(LOWORD(lParam), POINT{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)});
        return 0;
    case WM_SETTINGCHANGE:  // e.g. "ImmersiveColorSet": light or dark mode was switched
    case WM_DPICHANGED:     // display scale changed
    case WM_DISPLAYCHANGE:  // resolution or monitors changed
        // The tray redraws its icon only if the taskbar's theme or icon size really changed.
        if (tray_) tray_->UpdateIcon();
        return 0;
    case WM_CLOSE:  // sent by taskkill (without /f), and by install and uninstall to the running instance
        if ((tray_ && tray_->IsMenuOpen()) || inDialog_) {
            // The menu (inside Tray::PopUpMenu) or a dialog is running further up this call stack. Destroying
            // the window now would delete the Tray under it, so close them and come back once they've returned.
            if (tray_) tray_->CloseMenu();
            if (dialog_) PostMessageW(dialog_, WM_CLOSE, 0, 0);  // the same as Cancel
            PostMessageW(window_, WM_CLOSE, 0, 0);
            return 0;
        }
        DestroyWindow(window_);
        return 0;
    case WM_DESTROY:
        // Tear down while the window still exists: close the log window, stop listening, stop the worker, remove
        // the icon.
        logWindow_.reset();
        devices_.reset();
        worker_.reset();
        tray_.reset();
        SetWindowLongPtrW(window_, GWLP_USERDATA, 0);  // no more messages for this App
        window_ = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        if (message == taskbarCreatedMessage_ && message != 0 && tray_) {
            Log(L"taskbar created, adding the tray icon again");
            tray_->Add();
            return 0;
        }
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void App::OnDeviceChange(WPARAM event, LPARAM data) {
    if (!devices_) return;
    switch (devices_->OnDeviceChange(event, data)) {
    case DeviceChange::Connected:
        Log(L"G635 connected");
        TurnOffSidetone();
        break;
    case DeviceChange::Disconnected:
        Log(L"G635 disconnected");
        SetStatus(HeadsetStatus::Disconnected);
        break;
    case DeviceChange::HidArrived:
        // The headset's HID collections are up: try now instead of waiting. After a failed run (a first
        // plug-in can take longer than the 5 attempts while Windows installs the driver), start over.
        if (!devices_->IsConnected()) break;
        if (status_ == HeadsetStatus::Failed) SetStatus(HeadsetStatus::Applying);
        worker_->RequestApply();
        break;
    case DeviceChange::None:
        break;
    }
}

void App::OnWorkerResult(bool off) {
    if (!devices_ || !devices_->IsConnected()) return;  // unplugged in the meantime (same guard as the Mac app)
    SetStatus(off ? HeadsetStatus::SidetoneOff : HeadsetStatus::Failed);
}

void App::OnTrayEvent(UINT event, POINT anchor) {
    // Same menu for a left click (NIN_SELECT), a right click (WM_CONTEXTMENU) and the keyboard (NIN_KEYSELECT).
    if (event != NIN_SELECT && event != WM_CONTEXTMENU && event != NIN_KEYSELECT) return;
    if (inDialog_) {  // a dialog is open: show it again rather than a second menu
        if (dialog_) SetForegroundWindow(dialog_);
        return;
    }
    switch (tray_->PopUpMenu(anchor)) {
    case MenuCommand::SetAgain:
        Log(L"set sidetone off again");
        TurnOffSidetone();
        break;
    case MenuCommand::ShowLog:
        logWindow_->Show();
        break;
    case MenuCommand::Uninstall:
        Uninstall();
        break;
    case MenuCommand::Quit:
        Log(L"quit");
        DestroyWindow(window_);
        break;
    case MenuCommand::Status:  // can't happen: the status line is disabled
    case MenuCommand::None:
        break;
    }
}

void App::TurnOffSidetone() {
    SetStatus(HeadsetStatus::Applying);
    worker_->RequestApply();
}

void App::Uninstall() {
    inDialog_ = true;
    const bool confirmed = ConfirmUninstall(&dialog_);
    inDialog_ = false;
    if (!confirmed) {
        Log(L"uninstall cancelled");
        return;
    }

    const auto unregistered = Unregister();
    if (!unregistered) {
        Log(L"uninstall failed: %s", DescribeError(unregistered.error()).c_str());
        inDialog_ = true;
        ShowError(ui::kUninstallFailed, unregistered.error(), &dialog_);
        inDialog_ = false;
        return;
    }
    uninstalled_ = true;  // the files go once the app has stopped (wWinMain)
    DestroyWindow(window_);
}

void App::SetStatus(HeadsetStatus status) {
    if (status != status_) Log(L"status: %s", StatusText(status));
    status_ = status;
    tray_->Show(status);
}
