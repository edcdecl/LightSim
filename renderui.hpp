/*
 * Created by edcdecl on 4/6/2024.
 */

#ifndef LIGHTSIM_RENDERUI_HPP
#define LIGHTSIM_RENDERUI_HPP
#include "render.hpp"
#include <Windows.h>
#include <vector>
#include <atomic>

struct RenderUI {
    static LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static HANDLE Create(HINSTANCE hInstance, HWND& hWnd);
    static void AttachController(HWND hWnd, HWND hControllerWnd);
    std::atomic_flag keepRendering;
    HINSTANCE hInstance;
    Renderer* pRenderer;
    HDC hdcRenderedImage;
    HBITMAP hOldRenderedImageBMP;
    HWND hAttachedControllerWnd;
    explicit RenderUI(HINSTANCE hInstance) : keepRendering(),
                                             hInstance(hInstance),
                                             pRenderer(nullptr),
                                             hdcRenderedImage(nullptr),
                                             hOldRenderedImageBMP(nullptr) {}
};

#endif /* LIGHTSIM_RENDERUI_HPP */
