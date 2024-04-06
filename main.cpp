#include "render.hpp"
#include <format>

static WNDCLASSEXW wcex;
static WCHAR g_szClassName[] = L"LightSim";

static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static Renderer* rd = nullptr;
    static HDC hdcRenderDCCopy = nullptr;
    static HBITMAP hbmOldRenderDCCopy = nullptr;
    static bool bIsRenderingKeyDown = false;
    switch (message) {
        case WM_CREATE: {
            rd = new Renderer(hWnd, 8);
            auto referenceDC = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr);
            if (!referenceDC)
                throw std::runtime_error("Failed to create reference DC");
            hdcRenderDCCopy = CreateCompatibleDC(referenceDC);
            if (!hdcRenderDCCopy)
                throw std::runtime_error("Failed to create back buffer DC");
            hbmOldRenderDCCopy = static_cast<HBITMAP>(SelectObject(hdcRenderDCCopy, CreateCompatibleBitmap(referenceDC, rd->sizes.view, rd->sizes.view)));
            if (!hbmOldRenderDCCopy)
                throw std::runtime_error("Failed to create back buffer bitmap");
            if (!DeleteDC(referenceDC))
                throw std::runtime_error("Failed to delete reference DC");
            rd->hRenderThread = CreateThread(nullptr, 0, Renderer::Thread, rd, 0, nullptr);
            break;
        }
        case WM_COPYRENDER: {
            RECT rcClient;
            rd->hdcRender_mutex.lock();
            GetClientRect(hWnd, &rcClient);
            if (!BitBlt(hdcRenderDCCopy,
                        0,
                        0,
                        rd->sizes.view,
                        rd->sizes.view,
                        rd->hdcRender,
                        0,
                        0,
                        SRCCOPY))
                throw std::runtime_error("Failed to call BitBlt");
            rd->hdcRender_mutex.unlock();
            RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            auto hdcWindow = BeginPaint(hWnd, &ps);
            SetStretchBltMode(hdcWindow, HALFTONE);
            if (bIsRenderingKeyDown && rd->tick < 1500) {
                SetWindowTextW(hWnd, std::format(L"LightSim - F{:010} T{:05} - Rendering...", rd->frame, rd->tick).c_str());
                SetEvent(rd->hNextRenderEvent);
            } else
                SetWindowTextW(hWnd, std::format(L"LightSim - F{:010} T{:05}", rd->frame, rd->tick).c_str());
            RECT rcClient;
            if (!GetClientRect(hWnd, &rcClient))
                throw std::runtime_error("Failed to get drawable area dimensions");
            auto hdcWindow2 = CreateCompatibleDC(hdcWindow);
            if (!hdcWindow2)
                throw std::runtime_error("Failed to create drawing backbuffer DC");
            auto hbmOldWindow2 = SelectObject(hdcWindow2, CreateCompatibleBitmap(hdcWindow, rcClient.right, rcClient.bottom));
            if (!hbmOldWindow2)
                throw std::runtime_error("Failed to create drawing backbuffer bitmap");
            auto hbrBG = CreateSolidBrush(RGB(0, 0, 0x3F));
            if (!hbrBG)
                throw std::runtime_error("Failed to create brush");
            if (!FillRect(hdcWindow2, &rcClient, hbrBG))
                throw std::runtime_error("Failed to clear window");
            if (!DeleteObject(hbrBG))
                throw std::runtime_error("Failed to delete brush");
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
                            hdcRenderDCCopy,
                            0,
                            0,
                            rd->sizes.view,
                            rd->sizes.view,
                            SRCCOPY))
                throw std::runtime_error("Failed to call StretchBlt");
            if (!BitBlt(hdcWindow, 0, 0, rcClient.right, rcClient.bottom, hdcWindow2, 0, 0, SRCCOPY))
                throw std::runtime_error("Failed to call BitBlt");
            if (!DeleteObject(SelectObject(hdcWindow2, hbmOldWindow2)))
                throw std::runtime_error("Failed to delete drawing backbuffer bitmap");
            if (!DeleteDC(hdcWindow2))
                throw std::runtime_error("Failed to delete drawing backbuffer DC");
            if (!EndPaint(hWnd, &ps))
                throw std::runtime_error("Failed to call EndPaint");
            break;
        }
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_SPACE:
                    if (!bIsRenderingKeyDown) {
                        SetWindowTextW(hWnd, std::format(L"LightSim - F{:010} T{:05} - Rendering...", rd->frame, rd->tick).c_str());
                        SetEvent(rd->hNextRenderEvent);
                        bIsRenderingKeyDown = true;
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
            DeleteDC(hdcRenderDCCopy);
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
