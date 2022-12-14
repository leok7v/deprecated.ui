/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"
#include <Windows.h>

begin_c

// Glyphs in monospaced Windows fonts may have different width for non-ASCII
// characters. Thus even if edit is monospaced glyph measurements are used
// in text layout.

static void uic_edit_move_caret(uic_edit_t* e, int32_t ln, int32_t cl);

static int uic_edit_glyph_bytes(char start_byte_value) { // utf-8
    // return 1-4 bytes glyph starting with `start_byte_value` character
    uint8_t uc = (uint8_t)start_byte_value;
    // 0xxxxxxx
    if ((uc & 0x80) == 0x00) { return 1; }
    // 110xxxxx 10xxxxxx 0b1100=0xE 0x1100=0xC
    if ((uc & 0xE0) == 0xC0) { return 2; }
    // 1110xxxx 10xxxxxx 10xxxxxx 0b1111=0xF 0x1110=0xE
    if ((uc & 0xF0) == 0xE0) { return 3; }
    // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx 0b1111,1000=0xF8 0x1111,0000=0xF0
    if ((uc & 0xF8) == 0xF0) { return 4; }
    fatal_if(true, "incorrect UTF first byte 0%02X", uc);
    return -1;
}

static int32_t uic_edit_glyph_pos(const char* utf8, int32_t bytes, int32_t gp[]) {
    int i = 0;
    int k = 0;
    // gp[k] start postion in byte offset from utf8 text of glyph next after [k]
    // e.g. if uic_edit_glyph_bytes(utf8[0]) == 3 gp[0] will be 3
    // obviously the first glyph offset is 0 and thus is not kept
    while (i < bytes) {
        i += uic_edit_glyph_bytes(utf8[i]);
        if (gp != null) { gp[k] = i; }
        k++;
    }
    return k;
}

static int32_t uic_edit_glyphs(const char* utf8, int32_t bytes) {
    return uic_edit_glyph_pos(utf8, bytes, null);
}

static int32_t uic_edit_column_to_offset(const char* text,
        int32_t bytes, int32_t column) {
    int32_t c = 0;
    int32_t i = 0;
    if (bytes > 0) {
        while (c < column) {
            assert(i < bytes);
            i += uic_edit_glyph_bytes(text[i]);
            c++;
        }
    }
    assert(i <= bytes);
    return i;
}

static int32_t uic_edit_glyphs_in_paragraph(uic_edit_t* e, int32_t ln) {
    uic_edit_para_t* line = &e->para[ln];
    return uic_edit_glyphs(line->text, line->bytes);
}

static void uic_edit_allocate(void** pp, int32_t bytes, size_t element) {
    assert(*pp == null);
    *pp = malloc(bytes * (int32_t)element);
    fatal_if_null(*pp);
}

static void uic_edit_free(void** pp) {
    assert(*pp != null);
    free(*pp);
    *pp = null;
}

static void uic_edit_reallocate(void** pp, int32_t bytes, size_t element) {
    bytes *= (int32_t)element;
    if (*pp == null) {
        *pp = malloc(bytes);
    } else {
        *pp = realloc(*pp, bytes);
    }
    fatal_if_null(*pp);
}

static void uic_edit_append_paragraph(uic_edit_t* e, const char* s, int32_t n) {
    // uic_edit_append_paragraph() only used by uic_edit_init_with_text()
    assert(e->paragraphs < e->allocated / (int32_t)sizeof(uic_edit_para_t));
    uic_edit_para_t* p = &e->para[e->paragraphs];
    p->text = (char*)s;
    p->bytes = n;
    p->glyphs = 0;
    p->allocated = 0;
    p->runs = 0;
    p->run = null;
    p->ix2gp = null;
    p->gp2ix = null;
    e->paragraphs++;
}

static int32_t uic_edit_break(font_t f, char* text, int32_t width,
        int32_t* gp, int32_t gc) { // ~50 microseconds for 100 bytes
    // returns number of glyph (offset) from text of the last glyph that is
    // completely to the left of `w`
    assert(gc > 1, "cannot break single glyph");
    int i = 0;
    int j = gc;
    int32_t k = (i + j) / 2;
    while (i < j) {
        // important: gp[0] is number of bytes in the first glyph:
        int32_t px = gdi.measure_text(f, "%.*s", gp[k], text).x;
        if (px == width) { break; }
        if (px < width) { i = k + 1; } else { j = k; }
        k = (i + j) / 2;
    }
    return k;
}

static int32_t uic_edit_glyph_at_x(font_t f, char* text, int32_t bytes,
        int32_t x) {
    if (x == 0 || bytes == 0) {
        return 0;
    } else {
        int32_t* gp = (int32_t*)stackalloc(bytes * sizeof(int32_t));
        int gc = uic_edit_glyph_pos(text, bytes, gp);
        return gc <= 1 ? gc : uic_edit_break(f, text, x + 1, gp, gc); // +1 ???
    }
}

static void uic_edit_ix2gp_and_gp2idx(uic_edit_t* e, int32_t p) {
    uic_edit_para_t* para = &e->para[p];
    assert(para->ix2gp == null && para->gp2ix == null && para->glyphs == 0);
    int32_t gc = 0; // glyph count
    int32_t i = 0; // byte index
    while (i < para->bytes) {
        gc++;
        i += uic_edit_glyph_bytes(para->text[i]);
    }
    para->glyphs = gc;
    uic_edit_allocate(&para->ix2gp, para->bytes, sizeof(int32_t));
    uic_edit_allocate(&para->gp2ix, para->glyphs, sizeof(int32_t));
    i = 0;
    int32_t j = 0; // glyph index
    while (i < para->bytes) {
        para->ix2gp[i] = j;
        para->gp2ix[j] = i;
        const int32_t gb = uic_edit_glyph_bytes(para->text[i]);
        j++;
        i++;
        for (int32_t k = 0; k < gb - 1; k++) {
            para->ix2gp[i] = -1;
            i++;
        }
    }
}


// uic_edit::layout_pragraph breaks paragraph into `runs` according to `width`
// and `.wordbreak`, `.singleline`

static void uic_edit_layout_pragraph(uic_edit_t* e, int32_t pn) {
    assert(e->width > 0);
    uic_edit_para_t* para = &e->para[pn];
    if (para->run == null) {
        uic_edit_ix2gp_and_gp2idx(e, pn);
        assert(para->runs == 0 && para->run == null);
        const int32_t max_runs = para->bytes + 1;
        uic_edit_allocate(&para->run, max_runs, sizeof(uic_edit_run_t));
        uic_edit_run_t* run = para->run;
        font_t f = *e->ui.font;
        int32_t pixels = gdi.measure_text(f, "%.*s", para->bytes, para->text).x;
        if (pixels <= e->width) { // whole para fits into width
            para->runs = 1;
            run[0].gp = 0;
            run[0].bytes = para->bytes;
assert(uic_edit_glyphs(para->text, para->bytes) == para->glyphs);
            run[0].glyphs = para->glyphs;
            run[0].pixels = pixels;
        } else {
            int32_t rc = 0;  // runw count
            int32_t cl = 0; // column aka glyph count
            char* text = para->text;
            int32_t bytes = para->bytes;
            int32_t* gp = (int32_t*)stackalloc(bytes * sizeof(int32_t));
            // glyph position (offset in bytes)
            while (bytes > 0) {
                assert(rc < rc < max_runs);
                int32_t gc = uic_edit_glyph_pos(text, bytes, gp);
                assert(gc > 0);
                // expected at least one glyph
                int32_t glyphs = gc == 1 ?
                    1 : uic_edit_break(f, text, e->width, gp, gc);
                assert(0 <= glyphs - 1 && glyphs - 1 < gc);
                int32_t utf8bytes = gp[glyphs - 1];
                pixels = gdi.measure_text(f, "%.*s", utf8bytes, text).x;
// traceln("%d#%d bytes=%d utf8bytes=%d glyphs=%d pixels=%d width=%d %.*s", pn, rc, bytes, utf8bytes, glyphs, pixels, e->width, bytes, text);
                if (glyphs > 1 && utf8bytes < bytes && text[utf8bytes] != 0x20) {
                    // try to find word break SPACE character. utf8 space is 0x20
                    int32_t i = utf8bytes;
                    while (i > 0 && text[i - 1] != 0x20) { i--; }
                    if (i > 0 && i != utf8bytes) {
                        utf8bytes = i;
                        glyphs = uic_edit_glyphs(text, utf8bytes);
                        pixels = gdi.measure_text(f, "%.*s", utf8bytes, text).x;
                    }
                }
                run[rc].gp = cl;
                run[rc].bytes = utf8bytes;
                run[rc].glyphs = glyphs;
                run[rc].pixels = pixels;
                rc++;
                text += utf8bytes;
                assert(0 <= utf8bytes && utf8bytes <= bytes);
                bytes -= utf8bytes;
                cl += glyphs;
            }
            para->runs = rc; // truncate heap allocated array:
            uic_edit_reallocate(&para->run, rc, sizeof(uic_edit_run_t));
        }
        assert(para->runs >= 1);
    }
}

static void uic_edit_create_caret(uic_edit_t* e) {
    fatal_if_false(CreateCaret((HWND)app.window, null, 2, e->ui.em.y));
    fatal_if_false(SetCaretBlinkTime(400));
    fatal_if_false(ShowCaret((HWND)app.window));
    e->focused = true;
}

static void uic_edit_destroy_caret(uic_edit_t* e) {
    fatal_if_false(HideCaret((HWND)app.window));
    fatal_if_false(DestroyCaret());
    e->focused = false;
}

static void uic_edit_rescroll(uic_edit_t* e) {
    // when geometry (width/height) changes the run number in scroll
    // may became obsolete our of range and needs to be adjusted
    uic_edit_layout_pragraph(e, e->scroll_pn);
    e->scroll_rn = min(e->scroll_rn, e->para[e->scroll_pn].runs - 1);
}

static void uic_edit_dispose_paragraphs_layout(uic_edit_t* e) {
    for (int i = 0; i < e->paragraphs; i++) {
        if (e->para[i].run != null) {
            uic_edit_free(&e->para[i].run);
            uic_edit_free(&
            e->para[i].ix2gp);
            uic_edit_free(&
            e->para[i].gp2ix);
        }
        assert(e->para[i].run   == null);
        assert(e->para[i].ix2gp == null);
        assert(e->para[i].gp2ix == null);
        e->para[i].glyphs = 0;
        e->para[i].runs = 0;
    }
}

static void uic_edit_measure(uic_t* ui) { // bottom up
    ui->em = gdi.get_em(*ui->font);
//  traceln("%dx%d", ui->w, ui->h);
    assert(ui->tag == uic_tag_edit);
    // enforce minimum size - it makes it checking corner cases much simpler
    // and it's hard to edit anything in a smaller area - will result in bad UX
    if (ui->w < ui->em.x * 3) { ui->w = ui->em.x * 4; }
    if (ui->h < ui->em.y) { ui->h = ui->em.y; }
}

static void uic_edit_layout(uic_t* ui) { // top down
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
    // enforce minimum size - again
    if (ui->w < ui->em.x * 3) { ui->w = ui->em.x * 4; }
    if (ui->h < ui->em.y) { ui->h = ui->em.y; }
    if (e->width > 0 && ui->w != e->width) {
        uic_edit_dispose_paragraphs_layout(e);
    }
    e->width = ui->w;
    e->height = ui->h;
    uic_edit_rescroll(e);
    // For single line editor distribute vertical gap evenly between
    // top and bottom. For multiline snap top line to y coordinate 0
    // otherwise resizing view will result in up-down jiggling of the
    // whole text
    e->top    = e->multiline ? 0 : (e->ui.h - e->ui.em.y) / 2;
    e->bottom = e->multiline ? e->ui.h : e->ui.h - e->top - e->ui.em.y;
    if (e->focused) {
        // recreate caret because em.y may have changed
        uic_edit_destroy_caret(e);
        uic_edit_create_caret(e);
        if (app.focus && e->focused) {
            uic_edit_move_caret(e, e->selection.end.pn, e->selection.end.gp);
        }
    }
}

static uint64_t uic_edit_pos(int32_t ln, int32_t cl) {
    assert(ln >= 0 && cl >= 0);
    return ((uint64_t)ln << 32) | (uint64_t)cl;
}

// uic_edit::pg_to_xy() paragraph # glyph # -> (x,y) in 0,0 - width, heigth

static ui_point_t uic_edit_pg_to_xy(uic_edit_t* e, int32_t ln, int32_t cl) {
    ui_point_t pt = {-1, 0};
    pt.y = 0;
    for (int i = e->scroll_pn; i < e->paragraphs && pt.x < 0; i++) {
        uic_edit_layout_pragraph(e, i);
        const int32_t runs = e->para[i].runs;
        int32_t rn = min(runs - 1, e->scroll_rn);
        char* text = e->para[i].text;
        int32_t bytes = e->para[i].bytes;
        int32_t column = 0;
        const uic_edit_run_t* run = e->para[i].run;
        for (int j = 0; j < runs; j++) {
            bool last_run = j == runs - 1; // special case
            int32_t gc = run[j].glyphs;
            if (i > e->scroll_pn || j >= rn) {
                if (i == ln) {
                    // in the last `run` of a parageraph x after last glyph OK:
                    bool inside =
                        last_run && column <= cl && cl < column + gc + 1 ||
                        column <= cl && cl < column + gc;
                    if (inside) {
                        int32_t offset = uic_edit_column_to_offset(text, run[j].bytes, cl - column);
                        pt.x = gdi.measure_text(*e->ui.font, "%.*s", offset, text).x;
                        break;
                    }
                }
                pt.y += e->ui.em.y;
            }
            column += gc;
            text += run[j].bytes;
            bytes -= run[j].bytes;
        }
    }
    if (pt.x < 0) { // after last paragraph?
        assert(ln == e->paragraphs && cl == 0, "ln#cl: %d#%d", ln, cl);
        pt.x = 0;
    }
    return pt;
}

static int32_t uic_edit_glyph_width_px(uic_edit_t* e, int32_t ln, int32_t cl) {
    char* text = e->para[ln].text;
    int32_t bytes = e->para[ln].bytes;
    int32_t gc = uic_edit_glyphs(text, bytes);
    if (cl < gc) {
        char* s = text + uic_edit_column_to_offset(text, e->para[ln].bytes, cl);
        int32_t bytes_in_glyph = uic_edit_glyph_bytes(*s);
        return gdi.measure_text(*e->ui.font, "%.*s", bytes_in_glyph, s).x;
    } else {
        return 0; // 1 column after last glyph is allowed position
    }
}

// uic_edit::xy_to_pg() (x,y) (0,0, widh x height) -> paragraph # glyph #

static uic_edit_pg_t uic_edit_xy_to_pg(uic_edit_t* e, int32_t x, int32_t y) {
    font_t f = *e->ui.font;
    uic_edit_pg_t pg = {-1, -1};
    int32_t py = 0; // paragraph `y' coordinate
    for (int i = e->scroll_pn; i < e->paragraphs && pg.pn < 0; i++) {
        uic_edit_layout_pragraph(e, i);
        const int32_t runs = e->para[i].runs;
        int32_t rn = min(runs - 1, e->scroll_rn);
        char* s = e->para[i].text;
        int32_t bytes = e->para[i].bytes;
        int column = 0;
        const uic_edit_run_t* run = e->para[i].run;
        for (int j = 0; j < runs; j++) {
            const int32_t gc = run[j].glyphs;
            if (i > e->scroll_pn || j >= rn) {
                if (py <= y && y < py + e->ui.em.y) {
                    int32_t w = gdi.measure_text(f, "%.*s", run[j].bytes, s).x;
                    int32_t cl_in_run = uic_edit_glyph_at_x(f, s, run[j].bytes, x);
                    pg.pn = i;
                    pg.gp = column + cl_in_run;
                    // allow mouse click past half last glyph
                    int32_t cw = uic_edit_glyph_width_px(e, pg.pn, pg.gp);
                    if (cw > 0 && x >= w - cw / 2) {
                        pg.gp++; // snap to closest glyph's 'x'
                    } else if (cw > 0 && pg.gp < gc - 1) {
                        int32_t x0 = uic_edit_pg_to_xy(e, pg.pn, pg.gp).x;
                        int32_t x1 = uic_edit_pg_to_xy(e, pg.pn, pg.gp + 1).x;
//                      traceln("x0: %d cw: %d x: %d x1: %d", x0, cw, x, x1);
                        if (x1 - x < x - x0) {
                            pg.gp++; // snap to closest glyph's 'x'
                        }
                    }
                    break;
                }
                py += e->ui.em.y;
            }
            column += gc;
            s += run[j].bytes;
            bytes -= run[j].bytes;
        }
        if (py > e->ui.h) { break; }
    }
    return pg;
}

static void uic_edit_paint_selection(uic_edit_t* e,
        const uic_edit_run_t* r, char* text,
        int32_t ln, int32_t c0, int32_t c1) {
    uint64_t s0 = uic_edit_pos(e->selection.fro.pn, e->selection.fro.gp);
    uint64_t e0 = uic_edit_pos(e->selection.end.pn, e->selection.end.gp);
    if (s0 > e0) {
        uint64_t swap = e0;
        e0 = s0;
        s0 = swap;
    }
    uint64_t s1 = uic_edit_pos(ln, c0);
    uint64_t e1 = uic_edit_pos(ln, c1);
    if (s0 <= e1 && s1 <= e0) {
        uint64_t start = max(s0, s1) - c0;
        uint64_t end = min(e0, e1) - c0;
        if (start < end) {
            int32_t bytes = r->bytes;
            int32_t ofs0 = uic_edit_column_to_offset(text, bytes, (int32_t)(start & 0xFFFFFFFF));
            int32_t ofs1 = uic_edit_column_to_offset(text, bytes, (int32_t)(end   & 0xFFFFFFFF));
            int32_t x0 = gdi.measure_text(*e->ui.font, "%.*s", ofs0, text).x;
            int32_t x1 = gdi.measure_text(*e->ui.font, "%.*s", ofs1, text).x;
            brush_t b = gdi.set_brush(gdi.brush_color);
            color_t c = gdi.set_brush_color(rgb(48, 64, 72));
            gdi.fill(gdi.x + x0, gdi.y, x1 - x0, e->ui.em.y);
            gdi.set_brush_color(c);
            gdi.set_brush(b);
        }
    }
}

static void uic_edit_paint_para(uic_edit_t* e, int32_t pn) {
    uic_edit_para_t* para = &e->para[pn];
    uic_edit_layout_pragraph(e, pn);
    const int32_t runs = e->para[pn].runs;
    int32_t rn = min(runs - 1, e->scroll_rn);
    int32_t bytes = para->bytes;
    char* s = para->text;
    int32_t cl = 0;
    const uic_edit_run_t* run = para->run;
    for (int j = 0; j < runs && gdi.y < e->ui.y + e->bottom; j++) {
        int32_t gc = run[j].glyphs; // glyph count
        if (pn > e->scroll_pn || j >= rn) {
            gdi.x = e->ui.x;
            uic_edit_paint_selection(e, &run[j], s, pn, cl, cl + gc);
            gdi.text("%.*s", run[j].bytes, s);
            gdi.y += e->ui.em.y;
        }
        cl += gc;
        s += run[j].bytes;
        bytes -= run[j].bytes;
    }
}

static void uic_edit_paint(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    uic_edit_rescroll(e);
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(rgb(20, 20, 14));
    gdi.fill(0, 0, ui->w, ui->h);
    gdi.push(ui->x, ui->y + e->top);
    gdi.set_clip(ui->x, ui->y, ui->w, ui->h);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    gdi.set_font(f);
    gdi.set_text_color(ui->color);
    const int32_t bottom = ui->y + e->bottom;
    assert(e->scroll_pn <= e->paragraphs);
    for (int i = e->scroll_pn; i < e->paragraphs && gdi.y < bottom; i++) {
        uic_edit_paint_para(e, i);
    }
    gdi.set_clip(0, 0, 0, 0);
    gdi.pop();
}

static void uic_edit_set_caret(uic_edit_t* e, int32_t x, int32_t y) {
    if (e->caret.x != x || e->caret.y != y) {
        fatal_if_false(SetCaretPos(x, y));
        e->caret.x = x;
        e->caret.y = y;
    }
}

static void uic_edit_move_caret(uic_edit_t* e, int32_t pn, int32_t cl) {
    assert(e->focused && app.focused);
    ui_point_t pt = e->ui.w > 0 ? // ui.w == 0 means  no measure/layout yet
        uic_edit_pg_to_xy(e, pn, cl) : (ui_point_t){0, 0};
    uic_edit_set_caret(e, pt.x, pt.y + e->top);
    e->selection.end.pn = pn;
    e->selection.end.gp = cl;
    if (!app.shift && !e->mouse != 0) {
        e->selection.fro = e->selection.end;
    }
    e->ui.invalidate(&e->ui);
}

static void uic_edit_check_focus(uic_edit_t* e) {
    // focus is two stage afair: window can be focused or not
    // and single UI control inside window can have focus
    if (e->ui.w > 0 && e->ui.h > 0) {
        if (!app.focused) {
            if (e->focused) { uic_edit_destroy_caret(e); }
        } else if (app.focus == &e->ui) {
            if (!e->focused) {
                uic_edit_create_caret(e);
                uic_edit_move_caret(e, e->selection.end.pn, e->selection.end.gp);
            }
        } else {
            if (e->focused) { uic_edit_destroy_caret(e); }
        }
    }
}

static void uic_edit_scroll_up(uic_edit_t* e) {
    uic_edit_layout_pragraph(e, e->scroll_pn);
    if (e->scroll_rn < e->para[e->scroll_pn].runs - 1) {
        e->scroll_rn++;
    } else if (e->scroll_pn < e->paragraphs) {
        e->scroll_pn++;
        e->scroll_rn = 0;
    }
    e->ui.invalidate(&e->ui);
}

static void uic_edit_scroll_down(uic_edit_t* e) {
    uic_edit_layout_pragraph(e, e->scroll_pn);
    e->scroll_rn = min(e->scroll_rn, e->para[e->scroll_pn].runs - 1);
    if (e->scroll_rn == 0 && e->scroll_pn > 0) {
        e->scroll_pn--;
        e->scroll_rn = e->para[e->scroll_pn].runs - 1;
    } else if (e->scroll_rn > 0 && e->scroll_pn > 0) {
        e->scroll_rn--;
    }
    e->ui.invalidate(&e->ui);
}

static void uic_edit_mouse(uic_t* ui, int m, int unused(flags)) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    const int32_t x = app.mouse.x - e->ui.x;
    const int32_t y = app.mouse.y - e->ui.y - e->top;
    bool inside = 0 <= x && x < ui->w && 0 <= y && y < ui->h;
    bool left = m == messages.left_button_down;
    bool right = m == messages.right_button_down;
    if (e->focused  && inside && (left || right || e->mouse != 0)) {
        uic_edit_rescroll(e);
        uic_edit_pg_t p = uic_edit_xy_to_pg(e, x, y);
        bool at_top = e->selection.end.pn == 0 || e->selection.end.gp == 0;
        if (!at_top && y < ui->em.y) {
            uic_edit_scroll_down(e);
        }
        if (0 <= p.pn && 0 <= p.gp) {
            if (y > ui->h - ui->em.y) {
                uic_edit_scroll_up(e);
            }
            int32_t pn = p.pn;
            int32_t cl = p.gp;
            if (pn > e->paragraphs) { pn = max(0, e->paragraphs); }
            int32_t chars = uic_edit_glyphs_in_paragraph(e, pn);
            if (cl > chars) { cl = max(0, chars); }
            uic_edit_move_caret(e, pn, cl);
        }
    }
    if (!e->focused && inside && (left || right)) {
        app.focus = ui;
        uic_edit_check_focus(e);
    } else if (e->focused) {
        if (left)  { e->mouse |= (1 << 0); }
        if (right) { e->mouse |= (1 << 1); }
    }
    if (m == messages.left_button_up)  { e->mouse &= ~(1 << 0); }
    if (m == messages.right_button_up) { e->mouse &= ~(1 << 1); }
}

static void uic_edit_key_left(uic_edit_t* e, int32_t pn, int32_t cl) {
    if (pn > 0 || cl > 0) {
        if (e->scroll_pn == pn && e->scroll_rn == 0) {
            uic_edit_scroll_down(e);
        }
        if (cl > 0) {
            cl--;
        } else if (pn > 0) {
            pn--;
            cl = uic_edit_glyphs_in_paragraph(e, pn);
        }
        uic_edit_move_caret(e, pn, cl);
    }
}

static void uic_edit_key_right(uic_edit_t* e, int32_t pn, int32_t cl) {
    if (pn < e->paragraphs) {
        int32_t g_in_p = uic_edit_glyphs_in_paragraph(e, pn);
        if (cl < g_in_p) {
            ui_point_t pt = uic_edit_pg_to_xy(e, pn, cl + 1);
            if (pt.y + e->ui.em.y > e->bottom) { uic_edit_scroll_up(e); }
            cl++;
        } else {
            ui_point_t pt = uic_edit_pg_to_xy(e, pn + 1, 0);
            if (pt.y + e->ui.em.y > e->bottom) { uic_edit_scroll_up(e); }
            pn++;
            cl = 0;
        }
        uic_edit_move_caret(e, pn, cl);
    }
}

static void uic_edit_key_up(uic_edit_t* e, int32_t pn, int32_t cl) {
    if (pn == e->paragraphs) {
        assert(cl == 0); // positioned past EOF
        pn--;
        cl = uic_edit_glyphs(e->para[pn].text, e->para[pn].bytes);
        ui_point_t pt = uic_edit_pg_to_xy(e, pn, cl);
        pt.x = 0;
        cl = uic_edit_xy_to_pg(e, pt.x, pt.y).gp;
    } else {
        ui_point_t pt = uic_edit_pg_to_xy(e, pn, cl);
        pt.y -= 1;
        if (pt.y < e->ui.y) {
            uic_edit_scroll_down(e);
            pt = uic_edit_pg_to_xy(e, pn, cl);
            pt.y -= 1;
        }
        uic_edit_pg_t lc = uic_edit_xy_to_pg(e, pt.x, pt.y);
        if (lc.pn >= 0 && lc.gp >= 0) {
            pn = lc.pn;
            cl = lc.gp;
        }
    }
    uic_edit_move_caret(e, pn, cl);
}

static void uic_edit_key_down(uic_edit_t* e, int32_t pn, int32_t cl) {
    ui_point_t pt = uic_edit_pg_to_xy(e, pn, cl);
    pt.y += e->ui.em.y + 1;
    uic_edit_pg_t pg = uic_edit_xy_to_pg(e, pt.x, pt.y);
    if (pg.pn >= 0 && pg.gp >= 0) {
        pn = pg.pn;
        cl = pg.gp;
    } else if (pn == e->paragraphs - 1) {
        pn = e->paragraphs; // advance past EOF
        cl = 0;
    }
    if (pt.y + e->ui.em.y > e->bottom) {
        uic_edit_scroll_up(e);
    }
    uic_edit_move_caret(e, pn, cl);
}

static void uic_edit_key_pressed(uic_t* ui, int32_t key) {
    // TODO: vertical movement snap to the closest glyph after moving
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        if (key == VK_ESCAPE) { app.close(); }
        if (key == virtual_keys.down && e->selection.end.pn < e->paragraphs) {
            uic_edit_key_down(e, e->selection.end.pn, e->selection.end.gp);
        } else if (key == virtual_keys.up && e->paragraphs > 0) {
            uic_edit_key_up(e, e->selection.end.pn, e->selection.end.gp);
        } else if (key == virtual_keys.left) {
            uic_edit_key_left(e, e->selection.end.pn, e->selection.end.gp);
        } else if (key == virtual_keys.right) {
            uic_edit_key_right(e, e->selection.end.pn, e->selection.end.gp);
        }
    }
}

static void uic_edit_keyboard(uic_t* unused(ui), int32_t ch) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden && !ui->disabled);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        ui->invalidate(ui);
    }
}

static bool uic_edit_message(uic_t* ui, int32_t unused(message),
        int64_t unused(wp), int64_t unused(lp), int64_t* unused(rt)){
    assert(ui->tag == uic_tag_edit);
    uic_edit_check_focus((uic_edit_t*)ui);
    return false;
}

#define glyph_teddy_bear  "\xF0\x9F\xA7\xB8"
#define glyph_chinese_one "\xE5\xA3\xB9"
#define glyph_chinese_two "\xE8\xB4\xB0"
#define glyph_teddy_bear  "\xF0\x9F\xA7\xB8"
#define glyph_ice_cube    "\xF0\x9F\xA7\x8A"


static void uic_edit_init_with_text(uic_edit_t* e, const char* s, int32_t n) {
    fatal_if(e->paragraphs != 0);
    static const char empty[] = {0x00};
    for (int pass = 0; pass <= 1; pass++) {
        int32_t i = 0;
        const char* text = s;
        int32_t paragraphs = 0;
        while (i < n) {
            int32_t b = i;
            while (b < n && s[b] != '\n') { b++; }
            int32_t next = b + 1;
            if (b > i && s[b - 1] == '\r') { b--; } // CR LF
            if (pass == 0) {
                paragraphs++;
            } else {
                uic_edit_append_paragraph(e, b == i ? empty : text, b - i);
            }
            text = s + next;
            i = next;
        }
        if (pass == 0) {
            e->allocated = (paragraphs + 1023) / 1024 * 1024;
            e->allocated *= sizeof(uic_edit_para_t); // bytes
            e->para = malloc(e->allocated);
            fatal_if_null(e->para);
        }
    }
}

#define lorem_ipsum \
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Mauris blandit "    \
    "neque elementum felis ultricies, sed facilisis magna bibendum. Nullam "      \
    "blandit est cursus, vulputate orci eget, laoreet turpis. Etiam non purus "   \
    "ut tellus facilisis semper vel ut nisl. Quisque sagittis posuere ligula "    \
    "nec congue. Nunc pulvinar tincidunt dapibus. Vestibulum ullamcorper risus "  \
    "id ultrices vestibulum. Duis ac accumsan massa. Proin quis ex orci.  Sed "   \
    "tristique sagittis risus scelerisque iaculis. Duis tincidunt dictum mi, "    \
    "id venenatis risus mollis non. Nulla turpis libero, vestibulum sed urna "    \
    "eu, molestie porttitor orci. Fusce et nunc molestie, facilisis libero "      \
    "eget, egestas massa. Etiam lobortis lacus ut lacus rhoncus, in tincidunt "   \
    "tortor vehicula.\n"                                                          \
    "\n"                                                                          \
    "Sed efficitur gravida velit, nec finibus nibh malesuada non. Duis et ex "    \
    "tristique, accumsan tellus eu, venenatis nisl. Fusce vel egestas ante, "     \
    "interdum egestas velit. Sed laoreet convallis malesuada. Sed hendrerit ex "  \
    "nec tincidunt tempor. Morbi pulvinar ultricies mi, sed congue lacus congue " \
    "non. Mauris nec pulvinar tellus. Cras luctus eget dui nec fringilla. "       \
    "Cras non tortor vitae ipsum consequat placerat vel eu dolor. Proin posuere " \
    "turpis eros, eu fringilla ligula iaculis in. Cras nec convallis felis. "     \
    "Ut lorem enim, efficitur at augue vitae, tincidunt semper ligula. Morbi "    \
    "sit amet dolor quis felis fringilla tincidunt. Integer bibendum mauris "     \
    "nec metus varius, sit amet suscipit eros varius. Integer arcu nulla, "       \
    "efficitur sit amet commodo et, mattis ut purus.\n"                           \
    "\n"                                                                          \
    "Maecenas finibus posuere tortor, ac scelerisque nulla rhoncus et. "          \
    "Pellentesque vel velit ac felis ullamcorper tempor non vel felis. "          \
    "Donec ex diam, volutpat vel lobortis ut, ullamcorper ut nisi. Maecenas "     \
    "facilisis magna ornare orci placerat, id iaculis quam dapibus. Ut varius "   \
    "ante sed efficitur accumsan. Curabitur pulvinar pharetra orci at "           \
    "venenatis.Vestibulum ac tortor velit.\n"                                     \
    "\n"                                                                          \
    "Quisque quam quam, aliquet nec blandit eget, vestibulum et mi. Curabitur "   \
    "vel augue volutpat, facilisis ligula fringilla, dignissim orci. "            \
    "Sed lobortis nisi mauris, eget consectetur enim consequat ac. Pellentesque " \
    "vitae tellus vel turpis pretium dapibus nec eget odio. Pellentesque "        \
    "aliquam ultrices dolor, placerat porta metus iaculis sit amet. Vivamus at "  \
    "finibus dui. Nullam eu interdum nisi, et ultricies sem. Curabitur consequat "\
    "pretium magna, eu mattis urna aliquet non. In eget pulvinar erat. Praesent " \
    "mollis arcu nec augue placerat, et dictum elit dictum. Ut in mi at nisl "    \
    "laoreet hendrerit ut non orci. Etiam congue efficitur leo, at euismod nisi " \
    "vehicula non.\n"                                                             \
    "\n"                                                                          \
    "Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere "   \
    "cubilia curae; Integer in odio dignissim, fringilla quam vel, lacinia nibh. "\
    "Cras ligula lacus, commodo ac urna at, varius consequat quam. Duis lacinia " \
    "consectetur faucibus. Vestibulum fringilla diam sem, ac iaculis neque "      \
    "sollicitudin mollis. Vivamus aliquam vitae mi a venenatis. Vestibulum sit "  \
    "amet tellus dapibus, lobortis justo quis, semper sem. Vestibulum tincidunt " \
    "lorem enim, ut vestibulum eros varius id.\n"                                 \

static const char* test_content =
    "Hello World!\n"
    "Good bye Universe...\n"
    "\n"
    "0         10        20        30        40        50        60        70        80        90\n"
    "01234567890123456789012345678901234567890abcdefghi01234567890123456789012345678901234567890123456789\n"
    "0         10        20        30        40        50        60        70        80        90\n"
    "01234567890123456789012345678901234567890abcdefghi01234567890123456789012345678901234567890123456789\n"
    "\n"
    "0" glyph_chinese_one glyph_chinese_two "3456789\n"
    "\n"
    glyph_teddy_bear "\n"
    glyph_teddy_bear glyph_ice_cube glyph_teddy_bear glyph_ice_cube glyph_teddy_bear glyph_ice_cube "\n"
    glyph_teddy_bear glyph_ice_cube glyph_teddy_bear " - " glyph_ice_cube glyph_teddy_bear glyph_ice_cube "\n"
    "\n"
    lorem_ipsum
    "";

void uic_edit_init(uic_edit_t* e) {
    memset(e, 0, sizeof(*e));
    uic_init(&e->ui);
    e->ui.tag = uic_tag_edit;
    uic_edit_init_with_text(e, test_content, (int32_t)strlen(test_content));
//  e->para[0].text = strdup("Hello World!");
//  e->para[1].text = strdup("Good bye Universe...");
//  e->para[2].text = strdup("");
//  e->para[3].text = strdup(glyph_teddy_bear);
//  e->para[4].text = strdup(glyph_teddy_bear "\x20" glyph_ice_cube);
//  for (int i = 0; i < 10; i += 2) {
//      e->para[5 + i].text = strdup(glyph_teddy_bear "0         10        20        30        40        50        60        70        80        90");
//      e->para[6 + i].text = strdup(glyph_teddy_bear "01234567890123456789012345678901234567890abcdefghi01234567890"
//                                    glyph_chinese_one glyph_chinese_two                                            "3456789012345678901234567890123456789"
//                                    glyph_ice_cube);
//  }
//  e->paragraphs = 15;
//  for (int i = 0; i < e->paragraphs; i++) {
//      e->para[i].bytes = (int32_t)strlen(e->para[i].text);
//  }
    e->multiline   = true;
    e->monospaced  = false;
    e->wordbreak   = true;
    e->ui.color    = rgb(168, 168, 150); // colors.text;
    e->ui.font     = &app.fonts.H1;
    e->ui.paint    = uic_edit_paint;
    e->ui.measure  = uic_edit_measure;
    e->ui.layout   = uic_edit_layout;
    e->ui.mouse    = uic_edit_mouse;
    e->ui.key_down = uic_edit_key_pressed;
    e->ui.keyboard = uic_edit_keyboard;
    e->ui.message  = uic_edit_message;
    // Expected manifest.xml containing UTF-8 code page
    // for Translate message and WM_CHAR to deliver UTF-8 characters
    // see: https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
    if (GetACP() != 65001) {
        traceln("codepage: %d UTF-8 will not be supported", GetACP());
    }
    // at the moment of writing there is no API call to inform Windows about process
    // prefered codepage except manifest.xml file in resource #1.
    // Absence of manifest.xml will result to ancient and useless ANSI 1252 codepage
    // TODO: may be change quick.h to use CreateWindowW() and translate UTF16 to UTF8
}

end_c


