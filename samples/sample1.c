/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

begin_c

const char* title = "Sample1";

static void layout(view_t* view) {
    layouts.center(view);
}

static void paint(view_t* view) {
    // all UIC are transparent and expect parent to paint background
    // UI control paint is always called with a hollow brush
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, view->w, view->h);
}

static void init(void) {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_text(text, "Hello World!");
    static view_t* children[] = { &text.ui, null };
    app.ui->children = children;
}

app_t app = {
    .class_name = "sample1",
    .init = init,
    .wmin = 4.0f, // 4x2 inches
    .hmin = 2.0f
};

end_c
