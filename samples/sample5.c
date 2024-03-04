/* Copyright (c) Dmitry "Leo" Kuznetsov 2024 see LICENSE for details */
#include "quick.h"
#include "edit.h"

begin_c

static bool debug_layout; // = true;

const char* title = "Sample5";

static font_t font;

static uic_edit_t edit0;
static uic_edit_t edit1;
static uic_edit_t* edit[2] = { &edit0, &edit1 };

static int32_t focused(void) {
    // app.focus can point to a button, thus see which edit
    // control was focused last
    int32_t ix = -1;
    for (int32_t i = 0; i < countof(edit) && ix < 0; i++) {
        if (app.focus == &edit[i]->ui) { ix = i; }
        if (edit[i]->focused) { ix = i; }
    }
    return ix;
}

uic_button(full_screen, "&Full Screen", 7.5, {
    app.full_screen(!app.is_full_screen);
});

uic_button(quit, "&Quit", 7.5, { app.close(); });

uic_button(fuzz, "Fu&zz", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->fuzz(edit[ix]);
        fuzz->ui.pressed = edit[ix]->fuzzer != null;
    }
});

uic_checkbox(wb, "&Word Break", 7.5, { // checkbox?
    traceln("Word Break");
});

uic_checkbox(mono, "&Mono", 7.5, {
    int32_t ix = focused();
//  traceln("ix: %d %p %p %p", ix, app.focus, &edit0, &edit1);
    if (ix >= 0) {
        edit[ix]->set_font(edit[ix],
            mono->ui.pressed ? &app.fonts.mono : &font);
    } else {
        mono->ui.pressed = !mono->ui.pressed;
    }
});

uic_checkbox(sl, "&Single Line", 7.5, {
    traceln("Single Line");
});

uic_multiline(text, 0.0, "...");

uic_container(buttons, null,
    &full_screen.ui, &quit.ui, &fuzz.ui, &wb.ui, &mono.ui, &sl.ui);

uic_container(left, null, &edit0.ui, &edit1.ui);
uic_container(bottom, null, &text.ui);
uic_container(right, null, &buttons);

static void measure(uic_t* ui) {
    bottom.w = ui->w;
    bottom.h = max(ui->h / 10, ui->em.y * 2);  // 10% bottom
    text.ui.w = bottom.w;
    text.ui.h = bottom.h;
    buttons.w = 0;
    measurements.vertical(&buttons, ui->em.y);
    buttons.w += ui->em.x;
    right.w = buttons.w + ui->em.x / 2;
    right.h = ui->h - text.ui.h - ui->em.y;
    int32_t h = (ui->h - bottom.h - ui->em.y) / countof(edit);
    for (int32_t i = 0; i < countof(edit); i++) {
        edit[i]->ui.w = ui->w - right.w + ui->em.x;
        edit[i]->ui.h = h;
    }
    left.w = 0;
    measurements.vertical(&left, ui->em.y);
    left.w += ui->em.x;
    if (debug_layout) {
        traceln("%d,%d %dx%d", ui->x, ui->y, ui->w, ui->h);
        traceln("buttons %d,%d %dx%d", buttons.x, buttons.y, buttons.w, buttons.h);
        for (uic_t** c = buttons.children; c != null && *c != null; c++) {
            traceln("  %s %d,%d %dx%d", (*c)->text, (*c)->x, (*c)->y, (*c)->w, (*c)->h);
        }
        traceln("right %d,%d %dx%d", right.x, right.y, right.w, right.h);
        for (int32_t i = 0; i < countof(edit); i++) {
            traceln("[%d] %d,%d %dx%d", i, edit[i]->ui.x, edit[i]->ui.y,
                edit[i]->ui.w, edit[i]->ui.h);
        }
        traceln("left %d,%d %dx%d", left.x, left.y, left.w, left.h);
    }
}

static void layout(uic_t* ui) {
    left.x = ui->em.x / 2;
    left.y = ui->em.y / 2;
    layouts.vertical(&left, left.x, left.y, ui->em.y / 2);
    right.x = left.x + left.w;
    right.y = left.y + left.h;
    bottom.y  = ui->h - bottom.h;
    buttons.x = left.x + left.w;
    buttons.y = ui->em.y / 2;
    layouts.vertical(&buttons, buttons.x, buttons.y, ui->em.y / 2);
    text.ui.x = ui->em.x / 2;
    text.ui.y = ui->h - text.ui.h;
}

static void painted(void) {
    // because of blinking caret paint is called frequently
    int32_t ix = focused();
    if (ix >= 0) {
        bool fuzzing = edit[ix]->fuzzer != null;
        if (fuzz.ui.pressed != fuzzing) {
            fuzz.ui.pressed = fuzzing;
            fuzz.ui.invalidate(&fuzz.ui);
        }
        sprintf(text.ui.text, "%d:%d %d:%d %dx%d\n"
            "scroll %03d:%03d",
            edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
            edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
            edit[ix]->ui.w, edit[ix]->ui.h,
            edit[ix]->scroll.pn, edit[ix]->scroll.rn);
//      traceln("%d:%d %d:%d %dx%d scroll %03d:%03d",
//          edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
//          edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
//          edit[ix]->ui.w, edit[ix]->ui.h,
//          edit[ix]->scroll.pn, edit[ix]->scroll.rn);
    }
}

static void paint_frames(uic_t* ui) {
    for (uic_t** c = ui->children; c != null && *c != null; c++) {
        paint_frames(*c);
    }
    color_t fc[] = {
        colors.red, colors.green, colors.blue, colors.red,
        colors.yellow, colors.cyan, colors.magenta
    };
    static int32_t color;
    gdi.push(ui->x, ui->y + ui->h - ui->em.y);
    pen_t p = gdi.set_colored_pen(fc[color]);
    color_t c = gdi.set_text_color(fc[color]);
    brush_t b = gdi.set_brush(gdi.brush_hollow);
    gdi.rect(ui->x, ui->y, ui->w, ui->h);
    gdi.print("%s", ui->text);
    gdi.set_text_color(c);
    gdi.set_brush(b);
    gdi.set_pen(p);
    gdi.pop();
    color = (color + 1) % countof(fc);
}

static void null_paint(uic_t* ui) {
    for (uic_t** c = ui->children; c != null && *c != null; c++) {
        null_paint(*c);
    }
    if (ui != app.ui) {
        ui->paint = null;
    }
}

static void paint(uic_t * ui) {
    if (debug_layout) { null_paint(ui); }
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
    painted();
    if (debug_layout) { paint_frames(ui); }
}

static void open_file(const char* pathname) {
    char* file = null;
    int64_t bytes = 0;
    if (crt.memmap_read(pathname, &file, &bytes) == 0) {
        if (0 < bytes && bytes <= INT_MAX) {
            edit[0]->select_all(edit[0]);
            edit[0]->paste(edit[0], file, (int32_t)bytes);
            uic_edit_pg_t start = { .pn = 0, .gp = 0 };
            edit[0]->move(edit[0], start);
        }
        crt.memunmap(file, bytes);
    } else {
        app.toast(5.3, "\nFailed to open file \"%s\".\n%s\n",
                  pathname, crt.error(crt.err()));
    }
}

static void opened(void) {
    #if 1 // large font:
        font = gdi.font(app.fonts.mono, gdi.get_em(app.fonts.H3).y, -1);
    #else
        font = &app.fonts.regular;
    #endif
    edit[0]->set_font(edit[0], &font);
    if (app.argc > 1) {
        open_file(app.argv[1]);
    }
    if (debug_layout) {
        strprintf(left.text, "left");
        strprintf(right.text, "right");
        strprintf(buttons.text, "buttons");
        strprintf(edit0.ui.text, "edit0");
        strprintf(edit1.ui.text, "edit1");
    }
}

static void init(void) {
    app.title = title;
    app.ui->measure   = measure;
    app.ui->layout    = layout;
    app.ui->paint     = paint;
    static uic_t* children[] = { &left, &right, &bottom, null };
    app.ui->children = children;
    text.ui.font = &app.fonts.mono;
    strprintf(fuzz.ui.tip, "Ctrl+Shift+F5 to start / F5 to stop Fuzzing");
    for (int32_t i = 0; i < countof(edit); i++) {
        uic_edit_init(edit[i]);
    }
    app.focus = &edit[0]->ui;
}

app_t app = {
    .class_name = "sample5",
    .init   = init,
    .opened = opened,
    .wmin = 5.0f, // 5x5 inches
    .hmin = 3.0f
};

end_c
