/*
 * Created by edcdecl on 4/6/2024.
 */

#include "rendercontroller.hpp"
#include "renderui.hpp"
#include <CommCtrl.h>

static std::atomic_bool bWndClassRegistered = false;
static WNDCLASSEXW wcex;
static WCHAR g_szClassName[] = L"LightSimRC";
static WCHAR g_szWindowName[] = L"LightSim";

#define WXB_STATE_PTR 0

LRESULT RenderController::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto state = reinterpret_cast<RenderController*>(GetWindowLongPtrW(hWnd, WXB_STATE_PTR));
    switch (uMsg) {
        case WM_CREATE: {
            INITCOMMONCONTROLSEX iccex = {
                    .dwSize = sizeof(INITCOMMONCONTROLSEX),
                    .dwICC = ICC_STANDARD_CLASSES,
            };
            // TODO: finish
            InitCommonControlsEx(&iccex);
            state = reinterpret_cast<RenderController*>(reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams);
            SetWindowLongPtrW(hWnd, WXB_STATE_PTR, reinterpret_cast<LONG_PTR>(state));
            return 0;
        }
        case RCWM_ATTACH_RENDERUI: {
            state->hRenderUIWnd = reinterpret_cast<HWND>(wParam);
            // move this window so that it is directly right of the renderui window
            RECT rcRenderUI;
            GetWindowRect(state->hRenderUIWnd, &rcRenderUI);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            SetWindowPos(hWnd, nullptr, rcRenderUI.right, rcRenderUI.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

bool RegisterRenderControllerClass(HINSTANCE hInstance) {
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = RenderController::WndProc;
    wcex.cbWndExtra = sizeof(RenderController*);
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;
    return RegisterClassExW(&wcex);
}

struct crcp_t {
    HINSTANCE hInstance;
    HWND hWnd;
    HANDLE hWindowCreatedEvent;
};

static DWORD CreateRenderController(crcp_t* crcp) {
    MSG msg;
    auto pRenderController = new RenderController(crcp->hInstance);
    crcp->hWnd = CreateWindowExW(0, g_szClassName, g_szWindowName, WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                800, 800,
                                nullptr, nullptr, crcp->hInstance, pRenderController);
    SetEvent(crcp->hWindowCreatedEvent);
    ShowWindow(crcp->hWnd, SW_SHOWDEFAULT);
    UpdateWindow(crcp->hWnd);
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

HANDLE RenderController::Create(HINSTANCE hInstance, HWND& hWnd) {
    if (!bWndClassRegistered.exchange(true))
        RegisterRenderControllerClass(hInstance);
    crcp_t crcp = {hInstance, hWnd, CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    auto hThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(CreateRenderController), &crcp, 0, nullptr);
    WaitForSingleObject(crcp.hWindowCreatedEvent, INFINITE);
    CloseHandle(crcp.hWindowCreatedEvent);
    hWnd = crcp.hWnd;
    return hThread;
}
