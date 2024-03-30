#include "render.h"

static WNDCLASSEXW wcex;
static WCHAR g_szClassName[] = L"LightSim";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    struct RenderData rd;
    switch (message) {
        case WM_CREATE:
            if (!CreateRenderer(&rd, 6, hWnd))
                return -1;
            if (!(rd.hRenderThread = CreateThread(NULL, 0, RendererThread, &rd, 0, NULL)))
                return -1;
            SetEvent(rd.hNextRenderEvent);
            break;
        case WM_COPYRENDER:
            // copy bitmap render data to window
            break;
        case WM_DESTROY:
            DestroyRenderer(&rd);
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
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szClassName;
    return RegisterClassExW(&wcex);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    MSG msg;
    HWND hWnd;
    if (!regwc(hInstance))
        return 1;
    hWnd = CreateWindowExW(0, g_szClassName, L"LightSim", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 500, 500,
                           NULL, NULL, hInstance, NULL);
    if (!hWnd)
        return 1;
    UpdateWindow(hWnd);
    ShowWindow(hWnd, nCmdShow);
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
