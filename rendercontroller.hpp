/*
 * Created by edcdecl on 4/6/2024.
 */

#ifndef LIGHTSIM_RENDERCONTROLLER_HPP
#define LIGHTSIM_RENDERCONTROLLER_HPP
#include <Windows.h>

#define RCWM_ATTACH_RENDERUI (WM_USER + 1)

struct RenderController {
    static LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static HANDLE Create(HINSTANCE hInstance, HWND& hWnd);
    explicit RenderController(HINSTANCE hInstance) : hInstance(hInstance),
                                                     hRenderUIWnd(nullptr) {}
    HINSTANCE hInstance;
    HWND hRenderUIWnd;
    HWND hTicksStatic;
    HWND hFramesStatic;
};

#endif /* LIGHTSIM_RENDERCONTROLLER_HPP */
