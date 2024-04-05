#ifndef LIGHTSIM_RENDER_HPP
#define LIGHTSIM_RENDER_HPP
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <execution>
#include <cmath>

#define WM_COPYRENDER (WM_USER + 1)
#define WM_ERRORRENDER (WM_USER + 2)
#if NDEBUG
#define PARUNS std::execution::unseq,
#else
#define PARUNS
#endif

typedef double render_decimal;
struct Vec3 {
    render_decimal x, y, z;
};
struct Renderer final {
    struct sizes_t {
        long basic;
        long view;
        long image_data;
        long light;
    };
    struct GLASS_COLORS_t {
        int x;
        int y;
        int z;
    };
    const render_decimal ACCUMULATED_EXPOSURE;
    const Vec3 COLOR_SHIFT;
    const sizes_t sizes;
    GLASS_COLORS_t GLASS_COLORS;
    std::vector<Vec3> wave_height;
    std::vector<Vec3> wave_velocity;
    std::vector<Vec3> accumulated_light;
    std::vector<Vec3> rgb_mass;
    std::vector<render_decimal> pixel_mass;
    HANDLE hStopRenderEvent;
    HANDLE hRenderThread;
    HWND hWnd;
    long frame;
    long light_pos;
    unsigned batch_calculate;
    std::mutex dcrender_mutex;
    HDC hdcRender;
    std::mutex error_mutex;
    std::wstring error;
    HANDLE hNextRenderEvent;
    std::vector<Vec3> cmava;
    std::vector<Vec3> cmavb;
    std::vector<Vec3> cmavc;
    std::vector<Vec3> cmavd;
    std::vector<COLORREF> rmrgbdv;
    std::vector<Vec3> rmtmpv;
    void AppendError(const std::wstring &err) {
        std::lock_guard<std::mutex> lock(error_mutex);
        error += err;
    }

    void ShowError() {
        std::lock_guard<std::mutex> lock(error_mutex);
        if (error.empty())
            return;
        MessageBoxW(hWnd, error.c_str(), L"Error", MB_OK | MB_ICONERROR);
        error.clear();
    }

    explicit Renderer(HWND hWnd, int initial_size) : ACCUMULATED_EXPOSURE(0.0005),
                                                     COLOR_SHIFT(0.06, 0L, -0.06),
                                                     sizes(initial_size,
                                                           initial_size * 100,
                                                           (initial_size * 100) * (initial_size * 100),
                                                           lroundl(initial_size * 100.0 / 6.0)),
                                                     GLASS_COLORS(25, 25, 25),
                                                     wave_height(sizes.image_data),
                                                     wave_velocity(sizes.image_data),
                                                     accumulated_light(sizes.image_data),
                                                     rgb_mass(sizes.image_data),
                                                     pixel_mass(sizes.image_data),
                                                     hWnd(hWnd),
                                                     frame(0),
                                                     light_pos(static_cast<long>(floor(sizes.view / 5.0) * sizes.view + floor(sizes.view / 5.0) + 1)),
                                                     batch_calculate(1),
                                                     cmava(sizes.image_data),
                                                     cmavb(sizes.image_data),
                                                     cmavc(sizes.image_data),
                                                     cmavd(sizes.image_data),
                                                     rmrgbdv(sizes.image_data),
                                                     rmtmpv(sizes.image_data) {
        std::iota(pixel_mass.begin(), pixel_mass.end(), 1);
        std::transform(PARUNS pixel_mass.cbegin(), pixel_mass.cend(), pixel_mass.begin(), [this](render_decimal i) {
            return sqrtl(pow(floor(i / sizes.view) - (sizes.view / 2.0), 2) +
                         pow(((int)(i - 1) % sizes.view) - (sizes.view / 2.0), 2)) < sizes.view / 4.0 ? 0.75 : 1;
        });
        // shift it ahead of time, so we don't have to waste time calculating in the render method
        std::transform(PARUNS pixel_mass.cbegin(), pixel_mass.cend(), rgb_mass.begin(), [this](render_decimal pmv) {
            return Vec3{pmv - COLOR_SHIFT.x,
                        pmv - COLOR_SHIFT.y,
                        pmv - COLOR_SHIFT.z};
        });
        hNextRenderEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hNextRenderEvent) {
            AppendError(L"Failed to create next render event\n");
            PostMessageW(hWnd, WM_ERRORRENDER, 0, 0);
        }
        hStopRenderEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hStopRenderEvent) {
            AppendError(L"Failed to create stop render event\n");
            PostMessageW(hWnd, WM_ERRORRENDER, 0, 0);
        }
        {
            auto referenceDC = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr);
            hdcRender = CreateCompatibleDC(referenceDC);
            if (!hdcRender) {
                AppendError(L"Failed to create render DC\n");
                PostMessageW(hWnd, WM_ERRORRENDER, 0, 0);
            }
            auto hRenderBitmap = CreateCompatibleBitmap(referenceDC, sizes.view, sizes.view);
            if (!hRenderBitmap) {
                AppendError(L"Failed to create render bitmap\n");
                PostMessageW(hWnd, WM_ERRORRENDER, 0, 0);
            }
            DeleteDC(referenceDC);
            DeleteObject(SelectObject(hdcRender, hRenderBitmap));
        }
    }

    ~Renderer() {
        SetEvent(hStopRenderEvent);
        WaitForSingleObject(hRenderThread, INFINITE);
        CloseHandle(hRenderThread);
        CloseHandle(hNextRenderEvent);
        CloseHandle(hStopRenderEvent);
        DeleteDC(hdcRender);
    }

    void Calculate();
    void Render();
    static DWORD WINAPI Thread(LPVOID lpThreadParameter);
};

#endif /* LIGHTSIM_RENDER_HPP */
