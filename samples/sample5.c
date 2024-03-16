/* Copyright (c) Dmitry "Leo" Kuznetsov 2024 see LICENSE for details */
#include "quick.h"
#include "edit.h"

begin_c

static bool debug_layout; // = true;

const char* title = "Sample5";

// font scale:
static const double fs[] = {0.5, 0.75, 1.0, 1.25, 1.50, 1.75, 2.0}; 
// font scale index
static int32_t fx = 2; // fs[2] == 1.0 

static font_t mf; // mono font
static font_t pf; // proportional font

static uic_edit_t edit0;
static uic_edit_t edit1;
static uic_edit_t edit2;
static uic_edit_t* edit[3] = { &edit0, &edit1, &edit2 };

static int32_t focused(void);
static void focus_back_to_edit(void);

static void scaled_fonts() {
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

uic_button(full_screen, "&Full Screen", 7.5, {
    app.full_screen(!app.is_full_screen);
});

uic_button(quit, "&Quit", 7.5, { app.close(); });

uic_button(fuzz, "Fu&zz", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->fuzz(edit[ix]);
        fuzz->ui.pressed = edit[ix]->fuzzer != null;
        focus_back_to_edit();
    }
});

uic_checkbox(ro, "&Read Only", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->ro = ro->ui.pressed;
//      traceln("edit[%d].readonly: %d", ix, edit[ix]->ro);
        focus_back_to_edit();
    }
});

uic_checkbox(mono, "&Mono", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->set_font(edit[ix], mono->ui.pressed ? &mf : &pf);
        focus_back_to_edit();
    } else {
        mono->ui.pressed = !mono->ui.pressed;
    }
});

uic_checkbox(sl, "&Single Line", 7.5, {
    int32_t ix = focused();
    if (ix == 2) {
        sl->ui.pressed = true; // always single line
    } else if (0 <= ix && ix < 2) {
        uic_edit_t* e = edit[ix];
        e->sle = sl->ui.pressed;
//      traceln("edit[%d].multiline: %d", ix, e->multiline);
        if (e->sle) {
            e->select_all(e);
            e->paste(e, "Hello World! Single Line Edit", -1);
        }
        // alternatively app.layout() for everything or:
        e->ui.measure(&e->ui);
        e->ui.layout(&e->ui);
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

uic_button(fp, "Font Ctrl+", 7.5, { font_plus(); });

uic_button(fm, "Font Ctrl-", 7.5, { font_minus(); });

uic_multiline(text, 0.0, "...");

uic_container(right, null,
    &full_screen.ui, &quit.ui, &fuzz.ui,
    &fp.ui, &fm.ui,
    &mono.ui, &sl.ui, &ro.ui, &edit2.ui);

uic_container(left, null, &edit0.ui, &edit1.ui);
uic_container(bottom, null, &text.ui);

static void set_text(int32_t ix) {
    static char last[128];
    sprintf(text.ui.text, "%d:%d %d:%d %dx%d\n"
        "scroll %03d:%03d",
        edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
        edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
        edit[ix]->ui.w, edit[ix]->ui.h,
        edit[ix]->scroll.pn, edit[ix]->scroll.rn);
    if (0) {
        traceln("%d:%d %d:%d %dx%d scroll %03d:%03d",
            edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
            edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
            edit[ix]->ui.w, edit[ix]->ui.h,
            edit[ix]->scroll.pn, edit[ix]->scroll.rn);
    }
    // can be called before text.ui initialized
    if (text.ui.invalidate != null && !strequ(last, text.ui.text)) {
        text.ui.invalidate(&text.ui);
    }
    sprintf(last, "%s", text.ui.text);
}

static void after_paint(void) {
    // because of blinking caret paint is called frequently
    int32_t ix = focused();
    if (ix >= 0) {
        bool fuzzing = edit[ix]->fuzzer != null;
        if (fuzz.ui.pressed != fuzzing) {
            fuzz.ui.pressed = fuzzing;
            fuzz.ui.invalidate(&fuzz.ui);
        }
        set_text(ix);
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
    gdi.frame_with(ui->x, ui->y, ui->w, ui->h, fc[color]);
    color_t c = gdi.set_text_color(fc[color]);
    gdi.print("%s", ui->text);
    gdi.set_text_color(c);
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

static void paint(uic_t* ui) {
//  traceln("");
    if (debug_layout) { null_paint(ui); }
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
    int32_t ix = focused();
    for (int32_t i = 0; i < countof(edit); i++) {
        uic_t* e = &edit[i]->ui;
        color_t c = edit[i]->ro ?
            colors.tone_red : colors.btn_hover_highlight;
        gdi.frame_with(e->x - 1, e->y - 1, e->w + 2, e->h + 2,
            i == ix ? c : colors.dkgray4);
    }
    after_paint();
    if (debug_layout) { paint_frames(ui); }
    if (ix >= 0) {
        ro.ui.pressed = edit[ix]->ro;
        sl.ui.pressed = edit[ix]->sle;
        mono.ui.pressed = edit[ix]->ui.font == &mf;
    }
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
    if (app.argc > 1) {
        open_file(app.argv[1]);
    }
}

static int32_t focused(void) {
    // app.focus can point to a button, thus see which edit
    // control was focused last
    int32_t ix = -1;
    for (int32_t i = 0; i < countof(edit) && ix < 0; i++) {
        if (app.focus == &edit[i]->ui) { ix = i; }
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
        app.focus = &edit[ix]->ui; // return focus where it was
    }
}

static void every_100ms(void) {
//  traceln("");
    static uic_t* last;
    if (last != app.focus) { app.redraw(); }
    last = app.focus;
}

static void measure(uic_t* ui) {
//  traceln("");
    // gaps:
    const int32_t gx = ui->em.x;
    const int32_t gy = ui->em.y;
    right.h = ui->h - text.ui.h - gy;
    right.w = 0;
    measurements.vertical(&right, gy / 2);
    right.w += gx;
    bottom.w = text.ui.w - gx;
    bottom.h = text.ui.h;
    int32_t h = (ui->h - bottom.h - gy * 3) / countof(edit);
    for (int32_t i = 0; i < 2; i++) { // edit[0] and edit[1] only
        edit[i]->ui.w = ui->w - right.w - gx * 2;
        edit[i]->ui.h = h; // TODO: remove me - bad idea
    }
    left.w = 0;
    measurements.vertical(&left, gy);
    left.w += gx;
    edit2.ui.w = ro.ui.w; // only "width" height determined by text
    if (debug_layout) {
        traceln("%d,%d %dx%d", ui->x, ui->y, ui->w, ui->h);
        traceln("right %d,%d %dx%d", right.x, right.y, right.w, right.h);
        for (uic_t** c = right.children; c != null && *c != null; c++) {
            traceln("  %s %d,%d %dx%d", (*c)->text, (*c)->x, (*c)->y, (*c)->w, (*c)->h);
        }
        for (int32_t i = 0; i < countof(edit); i++) {
            traceln("[%d] %d,%d %dx%d", i, edit[i]->ui.x, edit[i]->ui.y,
                edit[i]->ui.w, edit[i]->ui.h);
        }
        traceln("left %d,%d %dx%d", left.x, left.y, left.w, left.h);
        traceln("bottom %d,%d %dx%d", bottom.x, bottom.y, bottom.w, bottom.h);
    }
}

static void layout(uic_t* ui) {
//  traceln("");
    // gaps:
    const int32_t gx2 = ui->em.x / 2;
    const int32_t gy2 = ui->em.y / 2;
    left.x = gx2;
    left.y = gy2;
    layouts.vertical(&left, left.x + gx2, left.y + gy2, gy2);
    right.x = left.x + left.w + gx2;
    right.y = left.y;
    bottom.x = gx2;
    bottom.y = ui->h - bottom.h;
    layouts.vertical(&right, right.x + gx2, right.y, gy2);
    text.ui.x = gx2;
    text.ui.y = ui->h - text.ui.h;
}

// limiting vertical height of SLE to 3 lines of text:

static void (*hooked_sle_measure)(uic_t* unused(ui));

static void measure_3_lines_sle(uic_t* ui) {
    // UX design decision:
    // 3 vertical visible runs SLE is friendlier in UX term
    // than not implemented horizontal scroll.
    assert(ui == &edit[2]->ui);
//  traceln("WxH: %dx%d <- r/o button", ro.ui.w, ro.ui.h);
    ui->w = ro.ui.w; // r/o button
    hooked_sle_measure(ui);
//  traceln("WxH: %dx%d (%dx%d) em: %d lines: %d", 
//          edit[2]->ui.w, edit[2]->ui.h, 
//          edit[2]->width, edit[2]->height, 
//          edit[2]->ui.em.y, edit[2]->ui.h / edit[2]->ui.em.y);
    if (ui->h > ui->em.y * 3) {
        ui->h = ui->em.y * 3;
        edit[2]->height = ui->h;
    }
}

static void key_pressed(uic_t* unused(ui), int32_t key) {
    if (app.has_focus() && key == virtual_keys.escape) { app.close(); }
    int32_t ix = focused();
    if (key == virtual_keys.f5) {
        if (ix >= 0) {
            uic_edit_t* e = edit[ix];
            if (app.ctrl && app.shift && e->fuzzer == null) {
                e->fuzz(e); // start on Ctrl+Shift+F5
            } else if (e->fuzzer != null) {
                e->fuzz(e); // stop on F5
            }
        }
    }
    if (app.ctrl) {
        if (key == virtual_keys.minus) {
            font_minus();
        } else if (key == virtual_keys.plus) {
            font_plus();
        } else if (key == '0') {
            font_reset();
        }
    }
    if (ix >= 0) { set_text(ix); }
}

static void edit_enter(uic_edit_t* e) {
    assert(e->sle);
    if (!app.shift) { // ignore shift ENRER:
        traceln("text: %.*s", e->para[0].bytes, e->para[0].text);
    }
}

// see edit.test.c

void uic_edit_init_with_lorem_ipsum(uic_edit_t* e); 
void uic_edit_fuzz(uic_edit_t* e);

static void init(void) {
    app.title = title;
    app.ui->measure     = measure;
    app.ui->layout      = layout;
    app.ui->paint       = paint;
    app.ui->key_pressed = key_pressed;
    scaled_fonts();
    static uic_t* children[] = { &left, &right, &bottom, null };
    app.ui->children = children;
    text.ui.font = &app.fonts.mono;
    strprintf(fuzz.ui.tip, "Ctrl+Shift+F5 to start / F5 to stop Fuzzing");
    for (int32_t i = 0; i < countof(edit); i++) {
        uic_edit_init(edit[i]);
        edit[i]->ui.font = &pf;
        uic_edit_init_with_lorem_ipsum(edit[i]);
    }
    app.focus = &edit[0]->ui;
    app.every_100ms = every_100ms;
    set_text(0); // need to be two lines for measure
    // edit[2] is SLE:
    uic_edit_init(edit[2]);
    hooked_sle_measure = edit[2]->ui.measure;
    edit[2]->ui.font = &pf;
    edit[2]->fuzz = uic_edit_fuzz;
    edit[2]->ui.measure = measure_3_lines_sle;
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
