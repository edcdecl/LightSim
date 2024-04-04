#include "render.hpp"

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)

#ifndef PI
#define PI 3.141592653589793238462643383279L
#endif

void Renderer::Calculate() {
    if (frame < 300) {
        auto repl = sinl((frame * 45.83662439235437L) * (PI / 180)) * 12;
        std::fill(PARUNS wave_height.begin() + light_pos - 1,
                  wave_height.begin() + (light_pos + sizes.light), Vec3{repl, repl, repl});
    }
    std::transform(PARUNS wave_height.cbegin(), wave_height.cend(), wave_velocity.cbegin(), wave_height.begin(),
                   [](const Vec3& wh, const Vec3& wv) {
        return Vec3{wh.x + wv.x,
                    wh.y + wv.y,
                    wh.z + wv.z};
    });
    std::transform(PARUNS accumulated_light.cbegin(), accumulated_light.cend(), wave_height.cbegin(), accumulated_light.begin(),
    [this](const Vec3& al, const Vec3& wh) {
        return Vec3{fabsl(wh.x) * ACCUMULATED_EXPOSURE + al.x,
                    fabsl(wh.y) * ACCUMULATED_EXPOSURE + al.y,
                    fabsl(wh.z) * ACCUMULATED_EXPOSURE + al.z};
    });
    std::copy(PARUNS wave_height.cbegin(), wave_height.cend() - 1, cmava.begin() + 1);
    cmava[0] = {0, 0, 0};

    std::copy(PARUNS wave_height.cbegin() + 1, wave_height.cend(), cmavb.begin());
    cmavb[sizes.image_data - 1] = {0, 0, 0};

    std::copy(PARUNS wave_height.cbegin(), wave_height.cend() - sizes.view, cmavc.begin() + sizes.view);
    std::fill(PARUNS cmavc.begin(), cmavc.begin() + sizes.view, Vec3{0, 0, 0});

    std::copy(PARUNS wave_height.cbegin() + sizes.view, wave_height.cend(), cmavd.begin());
    std::fill(PARUNS cmavd.end() - sizes.view, cmavd.end(), Vec3{0, 0, 0});

    std::transform(PARUNS cmava.cbegin(), cmava.cend(), cmavb.cbegin(), cmavb.begin(),
                   [](const Vec3& av, const Vec3& bv) {
        return Vec3{av.x + bv.x,
                    av.y + bv.y,
                    av.z + bv.z};
    });
    std::transform(PARUNS cmavb.cbegin(), cmavb.cend(), cmavc.cbegin(), cmavc.begin(),
                   [](const Vec3& bv, const Vec3& cv) {
        return Vec3{bv.x + cv.x,
                    bv.y + cv.y,
                    bv.z + cv.z};
    });
    std::transform(PARUNS cmavc.cbegin(), cmavc.cend(), cmavd.cbegin(), cmavd.begin(),
                   [](const Vec3& cv, const Vec3& dv) {
        return Vec3{cv.x + dv.x,
                    cv.y + dv.y,
                    cv.z + dv.z};
    });
    std::transform(PARUNS cmavd.cbegin(), cmavd.cend(), wave_height.cbegin(), cmavd.begin(),
                   [](const Vec3& dv, const Vec3& wh) {
        return Vec3{dv.x / 4.0 - wh.x,
                    dv.y / 4.0 - wh.y,
                    dv.z / 4.0 - wh.z};
    });
    std::transform(PARUNS cmavd.cbegin(), cmavd.cend(), rgb_mass.cbegin(), cmavd.begin(),
                   [](const Vec3& dv, const Vec3& sv) {
        return Vec3{dv.x * sv.x,
                    dv.y * sv.y,
                    dv.z * sv.z};
    });
    std::transform(PARUNS wave_velocity.cbegin(), wave_velocity.cend(), cmavd.cbegin(), wave_velocity.begin(),
                   [](const Vec3& wv, const Vec3& dv) {
        return Vec3{wv.x + dv.x,
                    wv.y + dv.y,
                    wv.z + dv.z};
    });
    ++frame;
}

void Renderer::Render() {
    std::transform(PARUNS accumulated_light.cbegin(), accumulated_light.cend(), rmtmpv.begin(),
                   [](const Vec3& alv) {
        return Vec3{
                powl((alv.x + 1 - fabsl(alv.x - 1)) / 2.0, 2) * 255,
                powl((alv.y + 1 - fabsl(alv.y - 1)) / 2.0, 2) * 255,
                powl((alv.z + 1 - fabsl(alv.z - 1)) / 2.0, 2) * 255};
    });
    std::transform(PARUNS rmtmpv.cbegin(), rmtmpv.cend(), pixel_mass.cbegin(), rmrgbdv.begin(),
                   [this](const Vec3& mapv, render_decimal mass) {
       auto tmp2 = mass < 1 ? Vec3{
               ((mapv.x + GLASS_COLORS.x + 255) - fabsl(mapv.x + GLASS_COLORS.x - 255)) / 2,
               ((mapv.y + GLASS_COLORS.y + 255) - fabsl(mapv.y + GLASS_COLORS.y - 255)) / 2,
               ((mapv.z + GLASS_COLORS.z + 255) - fabsl(mapv.z + GLASS_COLORS.z - 255)) / 2
       } : mapv;
       return RGB(lroundl(tmp2.x), lroundl(tmp2.y), lroundl(tmp2.z));
    });
    auto bmi = BITMAPINFO{
            .bmiHeader = {
                    .biSize = sizeof(BITMAPINFOHEADER),
                    .biWidth = sizes.view,
                    .biHeight = -sizes.view,
                    .biPlanes = 1,
                    .biBitCount = 32,
                    .biCompression = BI_RGB,
                    .biSizeImage = 0,
                    .biXPelsPerMeter = 0,
                    .biYPelsPerMeter = 0,
                    .biClrUsed = 0,
                    .biClrImportant = 0,
            }
    };
    dcrender_mutex.lock();
    SetDIBits(hdcRender, reinterpret_cast<HBITMAP>(GetCurrentObject(hdcRender, OBJ_BITMAP)),
              0, sizes.view, rmrgbdv.data(), &bmi, DIB_RGB_COLORS);
    dcrender_mutex.unlock();
}

DWORD WINAPI Renderer::Thread(LPVOID lpThreadParameter) {
    auto rd = (Renderer*)lpThreadParameter;
    while (TRUE) {
        HANDLE handles[2] = {rd->hStopRenderEvent, rd->hNextRenderEvent};
        if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0)
            break;
        ResetEvent(rd->hNextRenderEvent);
        rd->Calculate();
        rd->Render();
        PostMessageW(rd->hWnd, WM_COPYRENDER, 0, 0);
    }
    return 0;
}
