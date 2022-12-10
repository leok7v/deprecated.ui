/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"

begin_c

const char* title = "Sample5";

static uic_edit_t edit;

uic_button(button0, "Full Screen", 7.5, {
    app.full_screen(!app.is_full_screen);
});

uic_button(button1, "Quit", 7.5, {
    app.close();
});

uic_button(button2, "Word Break", 7.5, { // checkbox?
    traceln("Word Break");
});

uic_button(button3, "Mono", 7.5, {
    traceln("Mono");
});

uic_button(button4, "Single Line", 7.5, {
    traceln("Single Line");
});

uic_text(text, "...");

static_uic_container(buttons, null,
    &button0.ui, &button1.ui, &button2.ui, &button3.ui, &button4.ui);
static_uic_container(left, null, &edit.ui);
static_uic_container(right, null, &buttons);
static_uic_container(bottom, null, &text.ui);

static void measure(uic_t* ui) {
    bottom.w = ui->w;
    bottom.h = ui->h / 10;  // 10% bottom
    bottom.y = ui->h - bottom.h;
    text.ui.w = bottom.w;
    text.ui.h = bottom.h;
    assert(button1.ui.w > 0 && button1.ui.w == button2.ui.w);
    buttons.w = button1.ui.w;
    buttons.h = ui->h - text.ui.h -  ui->em.y;
    right.w = buttons.w + ui->em.x * 2;
    right.h = buttons.h;
    left.w = ui->w - right.w;
    left.h = ui->h - bottom.h;
    edit.ui.h = left.h;
    edit.ui.w = left.w;
}

static void layout(uic_t* ui) {
    buttons.y = ui->em.y;
    buttons.x = left.w + ui->em.x;
    text.ui.y = ui->h - text.ui.h;
    layouts.vertical(&buttons, buttons.x, buttons.y, 10);
}

static void paint(uic_t* ui) {
    // all UIC are transparent and expect parent to paint background
    // UI control paint is always called with a hollow brush
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
    sprintf(text.ui.text, "%d:%d %d:%d",
        edit.selection.start.line, edit.selection.start.column,
        edit.selection.end.line, edit.selection.end.column);
}

static void init() {
    app.title = title;
    app.ui->measure = measure;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_t* children[] = { &left, &right, &bottom, null };
    app.ui->children = children;
    uic_edit_init(&edit);
    // xxx
//  edit.ui.font = &app.fonts.mono;
    edit.top.line = 0;
}

static void openned() {
    app.focus = &edit.ui;
//  mono_H3 = gdi.font(app.fonts.mono, gdi.get_em(app.fonts.H3).y);
//  edit.ui.font = &mono_H3;
}

app_t app = {
    .class_name = "sample1",
    .init = init,
    .openned = openned,
    .min_width = 400,
    .min_height = 200
};

end_c
