#ifndef LIGHTSIM_RENDER_HPP
#define LIGHTSIM_RENDER_HPP
#include <DirectXMath.h>
#include <Windows.h>
#include <span>
#include <mutex>
#include <cmath>
#include <omp_llvm.h>
#include <vector>

#define WM_COPYRENDER (WM_USER + 1)
#define WM_ERRORRENDER (WM_USER + 2)

#define DVWAVE_HEIGHT (0)
#define DVWAVE_VELOCITY (1)
#define DVACCUMULATED_LIGHT (2)
#define DVRGB_MASS (3)

#define COLOR_SHIFT (DirectX::XMVectorSet(0.06, 0L, -0.06, 0))
#define GLASS_COLORS (DirectX::XMVectorSet(25, 25, 25, 0))

struct Renderer final {
    struct sizes_t {
        long basic;
        long view;
        long image_data;
        long light;
    } const sizes;
    struct vec_ent_t {
        DirectX::XMVECTOR wave_height;
        DirectX::XMVECTOR wave_velocity;
        DirectX::XMVECTOR accumulated_light;
        DirectX::XMVECTOR rgb_mass;
        struct rmtmp_t {
            DirectX::XMVECTOR a;
            DirectX::XMVECTOR b;
            DirectX::XMVECTOR c;
            DirectX::XMVECTOR d;
        } rmtmp;
    };
    constexpr static float ACCUMULATED_EXPOSURE = 0.0005;
    std::vector<vec_ent_t> datavecs;
    std::vector<DirectX::XMVECTOR> rmtmpv;
    std::vector<float> pixel_mass;
    std::vector<COLORREF> rmrgbdv;
    HANDLE hStopRenderEvent;
    HANDLE hRenderThread;
    HWND hWnd;
    long frame;
    long tick;
    long light_pos;
    std::mutex hdcRender_mutex;
    HDC hdcRender;
    HBITMAP hbmOldRender;
    std::mutex error_mutex;
    std::wstring error;
    HANDLE hNextRenderEvent;

    explicit Renderer(HWND hWnd, int initial_size) : sizes(initial_size,
                                                           initial_size * 100,
                                                           (initial_size * 100) * (initial_size * 100),
                                                           lroundl(initial_size * 100.0 / 6.0)),
                                                     datavecs(sizes.image_data),
                                                     rmtmpv(sizes.image_data),
                                                     pixel_mass(sizes.image_data),
                                                     rmrgbdv(sizes.image_data),
                                                     hWnd(hWnd),
                                                     frame(0),
                                                     tick(0),
                                                     light_pos(static_cast<long>(floor(sizes.view / 5.0) * sizes.view + floor(sizes.view / 5.0) + 1)) {
        omp_set_num_threads(omp_get_max_threads());
#pragma omp parallel for default(none) shared(pixel_mass, sizes, datavecs)
        for (size_t i = 0; i < sizes.image_data; ++i) {
            pixel_mass[i] = sqrt(pow(floor(i / sizes.view) - (sizes.view / 2.0), 2) +
                                 pow(((int)(i - 1) % sizes.view) - (sizes.view / 2.0), 2)) < sizes.view / 4.0 ? 0.75 : 1;
            // shift it ahead of time, so we don't have to waste time calculating in the render method
            datavecs[i].rgb_mass = DirectX::XMVectorSubtract(DirectX::XMVectorReplicate(pixel_mass[i]), COLOR_SHIFT);
        }
        hNextRenderEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hNextRenderEvent)
            throw std::runtime_error("Failed to create next render event\n");
        hStopRenderEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hStopRenderEvent)
            throw std::runtime_error("Failed to create stop render event\n");
        auto referenceDC = CreateDCW(L"DISPLAY", nullptr, nullptr, nullptr);
        if (!referenceDC)
            throw std::runtime_error("Failed to create reference DC\n");
        hdcRender = CreateCompatibleDC(referenceDC);
        if (!hdcRender)
            throw std::runtime_error("Failed to create render DC\n");
        auto hbmRender = CreateCompatibleBitmap(referenceDC, sizes.view, sizes.view);
        if (!hbmRender)
            throw std::runtime_error("Failed to create render bitmap\n");
        hbmOldRender = static_cast<HBITMAP>(SelectObject(hdcRender, hbmRender));
        if (!hbmOldRender)
            throw std::runtime_error("Failed to select render bitmap\n");
        if (!DeleteDC(referenceDC))
            throw std::runtime_error("Failed to delete reference DC\n");
    }

    ~Renderer() {
        SetEvent(hStopRenderEvent);
        WaitForSingleObject(hRenderThread, INFINITE);
        CloseHandle(hRenderThread);
        CloseHandle(hNextRenderEvent);
        CloseHandle(hStopRenderEvent);
        DeleteObject(SelectObject(hdcRender, hbmOldRender));
        DeleteDC(hdcRender);
    }

    void Calculate();
    void Render();
    static DWORD WINAPI Thread(LPVOID lpThreadParameter);
};

#endif /* LIGHTSIM_RENDER_HPP */
