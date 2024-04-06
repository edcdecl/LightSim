#include "render.hpp"
#include <filesystem>
#include <fstream>
#include <format>

using namespace DirectX;

#if 0
template<typename T, T Renderer::vec_ent_t::* r>
void dump(const std::vector<Renderer::vec_ent_t>& v, const std::string& file) {
    {
        std::ofstream f(std::format("../{}", file), std::ios::trunc);
        for (size_t i = 0; i < v.size(); ++i)
            f << std::format("{} {},{},{}\n", i, XMVectorGetX(v[i].*r), XMVectorGetY(v[i].*r), XMVectorGetZ(v[i].*r));
        f.close();
    }
    __debugbreak();
}
template<typename T, T Renderer::vec_ent_t::rmtmp_t::* r>
void dump(const std::vector<Renderer::vec_ent_t>& v, const std::string& file) {
    {
        std::ofstream f(std::format("../{}", file), std::ios::trunc);
        for (size_t i = 0; i < v.size(); ++i)
            f << std::format("{} {},{},{}\n", i,
                             XMVectorGetX(v[i].rmtmp.*r),
                             XMVectorGetY(v[i].rmtmp.*r),
                             XMVectorGetZ(v[i].rmtmp.*r));
        f.close();
    }
    __debugbreak();
}
#endif

void Renderer::Calculate() {
    size_t i;
    if (frame < 300) {
        auto repl = sin(XMConvertToRadians(frame * 45.83662439235437)) * 12;
#pragma omp parallel for default(none) shared(datavecs, sizes, repl, light_pos)
        for (i = light_pos - 1; i < light_pos + sizes.light; ++i)
            datavecs[i].wave_height = XMVectorReplicate(repl);
    }
#pragma omp parallel for default(none) shared(datavecs, sizes, ACCUMULATED_EXPOSURE)
    for (i = 0; i < sizes.image_data; ++i) {
        const auto ve = datavecs[i];
        const auto nwh = XMVectorAdd(ve.wave_height, ve.wave_velocity);
        datavecs[i] = vec_ent_t {
                .wave_height = nwh,
                .wave_velocity = ve.wave_velocity,
                .accumulated_light = XMVectorAdd(XMVectorMultiply(XMVectorAbs(nwh), XMVectorReplicate(ACCUMULATED_EXPOSURE)), ve.accumulated_light),
                .rgb_mass = ve.rgb_mass,
                .rmtmp = {
                        .a = XMVectorZero(),
                        .b = XMVectorZero(),
                        .c = XMVectorZero(),
                        .d = XMVectorZero(),
                }
        };
    }

#pragma omp parallel for default(none) shared(datavecs, sizes)
    for (i = 1; i < sizes.image_data; ++i)
        datavecs[i].rmtmp.a = datavecs[i - 1].wave_height;

#pragma omp parallel for default(none) shared(datavecs, sizes)
    for (i = 0; i < sizes.image_data - 1; ++i)
        datavecs[i].rmtmp.b = datavecs[i + 1].wave_height;

#pragma omp parallel for default(none) shared(datavecs, sizes)
    for (i = sizes.view; i < sizes.image_data; ++i)
        datavecs[i].rmtmp.c = datavecs[i - sizes.view].wave_height;

#pragma omp parallel for default(none) shared(datavecs, sizes)
    for (i = sizes.view; i < sizes.image_data; ++i)
        datavecs[i - sizes.view].rmtmp.d = datavecs[i].wave_height;

#pragma omp parallel for default(none) shared(datavecs)
    for (i = 0; i < datavecs.size(); ++i) {
        auto& dvi = datavecs[i];
        auto d = XMVectorAdd(
                XMVectorAdd(
                        XMVectorAdd(dvi.rmtmp.a, dvi.rmtmp.b),
                        dvi.rmtmp.c
                ),
                dvi.rmtmp.d
        );
        dvi.wave_velocity = XMVectorAdd(
                dvi.wave_velocity,
                XMVectorMultiply(
                        XMVectorSubtract(
                                XMVectorDivide(d, XMVectorReplicate(4)),
                                dvi.wave_height
                        ),
                        dvi.rgb_mass)
                );
    }
    ++frame;
}

void Renderer::Render() {
#pragma omp parallel for default(none) shared(datavecs, sizes, pixel_mass, rmrgbdv)
    for (size_t i = 0; i < sizes.image_data; ++i) {
        const auto dvv = datavecs[i];
        const auto iv = XMVectorMultiply(
                XMVectorPow(
                        XMVectorDivide(
                                XMVectorSubtract(
                                        XMVectorAdd(dvv.accumulated_light, XMVectorSplatOne()),
                                        XMVectorAbs(
                                                XMVectorSubtract(dvv.accumulated_light, XMVectorSplatOne())
                                        )
                                ),
                                XMVectorReplicate(2)
                        ),
                        XMVectorReplicate(2)
                ),
                XMVectorReplicate(255)
        );
        auto pv = pixel_mass[i] < 1 ?
                  XMVectorDivide(
                          XMVectorSubtract(
                                  XMVectorAdd(
                                          XMVectorAdd(iv, GLASS_COLORS),
                                          XMVectorReplicate(255)
                                  ),
                                  XMVectorAbs(
                                          XMVectorSubtract(
                                                  XMVectorAdd(iv, GLASS_COLORS),
                                                  XMVectorReplicate(255)
                                          )
                                  )
                          ),
                          XMVectorReplicate(2)
                  ) : iv;
        rmrgbdv[i] = RGB(lroundl(XMVectorGetX(pv)), lroundl(XMVectorGetY(pv)), lroundl(XMVectorGetZ(pv)));
    }
    auto bmi = BITMAPINFO {
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
    hdcRender_mutex.lock();
    if (!SetDIBitsToDevice(hdcRender,
                           0, 0, sizes.view, sizes.view,
                           0, 0,
                           0, sizes.view,
                           rmrgbdv.data(), &bmi, DIB_RGB_COLORS))
        throw std::runtime_error("Failed to set DIBits");
    hdcRender_mutex.unlock();

    {
        if (!std::filesystem::exists(std::format("./out/s{:02X}", sizes.basic)))
            std::filesystem::create_directory(std::format("./out/s{:02X}", sizes.basic));
        std::ofstream file(std::format("./out/s{:02X}/t{:05}.bmp", sizes.basic, tick), std::ios::binary);
        BITMAPFILEHEADER bfh = {
                .bfType = 0x4D42,
                .bfSize = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizes.image_data * 4),
                .bfReserved1 = 0,
                .bfReserved2 = 0,
                .bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER),
        };
        file.write(reinterpret_cast<const char*>(&bfh), sizeof(BITMAPFILEHEADER));
        file.write(reinterpret_cast<const char*>(&bmi.bmiHeader), sizeof(BITMAPINFOHEADER));
        file.write(reinterpret_cast<const char*>(rmrgbdv.data()), sizes.image_data * 4);
        file.close();
    }
}

DWORD WINAPI Renderer::Thread(LPVOID lpThreadParameter) {
    auto rd = (Renderer*)lpThreadParameter;
    while (TRUE) {
        HANDLE handles[2] = {rd->hStopRenderEvent, rd->hNextRenderEvent};
        if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0)
            break;
        ResetEvent(rd->hNextRenderEvent);
        for (size_t i = 0; i < 16; ++i)
            rd->Calculate();
        rd->Render();
        ++rd->tick;
        PostMessageW(rd->hWnd, WM_COPYRENDER, 0, 0);
    }
    return 0;
}
