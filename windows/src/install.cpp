#include "install.h"

#include <commctrl.h>  // TaskDialogIndirect
#include <shlobj.h>    // SHGetKnownFolderPath, IShellLinkW
#include <strsafe.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>

#include "../res/resource.h"
#include "log.h"
#include "strings.h"
#include "version.h"
#include "win32.h"

namespace {

// The steps, worded to follow "Couldn't " in an error. Windows-only, so they live here rather than in strings.h.
constexpr wchar_t kStepFindFolder[] = L"find the install folder";
constexpr wchar_t kStepClose[] = L"close the running SidetoneOff";
constexpr wchar_t kStepCreateFolder[] = L"create the install folder";
constexpr wchar_t kStepCopy[] = L"copy the app into its folder";
constexpr wchar_t kStepStartup[] = L"add it to startup";
constexpr wchar_t kStepShortcut[] = L"create the Start menu shortcut";
constexpr wchar_t kStepInstalledApps[] = L"add it to Installed apps";
constexpr wchar_t kStepLaunch[] = L"start the installed app";
constexpr wchar_t kStepRemoveStartup[] = L"remove it from startup";
constexpr wchar_t kStepRemoveInstalledApps[] = L"remove it from Installed apps";
constexpr wchar_t kStepRemoveSettings[] = L"remove its settings";
constexpr wchar_t kStepRemoveShortcut[] = L"remove the Start menu shortcut";
constexpr wchar_t kStepDelete[] = L"delete the app";

constexpr DWORD kCloseTimeoutMs = 5000;  // how long the running instance gets to quit before it is ended
constexpr int kBusyAttempts = 20;        // a file still in use is retried for up to 2 s
constexpr DWORD kBusyWaitMs = 100;
constexpr int kYesButton = 100;          // a dialog's own button: Install, Update or Uninstall

// The error value for a failed step. std::unexpected converts to any std::expected<T, InstallError>.
std::unexpected<InstallError> Failed(const wchar_t* step, DWORD code) {
    return std::unexpected(InstallError{step, code});
}

std::wstring Quoted(const std::wstring& text) {
    return L"\"" + text + L"\"";
}

// True if a delete succeeded, or found nothing to delete.
bool IsGone(DWORD error) {
    return error == ERROR_SUCCESS || error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

// Calls `operation`, a Win32 call that returns BOOL, until it succeeds or fails for a reason other than a file
// being in use: a process that has just exited can keep its exe busy for a moment. Returns ERROR_SUCCESS or the
// last error. (`const auto&` makes this a function that takes any callable, here a lambda: like a TS callback.)
DWORD RetryWhileBusy(const auto& operation) {
    for (int attempt = 1;; ++attempt) {
        if (operation()) return ERROR_SUCCESS;
        const DWORD error = GetLastError();
        const bool busy = error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION ||
                          error == ERROR_ACCESS_DENIED;
        if (!busy || attempt == kBusyAttempts) return error;
        Sleep(kBusyWaitMs);
    }
}

// Where everything goes (plan, section 5.3).
struct InstallPaths {
    std::wstring folder;    // %LOCALAPPDATA%\Programs\SidetoneOff
    std::wstring exe;       // ...\SidetoneOff\SidetoneOff.exe
    std::wstring shortcut;  // ...\Start Menu\Programs\SidetoneOff.lnk
};

// A known folder's path. `create`: make the folder if it doesn't exist yet; otherwise just compute its path.
std::expected<std::wstring, InstallError> KnownFolder(REFKNOWNFOLDERID folder, bool create) {
    PWSTR path = nullptr;
    const HRESULT result = SHGetKnownFolderPath(folder, create ? KF_FLAG_CREATE : KF_FLAG_DONT_VERIFY, nullptr, &path);
    const std::wstring copy = SUCCEEDED(result) ? path : L"";
    CoTaskMemFree(path);  // required whether the call succeeded or not
    if (FAILED(result)) return Failed(kStepFindFolder, static_cast<DWORD>(result));
    return copy;
}

std::expected<InstallPaths, InstallError> GetInstallPaths(bool create) {
    const auto programs = KnownFolder(FOLDERID_UserProgramFiles, create);
    if (!programs) return std::unexpected(programs.error());
    const auto startMenu = KnownFolder(FOLDERID_Programs, create);
    if (!startMenu) return std::unexpected(startMenu.error());
    const std::wstring folder = *programs + L"\\" + ids::kInstallFolder;
    return InstallPaths{
        .folder = folder,
        .exe = folder + L"\\" + ids::kExeName,
        .shortcut = *startMenu + L"\\" + ids::kShortcutName,
    };
}

// The path of this process's exe.
std::wstring ModulePath() {
    std::wstring path(32768, L'\0');  // the longest path Windows allows
    path.resize(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())));
    return path;
}

// A file's identity: its volume and its ID on that volume. Empty if the file can't be opened.
std::optional<FILE_ID_INFO> FileId(const std::wstring& path) {
    const UniqueHandle file = OwnFileHandle(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
                                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                        nullptr, OPEN_EXISTING, 0, nullptr));
    FILE_ID_INFO id{};
    if (!file || !GetFileInformationByHandleEx(file.get(), FileIdInfo, &id, sizeof(id))) return std::nullopt;
    return id;
}

// True if both paths name the same file, however they are spelled (case, short names, links).
bool IsSameFile(const std::wstring& a, const std::wstring& b) {
    const std::optional<FILE_ID_INFO> idA = FileId(a);
    const std::optional<FILE_ID_INFO> idB = FileId(b);
    return idA && idB && idA->VolumeSerialNumber == idB->VolumeSerialNumber &&
           std::ranges::equal(idA->FileId.Identifier, idB->FileId.Identifier);
}

// HKEY_CURRENT_USER\<subkey>, opened for writing, and created if needed.
std::expected<UniqueHkey, InstallError> CreateUserKey(const wchar_t* subkey, const wchar_t* step) {
    HKEY key = nullptr;
    const LSTATUS status =
        RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) return Failed(step, static_cast<DWORD>(status));
    return UniqueHkey(key);
}

LSTATUS SetString(HKEY key, const wchar_t* name, const std::wstring& value) {
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));  // with the terminating 0
}

LSTATUS SetDword(HKEY key, const wchar_t* name, DWORD value) {
    return RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

// Deletes a value under HKEY_CURRENT_USER\<subkey>. Nothing to delete is fine.
std::expected<void, InstallError> DeleteUserValue(const wchar_t* subkey, const wchar_t* name, const wchar_t* step) {
    const auto status = static_cast<DWORD>(RegDeleteKeyValueW(HKEY_CURRENT_USER, subkey, name));
    if (!IsGone(status)) return Failed(step, status);
    return {};
}

// Deletes HKEY_CURRENT_USER\<subkey> with everything in it. Nothing to delete is fine.
std::expected<void, InstallError> DeleteUserKey(const wchar_t* subkey, const wchar_t* step) {
    const auto status = static_cast<DWORD>(RegDeleteTreeW(HKEY_CURRENT_USER, subkey));
    if (!IsGone(status)) return Failed(step, status);
    return {};
}

// Starts and ends COM on this thread; the Start menu shortcut is a COM object.
class ComScope {
public:
    ComScope() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~ComScope() {
        if (SUCCEEDED(result_)) CoUninitialize();
    }

    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;

private:
    HRESULT result_;
};

// Releases a COM object (COM counts references; Release() drops ours) when its unique_ptr lets go of it.
struct ComReleaser {
    void operator()(IUnknown* object) const noexcept { object->Release(); }
};
using UniqueShellLink = std::unique_ptr<IShellLinkW, ComReleaser>;
using UniquePersistFile = std::unique_ptr<IPersistFile, ComReleaser>;

std::expected<void, InstallError> AddToStartup(const InstallPaths& paths) {
    const auto key = CreateUserKey(ids::kRunKey, kStepStartup);
    if (!key) return std::unexpected(key.error());
    // With --autostart, so the start at sign-in shows no notice (decision M14).
    const LSTATUS status = SetString(key->get(), ids::kRunValue, Quoted(paths.exe) + L" " + ids::kAutostartArgument);
    if (status != ERROR_SUCCESS) return Failed(kStepStartup, static_cast<DWORD>(status));
    return {};
}

std::expected<void, InstallError> CreateShortcut(const InstallPaths& paths) {
    const ComScope com;  // declared first, so COM ends after the objects below are released

    IShellLinkW* link = nullptr;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(result)) return Failed(kStepShortcut, static_cast<DWORD>(result));
    const UniqueShellLink ownedLink{link};
    result = link->SetPath(paths.exe.c_str());
    if (FAILED(result)) return Failed(kStepShortcut, static_cast<DWORD>(result));
    link->SetWorkingDirectory(paths.folder.c_str());
    link->SetDescription(ui::kShortcutDescription);

    IPersistFile* file = nullptr;
    result = link->QueryInterface(IID_PPV_ARGS(&file));
    if (FAILED(result)) return Failed(kStepShortcut, static_cast<DWORD>(result));
    const UniquePersistFile ownedFile{file};
    result = file->Save(paths.shortcut.c_str(), TRUE);
    if (FAILED(result)) return Failed(kStepShortcut, static_cast<DWORD>(result));
    return {};
}

// The entry in Settings > Apps > Installed apps.
std::expected<void, InstallError> AddToInstalledApps(const InstallPaths& paths) {
    const auto key = CreateUserKey(ids::kUninstallKey, kStepInstalledApps);
    if (!key) return std::unexpected(key.error());

    WIN32_FILE_ATTRIBUTE_DATA file{};
    const DWORD sizeKb =
        GetFileAttributesExW(paths.exe.c_str(), GetFileExInfoStandard, &file) ? (file.nFileSizeLow + 1023) / 1024 : 0;
    SYSTEMTIME today{};
    GetLocalTime(&today);
    wchar_t date[9];  // YYYYMMDD
    StringCchPrintfW(date, std::size(date), L"%04u%02u%02u", today.wYear, today.wMonth, today.wDay);

    const HKEY entry = key->get();
    const LSTATUS results[] = {
        SetString(entry, L"DisplayName", ui::kAppName),
        SetString(entry, L"DisplayIcon", paths.exe + L",0"),  // the exe's first icon
        SetString(entry, L"DisplayVersion", L"" SIDETONE_VERSION_STRING),
        SetString(entry, L"Publisher", ui::kPublisher),
        SetString(entry, L"InstallLocation", paths.folder),
        SetString(entry, L"InstallDate", date),
        SetString(entry, L"UninstallString", Quoted(paths.exe) + L" --uninstall"),
        SetDword(entry, L"EstimatedSize", sizeKb),
        SetDword(entry, L"NoModify", 1),  // no Modify or Repair buttons: there is nothing to modify
        SetDword(entry, L"NoRepair", 1),
    };
    for (const LSTATUS status : results) {
        if (status != ERROR_SUCCESS) return Failed(kStepInstalledApps, static_cast<DWORD>(status));
    }
    return {};
}

std::expected<void, InstallError> Launch(const InstallPaths& paths) {
    std::wstring commandLine = Quoted(paths.exe);  // CreateProcessW may write into it, so not a constant
    STARTUPINFOW startup{.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(paths.exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        paths.folder.c_str(), &startup, &process)) {
        return Failed(kStepLaunch, GetLastError());
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Log(L"started the installed app (process %lu)", process.dwProcessId);
    return {};
}

// Windows doesn't let a running exe be deleted, and this process is the installed exe. A hidden cmd.exe waits
// about 2 s (3 pings, 1 s apart), by when this process has exited, then deletes the exe, then the folder if
// it is empty.
std::expected<void, InstallError> DeleteAfterExit(const InstallPaths& paths) {
    wchar_t system[MAX_PATH];
    const UINT length = GetSystemDirectoryW(system, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return Failed(kStepDelete, GetLastError());
    const std::wstring systemFolder(system, length);
    const std::wstring cmd = systemFolder + L"\\cmd.exe";
    // /d skips cmd's AutoRun commands. /c "…" runs the line, then exits; cmd drops the outer quotes.
    std::wstring commandLine = Quoted(cmd) + L" /d /c \"" + Quoted(systemFolder + L"\\ping.exe") +
                               L" -n 3 127.0.0.1 >nul & del /f /q " + Quoted(paths.exe) + L" & rd " +
                               Quoted(paths.folder) + L"\"";
    STARTUPINFOW startup{.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION process{};
    // Started in the system folder: cmd can't remove a folder it is working in.
    if (!CreateProcessW(cmd.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        systemFolder.c_str(), &startup, &process)) {
        return Failed(kStepDelete, GetLastError());
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Log(L"the app and its folder will be deleted in about 2 s");
    return {};
}

// Closes the running tray app, if there is one: asks it to quit (WM_CLOSE, as taskkill does), waits up to 5 s,
// then ends it. The window class finds it whichever exe it runs from.
std::expected<void, InstallError> CloseRunningInstance() {
    const HWND window = FindWindowW(ids::kWindowClass, nullptr);
    if (!window) return {};
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == GetCurrentProcessId()) return {};  // that's us: nothing to close

    const UniqueHandle process{OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, processId)};
    if (!process) return Failed(kStepClose, GetLastError());
    Log(L"closing the running instance (process %lu)", processId);
    PostMessageW(window, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(process.get(), kCloseTimeoutMs) == WAIT_OBJECT_0) return {};

    Log(L"it didn't close within %lu ms; ending it", kCloseTimeoutMs);
    if (!TerminateProcess(process.get(), 1)) return Failed(kStepClose, GetLastError());
    WaitForSingleObject(process.get(), kCloseTimeoutMs);  // TerminateProcess only starts the ending
    return {};
}

// Keeps *openDialog, when given, set to the dialog's window while the dialog is open.
HRESULT CALLBACK TrackOpenDialog(HWND dialog, UINT notification, WPARAM, LPARAM, LONG_PTR data) {
    auto* openDialog = reinterpret_cast<HWND*>(data);
    if (openDialog && notification == TDN_CREATED) *openDialog = dialog;
    if (openDialog && notification == TDN_DESTROYED) *openDialog = nullptr;
    return S_OK;
}

// A task dialog titled "SidetoneOff". With `yesButton`, it has that button and Cancel, and returns true if the
// user picked it; without, it has OK only, and returns false. Esc, Alt+F4 and WM_CLOSE all cancel.
bool RunDialog(PCWSTR icon, const wchar_t* question, const wchar_t* text, const wchar_t* yesButton,
               bool cancelIsDefault, HWND* openDialog) {
    const TASKDIALOG_BUTTON buttons[] = {{kYesButton, yesButton}};
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hInstance = GetModuleHandleW(nullptr);  // where the app icon is
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = ui::kAppName;
    config.pszMainIcon = icon;
    config.pszMainInstruction = question;
    config.pszContent = text;
    if (yesButton) {
        config.pButtons = buttons;
        config.cButtons = 1;
        config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        config.nDefaultButton = cancelIsDefault ? IDCANCEL : kYesButton;
    } else {
        config.dwCommonButtons = TDCBF_OK_BUTTON;
    }
    config.pfCallback = TrackOpenDialog;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(openDialog);

    int picked = IDCANCEL;
    const HRESULT result = TaskDialogIndirect(&config, &picked, nullptr, nullptr);
    if (FAILED(result)) Log(L"can't show a dialog (0x%08lX)", static_cast<unsigned long>(result));
    return SUCCEEDED(result) && picked == kYesButton;
}

}  // namespace

std::wstring DescribeError(const InstallError& error) {
    // HRESULTs read best in hex (0x80070005), Win32 errors in decimal (5).
    wchar_t code[16];
    if (error.code & 0x80000000) StringCchPrintfW(code, std::size(code), L"0x%08lX", error.code);
    else StringCchPrintfW(code, std::size(code), L"%lu", error.code);
    return std::wstring(L"Couldn't ") + error.step + L": " + SystemErrorText(error.code) + L" (error " + code + L")";
}

bool IsInstalledCopy() {
    const auto paths = GetInstallPaths(false);
    return paths && IsSameFile(ModulePath(), paths->exe);
}

bool IsInstalled() {
    const auto paths = GetInstallPaths(false);
    return paths && GetFileAttributesW(paths->exe.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::expected<void, InstallError> Install() {
    const auto paths = GetInstallPaths(true);
    if (!paths) return std::unexpected(paths.error());
    Log(L"installing %hs into %s", SIDETONE_VERSION_STRING, paths->folder.c_str());

    const auto closed = CloseRunningInstance();
    if (!closed) return closed;

    if (!CreateDirectoryW(paths->folder.c_str(), nullptr)) {
        const DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) return Failed(kStepCreateFolder, error);
    }
    const std::wstring self = ModulePath();
    if (IsSameFile(self, paths->exe)) {
        Log(L"this is the installed exe already: nothing to copy");
    } else {
        const DWORD error = RetryWhileBusy([&] { return CopyFileW(self.c_str(), paths->exe.c_str(), FALSE); });
        if (error != ERROR_SUCCESS) return Failed(kStepCopy, error);
    }

    // Each step returns early on failure; the steps before it stay done, and installing again finishes the job.
    auto step = AddToStartup(*paths);
    if (step) step = CreateShortcut(*paths);
    if (step) step = AddToInstalledApps(*paths);
    if (step) step = Launch(*paths);
    if (step) Log(L"installed");
    return step;
}

std::expected<void, InstallError> Unregister() {
    auto step = DeleteUserValue(ids::kRunKey, ids::kRunValue, kStepRemoveStartup);
    if (step) step = DeleteUserValue(ids::kStartupApprovedKey, ids::kRunValue, kStepRemoveStartup);
    if (step) step = DeleteUserKey(ids::kUninstallKey, kStepRemoveInstalledApps);
    if (step) step = DeleteUserKey(ids::kAppKey, kStepRemoveSettings);
    if (!step) return step;

    const auto paths = GetInstallPaths(false);
    if (!paths) return std::unexpected(paths.error());
    if (!DeleteFileW(paths->shortcut.c_str())) {
        const DWORD error = GetLastError();
        if (!IsGone(error)) return Failed(kStepRemoveShortcut, error);
    }
    Log(L"removed from startup, the Start menu and Installed apps");
    return {};
}

std::expected<void, InstallError> RemoveInstalledFiles() {
    const auto paths = GetInstallPaths(false);
    if (!paths) return std::unexpected(paths.error());
    if (IsSameFile(ModulePath(), paths->exe)) return DeleteAfterExit(*paths);

    const DWORD error = RetryWhileBusy([&] { return DeleteFileW(paths->exe.c_str()); });
    if (!IsGone(error)) return Failed(kStepDelete, error);
    if (!RemoveDirectoryW(paths->folder.c_str()) && !IsGone(GetLastError())) {
        Log(L"left the install folder in place: it isn't empty");
    }
    Log(L"deleted the app");
    return {};
}

std::expected<void, InstallError> Uninstall() {
    Log(L"uninstalling");
    auto step = CloseRunningInstance();
    if (step) step = Unregister();
    if (step) step = RemoveInstalledFiles();
    return step;
}

bool ConfirmInstall(bool update) {
    return RunDialog(MAKEINTRESOURCEW(IDI_APP), update ? ui::kUpdateQuestion : ui::kInstallQuestion,
                     update ? ui::kUpdateText : ui::kInstallText, update ? ui::kUpdateButton : ui::kInstallButton,
                     false, nullptr);
}

bool ConfirmUninstall(HWND* openDialog) {
    return RunDialog(TD_WARNING_ICON, ui::kUninstallQuestion, ui::kUninstallText, ui::kUninstallButton, true,
                     openDialog);
}

void ShowError(const wchar_t* title, const InstallError& error, HWND* openDialog) {
    RunDialog(TD_ERROR_ICON, title, DescribeError(error).c_str(), nullptr, false, openDialog);
}
