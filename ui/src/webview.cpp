/*
 * WebView2 Implementation
 *
 * Initializes and manages the WebView2 control that hosts the React frontend
 */

#pragma warning(disable:4100)

#include "webview.h"
#include <webview2.h>
#include <wrl.h>
#include <string>
#include <shlwapi.h>
#include <shlobj.h>
#include <filesystem>
#include <stdio.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

/* Global WebView controller and environment */
static ComPtr<ICoreWebView2Controller> g_webviewController;
static ComPtr<ICoreWebView2> g_webview;
static ComPtr<ICoreWebView2_4> g_webview4;

/* Forward declarations */
static HRESULT OnWebViewCreated(HRESULT result, ICoreWebView2Controller* controller, HWND hwnd);
static void OnWebMessageReceived(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args);

/*
 * Get the path to the frontend assets
 */
static std::wstring GetFrontendPath() {
    /* Try multiple locations for frontend assets:
     * 1. ../resources/www (build directory)
     * 2. ./resources/www (same directory)
     * 3. ../../resources/www (from Release/Debug folder)
     * 4. Relative to executable: ../../frontend/dist
     * 5. In C:\ProgramData\Peek\www
     */

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    /* Remove filename to get directory */
    PathRemoveFileSpecW(exePath);

    fs::path exeDir(exePath);

    /* Try build/ui/resources/www (from build/ui/Release) */
    fs::path frontendPath = exeDir.parent_path() / L"resources" / L"www" / L"index.html";
    if (fs::exists(frontendPath)) {
        return frontendPath.wstring();
    }

    /* Try resources subdirectory (same level as exe) */
    frontendPath = exeDir / L"resources" / L"www" / L"index.html";
    if (fs::exists(frontendPath)) {
        return frontendPath.wstring();
    }

    /* Try relative to build root */
    frontendPath = exeDir.parent_path().parent_path() / L"frontend" / L"dist" / L"index.html";
    if (fs::exists(frontendPath)) {
        return frontendPath.wstring();
    }

    /* Try ProgramData */
    frontendPath = L"C:\\ProgramData\\Peek\\www\\index.html";
    if (fs::exists(frontendPath)) {
        return frontendPath.wstring();
    }

    /* Fallback: return expected path (will fail with error page) */
    return L"file:///" + exeDir.wstring() + L"/index.html";
}

/*
 * Create file URL from path
 */
static std::wstring PathToFileUrl(const std::wstring& path) {
    std::wstring url = L"file:///";
    url += path;

    /* Replace backslashes with forward slashes */
    for (auto& c : url) {
        if (c == L'\\') {
            c = L'/';
        }
    }

    return url;
}

/*
 * WebMessage received handler
 */
static void OnWebMessageReceived(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) {
    wchar_t* message = nullptr;
    args->get_WebMessageAsJson(&message);

    if (message) {
        /* Message from frontend is in JSON format
         * Expected format: {"type": "command", "data": {...}}
         */

        /* TODO: Parse JSON and forward to IPC client */

        CoTaskMemFree(message);
    }
}

/*
 * WebView creation callback
 */
static HRESULT OnWebViewCreated(HRESULT result, ICoreWebView2Controller* controller, HWND hwnd) {
    if (!SUCCEEDED(result)) {
        MessageBoxW(nullptr,
            L"Failed to create WebView2 instance.\n\n"
            L"Make sure you have WebView2 Runtime installed.\n\n"
            L"Download from: https://go.microsoft.com/fwlink/p/?LinkId=2124703",
            L"WebView2 Initialization Failed",
            MB_ICONERROR);
        return result;
    }

    g_webviewController = controller;
    g_webviewController->get_CoreWebView2(&g_webview);

    /* Resize WebView to fill the window */
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    g_webviewController->put_Bounds(bounds);

    /* Get WebView2 v4 interface for additional features */
    g_webview.As(&g_webview4);

    /* Register message handler */
    EventRegistrationToken token;
    g_webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) {
                OnWebMessageReceived(webview, args);
                return S_OK;
            }).Get(),
        &token);

    /* Load the real frontend */
    std::wstring frontendPath = GetFrontendPath();
    std::wstring fileUrl = PathToFileUrl(frontendPath);

    /* Debug: show what path we're trying to load */
    OutputDebugStringW((L"Loading frontend from: " + fileUrl).c_str());

    g_webview->Navigate(fileUrl.c_str());

    return S_OK;
}

/*
 * Initialize WebView2
 */
int webview_init(HWND hwnd) {
    /* Check if WebView2 runtime is available */
    LPWSTR versionInfo = nullptr;
    HRESULT hrCheck = GetAvailableCoreWebView2BrowserVersionString(nullptr, &versionInfo);

    if (FAILED(hrCheck) || !versionInfo) {
        MessageBoxW(nullptr,
            L"WebView2 Runtime not detected.\n\n"
            L"Please install Microsoft Edge WebView2 Runtime:\n"
            L"https://go.microsoft.com/fwlink/p/?LinkId=2124703\n\n"
            L"Or make sure Microsoft Edge is installed.",
            L"WebView2 Not Found",
            MB_ICONERROR | MB_OK);
        return 1;
    }

    /* Display detected version for debugging */
    wchar_t debugMsg[512];
    swprintf_s(debugMsg, 512, L"WebView2 Runtime found: %s", versionInfo);
    OutputDebugStringW(debugMsg);
    CoTaskMemFree(versionInfo);

    /* Create WebView2 environment (will be created asynchronously) */
    wchar_t userDataFolder[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, userDataFolder))) {
        wcscpy_s(userDataFolder, MAX_PATH, L"C:\\Users\\Default\\AppData\\Roaming\\Peek");
    }

    wcscat_s(userDataFolder, MAX_PATH, L"\\WebView2");

    /* Create the WebView2 environment asynchronously */
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                    /* Use auto-detected runtime */
        userDataFolder,             /* User data folder */
        nullptr,                    /* Environment options */
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (!SUCCEEDED(result)) {
                    wchar_t errorMsg[256];
                    swprintf_s(errorMsg, 256,
                        L"Failed to create WebView2 environment.\n\n"
                        L"Error code: 0x%08X\n\n"
                        L"Please try:\n"
                        L"1. Running as Administrator\n"
                        L"2. Reinstalling WebView2 Runtime",
                        (unsigned int)result);
                    MessageBoxW(nullptr, errorMsg, L"Error", MB_ICONERROR);
                    return result;
                }

                /* Create controller asynchronously */
                return env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) {
                            return OnWebViewCreated(result, controller, hwnd);
                        }).Get());
            }).Get());

    if (FAILED(hr)) {
        wchar_t errorMsg[512];
        swprintf_s(errorMsg, 512,
            L"Failed to initialize WebView2.\n\n"
            L"Error code: 0x%08X\n\n"
            L"Troubleshooting:\n"
            L"1. Check if WebView2 Runtime is installed\n"
            L"2. Try reinstalling from: https://go.microsoft.com/fwlink/p/?LinkId=2124703\n"
            L"3. Run as Administrator\n"
            L"4. Check antivirus/firewall settings",
            (unsigned int)hr);
        MessageBoxW(nullptr, errorMsg, L"WebView2 Initialization Error", MB_ICONERROR);
        return 1;
    }

    return 0;
}

/*
 * Cleanup
 */
void webview_cleanup(void) {
    if (g_webviewController) {
        g_webviewController->Close();
        g_webviewController = nullptr;
    }

    g_webview = nullptr;
    g_webview4 = nullptr;
}

/*
 * Execute JavaScript
 */
int webview_execute_script(const wchar_t *script) {
    if (!g_webview) {
        return -1;
    }

    g_webview->ExecuteScript(script, Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [](HRESULT error, PCWSTR result) {
            if (FAILED(error)) {
                /* Log error */
            }
            return S_OK;
        }).Get());

    return 0;
}

/*
 * Post message to WebView (from daemon)
 * This triggers window.chrome.webview.addEventListener('message', handler) in the frontend
 */
int webview_post_message(const char *message) {
    if (!g_webview) {
        return -1;
    }

    /* Convert to wide string */
    int size = MultiByteToWideChar(CP_UTF8, 0, message, -1, nullptr, 0);
    wchar_t* wmessage = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wmessage, size);

    /* Post to WebView */
    HRESULT hr = g_webview->PostWebMessageAsJson(wmessage);

    delete[] wmessage;

    return SUCCEEDED(hr) ? 0 : -1;
}

/*
 * Resize WebView to fill window client area
 */
void webview_resize(HWND hwnd) {
    if (g_webviewController) {
        RECT bounds;
        GetClientRect(hwnd, &bounds);
        g_webviewController->put_Bounds(bounds);
    }
}
