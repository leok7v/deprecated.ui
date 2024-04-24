/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "ui/win32.h"

begin_c

#define TITLE "Sample9"

static void init(void);

app_t app = {
    .class_name = "sample9",
    .init = init,
    .wmin = 11.0f, // 11x7 inches
    .hmin =  7.0f
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

static ui_slider_t zoomer;

#define glyph_onna        "\xE2\xBC\xA5" // Kanji Onna "Female"
#define glyph_two_squares "\xE2\xA7\x89" // "Two Joined Squares"
#define glyph_left        "\xE2\x86\x90" // "ShortLeftArrow"
#define glyph_up          "\xE2\x86\x91" // "ShortUpArrow"
#define glyph_right       "\xE2\x86\x92" // "ShortRightArrow"
#define glyph_down        "\xE2\x86\x93" // "ShortDownArrow"

ui_text(text_single_line, "Mandelbrot Explorer");

ui_text(toast_filename, "filename placeholder");

ui_multiline(text_multiline, 19.0, "Click inside or +/- to zoom;\n"
    "right mouse click to zoom out;\nuse "
    "touchpad or keyboard " glyph_left glyph_up glyph_down glyph_right
    " to pan");

ui_multiline(about, 34.56,
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

ui_messagebox(messagebox,
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

ui_button(open_file, "&Open", 7.5, {
    const char* fn = app.open_filename(
        app.known_folder(ui.folder.home),
        filter, countof(filter)); //  all files filer: null, 0
    if (fn[0] != 0) {
        strprintf(toast_filename.view.text, "%s", fn);
        traceln("%s", fn);
        app.show_toast(&toast_filename.view, 2.0);
    }
});

ui_button(button_full_screen, glyph_two_squares, 1, {
    button_full_screen->view.pressed = !button_full_screen->view.pressed;
    app.full_screen(button_full_screen->view.pressed);
    if (button_full_screen->view.pressed) {
        app.toast(2.75, "Press ESC to exit full screen");
    }
});

ui_button(button_locale, glyph_onna "A", 1, {
    button_locale->view.pressed = !button_locale->view.pressed;
    app.set_locale(button_locale->view.pressed ? "zh-CN" : "en-US");
    app.layout(); // because center panel layout changed
});

ui_button(button_about, "&About", 7.5, {
    app.show_toast(&about.view, 10.0);
});

ui_button(button_message_box, "&Message Box", 7.5, {
    app.show_toast(&messagebox.view, 0);
});

// ui_checkbox label can include "___" for "ON ": "OFF" state
ui_checkbox(scroll, "Scroll &Direction:", 0, {});

ui_container(panel_top, null, null);
ui_container(panel_bottom, null, null);
ui_container(panel_center, null, null);
ui_container(panel_right, null,
    &button_locale.view,
    &button_full_screen.view,
    &zoomer.view,
    &scroll.view,
    &open_file.view,
    &button_about.view,
    &button_message_box.view,
    &text_single_line.view,
    &text_multiline.view,
    null
);

static void panel_paint(ui_view_t* view) {
    gdi.push(view->x, view->y);
    gdi.set_clip(view->x, view->y, view->w, view->h);
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.dkgray1);
    gdi.fill(view->x, view->y, view->w, view->h);
    ui_pen_t p = gdi.create_pen(colors.dkgray4, panel_border);
    gdi.set_pen(p);
    gdi.move_to(view->x, view->y);
    if (view == &panel_right) {
        gdi.line(view->x + view->w, view->y);
        gdi.line(view->x + view->w, view->y + view->h);
        gdi.line(view->x, view->y + view->h);
        gdi.line(view->x, view->y);
    } else if (view == &panel_top || view == &panel_bottom) {
        gdi.line(view->x, view->y + view->h);
        gdi.line(view->x + view->w, view->y + view->h);
        gdi.move_to(view->x + view->w, view->y);
        gdi.line(view->x, view->y);
    } else {
        assert(view == &panel_center);
        gdi.line(view->x, view->y + view->h);
    }
    int32_t x = view->x + panel_border + max(1, em.x / 8);
    int32_t y = view->y + panel_border + max(1, em.y / 4);
    ui_pen_t s = gdi.set_colored_pen(view->color);
    gdi.set_brush(gdi.brush_hollow);
    gdi.rounded(x, y, em.x * 12, em.y, max(1, em.y / 4), max(1, em.y / 4));
    gdi.set_pen(s);
    ui_color_t color = gdi.set_text_color(view->color);
    gdi.x = view->x + panel_border + max(1, em.x / 2);
    gdi.y = view->y + panel_border + max(1, em.y / 4);
    gdi.text("%d,%d %dx%d %s", view->x, view->y, view->w, view->h, view->text);
    gdi.set_text_color(color);
    gdi.set_clip(0, 0, 0, 0);
    gdi.delete_pen(p);
    gdi.pop();
}

static void right_layout(ui_view_t* view) {
    if ( view->children != null) {
        int x = view->x + em.x;
        int y = view->y + em.y * 2;
        for (ui_view_t** it = view->children; *it != null; it++) {
            ui_view_t* ch = *it;
            ch->x = x;
            ch->y = y;
            y += ch->h + max(1, em.y / 2);
        }
    }
}

static void text_after(ui_view_t* view, const char* format, ...) {
    gdi.x = view->x + view->w + view->em.x;
    gdi.y = view->y;
    va_list va;
    va_start(va, format);
    gdi.vtextln(format, va);
    va_end(va);
}

static void right_paint(ui_view_t* view) {
    panel_paint(view);
    gdi.push(view->x, view->y);
    gdi.set_clip(view->x, view->y, view->w, view->h);
    gdi.x = button_locale.view.x + button_locale.view.w + em.x;
    gdi.y = button_locale.view.y;
    gdi.println("&Locale %s", button_locale.view.pressed ? "zh-CN" : "en-US");
    gdi.x = button_full_screen.view.x + button_full_screen.view.w + em.x;
    gdi.y = button_full_screen.view.y;
    gdi.println(app.is_full_screen ? app.nls("Restore from &Full Screen") :
        app.nls("&Full Screen"));
    gdi.x = text_multiline.view.x;
    gdi.y = text_multiline.view.y + text_multiline.view.h + max(1, em.y / 4);
    gdi.textln("Proportional");
    gdi.println("Monospaced");
    ui_font_t font = gdi.set_font(app.fonts.H1);
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
    text_after(&zoomer.view, "%.16f", zoom);
    text_after(&scroll.view, "%s", scroll.view.pressed ?
        app.nls("Natural") : app.nls("Reverse"));
    gdi.set_clip(0, 0, 0, 0);
    gdi.pop();
}

static void center_paint(ui_view_t* view) {
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(view->x, view->y, view->w, view->h);
    int x = (view->w - image.w) / 2;
    int y = (view->h - image.h) / 2;
//  gdi.alpha_blend(view->x + x, view->y + y, image.w, image.h, &image, 0.8);
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
    fatal_if_false(StretchBlt((HDC)app.canvas, view->x + x, view->y + y,
        image.w, image.h, dst, 0, 0,
        image.w, image.h, SRCCOPY));
    fatal_if_null(SelectBitmap(dst, d));
    fatal_if_null(SelectBitmap(src, s));
    fatal_if_false(DeleteBitmap(bitmap));
    fatal_if_false(DeleteDC(dst));
    fatal_if_false(DeleteDC(src));
    fatal_if_false(ReleaseDC((HWND)app.window, canvas));

}

static void measure(ui_view_t* view) {
    ui_point_t em_mono = gdi.get_em(app.fonts.mono);
    em = gdi.get_em(app.fonts.regular);
    view->em = em;
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

static void layout(ui_view_t* unused(view)) {
    assert(view->em.x > 0 && view->em.y > 0);
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

static void refresh(void);

static void zoom_out(void) {
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

static void mouse(ui_view_t* unused(view), int32_t m, int32_t unused(flags)) {
    int x = app.mouse.x - (panel_center.w - image.w) / 2 - panel_center.x;
    int y = app.mouse.y - (panel_center.h - image.h) / 2 - panel_center.y;
    if (0 <= x && x < image.w && 0 <= y && y < image.h) {
        if (m == ui.message.right_button_pressed) {
            if (zoom < 1) { zoom_out(); refresh(); }
        } else if (m == ui.message.left_button_pressed) {
            if (top < countof(stack)) { zoom_in(x, y); refresh(); }
        }
    }
    app.redraw(); // always to update Mouse: x, y info
}

static void zoomer_callback(ui_slider_t* slider) {
    double z = 1;
    for (int i = 0; i < slider->value; i++) { z /= 2; }
    while (zoom > z) { zoom_in(image.w / 2, image.h / 2); }
    while (zoom < z) { zoom_out(); }
    refresh();
}

static void mousewheel(ui_view_t* unused, int32_t dx, int32_t dy) {
    (void)unused;
    if (!scroll.view.pressed) { dy = -dy; }
    if (!scroll.view.pressed) { dx = -dx; }
    sx = sx + zoom * dx / image.w;
    sy = sy + zoom * dy / image.h;
    refresh();
}

static void character(ui_view_t* view, const char* utf8) {
    char ch = utf8[0];
    if (ch == 'q' || ch == 'Q') {
        app.close();
    } else if (ch == 033 && app.is_full_screen) {
        button_full_screen_callback(&button_full_screen);
    } else if (ch == '+' || ch == '=') {
        zoom /= 2; refresh();
    } else if (ch == '-' || ch == '_') {
        zoom = min(zoom * 2, 1.0); refresh();
    } else if (ch == '<' || ch == ',') {
        mousewheel(view, +image.w / 8, 0);
    } else if (ch == '>' || ch == '.') {
        mousewheel(view, -image.w / 8, 0);
    } else if (ch == 3) { // Ctrl+C
        clipboard.copy_bitmap(&image);
    }
}

static void keyboard(ui_view_t* view, int32_t vk) {
    if (vk == ui.key.up) {
        mousewheel(view, 0, +image.h / 8);
    } else if (vk == ui.key.down) {
        mousewheel(view, 0, -image.h / 8);
    } else if (vk == ui.key.left) {
        mousewheel(view, +image.w / 8, 0);
    } else if (vk == ui.key.right) {
        mousewheel(view, -image.w / 8, 0);
    }
}

static void init_panel(ui_view_t* panel, const char* text, ui_color_t color,
        void (*paint)(ui_view_t*)) {
    strprintf(panel->text, "%s", text);
    panel->color = color;
    panel->paint = paint;
}

static void opened(void) {
    int n = countof(pixels);
    static_assert(sizeof(pixels[0][0]) == 4, "4 bytes per pixel");
    static_assert(countof(pixels) == countof(pixels[0]), "square");
    gdi.image_init(&image, n, n, (int)sizeof(pixels[0][0]), (uint8_t*)pixels);
    init_panel(&panel_top, "top", colors.orange, panel_paint);
    init_panel(&panel_center, "center", colors.off_white, center_paint);
    init_panel(&panel_bottom, "bottom", colors.tone_blue, panel_paint);
    init_panel(&panel_right, "right", colors.tone_green, right_paint);
    panel_right.layout = right_layout;
    text_single_line.highlight = true;
    text_multiline.highlight = true;
    text_multiline.hovered = true;
    strprintf(text_multiline.view.tip, "%s",
        "Ctrl+C or Right Mouse click to copy text to clipboard");
    toast_filename.view.font = &app.fonts.H1;
    about.view.font = &app.fonts.H3;
    button_locale.view.shortcut = 'l';
    button_full_screen.view.shortcut = 'f';
    ui_slider_init(&zoomer, "Zoom: 1 / (2^%d)", 7.0, 0, countof(stack) - 1,
        zoomer_callback);
    strcopy(button_message_box.view.tip, "Show Yes/No message box");
    strcopy(button_about.view.tip, "Show About message box");
    refresh();
}

static void init(void) {
    app.title = TITLE;
    app.view->measure = measure;
    app.view->layout = layout;
    app.view->character   = character;
    app.view->key_pressed = keyboard; // virtual_keys
    app.view->mousewheel = mousewheel;
    app.opened = opened;
    static ui_view_t* root_children[] = { &panel_top, &panel_center,
        &panel_right, &panel_bottom, null };
    app.view->children = root_children;
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
            static ui_color_t palette[16] = {
                rgb( 66,  30,  15),  rgb( 25,   7,  26),
                rgb(  9,   1,  47),  rgb(  4,   4,  73),
                rgb(  0,   7, 100),  rgb( 12,  44, 138),
                rgb( 24,  82, 177),  rgb( 57, 125, 209),
                rgb(134, 181, 229),  rgb(211, 236, 248),
                rgb(241, 233, 191),  rgb(248, 201,  95),
                rgb(255, 170,   0),  rgb(204, 128,   0),
                rgb(153,  87,   0),  rgb(106,  52,   3)
            };
            ui_color_t color = palette[iteration % countof(palette)];
            uint8_t* px = &((uint8_t*)im->pixels)[r * im->w * 4 + c * 4];
            px[3] = 0xFF;
            px[0] = (color >> 16) & 0xFF;
            px[1] = (color >>  8) & 0xFF;
            px[2] = (color >>  0) & 0xFF;
        }
    }
}

static void refresh(void) {
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
