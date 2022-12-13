/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"
#include <Windows.h>

begin_c

// Glyphs in monospaced Windows fonts may have different width for non-ASCII
// characters. Thus even if edit is monospaced glyph measurements are used
// in text layout.

typedef struct uic_edit_run_s {
    int32_t bytes;
    int32_t glyphs;
    int32_t pixels;
    int32_t cl; // number of glyph from the start of paragraph
} uic_edit_run_t;

typedef struct uic_edit_runs_s {
    int32_t count;
    // only needed for vertically visible runs by there can be a lot runs
    // e.g. for a very long single line obfuscated .js file
    uic_edit_run_t run[1024]; // TODO: allocate on heap, reuse and dispose on measure
} uic_edit_runs_t;

static void uic_edit_set_caret(uic_edit_t* e, int32_t ln, int32_t cl);

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
//      traceln("%d %d %d %d %d %.*s", i, j, k, px, w, gp[k], text);
        if (px == width) { break; }
        if (px < width) { i = k + 1; } else { j = k; }
        k = (i + j) / 2;
    }
//  int32_t wi = gdi.measure_text(f, "%.*s", i, text).x;
//  int32_t wj = gdi.measure_text(f, "%.*s", j, text).x;
//  int32_t wk = gdi.measure_text(f, "%.*s", k, text).x;
//  traceln("i %d j %d k %d %d %d %d", i, j, k, wi, wj, wk);
    return k;
}

static int32_t uic_edit_glyph_at_x(font_t f, char* text, int32_t bytes,
        int32_t x) {
    if (x == 0 || bytes == 0) {
        return 0;
    } else {
        int32_t* gp = (int32_t*)stackalloc(bytes * sizeof(int32_t));
        int gc = uic_edit_glyph_pos(text, bytes, gp);
        return gc <= 1 ? gc : uic_edit_break(f, text, x, gp, gc);
    }
}

// uic_edit::layout_pragraph breaks paragraph into `runs` according to `width`
// and `.wordbreak`, `.singleline`

static void uic_edit_layout_pragraph(uic_edit_t* e, int32_t p, int32_t width,
        uic_edit_runs_t* r) {
    const uic_edit_para_t* para = &e->para[p];
    font_t f = *e->ui.font;
    int32_t pixels = gdi.measure_text(f, "%.*s", para->bytes, para->text).x;
    if (pixels <= width) { // whole para fits into width
        r->count = 1;
        r->run[0].cl = 0;
        r->run[0].bytes = para->bytes;
        r->run[0].glyphs = uic_edit_glyphs(para->text, para->bytes);
        r->run[0].pixels = pixels;
    } else {
        int32_t rc = 0;  // runw count
        int32_t cl = 0; // column aka glyph count
        char* text = para->text;
        int32_t bytes = para->bytes;
        int32_t* gp = (int32_t*)stackalloc(bytes * sizeof(int32_t));
        // glyph position (offset in bytes)
        while (rc < countof(r->run) && bytes > 0) {
            int32_t gc = uic_edit_glyph_pos(text, bytes, gp);
            assert(gc > 0);
            // expected at least one glyph
            int32_t glyphs = gc == 1 ? 1 : uic_edit_break(f, text, width, gp, gc);
            assert(0 <= glyphs - 1 && glyphs - 1 < gc);
            int32_t utf8bytes = gp[glyphs - 1];
            pixels = gdi.measure_text(f, "%.*s", utf8bytes, text).x;
            if (glyphs > 1 && pixels > width) {
                // try to find word break SPACE character. utf8 space is 0x20
                int32_t i = utf8bytes;
                while (i > 0 && text[i - 1] != 0x20) { i--; }
                if (i > 0 && i != utf8bytes) {
                    utf8bytes = i;
                    glyphs = uic_edit_glyphs(text, utf8bytes);
                    pixels = gdi.measure_text(f, "%.*s", utf8bytes, text).x;
                }
            }
            r->run[rc].cl = cl;
            r->run[rc].bytes = utf8bytes;
            r->run[rc].glyphs = glyphs;
            r->run[rc].pixels = pixels;
//          traceln("run[%d].bytes: %d .glyphs: %d .pixels: %d %.*s", rc, r->run[rc].bytes, r->run[rc].glyphs, r->run[rc].pixels, r->run[rc].bytes, text);
            rc++;
            text += utf8bytes;
            assert(0 <= utf8bytes && utf8bytes <= bytes);
            bytes -= utf8bytes;
            cl += glyphs;
//          if (bytes > 0) {
//              int32_t pixels = gdi.measure_text(f, "%.*s", bytes, text).x;
//              if (pixels < width) {
//                  r->run[c].cl = cl;
//                  r->run[c].bytes = bytes;
//                  r->run[c].glyphs = uic_edit_glyphs(text, bytes);
//                  r->run[c].pixels = pixels;
//                  traceln("run[%d].bytes: %d glyphs: %d pixels: %d", c, r->run[c].bytes, r->run[c].glyphs, r->run[c].pixels);
//                  c++;
//              }
//              break;
//          }
        }
        r->count = rc;
    }
    assert(r->count >= 1);
//  traceln("runs=%d", r->count);
}

static void uic_edit_create_caret(uic_edit_t* e) {
//  traceln("");
    fatal_if_false(CreateCaret((HWND)app.window, null, 2, e->ui.em.y));
    fatal_if_false(ShowCaret((HWND)app.window));
    fatal_if_false(SetCaretBlinkTime(400));
    e->focused = true;
}

static void uic_edit_destroy_caret(uic_edit_t* e) {
//  traceln("");
    fatal_if_false(HideCaret((HWND)app.window));
    fatal_if_false(DestroyCaret());
    e->focused = false;
}

static void uic_edit_measure(uic_t* ui) { // bottom up
    ui->em = gdi.get_em(*ui->font);
    assert(ui->tag == uic_tag_edit);
    // enforce minimum size - this makes it simpler to check corner cases
    if (ui->w < ui->em.x * 3) { ui->w = ui->em.x * 4; }
    if (ui->h < ui->em.y) { ui->h = ui->em.y; }
}

static void uic_edit_layout(uic_t* ui) { // top down
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
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
            uic_edit_set_caret(e, e->selection.end.ln, e->selection.end.cl);
        }
    }
}

static uint64_t uic_edit_pos(int32_t ln, int32_t cl) {
    assert(ln >= 0 && cl >= 0);
    return ((uint64_t)ln << 32) | (uint64_t)cl;
}

static ui_point_t uic_edit_lc_to_xy(uic_edit_t* e, int32_t ln, int32_t cl) {
    ui_point_t pt = {-1, 0};
    pt.y = 0;
    uic_edit_runs_t r;
    int32_t sc = e->scroll.cl;
    for (int i = e->scroll.ln; i < e->paragraphs && pt.x < 0; i++) {
        uic_edit_layout_pragraph(e, i, e->ui.w, &r);
        char* text = e->para[i].text;
        int32_t bytes = e->para[i].bytes;
        int column = 0;
        for (int j = 0; j < r.count; j++) {
            bool last_run = j == r.count - 1; // special case
            int32_t gc = r.run[j].glyphs;
//          traceln("p#r: %d#%d gc: %d y: [%d..%d[", i, j, gc, pt.y, pt.y +  + e->ui.em.y);
            if (column >= sc) {
                sc = 0;
                if (i == ln) {
//                  if (gc == 0) {
//                      traceln("column: %d cl: %d column + gc: %d", column, cl, column + gc);
//                  }
                    // in the last `run` of a parageraph x after last glyph OK:
                    bool inside =
                        last_run && column <= cl && cl < column + gc + 1 ||
                        column <= cl && cl < column + gc;
                    if (inside) {
                        int32_t offset = uic_edit_column_to_offset(text, r.run[j].bytes, cl - column);
                        pt.x = gdi.measure_text(*e->ui.font, "%.*s", offset, text).x;
                        break;
                    }
                }
                pt.y += e->ui.em.y;
            }
            column += gc;
            text += r.run[j].bytes;
            bytes -= r.run[j].bytes;
        }
        if (i == ln && pt.x < 0) {
            traceln("%d", uic_edit_glyphs(e->para[ln].text, e->para[ln].bytes));
        }
    }
    if (pt.x < 0) { // after last paragraph?
        assert(ln == e->paragraphs && cl == 0, "ln#cl: %d#%d", ln, cl);
        pt.x = 0;
    }
//  traceln("lc: %d:%d x,y: %d,%d ", ln, cl, pt.x, pt.y);
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

static uic_edit_lc_t uic_edit_xy_to_lc(uic_edit_t* e, int32_t x, int32_t y) {
    font_t f = *e->ui.font;
    uic_edit_lc_t p = {-1, -1};
    int32_t line_y = 0;
    int32_t sc = e->scroll.cl;
//  traceln("x: %d y: %d", x, y);
    for (int i = e->scroll.ln; i < e->paragraphs && p.ln < 0; i++) {
        uic_edit_runs_t r;
        uic_edit_layout_pragraph(e, i, e->ui.w, &r);
        char* s = e->para[i].text;
        int32_t bytes = e->para[i].bytes;
        int column = 0;
        for (int j = 0; j < r.count; j++) {
            int32_t gc = r.run[j].glyphs;
//          traceln("p#r: %d#%d gc: %d y: [%d..%d[", i, j, gc, line_y, line_y +  + e->ui.em.y);
            if (column >= sc) {
                sc = 0;
                if (line_y <= y && y < line_y + e->ui.em.y) {
                    int32_t w = gdi.measure_text(f, "%.*s", r.run[j].bytes, s).x;
                    int32_t cl_in_run = uic_edit_glyph_at_x(f, s, r.run[j].bytes, x);
                    p.ln = i;
                    p.cl = column + cl_in_run;
                    // allow mouse click past half last glyph
                    int32_t cw = uic_edit_glyph_width_px(e, p.ln, p.cl);
                    if (cw > 0 && x >= w - cw / 2) {
                        p.cl++; // snap to closest glyph's 'x'
                    }
//                  traceln("x: %d w: %d cw: %d %d:%d", x, w, cw, p.ln, p.cl);
                    break;
                }
                line_y += e->ui.em.y;
            }
            column += gc;
            s += r.run[j].bytes;
            bytes -= r.run[j].bytes;
        }
        if (line_y > e->ui.h) { break; }
    }
//  traceln("x,y: %d,%d lc: %d:%d", x, y, p.ln, p.cl);
    return p;
}

static void uic_edit_paint_selection(uic_edit_t* e, int32_t ln, int32_t c0, int32_t c1) {
    uint64_t s0 = uic_edit_pos(e->selection.fro.ln,
        e->selection.fro.cl);
    uint64_t e0 = uic_edit_pos(e->selection.end.ln,
        e->selection.end.cl);
    if (s0 > e0) {
        uint64_t swap = e0;
        e0 = s0;
        s0 = swap;
    }
    uint64_t s1 = uic_edit_pos(ln, c0);
    uint64_t e1 = uic_edit_pos(ln, c1);
    if (s0 <= e1 && s1 <= e0) {
        uint64_t start = max(s0, s1);
        uint64_t end = min(e0, e1);
        if (start < end) {
            char* text = e->para[ln].text;
            int32_t bytes = e->para[ln].bytes;
            int32_t ofs0 = uic_edit_column_to_offset(text, bytes, (int32_t)(start & 0xFFFFFFFF));
            int32_t ofs1 = uic_edit_column_to_offset(text, bytes, (int32_t)(end   & 0xFFFFFFFF));
            int32_t x0 = gdi.measure_text(*e->ui.font, "%.*s", ofs0, text).x;
            int32_t x1 = gdi.measure_text(*e->ui.font, "%.*s", ofs1, text).x;
//          traceln("line %d selection: %016llX..%016llX  %d %d  %d %d", ln, s0, e0, ofs0, ofs1, x0, x1);
            brush_t b = gdi.set_brush(gdi.brush_color);
            color_t c = gdi.set_brush_color(rgb(48, 64, 72));
            gdi.fill(gdi.x + x0, gdi.y, x1 - x0, e->ui.em.y);
            gdi.set_brush_color(c);
            gdi.set_brush(b);
        }
    }
}

static void uic_edit_paint_para(uic_edit_t* e, int32_t ln, int32_t sc) {
    uic_edit_para_t* line = &e->para[ln];
    uic_edit_runs_t r;
    uic_edit_layout_pragraph(e, ln, e->ui.w, &r);
    int32_t bytes = line->bytes;
    char* s = line->text;
    int32_t cl = 0;
    int32_t aw = 0; // accumulated width
    for (int i = 0; i < r.count && gdi.y < e->ui.y + e->bottom; i++) {
        int32_t gc = r.run[i].glyphs; // glyph count
        if (cl >= sc) {
            gdi.x -= aw;
            uic_edit_paint_selection(e, ln, cl, cl + gc);
            gdi.x += aw;
            int32_t x = gdi.x;
            gdi.text("%.*s", r.run[i].bytes, s);
            int32_t w = gdi.x - x;
            aw += w;
            gdi.x -= w;
            gdi.y += e->ui.em.y;
        }
        cl += gc;
        s += r.run[i].bytes;
        bytes -= r.run[i].bytes;
    }
}

static void uic_edit_paint(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(rgb(20, 20, 14));
    gdi.fill(0, 0, ui->w, ui->h);
    gdi.push(ui->x, ui->y + e->top);
    gdi.set_clip(ui->x, ui->y, ui->w, ui->h);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    gdi.set_font(f);
    gdi.set_text_color(ui->color);
    const int32_t bottom = ui->y + e->bottom;
    assert(e->scroll.ln <= e->paragraphs);
    int32_t sc = e->scroll.cl;
    for (int i = e->scroll.ln; i < e->paragraphs && gdi.y < bottom; i++) {
        uic_edit_paint_para(e, i, sc);
        sc = 0;
    }
    gdi.set_clip(0, 0, 0, 0);
    gdi.pop();
}

static void uic_edit_set_caret(uic_edit_t* e, int32_t ln, int32_t cl) {
    assert(e->focused && app.focused);
    ui_point_t pt = e->ui.w > 0 ? // ui.w == 0 means  no measure/layout yet
        uic_edit_lc_to_xy(e, ln, cl) : (ui_point_t){0, 0};
//  traceln("%d:%d (%d,%d)", ln, cl, pt.x, pt.y);
    fatal_if_false(SetCaretPos(pt.x, pt.y + e->top));
    e->selection.end.ln = ln;
    e->selection.end.cl = cl;
    if (!app.shift) {
        e->selection.fro = e->selection.end;
    } else {
//      traceln("selection %d:%d %d:%d",
//          e->selection.fro.ln, e->selection.fro.cl,
//          e->selection.end.ln, e->selection.end.cl);
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
                uic_edit_set_caret(e, e->selection.end.ln, e->selection.end.cl);
            }
        } else {
            if (e->focused) { uic_edit_destroy_caret(e); }
        }
    }
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
    if (inside && left || right) {
        app.focus = ui;
        uic_edit_check_focus(e);
        uic_edit_lc_t p = uic_edit_xy_to_lc(e, x, y);
        if (0 <= p.ln && 0 <= p.cl) {
            int32_t ln   = p.ln;
            int32_t cl = p.cl;
            if (ln > e->paragraphs) { ln = max(0, e->paragraphs); }
            int32_t chars = uic_edit_glyphs_in_paragraph(e, ln);
            if (cl > chars) { cl = max(0, chars); }
            traceln("%d %d [%d:%d]", x, y, ln, cl);
            uic_edit_set_caret(e, ln, cl);
        }
    }
}

static void uic_edit_scroll_up(uic_edit_t* e) {
//  traceln("scroll up");
    uic_edit_lc_t lc = {e->scroll.ln, -1};
    uic_edit_runs_t r;
    uic_edit_para_t* line = &e->para[e->scroll.ln];
    uic_edit_layout_pragraph(e, e->scroll.ln, e->ui.w, &r);
    char* s = line->text;
    int32_t c = 0;
    for (int i = 0; i < r.count && lc.cl < 0; i++) {
        // glyph count:
        int32_t gc = r.run[i].glyphs;
        if (c > e->scroll.cl) {
            lc.cl = c;
        }
        s += r.run[i].bytes;
        c += gc;
    }
    if (lc.cl < 0) {
        lc.ln++;
        lc.cl = 0;
    }
    assert(lc.ln >= 0 && lc.cl >= 0);
    e->scroll.ln = lc.ln;
    e->scroll.cl = lc.cl;
}

static void uic_edit_scroll_down(uic_edit_t* e) {
//  traceln("scroll down");
    uic_edit_runs_t r;
    if (e->scroll.cl == 0 && e->scroll.ln > 0) {
        e->scroll.ln--;
        uic_edit_layout_pragraph(e, e->scroll.ln, e->ui.w, &r);
        e->scroll.cl = r.run[r.count - 1].cl;
    } else if (e->scroll.cl > 0 && e->scroll.ln > 0) {
        uic_edit_layout_pragraph(e, e->scroll.ln, e->ui.w, &r);
        for (int i = 0; i < r.count; i++) {
            if (r.run[i].cl == e->scroll.cl) {
                e->scroll.cl = i > 0 ? r.run[i - 1].cl : 0;
            }
        }
    }
}

static void uic_edit_scroll_into_view(uic_edit_t* e, int32_t ln, int32_t cl) {
    int32_t visible_lines = (e->ui.h + e->ui.em.y - 1) / e->ui.em.y;
    ui_point_t pt = uic_edit_lc_to_xy(e, ln, cl);
    if (pt.y >= visible_lines * e->ui.em.y) {
        uic_edit_scroll_up(e);
    } else if (pt.y < 0) {
        uic_edit_scroll_down(e);
    } else {
    }
}

static void uic_edit_key_left(uic_edit_t* e, int32_t ln, int32_t cl) {
    if (ln > 0 || cl > 0) {
        if (e->scroll.ln == ln && e->scroll.cl == cl) {
            uic_edit_scroll_down(e);
        }
        if (cl > 0) {
            cl--;
        } else if (ln > 0) {
            ln--;
            cl = uic_edit_glyphs_in_paragraph(e, ln);
        }
        uic_edit_set_caret(e, ln, cl);
    }
}

static void uic_edit_key_right(uic_edit_t* e, int32_t ln, int32_t cl) {
    if (ln < e->paragraphs) {
        int32_t g_in_p = uic_edit_glyphs_in_paragraph(e, ln);
//      traceln("p#l: %d:%d  glyphs on paragaraph %d", cl, ln, g_in_p);
        if (cl < g_in_p) {
            ui_point_t pt = uic_edit_lc_to_xy(e, ln, cl + 1);
            if (pt.y + e->ui.em.y > e->bottom) { uic_edit_scroll_up(e); }
            cl++;
        } else {
            ui_point_t pt = uic_edit_lc_to_xy(e, ln + 1, 0);
            if (pt.y + e->ui.em.y > e->bottom) { uic_edit_scroll_up(e); }
            ln++;
            cl = 0;
        }
        uic_edit_set_caret(e, ln, cl);
    }
}

static void uic_edit_key_up(uic_edit_t* e, int32_t ln, int32_t cl) {
    if (ln == e->paragraphs) {
        assert(cl == 0); // positioned past EOF
        ln--;
        cl = uic_edit_glyphs(e->para[ln].text, e->para[ln].bytes);
        ui_point_t pt = uic_edit_lc_to_xy(e, ln, cl);
        pt.y -= 1;
        cl = uic_edit_xy_to_lc(e, pt.x, pt.y).cl;
    } else {
        ui_point_t pt = uic_edit_lc_to_xy(e, ln, cl);
        pt.y -= 1;
        if (pt.y < e->ui.y) {
            uic_edit_scroll_down(e);
            pt = uic_edit_lc_to_xy(e, ln, cl);
            pt.y -= 1;
        }
        uic_edit_lc_t lc = uic_edit_xy_to_lc(e, pt.x, pt.y);
        if (lc.ln >= 0 && lc.cl >= 0) {
            ln = lc.ln;
            cl = lc.cl;
        }
    }
    uic_edit_set_caret(e, ln, cl);
}

static void uic_edit_key_down(uic_edit_t* e, int32_t ln, int32_t cl) {
    ui_point_t pt = uic_edit_lc_to_xy(e, ln, cl);
//  traceln("%d:%d %d,%d -> %d", ln, cl, pt.x, pt.y, pt.y + ui->em.y + 1);
    pt.y += e->ui.em.y + 1;
    uic_edit_lc_t plc = uic_edit_xy_to_lc(e, pt.x, pt.y);
    if (plc.ln >= 0 && plc.cl >= 0) {
        ln = plc.ln;
        cl = plc.cl;
    } else if (ln == e->paragraphs - 1) {
        ln = e->paragraphs; // advance past EOF
        cl = 0;
    }
    if (pt.y + e->ui.em.y > e->bottom) {
        uic_edit_scroll_up(e);
    }
    uic_edit_set_caret(e, ln, cl);
}

static void uic_edit_key_pressed(uic_t* ui, int32_t key) {
    // TODO: vertical movement snap to the closest glyph after moving
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        if (key == VK_ESCAPE) { app.close(); }
        if (key == virtual_keys.down && e->selection.end.ln < e->paragraphs) {
            uic_edit_key_down(e, e->selection.end.ln, e->selection.end.cl);
        } else if (key == virtual_keys.up && e->paragraphs > 0) {
            uic_edit_key_up(e, e->selection.end.ln, e->selection.end.cl);
        } else if (key == virtual_keys.left) {
            uic_edit_key_left(e, e->selection.end.ln, e->selection.end.cl);
        } else if (key == virtual_keys.right) {
            uic_edit_key_right(e, e->selection.end.ln, e->selection.end.cl);
        }
    }
}

static void uic_edit_keyboard(uic_t* unused(ui), int32_t ch) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden && !ui->disabled);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
//      traceln("char=%08X", ch);
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

void uic_edit_init(uic_edit_t* e) {
    uic_init(&e->ui);
    e->ui.tag = uic_tag_edit;
    e->para[0].text = strdup("Hello World!");
    e->para[1].text = strdup("Good bye Universe...");
    e->para[2].text = strdup("");
    e->para[3].text = strdup(glyph_teddy_bear);
    e->para[4].text = strdup(glyph_teddy_bear "\x20" glyph_ice_cube);
    for (int i = 0; i < 10; i += 2) {
        e->para[5 + i].text = strdup(glyph_teddy_bear "0         10        20        30        40        50        60        70        80        90");
        e->para[6 + i].text = strdup(glyph_teddy_bear "01234567890123456789012345678901234567890abcdefghi01234567890"
                                      glyph_chinese_one glyph_chinese_two                                            "3456789012345678901234567890123456789"
                                      glyph_ice_cube);
    }
    e->paragraphs = 15;
    for (int i = 0; i < e->paragraphs; i++) {
        e->para[i].bytes = (int32_t)strlen(e->para[i].text);
    }
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
}

end_c
