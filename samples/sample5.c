/* Copyright (c) Dmitry "Leo" Kuznetsov 2024 see LICENSE for details */
#include "quick.h"
#include "ut/ut.h"
#include "edit.h"

begin_c

static bool debug_layout; // = true;

const char* title = "Sample5";

// font scale:
static const double fs[] = {0.5, 0.75, 1.0, 1.25, 1.50, 1.75, 2.0};
// font scale index
static int32_t fx = 2; // fs[2] == 1.0

static ui_font_t mf; // mono font
static ui_font_t pf; // proportional font

static ui_edit_t edit0;
static ui_edit_t edit1;
static ui_edit_t edit2;
static ui_edit_t* edit[3] = { &edit0, &edit1, &edit2 };

static int32_t focused(void);
static void focus_back_to_edit(void);

static void scaled_fonts(void) {
    assert(0 <= fx && fx < countof(fs));
    if (mf != null) { gdi.delete_font(mf); }
    mf = gdi.font(app.fonts.mono,
                  (int32_t)(gdi.font_height(app.fonts.mono) * fs[fx] + 0.5),
                  -1);
    if (pf != null) { gdi.delete_font(pf); }
    pf = gdi.font(app.fonts.regular,
                  (int32_t)(gdi.font_height(app.fonts.regular) * fs[fx] + 0.5),
                  -1);
}

ui_button(full_screen, "&Full Screen", 7.5, {
    app.full_screen(!app.is_full_screen);
});

ui_button(quit, "&Quit", 7.5, { app.close(); });

ui_button(fuzz, "Fu&zz", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->fuzz(edit[ix]);
        fuzz->view.pressed = edit[ix]->fuzzer != null;
        focus_back_to_edit();
    }
});

ui_checkbox(ro, "&Read Only", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->ro = ro->view.pressed;
//      traceln("edit[%d].readonly: %d", ix, edit[ix]->ro);
        focus_back_to_edit();
    }
});

ui_checkbox(mono, "&Mono", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->set_font(edit[ix], mono->view.pressed ? &mf : &pf);
        focus_back_to_edit();
    } else {
        mono->view.pressed = !mono->view.pressed;
    }
});

ui_checkbox(sl, "&Single Line", 7.5, {
    int32_t ix = focused();
    if (ix == 2) {
        sl->view.pressed = true; // always single line
    } else if (0 <= ix && ix < 2) {
        ui_edit_t* e = edit[ix];
        e->sle = sl->view.pressed;
//      traceln("edit[%d].multiline: %d", ix, e->multiline);
        if (e->sle) {
            e->select_all(e);
            e->paste(e, "Hello World! Single Line Edit", -1);
        }
        // alternatively app.layout() for everything or:
        e->view.measure(&e->view);
        e->view.layout(&e->view);
        focus_back_to_edit();
    }
});

static void font_plus(void) {
    if (fx < countof(fs) - 1) {
        fx++;
        scaled_fonts();
        app.layout();
    }
}

static void font_minus(void) {
    if (fx > 0) {
        fx--;
        scaled_fonts();
        app.layout();
    }
}

static void font_reset(void) {
    fx = 2;
    scaled_fonts();
    app.layout();
}

ui_button(fp, "Font Ctrl+", 7.5, { font_plus(); });

ui_button(fm, "Font Ctrl-", 7.5, { font_minus(); });

ui_multiline(text, 0.0, "...");

ui_container(right, null,
    &full_screen.view, &quit.view, &fuzz.view,
    &fp.view, &fm.view,
    &mono.view, &sl.view, &ro.view, &edit2.view);

ui_container(left, null, &edit0.view, &edit1.view);
ui_container(bottom, null, &text.view);

static void set_text(int32_t ix) {
    static char last[128];
    strprintf(text.view.text, "%d:%d %d:%d %dx%d\n"
        "scroll %03d:%03d",
        edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
        edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
        edit[ix]->view.w, edit[ix]->view.h,
        edit[ix]->scroll.pn, edit[ix]->scroll.rn);
    if (0) {
        traceln("%d:%d %d:%d %dx%d scroll %03d:%03d",
            edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
            edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
            edit[ix]->view.w, edit[ix]->view.h,
            edit[ix]->scroll.pn, edit[ix]->scroll.rn);
    }
    // can be called before text.ui initialized
    if (text.view.invalidate != null && !strequ(last, text.view.text)) {
        text.view.invalidate(&text.view);
    }
    strprintf(last, "%s", text.view.text);
}

static void after_paint(void) {
    // because of blinking caret paint is called frequently
    int32_t ix = focused();
    if (ix >= 0) {
        bool fuzzing = edit[ix]->fuzzer != null;
        if (fuzz.view.pressed != fuzzing) {
            fuzz.view.pressed = fuzzing;
            fuzz.view.invalidate(&fuzz.view);
        }
        set_text(ix);
    }
}

static void paint_frames(ui_view_t* view) {
    for (ui_view_t** c = view->children; c != null && *c != null; c++) {
        paint_frames(*c);
    }
    ui_color_t fc[] = {
        colors.red, colors.green, colors.blue, colors.red,
        colors.yellow, colors.cyan, colors.magenta
    };
    static int32_t color;
    gdi.push(view->x, view->y + view->h - view->em.y);
    gdi.frame_with(view->x, view->y, view->w, view->h, fc[color]);
    ui_color_t c = gdi.set_text_color(fc[color]);
    gdi.print("%s", view->text);
    gdi.set_text_color(c);
    gdi.pop();
    color = (color + 1) % countof(fc);
}

static void null_paint(ui_view_t* view) {
    for (ui_view_t** c = view->children; c != null && *c != null; c++) {
        null_paint(*c);
    }
    if (view != app.view) {
        view->paint = null;
    }
}

static void paint(ui_view_t* view) {
//  traceln("");
    if (debug_layout) { null_paint(view); }
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, view->w, view->h);
    int32_t ix = focused();
    for (int32_t i = 0; i < countof(edit); i++) {
        ui_view_t* e = &edit[i]->view;
        ui_color_t c = edit[i]->ro ?
            colors.tone_red : colors.btn_hover_highlight;
        gdi.frame_with(e->x - 1, e->y - 1, e->w + 2, e->h + 2,
            i == ix ? c : colors.dkgray4);
    }
    after_paint();
    if (debug_layout) { paint_frames(view); }
    if (ix >= 0) {
        ro.view.pressed = edit[ix]->ro;
        sl.view.pressed = edit[ix]->sle;
        mono.view.pressed = edit[ix]->view.font == &mf;
    }
}

static void open_file(const char* pathname) {
    char* file = null;
    int64_t bytes = 0;
    if (mem.map_ro(pathname, &file, &bytes) == 0) {
        if (0 < bytes && bytes <= INT64_MAX) {
            edit[0]->select_all(edit[0]);
            edit[0]->paste(edit[0], file, (int32_t)bytes);
            ui_edit_pg_t start = { .pn = 0, .gp = 0 };
            edit[0]->move(edit[0], start);
        }
        mem.unmap(file, bytes);
    } else {
        app.toast(5.3, "\nFailed to open file \"%s\".\n%s\n",
                  pathname, str.error(runtime.err()));
    }
}

static void opened(void) {
    if (args.c > 1) { open_file(args.v[1]); }
}

static int32_t focused(void) {
    // app.focus can point to a button, thus see which edit
    // control was focused last
    int32_t ix = -1;
    for (int32_t i = 0; i < countof(edit) && ix < 0; i++) {
        if (app.focus == &edit[i]->view) { ix = i; }
        if (edit[i]->focused) { ix = i; }
    }
    static int32_t last_ix = -1;
    if (ix < 0) { ix = last_ix; }
    last_ix = ix;
    return ix;
}

static void focus_back_to_edit(void) {
    const int32_t ix = focused();
    if (ix >= 0) {
        app.focus = &edit[ix]->view; // return focus where it was
    }
}

static void every_100ms(void) {
//  traceln("");
    static ui_view_t* last;
    if (last != app.focus) { app.redraw(); }
    last = app.focus;
}

static void measure(ui_view_t* view) {
//  traceln("");
    // gaps:
    const int32_t gx = view->em.x;
    const int32_t gy = view->em.y;
    right.h = view->h - text.view.h - gy;
    right.w = 0;
    measurements.vertical(&right, gy / 2);
    right.w += gx;
    bottom.w = text.view.w - gx;
    bottom.h = text.view.h;
    int32_t h = (view->h - bottom.h - gy * 3) / countof(edit);
    for (int32_t i = 0; i < 2; i++) { // edit[0] and edit[1] only
        edit[i]->view.w = view->w - right.w - gx * 2;
        edit[i]->view.h = h; // TODO: remove me - bad idea
    }
    left.w = 0;
    measurements.vertical(&left, gy);
    left.w += gx;
    edit2.view.w = ro.view.w; // only "width" height determined by text
    if (debug_layout) {
        traceln("%d,%d %dx%d", view->x, view->y, view->w, view->h);
        traceln("right %d,%d %dx%d", right.x, right.y, right.w, right.h);
        for (ui_view_t** c = right.children; c != null && *c != null; c++) {
            traceln("  %s %d,%d %dx%d", (*c)->text, (*c)->x, (*c)->y, (*c)->w, (*c)->h);
        }
        for (int32_t i = 0; i < countof(edit); i++) {
            traceln("[%d] %d,%d %dx%d", i, edit[i]->view.x, edit[i]->view.y,
                edit[i]->view.w, edit[i]->view.h);
        }
        traceln("left %d,%d %dx%d", left.x, left.y, left.w, left.h);
        traceln("bottom %d,%d %dx%d", bottom.x, bottom.y, bottom.w, bottom.h);
    }
}

static void layout(ui_view_t* view) {
//  traceln("");
    // gaps:
    const int32_t gx2 = view->em.x / 2;
    const int32_t gy2 = view->em.y / 2;
    left.x = gx2;
    left.y = gy2;
    layouts.vertical(&left, left.x + gx2, left.y + gy2, gy2);
    right.x = left.x + left.w + gx2;
    right.y = left.y;
    bottom.x = gx2;
    bottom.y = view->h - bottom.h;
    layouts.vertical(&right, right.x + gx2, right.y, gy2);
    text.view.x = gx2;
    text.view.y = view->h - text.view.h;
}

// limiting vertical height of SLE to 3 lines of text:

static void (*hooked_sle_measure)(ui_view_t* unused(view));

static void measure_3_lines_sle(ui_view_t* view) {
    // UX design decision:
    // 3 vertical visible runs SLE is friendlier in UX term
    // than not implemented horizontal scroll.
    assert(view == &edit[2]->view);
//  traceln("WxH: %dx%d <- r/o button", ro.view.w, ro.view.h);
    view->w = ro.view.w; // r/o button
    hooked_sle_measure(view);
//  traceln("WxH: %dx%d (%dx%d) em: %d lines: %d",
//          edit[2]->view.w, edit[2]->view.h,
//          edit[2]->width, edit[2]->height,
//          edit[2]->view.em.y, edit[2]->view.h / edit[2]->view.em.y);
    int32_t max_lines = edit[2]->focused ? 3 : 1;
    if (view->h > view->em.y * max_lines) {
        view->h = view->em.y * max_lines;
    }
}

static void key_pressed(ui_view_t* unused(view), int32_t key) {
    if (app.has_focus() && key == ui.key.escape) { app.close(); }
    int32_t ix = focused();
    if (key == ui.key.f5) {
        if (ix >= 0) {
            ui_edit_t* e = edit[ix];
            if (app.ctrl && app.shift && e->fuzzer == null) {
                e->fuzz(e); // start on Ctrl+Shift+F5
            } else if (e->fuzzer != null) {
                e->fuzz(e); // stop on F5
            }
        }
    }
    if (app.ctrl) {
        if (key == ui.key.minus) {
            font_minus();
        } else if (key == ui.key.plus) {
            font_plus();
        } else if (key == '0') {
            font_reset();
        }
    }
    if (ix >= 0) { set_text(ix); }
}

static void edit_enter(ui_edit_t* e) {
    assert(e->sle);
    if (!app.shift) { // ignore shift ENRER:
        traceln("text: %.*s", e->para[0].bytes, e->para[0].text);
    }
}

// see edit.test.c

void ui_edit_init_with_lorem_ipsum(ui_edit_t* e);
void ui_edit_fuzz(ui_edit_t* e);
void ui_edit_next_fuzz(ui_edit_t* e);

static void init(void) {
    app.title = title;
    app.view->measure     = measure;
    app.view->layout      = layout;
    app.view->paint       = paint;
    app.view->key_pressed = key_pressed;
    scaled_fonts();
    static ui_view_t* children[] = { &left, &right, &bottom, null };
    app.view->children = children;
    text.view.font = &app.fonts.mono;
    strprintf(fuzz.view.tip, "Ctrl+Shift+F5 to start / F5 to stop Fuzzing");
    for (int32_t i = 0; i < countof(edit); i++) {
        ui_edit_init(edit[i]);
        edit[i]->view.font = &pf;
        edit[i]->fuzz = ui_edit_fuzz;
        edit[i]->next_fuzz = ui_edit_next_fuzz;
        ui_edit_init_with_lorem_ipsum(edit[i]);
    }
    app.focus = &edit[0]->view;
    app.every_100ms = every_100ms;
    set_text(0); // need to be two lines for measure
    // edit[2] is SLE:
    ui_edit_init(edit[2]);
    hooked_sle_measure = edit[2]->view.measure;
    edit[2]->view.font = &pf;
    edit[2]->fuzz = ui_edit_fuzz;
    edit[2]->next_fuzz = ui_edit_next_fuzz;
    edit[2]->view.measure = measure_3_lines_sle;
    edit[2]->sle = true;
    edit[2]->select_all(edit[2]);
    edit[2]->paste(edit[2], "Single line edit", -1);
    edit[2]->enter = edit_enter;
}

app_t app = {
    .class_name = "sample5",
    .init   = init,
    .opened = opened,
    .wmin = 3.0f, // 3x2 inches
    .hmin = 2.0f
};

end_c
