/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"
#include <Windows.h>

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
    fatal_if_false(CreateCaret((HWND)app.window, null, 2, gdi.get_em(app.fonts.H3).y));
    SetCaretBlinkTime(400);
    // Expected manifest.xml containing UTF-8 code page
    // for Translate message and WM_CHAR to deliver UTF-8 characters
    // see: https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
    assert(GetACP() == 65001, "GetACP()=%d", GetACP());
    // at the moment of writing there is no API call to inform Windows about process
    // prefered codepage except manifest.xml file in resource #1.
    // Absence of manifest.xml will result to ancient and useless ANSI 1252 codepage
}

app_t app = {
    .class_name = "sample1",
    .init = init,
    .openned = openned,
    .min_width = 400,
    .min_height = 200
};

end_c
