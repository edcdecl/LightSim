#include "render.hpp"
#include <format>

static WNDCLASSEXW wcex;
static WCHAR g_szClassName[] = L"LightSim";

static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static Renderer* rd = nullptr;
    static HDC hBackBufferDC = nullptr;
    static bool bIsRenderingKeyDown = false;
    switch (message) {
        case WM_CREATE: {
            rd = new Renderer(hWnd, 12);
            {
                auto referenceDC = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr);
                hBackBufferDC = CreateCompatibleDC(referenceDC);
                if (!hBackBufferDC) {
                    MessageBoxW(hWnd, L"Failed to create back buffer DC", L"Error", MB_OK);
                    return -1;
                }
                DeleteObject(SelectObject(hBackBufferDC, CreateCompatibleBitmap(referenceDC, rd->sizes.view, rd->sizes.view)));
                DeleteDC(referenceDC);
            }
            rd->hRenderThread = CreateThread(nullptr, 0, Renderer::Thread, rd, 0, nullptr);
            break;
        }
        case WM_COPYRENDER: {
            RECT rcClient;
            rd->dcrender_mutex.lock();
            GetClientRect(hWnd, &rcClient);
            if (BitBlt(hBackBufferDC,
                       0,
                       0,
                       rd->sizes.view,
                       rd->sizes.view,
                       rd->hdcRender,
                       0,
                       0,
                       SRCCOPY)) {
                rd->dcrender_mutex.unlock();
                RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
            } else {
                rd->dcrender_mutex.unlock();
                rd->AppendError(L"BitBlt failed\n");
                PostMessageW(hWnd, WM_ERRORRENDER, 0, 0);
            }
            break;
        }
        case WM_ERRORRENDER: {
            rd->ShowError();
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            auto hdcWindow = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            auto hdcWindow2 = CreateCompatibleDC(hdcWindow);
            DeleteObject(SelectObject(hdcWindow2, CreateCompatibleBitmap(hdcWindow, rcClient.right, rcClient.bottom)));
            {
                if (bIsRenderingKeyDown) {
                    SetWindowTextW(hWnd, std::format(L"LightSim - F{:05} - Rendering...", rd->frame).c_str());
                    SetEvent(rd->hNextRenderEvent);
                } else
                    SetWindowTextW(hWnd, std::format(L"LightSim - F{:05}", rd->frame).c_str());
            }
            auto hbrBlack = CreateSolidBrush(RGB(0, 0, 0x7F));
            FillRect(hdcWindow2, &rcClient, hbrBlack);
            DeleteObject(hbrBlack);
            auto mindim = std::min(rcClient.bottom, rcClient.right);
            int w = mindim;
            int h = mindim;
            int x = rcClient.right / 2 - w / 2;
            int y = rcClient.bottom / 2 - h / 2;
            if (!StretchBlt(hdcWindow2,
                            x,
                            y,
                            w,
                            h,
                            hBackBufferDC,
                            0,
                            0,
                            rd->sizes.view,
                            rd->sizes.view,
                            SRCCOPY)) {
                rd->AppendError(L"BitBlt failed\n");
                PostMessageW(hWnd, WM_ERRORRENDER, 0, 0);
                DeleteDC(hdcWindow2);
                return 0;
            }
            if (!BitBlt(hdcWindow, 0, 0, rcClient.right, rcClient.bottom, hdcWindow2, 0, 0, SRCCOPY)) {
                rd->AppendError(L"BitBlt failed\n");
                PostMessageW(hWnd, WM_ERRORRENDER, 0, 0);
                DeleteDC(hdcWindow2);
                return 0;
            }
            DeleteDC(hdcWindow2);
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_SPACE:
                    if (!bIsRenderingKeyDown) {
                        SetWindowTextW(hWnd, std::format(L"LightSim - F{:05} - Rendering...", rd->frame).c_str());
                        SetEvent(rd->hNextRenderEvent);
                        bIsRenderingKeyDown = true;
                    }
                    break;
                case VK_OEM_PLUS:
                    if (!bIsRenderingKeyDown && rd->batch_calculate > 0) {
                        ++rd->batch_calculate;
                        SetWindowTextW(hWnd, std::format(L"LightSim - F{:05} - New batch value: {}", rd->frame, rd->batch_calculate).c_str());
                    }
                    break;
                case VK_OEM_MINUS:
                    if (!bIsRenderingKeyDown) {
                        --rd->batch_calculate;
                        SetWindowTextW(hWnd, std::format(L"LightSim - F{:05} - New batch value: {}", rd->frame, rd->batch_calculate).c_str());
                    }
                    break;
                default:
                    break;
            }
            break;
        case WM_KEYUP:
            if (wParam == VK_SPACE)
                bIsRenderingKeyDown = false;
            break;
        case WM_DESTROY:
            delete rd;
            DeleteDC(hBackBufferDC);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

static inline BOOL regwc(HINSTANCE hInstance) {
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;
    return RegisterClassExW(&wcex);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    MSG msg;
    HWND hWnd;
    SetProcessDPIAware();
    if (!regwc(hInstance))
        return 1;
    hWnd = CreateWindowExW(0, g_szClassName, L"LightSim", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
                           nullptr, nullptr, hInstance, nullptr);
    if (!hWnd)
        return 1;
    UpdateWindow(hWnd);
    ShowWindow(hWnd, nCmdShow);
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
