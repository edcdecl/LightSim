/*
 * Created by edcdecl on 4/6/2024.
 */

#include "renderui.hpp"
#include "rendercontroller.hpp"

static std::atomic_bool bWndClassRegistered = false;
static WNDCLASSEXW wcex;
static WCHAR g_szClassName[] = L"LightSimRUI";
static WCHAR g_szWindowName[] = L"LightSim";

#define RUIWM_ATTACH_CONTROLLER (RUIWM_COPYRENDER + 1)
#define WXB_STATE_PTR 0

LRESULT RenderUI::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto state = reinterpret_cast<RenderUI*>(GetWindowLongPtrW(hWnd, WXB_STATE_PTR));
    switch (message) {
        case WM_CREATE: {
            state = reinterpret_cast<RenderUI*>(reinterpret_cast<LPCREATESTRUCTW>(lParam)->lpCreateParams);
            state->hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hWnd, GWLP_HINSTANCE));
            state->pRenderer = new Renderer(hWnd, 8);
            auto referenceDC = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr);
            state->hdcRenderedImage = CreateCompatibleDC(referenceDC);
            state->hOldRenderedImageBMP = static_cast<HBITMAP>(SelectObject(state->hdcRenderedImage, CreateCompatibleBitmap(referenceDC, state->pRenderer->sizes.view, state->pRenderer->sizes.view)));
            DeleteDC(referenceDC);
            state->pRenderer->hRenderThread = CreateThread(nullptr, 0, Renderer::Thread, state->pRenderer, 0, nullptr);
            SetWindowLongPtrW(hWnd, WXB_STATE_PTR, reinterpret_cast<LONG_PTR>(state));
            return 0;
        }
        case WM_MOVING: {
            if (state->hAttachedControllerWnd)
                SetWindowPos(state->hAttachedControllerWnd, nullptr,
                             reinterpret_cast<RECT*>(lParam)->right, reinterpret_cast<RECT*>(lParam)->top,
                             0, 0,
                             SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SIZING: {
            if (state->hAttachedControllerWnd)
                SetWindowPos(state->hAttachedControllerWnd, nullptr,
                             reinterpret_cast<RECT*>(lParam)->right, reinterpret_cast<RECT*>(lParam)->top,
                             0, 0,
                             SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            return 0;
        }
        case RUIWM_ATTACH_CONTROLLER: {
            auto hControllerWnd = reinterpret_cast<HWND>(wParam);
            PostMessageW(hControllerWnd, RCWM_ATTACH_RENDERUI, reinterpret_cast<WPARAM>(hWnd), 0);
            state->hAttachedControllerWnd = hControllerWnd;
            return 0;
        }
        case RUIWM_COPYRENDER: {
            state->pRenderer->hdcRender_mutex.lock();
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            if (!SetDIBitsToDevice(state->hdcRenderedImage,
                                   0, 0, state->pRenderer->sizes.view, state->pRenderer->sizes.view,
                                   0, 0,
                                   0, state->pRenderer->sizes.view,
                                   state->pRenderer->rgbRenderedPixels.data(), &state->pRenderer->bmInfo, DIB_RGB_COLORS))
                throw std::runtime_error("Failed to call BitBlt");
            state->pRenderer->hdcRender_mutex.unlock();
            RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            auto hdcWindow = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            auto hdcBackBuffer = CreateCompatibleDC(hdcWindow);
            auto hbmOldBackBuffer = SelectObject(hdcBackBuffer, CreateCompatibleBitmap(hdcWindow, rcClient.right, rcClient.bottom));
            SetStretchBltMode(hdcBackBuffer, COLORONCOLOR);
            auto hbrBG = CreateSolidBrush(RGB(0, 0, 0x3F));
            FillRect(hdcBackBuffer, &rcClient, hbrBG);
            DeleteObject(hbrBG);
            auto mindim = std::min(rcClient.bottom, rcClient.right);
            int w = mindim;
            int h = mindim;
            int x = rcClient.right / 2 - w / 2;
            int y = rcClient.bottom / 2 - h / 2;
            StretchBlt(hdcBackBuffer,
                       x, y,
                       w, h,
                       state->hdcRenderedImage,
                       0, 0,
                       state->pRenderer->sizes.view, state->pRenderer->sizes.view,
                       SRCCOPY);
            BitBlt(hdcWindow, 0, 0, rcClient.right, rcClient.bottom, hdcBackBuffer, 0, 0, SRCCOPY);
            DeleteObject(SelectObject(hdcBackBuffer, hbmOldBackBuffer));
            DeleteDC(hdcBackBuffer);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            delete state;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

static bool RegisterRenderUIClass(HINSTANCE hInstance) {
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = RenderUI::WndProc;
    wcex.cbWndExtra = sizeof(RenderUI*);
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;
    return RegisterClassExW(&wcex);
}

struct cruip_t {
    HINSTANCE hInstance;
    HWND hWnd;
    HANDLE hWindowCreatedEvent;
};

static DWORD CreateRenderUI(cruip_t* cruip) {
    MSG msg;
    auto pRenderUI = new RenderUI(cruip->hInstance);
    cruip->hWnd = CreateWindowExW(0, g_szClassName, g_szWindowName, WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                800, 800,
                                nullptr, nullptr, cruip->hInstance, pRenderUI);
    SetEvent(cruip->hWindowCreatedEvent);
    ShowWindow(cruip->hWnd, SW_SHOWDEFAULT);
    UpdateWindow(cruip->hWnd);
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

HANDLE RenderUI::Create(HINSTANCE hInstance, HWND& hWnd) {
    if (!bWndClassRegistered.exchange(true))
        RegisterRenderUIClass(hInstance);
    cruip_t cruip = {hInstance, nullptr, CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    auto hThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(CreateRenderUI), &cruip, 0, nullptr);
    WaitForSingleObject(cruip.hWindowCreatedEvent, INFINITE);
    CloseHandle(cruip.hWindowCreatedEvent);
    hWnd = cruip.hWnd;
    return hThread;
}

void RenderUI::AttachController(HWND hWnd, HWND hControllerWnd) {
    PostMessageW(hWnd, RUIWM_ATTACH_CONTROLLER, reinterpret_cast<WPARAM>(hControllerWnd), 0);
}
