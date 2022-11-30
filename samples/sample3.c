/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

begin_c

const char* title = "Sample3";

static volatile int index; // index of image to paint, !ix to render
static image_t image[2];
static byte pixels[2][4 * 4096 * 4096];

static thread_t thread;
static event_t wake;
static event_t quit;

static volatile bool rendering;
static volatile bool stop;
static volatile double render_time;

uic_button(full_screen, "\xE2\xA7\x89", 1.0, {
    b->ui.pressed = !b->ui.pressed;
    app.full_screen(b->ui.pressed);
});

static void paint(uic_t* ui) {
    int k = index;
    gdi.draw_image(0, 0, ui->w, ui->h, &image[k]);
    gdi.x = ui->em.x;
    gdi.y = ui->em.y / 4;
    gdi.set_text_color(colors.orange);
    gdi.textln("Try Full Screen Button there --->");
    gdi.y = ui->h - ui->em.y * 3 / 2;
    gdi.set_text_color(colors.orange);
    gdi.textln("render time %.1f ms / avg paint time %.1f ms",
        render_time * 1000, app.paint_avg * 1000);
}

static void request_rendering() {
    rendering = true;
    events.set(wake);
}

static void stop_rendering() {
    if (rendering) {
        stop = true;
        while (rendering || stop) { crt.sleep(0.01); }
    }
}

static void measure(uic_t* ui) {
    // called on window resize
    ui->w = app.crc.w;
    ui->h = app.crc.h;
    const int w = ui->w;
    const int h = ui->h;
    image_t* im = &image[index];
    if (w != im->w || h != im->h) {
        stop_rendering();
        im = &image[!index];
        gdi.image_dispose(im);
        fatal_if(w * h * 4 > countof(pixels[!index]),
            "increase size of pixels[][%d * %d * 4]", w, h);
        gdi.image_init(im, w, h, 4, pixels[!index]);
        request_rendering();
    }
}

static void layout(uic_t* ui) {
    full_screen.ui.x = ui->w - full_screen.ui.w - ui->em.x / 4;
    full_screen.ui.y = ui->em.y / 4;
}

static void renderer(void* unused); // renderer thread

static void openned() {
    fatal_if(app.crc.w * app.crc.h * 4 > countof(pixels[0]),
        "increase size of pixels[][%d * %d * 4]", app.crc.w, app.crc.h);
    gdi.image_init(&image[0], app.crc.w, app.crc.h, 4, pixels[0]);
    gdi.image_init(&image[1], app.crc.w, app.crc.h, 4, pixels[1]);
    thread = threads.start(renderer, null);
    request_rendering();
//  uic_button_init(&full_screen, "\xE2\xA7\x89", 1, full_screen_callback);
    strprintf(full_screen.ui.tip, "&Full Screen");
    full_screen.ui.shortcut = 'F';
}

static void closed() {
    events.set(quit);
    threads.join(thread);
    thread = null;
    gdi.image_dispose(&image[0]);
    gdi.image_dispose(&image[1]);
}

static void keyboard(uic_t* unused, int32_t ch) {
    (void)unused;
    if (ch == 'q' || ch == 'Q') { app.close(); }
    if (app.is_full_screen && ch == 033) {
        full_screen_callback(&full_screen);
    }
}

static void fini() {
    events.dispose(wake);
    events.dispose(quit);
    wake = null;
    quit = null;
}

static void init() {
    app.title = title;
    threads.realtime();
    app.fini = fini;
    app.closed = closed;
    app.openned = openned;
    static uic_t* children[] = { &full_screen.ui, null};
    app.ui->children = children;
    app.ui->layout = layout;
    app.ui->measure = measure;
    app.ui->paint = paint;
    app.ui->keyboard = keyboard;
    wake = events.create();
    quit = events.create();
}

app_t app = {
    .class_name = "sample3",
    .init = init,
    .min_width = 640,
    .min_height = 480
};

static double scale(int x, int n, double low, double hi) {
    return x / (double)(n - 1) * (hi - low) + low;
}

static void mandelbrot(image_t* im) {
    double time = crt.seconds();
    for (int r = 0; r < im->h && !stop; r++) {
        double y0 = scale(r, im->h, -1.12, 1.12);
        for (int c = 0; c < im->w && !stop; c++) {
            double x0 = scale(c, im->w, -2.00, 0.47);
            double x = 0;
            double y = 0;
            int iteration = 0;
            enum { max_iteration = 100 };
            while (x* x + y * y <= 2 * 2 && iteration < max_iteration && !stop) {
                double t = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = t;
                iteration++;
            }
            static color_t palette[16] = {
                rgb( 66,  30,  15),  rgb( 25,   7,  26),
                rgb(  9,   1,  47),  rgb(  4,   4,  73),
                rgb(  0,   7, 100),  rgb( 12,  44, 138),
                rgb( 24,  82, 177),  rgb( 57, 125, 209),
                rgb(134, 181, 229),  rgb(211, 236, 248),
                rgb(241, 233, 191),  rgb(248, 201,  95),
                rgb(255, 170,   0),  rgb(204, 128,   0),
                rgb(153,  87,   0),  rgb(106,  52,   3)
            };
            color_t color = palette[iteration % countof(palette)];
            byte* px = &((byte*)im->pixels)[r * im->w * 4 + c * 4];
            px[3] = 0xFF;
            px[0] = (color >> 16) & 0xFF;
            px[1] = (color >>  8) & 0xFF;
            px[2] = (color >>  0) & 0xFF;
        }
    }
    render_time = crt.seconds() - time;
}

static void renderer(void* unused) {
    (void)unused;
    threads.name("renderer");
    threads.realtime();
    event_t es[2] = {wake, quit};
    for (;;) {
        int e = events.wait_any(countof(es), es);
        if (e != 0) { break; }
        int k = !index;
        app.set_cursor(app.cursor_wait);
        mandelbrot(&image[k]);
        app.set_cursor(app.cursor_arrow);
        if (!stop) { index = !index; app.redraw(); }
        stop = false;
        rendering = false;
    }
}

end_c
