/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "stb/stb_image.h"

begin_c

const char* title = "Sample4";

static image_t image[2];

static char filename[260]; // c:\Users\user\Pictures\mandrill-4.2.03.png

static void init(void);

static int  console(void) {
    fatal_if(true, "%s only SUBSYSTEM:WINDOWS", app.argv[0]);
    return 1;
}

app_t app = {
    .class_name = "sample4",
    .init = init,
    .main = console, // optional
    .wmin = 6.0f, // 6x4 inches
    .hmin = 4.0f
};

static void* load_image(const byte* data, int64_t bytes, int32_t* w, int32_t* h,
    int32_t* bpp, int32_t preferred_bytes_per_pixel);

static void load_images(void) {
    int r = 0;
    void* data = null;
    int64_t bytes = 0;
    for (int i = 0; i < countof(image); i++) {
        if (i == 0) {
            r = crt.memmap_read(filename, &data, &bytes);
        } else {
            r = crt.memmap_res("sample_png", &data, &bytes);
        }
        fatal_if_not_zero(r);
        int w = 0;
        int h = 0;
        int bpp = 0; // bytes (!) per pixel
        void* pixels = load_image(data, bytes, &w, &h, &bpp, 0);
        fatal_if_null(pixels);
        gdi.image_init(&image[i], w, h, bpp, pixels);
        free(pixels);
        // do not unmap resources:
        if (i == 0) { crt.memunmap(data, bytes); }
    }
}

static void paint(uic_t* ui) {
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
    if (image[1].w > 0 && image[1].h > 0) {
        int w = min(ui->w, image[1].w);
        int h = min(ui->h, image[1].h);
        int x = (ui->w - w) / 2;
        int y = (ui->h - h) / 2;
        gdi.set_clip(0, 0, ui->w, ui->h);
        gdi.draw_image(x, y, w, h, &image[1]);
        gdi.set_clip(0, 0, 0, 0);
    }
    if (image[0].w > 0 && image[0].h > 0) {
        int x = (ui->w - image[0].w) / 2;
        int y = (ui->h - image[0].h) / 2;
        gdi.draw_image(x, y, image[0].w, image[0].h, &image[0]);
    }
}

static void download(void) {
    static const char* url =
        "https://upload.wikimedia.org/wikipedia/commons/c/c1/"
        "Wikipedia-sipi-image-db-mandrill-4.2.03.png";
    if (access(filename, 04) != 0) {
        char cmd[256];
        strprintf(cmd, "curl.exe  --silent --fail --create-dirs "
            "\"%s\" --output \"%s\" 2>nul >nul", url, filename);
        int r = system(cmd);
        if (r != 0) {
            traceln("download %s failed %d %s", filename, r, strerror(r));
        }
    }
}

static void init(void) {
    app.title = title;
    app.ui->paint = paint;
    strprintf(filename, "%s\\mandrill-4.2.03.png",
        app.known_folder(known_folder_pictures));
    download();
    load_images();
}

static void* load_image(const byte* data, int64_t bytes, int32_t* w, int32_t* h,
    int32_t* bpp, int32_t preferred_bytes_per_pixel) {
    void* pixels = stbi_load_from_memory((byte const*)data, (int)bytes, w, h,
        bpp, preferred_bytes_per_pixel);
    return pixels;
}

end_c

