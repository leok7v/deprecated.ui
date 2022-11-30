/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include <Windows.h>
#include <WindowsX.h>

begin_c

#define TITLE "Sample9"

static void init();

app_t app = {
    .class_name = "sample9",
    .init = init,
    .min_width = 1600,
    .min_height = 990
};

static ui_point_t em;
static int32_t panel_border;
static int32_t frame_border;

static image_t image;
static uint32_t pixels[1024][1024];

static double zoom = 0.5;
static double sx = 0.25; // [0..1]
static double sy = 0.25; // [0..1]

static struct { double x; double y; } stack[52];
static int top = 1; // because it is already zoomed in once above

static uic_slider_t zoomer;

#define glyph_onna        "\xE2\xBC\xA5" // Kanji Onna "Female"
#define glyph_two_squares "\xE2\xA7\x89" // "Two Joined Squares"
#define glyph_left        "\xE2\x86\x90" // "ShortLeftArrow"
#define glyph_up          "\xE2\x86\x91" // "ShortUpArrow"
#define glyph_right       "\xE2\x86\x92" // "ShortRightArrow"
#define glyph_down        "\xE2\x86\x93" // "ShortDownArrow"

uic_text(text_single_line, "Mandelbrot Explorer");

uic_text(toast_filename, "filename placeholder");

uic_multiline(text_multiline, 19.0, "Click inside or +/- to zoom;\n"
    "right mouse click to zoom out;\nuse "
    "touchpad or keyboard " glyph_left glyph_up glyph_down glyph_right
    " to pan");

uic_multiline(about, 34.56,
    "\nClick inside Mandelbrot Julia Set fractal to zoom in into interesting "
    "areas. Right mouse click to zoom out.\n"
    "Use Win + Shift + S to take a screenshot of something "
    "beautiful that caught your eye."
    "\n\n"
    "This sample also a showcase of controls like checkbox, message box, "
    "tooltips, clipboard copy as well as full sreen handling, open "
    "file dialog and on-the-fly locale swticing for simple and possibly "
    "incorrect Simplified Chinese localization."
    "\n\n"
    "Press ESC or click the \xC3\x97 button in right top corner "
    "to dismiss this message or just wait - it will disappear by "
    "itself in 10 seconds.\n");

uic_messagebox(messagebox,
    "\"Pneumonoultramicroscopicsilicovolcanoconiosis\"\n"
    "is it the longest English language word or not?", {
    traceln("option=%d", option); // -1 or index of { "&Yes", "&No" }
}, "&Yes", "&No");

static const char* filter[] = {
    "All Files", "*",
    "Image Files", ".png;.jpg",
    "Text Files", ".txt;.doc;.ini",
    "Executables", ".exe"
};

uic_button(open_file, "&Open", 7.5, {
    const char* fn = app.open_filename(
        app.known_folder(known_folder_home),
        filter, countof(filter)); //  all files filer: null, 0
    if (fn[0] != 0) {
        strprintf(toast_filename.ui.text, "%s", fn);
        traceln("%s", fn);
        app.show_toast(&toast_filename.ui, 2.0);
    }
});

uic_button(button_full_screen, glyph_two_squares, 1, {
    b->ui.pressed = !b->ui.pressed;
    app.full_screen(b->ui.pressed);
    if (b->ui.pressed) { app.toast(2.75, "Press ESC to exit full screen"); }
});

uic_button(button_locale, glyph_onna "A", 1, {
    b->ui.pressed = !b->ui.pressed;
    app.set_locale(b->ui.pressed ? "zh-CN" : "en-US");
    app.layout(); // because center panel layout changed
});

uic_button(button_about, "&About", 7.5, {
    app.show_toast(&about.ui, 10.0);
});

uic_button(button_message_box, "&Message Box", 7.5, {
    app.show_toast(&messagebox.ui, 0);
});

// uic_checkbox label can include "___" for "ON ": "OFF" state
uic_checkbox(scroll, "Scroll &Direction:", 0, {});

uic_container(panel_top, null, null);
uic_container(panel_bottom, null, null);
uic_container(panel_center, null, null);
uic_container(panel_right, null,
    &button_locale.ui,
    &button_full_screen.ui,
    &zoomer.ui,
    &scroll.ui,
    &open_file.ui,
    &button_about.ui,
    &button_message_box.ui,
    &text_single_line.ui,
    &text_multiline.ui,
    null
);

static void panel_paint(uic_t* ui) {
    gdi.push(ui->x, ui->y);
    gdi.set_clip(ui->x, ui->y, ui->w, ui->h);
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.dkgray1);
    gdi.fill(ui->x, ui->y, ui->w, ui->h);
    pen_t p = gdi.create_pen(colors.dkgray4, panel_border);
    gdi.set_pen(p);
    gdi.move_to(ui->x, ui->y);
    if (ui == &panel_right) {
        gdi.line(ui->x + ui->w, ui->y);
        gdi.line(ui->x + ui->w, ui->y + ui->h);
        gdi.line(ui->x, ui->y + ui->h);
        gdi.line(ui->x, ui->y);
    } else if (ui == &panel_top || ui == &panel_bottom) {
        gdi.line(ui->x, ui->y + ui->h);
        gdi.line(ui->x + ui->w, ui->y + ui->h);
        gdi.move_to(ui->x + ui->w, ui->y);
        gdi.line(ui->x, ui->y);
    } else {
        assert(ui == &panel_center);
        gdi.line(ui->x, ui->y + ui->h);
    }
    int32_t x = ui->x + panel_border + max(1, em.x / 8);
    int32_t y = ui->y + panel_border + max(1, em.y / 4);
    pen_t s = gdi.set_colored_pen(ui->color);
    gdi.set_brush(gdi.brush_hollow);
    gdi.rounded(x, y, em.x * 12, em.y, max(1, em.y / 4), max(1, em.y / 4));
    gdi.set_pen(s);
    color_t color = gdi.set_text_color(ui->color);
    gdi.x = ui->x + panel_border + max(1, em.x / 2);
    gdi.y = ui->y + panel_border + max(1, em.y / 4);
    gdi.text("%d,%d %dx%d %s", ui->x, ui->y, ui->w, ui->h, ui->text);
    gdi.set_text_color(color);
    gdi.set_clip(0, 0, 0, 0);
    gdi.delete_pen(p);
    gdi.pop();
}

static void right_layout(uic_t* ui) {
    if ( ui->children != null) {
        int x = ui->x + em.x;
        int y = ui->y + em.y * 2;
        for (uic_t** it = ui->children; *it != null; it++) {
            uic_t* ch = *it;
            ch->x = x;
            ch->y = y;
            y += ch->h + max(1, em.y / 2);
        }
    }
}

static void text_after(uic_t* ui, const char* format, ...) {
    gdi.x = ui->x + ui->w + ui->em.x;
    gdi.y = ui->y;
    va_list va;
    va_start(va, format);
    gdi.vtextln(format, va);
    va_end(va);
}

static void right_paint(uic_t* ui) {
    panel_paint(ui);
    gdi.push(ui->x, ui->y);
    gdi.set_clip(ui->x, ui->y, ui->w, ui->h);
    gdi.x = button_locale.ui.x + button_locale.ui.w + em.x;
    gdi.y = button_locale.ui.y;
    gdi.println("&Locale %s", button_locale.ui.pressed ? "zh-CN" : "en-US");
    gdi.x = button_full_screen.ui.x + button_full_screen.ui.w + em.x;
    gdi.y = button_full_screen.ui.y;
    gdi.println(app.is_full_screen ? app.nls("Restore from &Full Screen") :
        app.nls("&Full Screen"));
    gdi.x = text_multiline.ui.x;
    gdi.y = text_multiline.ui.y + text_multiline.ui.h + max(1, em.y / 4);
    gdi.textln("Proportional");
    gdi.println("Monospaced");
    font_t font = gdi.set_font(app.fonts.H1);
    gdi.textln("H1 %s", app.nls("Header"));
    gdi.set_font(app.fonts.H2); gdi.textln("H2 %s", app.nls("Header"));
    gdi.set_font(app.fonts.H3); gdi.textln("H3 %s", app.nls("Header"));
    gdi.set_font(font);
    gdi.println("%s %dx%d", app.nls("Client area"), app.crc.w, app.crc.h);
    gdi.println("%s %dx%d", app.nls("Window"), app.wrc.w, app.wrc.h);
    gdi.println("%s %dx%d", app.nls("Monitor"), app.mrc.w, app.mrc.h);
    gdi.println("%s %d %d", app.nls("Left Top"), app.wrc.x, app.wrc.y);
    gdi.println("%s %d %d", app.nls("Mouse"), app.mouse.x, app.mouse.y);
    gdi.println("%d x paint()", app.paint_count);
    gdi.println("%.1fms (max %.1f avg %.1f)", app.paint_time * 1000.0,
        app.paint_max * 1000.0, app.paint_avg * 1000.0);
    text_after(&zoomer.ui, "%.16f", zoom);
    text_after(&scroll.ui, "%s", scroll.ui.pressed ?
        app.nls("Natural") : app.nls("Reverse"));
    gdi.set_clip(0, 0, 0, 0);
    gdi.pop();
}

static void center_paint(uic_t* ui) {
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(ui->x, ui->y, ui->w, ui->h);
    int x = (ui->w - image.w) / 2;
    int y = (ui->h - image.h) / 2;
//  gdi.alpha_blend(ui->x + x, ui->y + y, image.w, image.h, &image, 0.8);
    HDC canvas = GetDC((HWND)app.window);
    fatal_if_null(canvas);
    HDC src = CreateCompatibleDC(canvas); fatal_if_null(src);
    HDC dst = CreateCompatibleDC(canvas); fatal_if_null(dst);
    HBITMAP bitmap = CreateCompatibleBitmap(canvas, image.w, image.h);
//  HBITMAP bitmap = CreateBitmap(image.w, image.h, 1, 32, null);
    fatal_if_null(bitmap);
    HBITMAP s = SelectBitmap(src, image.bitmap); fatal_if_null(s);
    HBITMAP d = SelectBitmap(dst, bitmap);     fatal_if_null(d);
    POINT pt = { 0 };
    fatal_if_false(SetBrushOrgEx(dst, 0, 0, &pt));
    fatal_if_false(StretchBlt(dst, 0, 0, image.w, image.h, src, 0, 0,
        image.w, image.h, SRCCOPY));
    ///
    fatal_if_false(StretchBlt((HDC)app.canvas, ui->x + x, ui->y + y,
        image.w, image.h, dst, 0, 0,
        image.w, image.h, SRCCOPY));
    fatal_if_null(SelectBitmap(dst, d));
    fatal_if_null(SelectBitmap(src, s));
    fatal_if_false(DeleteBitmap(bitmap));
    fatal_if_false(DeleteDC(dst));
    fatal_if_false(DeleteDC(src));
    fatal_if_false(ReleaseDC((HWND)app.window, canvas));

}

static void measure(uic_t* ui) {
    ui_point_t em_mono = gdi.get_em(app.fonts.mono);
    em = gdi.get_em(app.fonts.regular);
    ui->em = em;
    panel_border = max(1, em_mono.y / 4);
    frame_border = max(1, em_mono.y / 8);
    assert(panel_border > 0 && frame_border > 0);
    const int32_t w = app.width;
    const int32_t h = app.height;
    // measure ui elements
    panel_top.w = (int32_t)(0.70 * w);
    panel_top.h = em.y * 2;
    panel_bottom.w = panel_top.w;
    panel_bottom.h = em.y * 2;
    panel_right.w = w - panel_bottom.w;
    panel_right.h = h;
    panel_center.w = panel_bottom.w;
    panel_center.h = h - panel_bottom.h - panel_top.h;
}

static void layout(uic_t* ui) {
    assert(ui->em.x > 0 && ui->em.y > 0); (void)ui;
    const int32_t h = app.height;
    panel_top.x = 0;
    panel_top.y = 0;
    panel_bottom.x = 0;
    panel_bottom.y = h - panel_bottom.h;
    panel_right.x = panel_bottom.w;
    panel_right.y = 0;
    panel_center.x = 0;
    panel_center.y = panel_top.h;
}

static void refresh();

static void zoom_out() {
    assert(top > 0);
    top--;
    sx = stack[top].x;
    sy = stack[top].y;
    zoom *= 2;
}

static void zoom_in(int x, int y) {
    assert(top < countof(stack));
    stack[top].x = sx;
    stack[top].y = sy;
    top++;
    zoom /= 2;
    sx += zoom * x / image.w;
    sy += zoom * y / image.h;
}

static void mouse(uic_t* ui, int32_t m, int32_t flags) {
    (void)ui; (void)m; (void)flags;
    int x = app.mouse.x - (panel_center.w - image.w) / 2 - panel_center.x;
    int y = app.mouse.y - (panel_center.h - image.h) / 2 - panel_center.y;
    if (0 <= x && x < image.w && 0 <= y && y < image.h) {
        if (m == messages.right_button_down) {
            if (zoom < 1) { zoom_out(); refresh(); }
        } else if (m == messages.left_button_down) {
            if (top < countof(stack)) { zoom_in(x, y); refresh(); }
        }
    }
    app.redraw(); // always to update Mouse: x, y info
}

static void zoomer_callback(uic_slider_t* slider) {
    double z = 1;
    for (int i = 0; i < slider->value; i++) { z /= 2; }
    while (zoom > z) { zoom_in(image.w / 2, image.h / 2); }
    while (zoom < z) { zoom_out(); }
    refresh();
}

static void mousewheel(uic_t* unused, int32_t dx, int32_t dy) {
    (void)unused;
    if (!scroll.ui.pressed) { dy = -dy; }
    if (!scroll.ui.pressed) { dx = -dx; }
    sx = sx + zoom * dx / image.w;
    sy = sy + zoom * dy / image.h;
    refresh();
}

static void keyboard(uic_t* ui, int32_t ch) {
    if (ch == 'q' || ch == 'Q') {
        app.close();
    } else if (ch == 033 && app.is_full_screen) {
        button_full_screen_callback(&button_full_screen);
    } else if (ch == '+' || ch == '=') {
        zoom /= 2; refresh();
    } else if (ch == '-' || ch == '_') {
        zoom = min(zoom * 2, 1.0); refresh();
    } else if (ch == virtual_keys.up) {
        mousewheel(ui, 0, +image.h / 8);
    } else if (ch == virtual_keys.down) {
        mousewheel(ui, 0, -image.h / 8);
    } else if (ch == virtual_keys.left || ch == '<' || ch == ',') {
        mousewheel(ui, +image.w / 8, 0);
    } else if (ch == virtual_keys.right || ch == '>' || ch == '.') {
        mousewheel(ui, -image.w / 8, 0);
    } else if (ch == 3) { // Ctrl+C
        clipboard.copy_bitmap(&image);
    }
}

static void init_panel(uic_t* panel, const char* text, color_t color,
        void (*paint)(uic_t*)) {
    strprintf(panel->text, "%s", text);
    panel->color = color;
    panel->paint = paint;
}

static void openned() {
    int n = countof(pixels);
    static_assert(sizeof(pixels[0][0]) == 4, "4 bytes per pixel");
    static_assert(countof(pixels) == countof(pixels[0]), "square");
    if (app.mrc.h < app.min_height) {
        n = n / 2;
    } else {
        app.min_width = 1800;
        app.min_height = 1300l;
    }
    gdi.image_init(&image, n, n, (int)sizeof(pixels[0][0]), (byte*)pixels);
    init_panel(&panel_top, "top", colors.orange, panel_paint);
    init_panel(&panel_center, "center", colors.off_white, center_paint);
    init_panel(&panel_bottom, "bottom", colors.tone_blue, panel_paint);
    init_panel(&panel_right, "right", colors.tone_green, right_paint);
    panel_right.layout = right_layout;
    text_single_line.highlight = true;
    text_multiline.highlight = true;
    text_multiline.hovered = true;
    strprintf(text_multiline.ui.tip, "%s",
        "Ctrl+C or Right Mouse click to copy text to clipboard");
    toast_filename.ui.font = &app.fonts.H1;
    about.ui.font = &app.fonts.H3;
    button_locale.ui.shortcut = 'l';
    button_full_screen.ui.shortcut = 'f';
    uic_slider_init(&zoomer, "Zoom: 1 / (2^%d)", 7.0, 0, countof(stack) - 1,
        zoomer_callback);
    refresh();
}

static void init() {
    app.title = TITLE;
    app.ui->measure = measure;
    app.ui->layout = layout;
    app.ui->keyboard = keyboard;
    app.ui->key_down = keyboard; // virtual_keys
    app.ui->mousewheel = mousewheel;
    app.openned = openned;
    static uic_t* root_children[] = { &panel_top, &panel_center,
        &panel_right, &panel_bottom, null };
    app.ui->children = root_children;
    panel_center.mouse = mouse;
}

static double scale0to1(int v, int range, double sh, double zm) {
    return sh + zm * v / range;
}

static double into(double v, double lo, double hi) {
    assert(0 <= v && v <= 1);
    return v * (hi - lo) + lo;
}

static void mandelbrot(image_t* im) {
    for (int r = 0; r < im->h; r++) {
        double y0 = into(scale0to1(r, im->h, sy, zoom), -1.12, 1.12);
        for (int c = 0; c < im->w; c++) {
            double x0 = into(scale0to1(c, im->w, sx, zoom), -2.00, 0.47);
            double x = 0;
            double y = 0;
            int iteration = 0;
            enum { max_iteration = 100 };
            while (x* x + y * y <= 2 * 2 && iteration < max_iteration) {
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
}

static void refresh() {
    if (sx < 0) { sx = 0; }
    if (sx > 1 - zoom) { sx = 1 - zoom; }
    if (sy < 0) { sy = 0; }
    if (sy > 1 - zoom) { sy = 1 - zoom; }
    if (zoom == 1) { sx = 0; sy = 0; }
    zoomer.value = 0;
    double z = 1;
    while (z != zoom) { zoomer.value++; z /= 2; }
    zoomer.value = min(zoomer.value, zoomer.vmax);
    mandelbrot(&image);
    app.redraw();
}

end_c
