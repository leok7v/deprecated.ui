/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"

begin_c

const char* title = "Sample5";

static void layout(uic_t* ui) {
    ui->children[0]->x = 0;
    ui->children[0]->y = 0;
    ui->children[0]->w = ui->w;
    ui->children[0]->h = ui->h;
}

static void paint(uic_t* ui) {
    // all UIC are transparent and expect parent to paint background
    // UI control paint is always called with a hollow brush
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
}

static uic_edit_t edit;

static font_t mono_H3;

static void init() {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_t* children[] = { &edit.ui, null };
    app.ui->children = children;
    uic_edit_init(&edit);
    // xxx
    edit.top.line = 0;
}

static void openned() {
    mono_H3 = gdi.font(app.fonts.mono, gdi.get_em(app.fonts.H3).y);
    edit.ui.font = &mono_H3;
}

app_t app = {
    .class_name = "sample1",
    .init = init,
    .openned = openned,
    .min_width = 400,
    .min_height = 200
};

end_c
