/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"

begin_c

const char* title = "Sample5";

static uic_edit_t edit;

uic_button(full_screen, "Full Screen", 7.5, {
    app.full_screen(!app.is_full_screen);
});

uic_button(quit, "Quit", 7.5, {
    app.close();
});

uic_button(fuzz, "Fuzz", 7.5, {
    edit.fuzz(&edit);
    fuzz->ui.pressed = edit.fuzzer != null;
});

uic_checkbox(wb, "Word Break", 7.5, { // checkbox?
    traceln("Word Break");
});

uic_checkbox(mono, "Mono", 7.5, {
    traceln("Mono");
    edit.set_font(&edit, &app.fonts.mono);
});

uic_checkbox(sl, "Single Line", 7.5, {
    traceln("Single Line");
});

uic_multiline(text, 0.0, "...");

uic_container(buttons, null,
    &full_screen.ui, &quit.ui, &fuzz.ui, &wb.ui, &mono.ui, &sl.ui);
uic_container(left, null, &edit.ui);
uic_container(right, null, &buttons);
uic_container(bottom, null, &text.ui);

static void measure(uic_t* ui) {
    bottom.w = ui->w;
    bottom.h = max(ui->h / 10, ui->em.y * 2);  // 10% bottom
    bottom.y = ui->h - bottom.h;
    text.ui.w = bottom.w;
    text.ui.h = bottom.h;
    assert(full_screen.ui.w > 0 && full_screen.ui.w == quit.ui.w);
    buttons.w = full_screen.ui.w;
    buttons.h = ui->h - text.ui.h -  ui->em.y;
    right.w = buttons.w + ui->em.x * 2;
    right.h = buttons.h;
    left.w = ui->w - right.w;
    left.h = ui->h - bottom.h;
    edit.ui.w = left.w - ui->em.x;
    edit.ui.h = left.h - ui->em.y;
}

static void layout(uic_t* ui) {
    buttons.y = ui->em.y;
    buttons.x = left.w + ui->em.x;
    text.ui.y = ui->h - text.ui.h;
    layouts.vertical(&buttons, buttons.x, buttons.y, 10);
    edit.ui.x = left.w - edit.ui.w;
    edit.ui.y = left.h - edit.ui.h;
}

static void paint(uic_t* ui) {
    // all UIC are transparent and expect parent to paint background
    // UI control paint is always called with a hollow brush
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
}

static void open_file(const char* pathname) {
    char* file = null;
    int64_t bytes = 0;
    if (crt.memmap_read(pathname, &file, &bytes) == 0) {
        if (0 < bytes && bytes <= INT_MAX) {
            edit.select_all(&edit);
            edit.paste(&edit, file, (int32_t)bytes);
            uic_edit_pg_t start = { .pn = 0, .gp = 0 };
            edit.move(&edit, start);
        }
        crt.memunmap(file, bytes);
    } else {
        app.toast(5.3, "\nFailed to open file \"%s\".\n%s\n",
                  pathname, crt.error(crt.err()));
    }
}

static void openned(void) {
    app.focus = &edit.ui;
    #if 1 // large font:
        static font_t mono_H3;
        mono_H3 = gdi.font(app.fonts.mono, gdi.get_em(app.fonts.H3).y, -1);
        edit.ui.font = &mono_H3;
    #elif 0
        edit.ui.font = &app.fonts.mono;
    #else
        edit.ui.font = &app.fonts.regular;
    #endif
    if (app.argc > 1) {
        open_file(app.argv[1]);
    }
}

static void every_100ms(uic_t* unused(ui)) {
    bool fuzzing = edit.fuzzer != null;
    if (fuzz.ui.pressed != fuzzing) {
        fuzz.ui.pressed = fuzzing;
        fuzz.ui.invalidate(&fuzz.ui);
    }
    sprintf(text.ui.text, "%d:%d %d:%d %dx%d\n"
            "scroll %03d:%03d",
        edit.selection[0].pn, edit.selection[0].gp,
        edit.selection[1].pn, edit.selection[1].gp,
        edit.ui.w, edit.ui.h,
        edit.scroll.pn, edit.scroll.rn);
    text.ui.invalidate(&text.ui);
}

static void init(void) {
    app.title = title;
    app.ui->measure = measure;
    app.ui->layout = layout;
    app.ui->paint = paint;
    app.ui->every_100ms = every_100ms;
    static uic_t* children[] = { &left, &right, &bottom, null };
    app.ui->children = children;
    text.ui.font = &app.fonts.mono;
    strprintf(fuzz.ui.tip, "Ctrl+Shift+Alt+F5 to start and F5 to stop Fuzzing");
    uic_edit_init(&edit);
}

app_t app = {
    .class_name = "sample5",
    .init = init,
    .openned = openned,
    .wmin = 4.0f, // 4x2 inches
    .hmin = 2.0f
};

end_c
