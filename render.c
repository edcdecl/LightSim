#include "render.h"
#include <math.h>

#define nn(p) if (!(p)) return FALSE

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)

BOOL CreateRenderer(struct RenderData* rd, int initial_size, HWND hWnd) {
    rd->ACCUMULATED_EXPOSURE = 0.0005;
    rd->GLASS_COLORS.x = 50;
    rd->GLASS_COLORS.y = 60;
    rd->GLASS_COLORS.z = 70;
    rd->size.size = initial_size;
    rd->size.v2 = initial_size * 100;
    rd->size.vector_array = (initial_size * 100) * (initial_size * 100);
    rd->size.light = (int)round(rd->size.v2 / 6.0);
    rd->hWnd = hWnd;
    rd->tick = 0;
    rd->frame = 0;
    rd->light_pos = (rd->size.v2 / 5) * rd->size.v2 + (rd->size.v2 / 5) + 1;
    nn(rd->image_data = calloc(rd->size.vector_array, sizeof(struct Vec3)));
    nn(rd->wave_height = calloc(rd->size.vector_array, sizeof(struct Vec3)));
    nn(rd->wave_velocity = calloc(rd->size.vector_array, sizeof(struct Vec3)));
    nn(rd->accumulated_light = calloc(rd->size.vector_array, sizeof(struct Vec3)));
    nn(rd->COLOR_SHIFT = calloc(rd->size.vector_array, sizeof(struct Vec3)));
    {
        struct Vec3 color = {0.06, 0, -0.06};
        for (int i = 0; i < rd->size.vector_array; ++i)
            rd->COLOR_SHIFT[i] = color;
    }
    nn(rd->pixel_mass = calloc(rd->size.vector_array, sizeof(double)));
    for (int i = 0; i < rd->size.vector_array; ++i) {
#define i2s (rd->size.v2)
        rd->pixel_mass[i] = sqrt(
                pow((i / i2s) - (i2s / 2.0), 2) +
                pow((i - 1) % i2s - (i2s / 2.0), 2)) < (i2s / 4.0) ? 0.75 : 1;
#undef i2s
    }
    nn(rd->rgb_mass = calloc(rd->size.vector_array, sizeof(struct Vec3)));
    for (int i = 0; i < rd->size.vector_array; ++i) {
        double pmv = rd->pixel_mass[i];
        rd->rgb_mass[i] = (struct Vec3) {pmv, pmv, pmv};
    }
    nn(rd->hStopRenderEvent = CreateEventW(NULL, TRUE, FALSE, NULL));
    nn(rd->hNextRenderEvent = CreateEventW(NULL, TRUE, FALSE, NULL));
    nn(rd->hBitmap = CreateCompatibleBitmap(GetDC(hWnd), 0, 0));
    return TRUE;
}

BOOL DestroyRenderer(struct RenderData* rd) {
    SetEvent(rd->hStopRenderEvent);
    WaitForSingleObject(rd->hRenderThread, INFINITE);
    free(rd->image_data);
    free(rd->wave_height);
    free(rd->wave_velocity);
    free(rd->accumulated_light);
    free(rd->COLOR_SHIFT);
    free(rd->pixel_mass);
    free(rd->rgb_mass);
    CloseHandle(rd->hStopRenderEvent);
    CloseHandle(rd->hNextRenderEvent);
    CloseHandle(rd->hRenderThread);
    DeleteObject(rd->hBitmap);
    return TRUE;
}

static void data_with_items_x_to_y_replaced_with_value(struct Vec3* data, size_t x, size_t y, struct Vec3 value) {
    for (size_t i = x; i < y; ++i)
        data[i] = value;
}

static void sum_vec3(struct Vec3* d, const struct Vec3* a, const struct Vec3* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        d[i].x = a[i].x + b[i].x;
        d[i].y = a[i].y + b[i].y;
        d[i].z = a[i].z + b[i].z;
    }
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static void sum2x_vec3(struct Vec3* d1, const struct Vec3* a1, const struct Vec3* b1, size_t n1,
                       struct Vec3* d2, const struct Vec3* a2, const struct Vec3* b2, size_t n2) {
    size_t i = 0;
    for (; i < MIN(n1, n2); ++i) {
        d1[i].x = a1[i].x + b1[i].x;
        d2[i].x = a2[i].x + b2[i].x;
        d1[i].y = a1[i].y + b1[i].y;
        d2[i].y = a2[i].y + b2[i].y;
        d1[i].z = a1[i].z + b1[i].z;
        d2[i].z = a2[i].z + b2[i].z;
    }
    if (n1 == n2)
        return;
    else if (n1 > n2) {
        for (; i < n1; ++i) {
            d1[i].x = a1[i].x + b1[i].x;
            d1[i].y = a1[i].y + b1[i].y;
            d1[i].z = a1[i].z + b1[i].z;
        }
    } else {
        for (; i < n2; ++i) {
            d2[i].x = a2[i].x + b2[i].x;
            d2[i].y = a2[i].y + b2[i].y;
            d2[i].z = a2[i].z + b2[i].z;
        }
    }
}

typedef void(tfv3f_t)(struct Vec3* d, const struct Vec3* s);
static void transform_vec3(struct Vec3* d, const struct Vec3* s, size_t n, tfv3f_t* tfunc) {
    for (size_t i = 0; i < n; ++i)
        tfunc(&d[i], &s[i]);
}

static tfv3f_t tfv3f1;
static void tfv3f1(struct Vec3* d, const struct Vec3* s) {
    static double _ACCUMULATED_EXPOSURE = 0;
    if (UNLIKELY(!s)) {
        _ACCUMULATED_EXPOSURE = *(double*)d;
        return;
    }
    d->x = fabs(s->x) * _ACCUMULATED_EXPOSURE;
}

static BOOL calculate(struct RenderData* rd) {
    if (LIKELY(rd->frame < 300)) {
        double repl = sin(rd->frame * 45.83662439235437) * 12;
        data_with_items_x_to_y_replaced_with_value(rd->wave_height, rd->light_pos, rd->light_pos + rd->size.light,
                                                   (struct Vec3) {repl, repl, repl});
    }
    sum_vec3(rd->wave_height, rd->wave_height, rd->wave_velocity, rd->size.vector_array);
    struct Vec3* tmp;
    nn(tmp = calloc(rd->size.vector_array, sizeof(struct Vec3)));
    tfv3f1((struct Vec3*)&rd->ACCUMULATED_EXPOSURE, NULL);
    transform_vec3(tmp, rd->wave_height, rd->size.vector_array, tfv3f1);
    sum_vec3(rd->accumulated_light, rd->accumulated_light, tmp, rd->size.vector_array);
    free(tmp);
    {
        // do not use memset for readability
        struct Vec3* a;
        struct Vec3* b;
        struct Vec3* c;
        struct Vec3* d;
        const size_t v3s = sizeof(struct Vec3);
        int i;

        nn(a = calloc(rd->size.vector_array + 1, v3s));
        a[0] = (struct Vec3) {0, 0, 0};
        for (i = 0; i < rd->size.vector_array; ++i)
            a[i + 1] = rd->wave_height[i];
        nn(b = calloc(rd->size.vector_array, v3s));
        for (i = 0; i < rd->size.vector_array; ++i)
            b[i] = rd->wave_height[i + 1];
        b[rd->size.vector_array - 1] = (struct Vec3) {0, 0, 0};

        nn(c = calloc(rd->size.vector_array + rd->size.v2, v3s));
        for (i = 0; i < rd->size.v2; ++i)
            c[i] = (struct Vec3){0, 0, 0};
        // TODO: finish
        for (size_t j = (i = 0), ) {
            //
        }
        memcpy(c + rd->size.v2, rd->wave_height, rd->size.vector_array * v3s);

        nn(d = calloc((rd->size.vector_array - (rd->size.v2 + 1)) + rd->size.v2, v3s));
        memcpy(d, rd->wave_height + rd->size.v2 + 1, (rd->size.vector_array - (rd->size.v2 + 1)) * v3s);
        memset(d + (rd->size.vector_array - (rd->size.v2 + 1)), 0, rd->size.v2 * v3s);
    }
    ++rd->frame;
    return TRUE;
}

DWORD WINAPI RendererThread(LPVOID lpThreadParameter) {
    struct RenderData* rd = (struct RenderData*)lpThreadParameter;
    while (TRUE) {
        if (WaitForMultipleObjects(2, (HANDLE[]){rd->hStopRenderEvent, rd->hNextRenderEvent}, FALSE, INFINITE) == WAIT_OBJECT_0)
            break;
        ResetEvent(rd->hNextRenderEvent);
        if (!calculate(rd) || !calculate(rd) ||
            !calculate(rd) || !calculate(rd) ||
            !calculate(rd) || !calculate(rd) ||
            !calculate(rd) || !calculate(rd) ||
            !calculate(rd) || !calculate(rd))
            break;
        ++rd->tick;
        PostMessageW(rd->hWnd, WM_COPYRENDER, 0, 0);
    }
    return 0;
}
