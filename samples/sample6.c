/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

begin_c

const char* title = "Sample6";

struct {
    int32_t bpp; // bytes per pixel
    int32_t w;
    int32_t h;
    int32_t frames;
    int32_t* delays; // delays[frames];
    byte* pixels;
} gif;

static image_t  image;
static int32_t  index; // animation index 0..gif.frames - 1
static event_t  quit;
static thread_t thread;

static void init();
static void fini();

static int  console() {
    fatal_if(true, "%s only SUBSYSTEM:WINDOWS", app.argv[0]);
    return 1;
}

app_t app = {
    .class_name = "sample4",
    .init = init,
    .fini = fini,
    .main = console, // optional
    .min_width = 640,
    .min_height = 640,
    .max_width = 640,
    .max_height = 640
};

static void* load_image(const byte* data, int64_t bytes, int32_t* w, int32_t* h,
    int32_t* bpp, int32_t preferred_bytes_per_pixel);

static void* load_animated_gif(const byte* data, int64_t bytes,
    int32_t** delays, int32_t* w, int32_t* h, int32_t* frames, int32_t* bpp,
    int32_t preferred_bytes_per_pixel);

static void paint(uic_t* ui) {
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
    int w = min(ui->w, image.w);
    int h = min(ui->h, image.h);
    int x = (ui->w - w) / 2;
    int y = (ui->h - h) / 2;
    gdi.set_clip(0, 0, ui->w, ui->h);
    gdi.draw_image(x, y, w, h, &image);
    gdi.set_clip(0, 0, 0, 0);
    if (gif.pixels != null) {
        byte* p = gif.pixels + gif.w * gif.h * gif.bpp * index;
        image_t frame = { 0 };
        gdi.image_init(&frame, gif.w, gif.h, gif.bpp, p);
        x = (ui->w - gif.w) / 2;
        y = (ui->h - gif.h) / 2;
        gdi.alpha_blend(x, y, gif.w, gif.h, &frame, 1.0);
        gdi.image_dispose(&frame);
    }
}

static void load_gif(void* unused(ignored)) {
    cursor_t cursor = app.cursor;
    app.set_cursor(app.cursor_wait);
    void* data = null;
    int64_t bytes = 0;
    int r = crt.memmap_res("groot_gif", &data, &bytes);
    fatal_if_not_zero(r);
    gif.pixels = load_animated_gif(data, bytes, &gif.delays,
        &gif.w, &gif.h, &gif.frames, &gif.bpp, 4);
    fatal_if(gif.pixels == null || gif.bpp != 4 || gif.frames < 1);
    // resources cannot be unmapped do not call crt.memunmap()
    app.set_cursor(cursor);
    for (;;) {
        app.redraw();
        if (events.wait_or_timeout(quit, gif.delays[index] * 0.001) == 0) {
            break;
        }
        index = (index + 1) % gif.frames;
    }
}

static void init() {
    app.title = title;
    app.ui->paint = paint;
    quit = events.create();
    thread = threads.start(load_gif, null);
    void* data = null;
    int64_t bytes = 0;
    fatal_if_not_zero(crt.memmap_res("sample_png", &data, &bytes));
    int w = 0;
    int h = 0;
    int bpp = 0; // bytes (!) per pixel
    void* pixels = load_image(data, bytes, &w, &h, &bpp, 0);
    fatal_if_null(pixels);
    gdi.image_init(&image, w, h, bpp, pixels);
    free(pixels);
}

static void fini() {
    gdi.image_dispose(&image);
    free(gif.pixels);
    free(gif.delays);
    events.set(quit);
    threads.join(thread);
    events.dispose(quit);
}

end_c

begin_c

#pragma warning(disable: 4459) // parameter/local hides global declaration
#pragma warning(disable: 4244) // conversion from '...' to '...', possible loss of data

#define STBI_ASSERT(x) assert(x)
#include "stb_image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void* load_image(const byte* data, int64_t bytes, int32_t* w, int32_t* h,
    int32_t* bpp, int32_t preferred_bytes_per_pixel) {
    void* pixels = stbi_load_from_memory((byte const*)data, (int)bytes, w, h,
        bpp, preferred_bytes_per_pixel);
    return pixels;
}

static void* load_animated_gif(const byte* data, int64_t bytes,
    int32_t** delays, int32_t* w, int32_t* h, int32_t* frames, int32_t* bpp,
    int32_t preferred_bytes_per_pixel) {
    stbi_uc* pixels = stbi_load_gif_from_memory(data, bytes,
        delays, w, h, frames,
        bpp, preferred_bytes_per_pixel);
    return pixels;
}

end_c

