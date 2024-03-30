#ifndef LIGHTSIM_RENDER_H
#define LIGHTSIM_RENDER_H
#include <Windows.h>
#include <stdint.h>

#define WM_COPYRENDER (WM_USER + 1)

struct Vec3 {
    double x, y, z;
};

struct RenderData {
    struct {
        int size;
        int v2;
        int vector_array;
        int light;
    } size;
    struct {
        int x;
        int y;
        int z;
    } GLASS_COLORS;
    struct Vec3* image_data;
    struct Vec3* wave_height;
    struct Vec3* wave_velocity;
    struct Vec3* accumulated_light;
    struct Vec3* COLOR_SHIFT;
    double* pixel_mass;
    struct Vec3* rgb_mass;
    HANDLE hStopRenderEvent;
    HANDLE hNextRenderEvent;
    HANDLE hRenderThread;
    HWND hWnd;
    HBITMAP hBitmap;
    double ACCUMULATED_EXPOSURE;
    int tick;
    int frame;
    int light_pos;
};

BOOL CreateRenderer(struct RenderData* rd, int initial_size, HWND hWnd);
BOOL DestroyRenderer(struct RenderData* rd);
DWORD WINAPI RendererThread(LPVOID lpThreadParameter);

#endif /* LIGHTSIM_RENDER_H */
