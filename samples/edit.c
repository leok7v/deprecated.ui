/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"
#include <Windows.h>


// http://worrydream.com/refs/Tesler%20-%20A%20Personal%20History%20of%20Modeless%20Text%20Editing%20and%20Cut-Copy-Paste.pdf
// https://web.archive.org/web/20221216044359/http://worrydream.com/refs/Tesler%20-%20A%20Personal%20History%20of%20Modeless%20Text%20Editing%20and%20Cut-Copy-Paste.pdf

// Rich text options that are not addressed yet:
// * Color of ranges (usefull for code editing)
// * Soft line breaks inside the paragraph (useful for e.g. bullet lists of options)
// * Bold/Italic/Underline (along with color ranges)
// * Multifont (as long as run vertical size is the maximum of font)
// * Kerning (?! like in overhung "Fl"

begin_c

// Glyphs in monospaced Windows fonts may have different width for non-ASCII
// characters. Thus even if edit is monospaced glyph measurements are used
// in text layout.

static uint64_t uic_edit_uint64(int32_t high, int32_t low) {
    assert(high >= 0 && low >= 0);
    return ((uint64_t)high << 32) | (uint64_t)low;
}

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

// uic_edit_g2b() return number of glyphs in text and fills optional
// g2b[] array with glyphs positions. However g2b[0] is length of first
// glyph, search for usages to see how it is a bit unorthodox.
// TODO: changing it to positions makes some code simpler (not checking
//       for gp == 0 and some code stackalloc() +1 position of natural.
//       may consider changing it.

static int32_t uic_edit_g2b(const char* utf8, int32_t bytes, int32_t g2b[]) {
    int i = 0;
    int k = 0;
    // g2b[k] start postion in byte offset from utf8 text of glyph next after [k]
    // e.g. if uic_edit_glyph_bytes(utf8[0]) == 3 g2b[0] will be 3
    // obviously the first glyph offset is 0 and thus is not kept
    while (i < bytes) {
        i += uic_edit_glyph_bytes(utf8[i]);
        if (g2b != null) { g2b[k] = i; }
        k++;
    }
    return k;
}

static int32_t uic_edit_glyphs(const char* utf8, int32_t bytes) {
    return uic_edit_g2b(utf8, bytes, null);
}

static int32_t uic_edit_gp_to_bytes(const char* s, int32_t bytes, int32_t gp) {
    int32_t c = 0;
    int32_t i = 0;
    if (bytes > 0) {
        while (c < gp) {
            assert(i < bytes);
            i += uic_edit_glyph_bytes(s[i]);
            c++;
        }
    }
    assert(i <= bytes);
    return i;
}

static void uic_edit_allocate(void** pp, int32_t bytes, size_t element) {
    assert(*pp == null);
    *pp = malloc(bytes * element);
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
        uic_edit_allocate(pp, bytes, 1);
    } else {
        *pp = realloc(*pp, bytes);
        fatal_if_null(*pp);
    }
}

static int32_t uic_edit_break(font_t f, char* text, int32_t width,
        int32_t* g2b, int32_t gc) { // ~50 microseconds for 100 bytes
    // returns number of glyph (offset) from text of the last glyph that is
    // completely to the left of `w`
    assert(gc > 1, "cannot break single glyph");
    int i = 0;
    int j = gc;
    int32_t k = (i + j) / 2;
    while (i < j) {
        // important: g2b[0] is number of bytes in the first glyph:
        int32_t px = gdi.measure_text(f, "%.*s", g2b[k], text).x;
        if (px == width) { break; }
        if (px < width) { i = k + 1; } else { j = k; }
        k = (i + j) / 2;
    }
    return k;
}

static int32_t uic_edit_glyph_at_x(font_t f, char* s, int32_t n, int32_t x) {
    if (x == 0 || n == 0) {
        return 0;
    } else {
        // TODO: consider replacing stackalloc() with heap malloc() / free()
        //       pairs because 1MB stack limits number of glyphs per paragraph
        //       to about 200,000?
        int32_t* g2b = (int32_t*)stackalloc(n * sizeof(int32_t));
        int gc = uic_edit_g2b(s, n, g2b);
        return gc <= 1 ? gc : uic_edit_break(f, s, x + 1, g2b, gc); // +1 ???
    }
}

// uic_edit::layout_pragraph breaks paragraph into `runs` according to `width`
// and TODO: `.wordbreak`, `.singleline`

static const uic_edit_run_t* uic_edit_pragraph_runs(uic_edit_t* e, int32_t pn,
        int32_t* runs) {
    assert(e->width > 0);
    const uic_edit_run_t* r = null;
    if (pn == e->paragraphs) {
        static const uic_edit_run_t eof_run = { 0 };
        *runs = 1;
        r = &eof_run;
    } else {
        assert(0 <= pn && pn < e->paragraphs);
        uic_edit_para_t* para = &e->para[pn];
        if (para->run == null) {
            assert(para->runs == 0 && para->run == null && para->glyphs == 0);
            para->glyphs = uic_edit_glyphs(para->text, para->bytes);
            const int32_t max_runs = para->bytes + 1;
            uic_edit_allocate(&para->run, max_runs, sizeof(uic_edit_run_t));
            uic_edit_run_t* run = para->run;
            font_t f = *e->ui.font;
            int32_t pixels = gdi.measure_text(f, "%.*s", para->bytes, para->text).x;
            if (pixels <= e->width) { // whole para fits into width
                para->runs = 1;
                run[0].bp = 0;
                run[0].gp = 0;
                run[0].bytes = para->bytes;
                run[0].glyphs = para->glyphs;
                run[0].pixels = pixels;
            } else {
                int32_t rc = 0; // runw count
                int32_t ix = 0; // glyph index from to start of paragraph
                char* text = para->text;
                int32_t bytes = para->bytes;
                int32_t* pos = (int32_t*)stackalloc(bytes * sizeof(int32_t));
                // glyph position (offset in bytes)
                while (bytes > 0) {
                    assert(rc < rc < max_runs);
                    int32_t gc = uic_edit_g2b(text, bytes, pos);
                    assert(gc > 0);
                    // expected at least one glyph
                    int32_t glyphs = gc == 1 ?
                        1 : uic_edit_break(f, text, e->width, pos, gc);
                    assert(0 <= glyphs - 1 && glyphs - 1 < gc);
                    int32_t utf8bytes = pos[glyphs - 1];
                    pixels = gdi.measure_text(f, "%.*s", utf8bytes, text).x;
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
                    run[rc].bp = (int32_t)(text - para->text);
                    run[rc].gp = ix;
                    run[rc].bytes = utf8bytes;
                    run[rc].glyphs = glyphs;
                    run[rc].pixels = pixels;
                    rc++;
                    text += utf8bytes;
                    assert(0 <= utf8bytes && utf8bytes <= bytes);
                    bytes -= utf8bytes;
                    ix += glyphs;
                }
                para->runs = rc; // truncate heap allocated array:
                uic_edit_reallocate(&para->run, rc, sizeof(uic_edit_run_t));
            }
        }
        *runs = para->runs;
        r = para->run;
    }
    assert(r != null && *runs >= 1);
    return r;
}

static int32_t uic_edit_pragraph_run_count(uic_edit_t* e, int32_t pn) {
    int32_t runs = 0;
    (void)uic_edit_pragraph_runs(e, pn, &runs);
    return runs;
}

static int32_t uic_edit_glyphs_in_paragraph(uic_edit_t* e, int32_t pn) {
    (void)uic_edit_pragraph_run_count(e, pn); // word break paragraph into runs
    return e->para[pn].glyphs;
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

static void uic_edit_dispose_paragraphs_layout(uic_edit_t* e) {
    for (int32_t i = 0; i < e->paragraphs; i++) {
        if (e->para[i].run != null) {
            uic_edit_free(&e->para[i].run);
        }
        assert(e->para[i].run   == null);
        e->para[i].glyphs = 0;
        e->para[i].runs = 0;
    }
}

// Paragraph number, glyph number -> run number

static const uic_edit_pr_t uic_edit_pg_to_pr(uic_edit_t* e, const uic_edit_pg_t pg) {
    uic_edit_pr_t pr = { .pn = pg.pn, .rn = -1 };
    if (pg.pn == e->paragraphs || e->para[pg.pn].bytes == 0) { // last or empty
        assert(pg.gp == 0);
        pr.rn = 0;
    } else {
        assert(0 <= pg.pn && pg.pn < e->paragraphs);
        int32_t runs = 0;
        const uic_edit_run_t* run = uic_edit_pragraph_runs(e, pg.pn, &runs);
        if (pg.gp == e->para[pg.pn].glyphs + 1) {
            pr.rn = runs - 1; // past last glyph ??? is this correct?
        } else {
            assert(0 <= pg.gp && pg.gp <= e->para[pg.pn].glyphs);
            for (int32_t j = 0; j < runs && pr.rn < 0; j++) {
                const int last_run = j == runs - 1;
                const int32_t start = run[j].gp;
                const int32_t end = run[j].gp + run[j].glyphs + last_run;
                if (start <= pg.gp && pg.gp < end) {
                    pr.rn = j;
                }
            }
            assert(pr.rn >= 0);
        }
    }
    return pr;
}

static int32_t uic_edit_runs_between(uic_edit_t* e, const uic_edit_pg_t pg0,
        const uic_edit_pg_t pg1) {
    assert(uic_edit_uint64(pg0.pn, pg0.gp) <= uic_edit_uint64(pg1.pn, pg1.gp));
    int32_t rn0 = uic_edit_pg_to_pr(e, pg0).rn;
    int32_t rn1 = uic_edit_pg_to_pr(e, pg1).rn;
    int32_t rc = 0;
    for (int32_t i = pg0.pn; i <= pg1.pn; i++) {
        const int32_t runs = uic_edit_pragraph_run_count(e, i);
        if (i == pg0.pn) {
            rc += runs - rn0;
        } else if (i == pg1.pn) {
            rc += rn1;
        } else {
            rc += runs;
        }
    }
    return rc;
}

static uic_edit_pg_t uic_edit_scroll_pg(uic_edit_t* e) {
    int32_t runs = 0;
    const uic_edit_run_t* run = uic_edit_pragraph_runs(e, e->scroll.pn, &runs);
    assert(0 <= e->scroll.rn && e->scroll.rn < runs);
    return (uic_edit_pg_t) { .pn = e->scroll.pn, .gp = run[e->scroll.rn].gp };
}

static int32_t uic_edit_first_visible_run(uic_edit_t* e, int32_t pn) {
    return pn == e->scroll.pn ? e->scroll.rn : 0;
}

// uic_edit::pg_to_xy() paragraph # glyph # -> (x,y) in [0,0  width x heigth]

static ui_point_t uic_edit_pg_to_xy(uic_edit_t* e, const uic_edit_pg_t pg) {
    ui_point_t pt = { .x = -1, .y = 0 };
    for (int32_t i = e->scroll.pn; i < e->paragraphs && pt.x < 0; i++) {
        int32_t runs = 0;
        const uic_edit_run_t* run = uic_edit_pragraph_runs(e, i, &runs);
        for (int32_t j = uic_edit_first_visible_run(e, i); j < runs; j++) {
            const int last_run = j == runs - 1;
            int32_t gc = run[j].glyphs;
            if (i == pg.pn) { // in the last `run` of a parageraph x after last glyph is OK
                if (run[j].gp <= pg.gp && pg.gp < run[j].gp + gc + last_run) {
                    const char* s = e->para[i].text + run[j].bp;
                    int32_t ofs = uic_edit_gp_to_bytes(s, run[j].bytes,
                        pg.gp - run[j].gp);
                    pt.x = gdi.measure_text(*e->ui.font, "%.*s", ofs, s).x;
                    break;
                }
            }
            pt.y += e->ui.em.y;
        }
    }
    if (pg.pn == e->paragraphs) { pt.x = 0; }
    if (0 <= pt.x && pt.x < e->width && 0 <= pt.y && pt.y <= e->height) {
        // all good, inside visible rectangle or right after it
    } else {
        traceln("outside (%d,%d) %dx%d", pt.x, pt.y, e->width, e->height);
    }
    return pt;
}

static int32_t uic_edit_glyph_width_px(uic_edit_t* e, const uic_edit_pg_t pg) {
    char* text = e->para[pg.pn].text;
    int32_t gc = e->para[pg.pn].glyphs;
    if (pg.gp == 0 &&  gc == 0) {
        return 0; // empty paragraph
    } else if (pg.gp < gc) {
        char* s = text + uic_edit_gp_to_bytes(text, e->para[pg.pn].bytes, pg.gp);
        int32_t bytes_in_glyph = uic_edit_glyph_bytes(*s);
        return gdi.measure_text(*e->ui.font, "%.*s", bytes_in_glyph, s).x;
    } else {
        assert(pg.gp == gc, "only next position past last glyph is allowed");
        return 0;
    }
}

// uic_edit::xy_to_pg() (x,y) (0,0, widh x height) -> paragraph # glyph #

static uic_edit_pg_t uic_edit_xy_to_pg(uic_edit_t* e, int32_t x, int32_t y) {
    font_t f = *e->ui.font;
    uic_edit_pg_t pg = {-1, -1};
    int32_t py = 0; // paragraph `y' coordinate
    for (int32_t i = e->scroll.pn; i < e->paragraphs && pg.pn < 0; i++) {
        int32_t runs = 0;
        const uic_edit_run_t* run = uic_edit_pragraph_runs(e, i, &runs);
        for (int32_t j = uic_edit_first_visible_run(e, i); j < runs && pg.pn < 0; j++) {
            const uic_edit_run_t* r = &run[j];
            char* s = e->para[i].text + run[j].bp;
            if (py <= y && y < py + e->ui.em.y) {
                int32_t w = gdi.measure_text(f, "%.*s", r->bytes, s).x;
                pg.pn = i;
                if (x >= w) {
                    const int last_run = j == runs - 1;
                    pg.gp = r->gp + max(0, r->glyphs - 1 + last_run);
                } else {
                    pg.gp = r->gp + uic_edit_glyph_at_x(f, s, r->bytes, x);
                    if (pg.gp < r->glyphs - 1) {
                        uic_edit_pg_t right = {pg.pn, pg.gp + 1};
                        int32_t x0 = uic_edit_pg_to_xy(e, pg).x;
                        int32_t x1 = uic_edit_pg_to_xy(e, right).x;
                        if (x1 - x < x - x0) {
                            pg.gp++; // snap to closest glyph's 'x'
                        }
                    }
                }
            } else {
                py += e->ui.em.y;
            }
        }
        if (py > e->height) { break; }
    }
    if (pg.pn < 0 && pg.gp < 0) {
        pg.pn = e->paragraphs;
        pg.gp = 0;
    }
    return pg;
}

static void uic_edit_paint_selection(uic_edit_t* e, const uic_edit_run_t* r,
        const char* text, int32_t pn, int32_t c0, int32_t c1) {
    uint64_t s0 = uic_edit_uint64(e->selection[0].pn, e->selection[0].gp);
    uint64_t e0 = uic_edit_uint64(e->selection[1].pn, e->selection[1].gp);
    if (s0 > e0) {
        uint64_t swap = e0;
        e0 = s0;
        s0 = swap;
    }
    uint64_t s1 = uic_edit_uint64(pn, c0);
    uint64_t e1 = uic_edit_uint64(pn, c1);
    if (s0 <= e1 && s1 <= e0) {
        uint64_t start = max(s0, s1) - c0;
        uint64_t end = min(e0, e1) - c0;
        if (start < end) {
            int32_t fro = (int32_t)start;
            int32_t to  = (int32_t)end;
            int32_t ofs0 = uic_edit_gp_to_bytes(text, r->bytes, fro);
            int32_t ofs1 = uic_edit_gp_to_bytes(text, r->bytes, to);
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
    int32_t runs = 0;
    const uic_edit_run_t* run = uic_edit_pragraph_runs(e, pn, &runs);
    for (int32_t j = uic_edit_first_visible_run(e, pn);
                 j < runs && gdi.y < e->ui.y + e->bottom; j++) {
        char* text = e->para[pn].text + run[j].bp;
        gdi.x = e->ui.x;
        uic_edit_paint_selection(e, &run[j], text, pn, run[j].gp, run[j].gp + run[j].glyphs);
        gdi.text("%.*s", run[j].bytes, text);
        gdi.y += e->ui.em.y;
    }
}

static void uic_edit_set_caret(uic_edit_t* e, int32_t x, int32_t y) {
    if (e->caret.x != x || e->caret.y != y) {
        if (e->focused && app.focused) {
            fatal_if_false(SetCaretPos(e->ui.x + x, e->ui.y + y));
        }
        e->caret.x = x;
        e->caret.y = y;
    }
}

// uic_edit_scroll_up() text moves up (north) in the visible view,
// scroll position increments moves down (south)

static void uic_edit_scroll_up(uic_edit_t* e, int32_t run_count) {
    assert(0 < run_count, "does it make sense to have 0 scroll?");
    const uic_edit_pg_t eof = {.pn = e->paragraphs, .gp = 0};
    while (run_count > 0 && e->scroll.pn < e->paragraphs) {
        uic_edit_pg_t scroll = uic_edit_scroll_pg(e);
        int32_t between = uic_edit_runs_between(e, scroll, eof);
        if (between <= e->visible_runs - 1) {
            run_count = 0; // enough
        } else {
            int32_t runs = uic_edit_pragraph_run_count(e, e->scroll.pn);
            if (e->scroll.rn < runs - 1) {
                e->scroll.rn++;
            } else if (e->scroll.pn < e->paragraphs) {
                e->scroll.pn++;
                e->scroll.rn = 0;
            }
            run_count--;
            assert(e->scroll.pn >= 0 && e->scroll.rn >= 0);
        }
    }
    e->ui.invalidate(&e->ui);
}

// uic_edit_scroll_dw() text moves down (south) in the visible view,
// scroll position decrements moves up (north)

static void uic_edit_scroll_down(uic_edit_t* e, int32_t run_count) {
    assert(0 < run_count, "does it make sense to have 0 scroll?");
    while (run_count > 0 && (e->scroll.pn > 0 || e->scroll.rn > 0)) {
        int32_t runs = uic_edit_pragraph_run_count(e, e->scroll.pn);
        e->scroll.rn = min(e->scroll.rn, runs - 1);
        if (e->scroll.rn == 0 && e->scroll.pn > 0) {
            e->scroll.pn--;
            e->scroll.rn = uic_edit_pragraph_run_count(e, e->scroll.pn) - 1;
        } else if (e->scroll.rn > 0) {
            e->scroll.rn--;
        }
        assert(e->scroll.pn >= 0 && e->scroll.rn >= 0);
        assert(0 <= e->scroll.rn &&
                    e->scroll.rn < uic_edit_pragraph_run_count(e, e->scroll.pn));
        run_count--;
    }
    e->ui.invalidate(&e->ui);
}

static void uic_edit_scroll_into_view(uic_edit_t* e, const uic_edit_pg_t pg) {
    if (e->paragraphs > 0) {
        const int32_t rn = uic_edit_pg_to_pr(e, pg).rn;
        const uint64_t scroll = uic_edit_uint64(e->scroll.pn, e->scroll.rn);
        const uint64_t caret  = uic_edit_uint64(pg.pn, rn);
        uint64_t last = 0;
        int32_t py = 0;
        for (int32_t i = e->scroll.pn; i < e->paragraphs && py < e->bottom; i++) {
            int32_t runs = uic_edit_pragraph_run_count(e, i);
            for (int32_t j = uic_edit_first_visible_run(e, i); j < runs && py < e->bottom; j++) {
                last = uic_edit_uint64(i, j);
                py += e->ui.em.y;
            }
        }
        uic_edit_pg_t last_paragraph = {.pn = e->paragraphs - 1,
            .gp = e->para[e->paragraphs - 1].glyphs };
        uic_edit_pr_t lp = uic_edit_pg_to_pr(e, last_paragraph);
        uint64_t eof = uic_edit_uint64(e->paragraphs - 1, lp.rn);
        if (last == eof && py <= e->bottom - e->ui.em.y) {
            // vertical white space for EOF on the screen
            last = uic_edit_uint64(e->paragraphs, 0);
        }
        if (scroll <= caret && caret < last) {
            // no scroll
        } else if (caret < scroll) {
            e->scroll.pn = pg.pn;
            e->scroll.rn = rn;
        } else {
            assert(caret >= last);
            e->scroll.pn = pg.pn;
            e->scroll.rn = rn;
            while (e->scroll.pn > 0 || e->scroll.rn > 0) {
                ui_point_t pt = uic_edit_pg_to_xy(e, pg);
                if (pt.y + e->ui.em.y > e->bottom - e->ui.em.y) { break; }
                if (e->scroll.rn > 0) {
                    e->scroll.rn--;
                } else {
                    e->scroll.pn--;
                    e->scroll.rn = uic_edit_pragraph_run_count(e, e->scroll.pn) - 1;
                }
            }
        }
    }
}

static void uic_edit_move_caret(uic_edit_t* e, const uic_edit_pg_t pg) {
    uic_edit_scroll_into_view(e, pg);
    ui_point_t pt = e->ui.w > 0 ? // ui.w == 0 means  no measure/layout yet
        uic_edit_pg_to_xy(e, pg) : (ui_point_t){0, 0};
    uic_edit_set_caret(e, pt.x, pt.y + e->top);
    e->selection[1] = pg;
    if (!app.shift && !e->mouse != 0) {
        e->selection[0] = e->selection[1];
    }
    e->ui.invalidate(&e->ui);
}

static char* uic_edit_ensure(uic_edit_t* e, int32_t pn, int32_t bytes,
        int32_t preserve) {
    assert(bytes >= 0 && preserve <= bytes);
    if (bytes <= e->para[pn].allocated) {
        // enough memory already allocated - do nothing
    } else if (e->para[pn].allocated > 0) {
        assert(preserve <= e->para[pn].allocated);
        uic_edit_reallocate(&e->para[pn].text, bytes, 1);
        fatal_if_null(e->para[pn].text);
        e->para[pn].allocated = bytes;
    } else {
        assert(e->para[pn].allocated == 0);
        char* text = null;
        uic_edit_allocate(&text, bytes, 1);
        e->para[pn].allocated = bytes;
        memcpy(text, e->para[pn].text, preserve);
        e->para[pn].text = text;
        e->para[pn].bytes = preserve;
    }
    return e->para[pn].text;
}

#define uic_clip_append(a, ab, mx, text, bytes) do {  \
    int32_t ba = (bytes); /* bytes to append */       \
    if (a != null) {                                  \
        assert(ab <= mx);                             \
        memcpy(a, text, ba);                          \
        a += ba;                                      \
    }                                                 \
    ab += ba;                                         \
} while (0)

static uic_edit_pg_t uic_edit_op(uic_edit_t* e, bool cut,
        uic_edit_pg_t from, uic_edit_pg_t to, char* text, int32_t* bytes) {
    char* a = text; // append
    int32_t ab = 0; // appended bytes
    int32_t limit = bytes != null ? *bytes : 0; // max byes in text
    uint64_t f = uic_edit_uint64(from.pn, from.gp);
    uint64_t t = uic_edit_uint64(to.pn, to.gp);
    if (f != t) {
        uic_edit_dispose_paragraphs_layout(e);
        if (f > t) { uint64_t swap = t; t = f; f = swap; }
        int32_t pn0 = (int32_t)(f >> 32);
        int32_t gp0 = (int32_t)(f);
        int32_t pn1 = (int32_t)(t >> 32);
        int32_t gp1 = (int32_t)(t);
        if (pn1 == e->paragraphs) { // last empty paragraph
            assert(gp1 == 0);
            pn1 = e->paragraphs - 1;
            gp1 = uic_edit_g2b(e->para[pn1].text, e->para[pn1].bytes, null);
        }
        const int32_t bytes0 = e->para[pn0].bytes;
        char* s0 = e->para[pn0].text;
        char* s1 = e->para[pn1].text;
        // worst case estimate: same number of glyphs as bytes
        int32_t* g2b0 = (int32_t*)stackalloc(bytes0 * sizeof(int32_t));
        uic_edit_g2b(s0, bytes0, g2b0);
        const int bp0 = gp0 == 0 ? 0 : g2b0[gp0 - 1];
        if (pn0 == pn1) { // edit inside same paragraph
            const int bp1 = gp1 == 0 ? 0 : g2b0[gp1 - 1];
            uic_clip_append(a, ab, limit, s0 + bp0, bp1 - bp0);
            if (cut) {
                if (e->para[pn0].allocated == 0) {
                    s0 = null;
                    int32_t n = bytes0 - (bp1 - bp0);
                    uic_edit_allocate(&s0, n, 1);
                    memcpy(s0, e->para[pn0].text, bp0);
                    e->para[pn0].text = s0;
                    e->para[pn0].allocated = n;
                }
                memcpy(s0 + bp0, s1 + bp1, bytes0 - bp1);
                e->para[pn0].bytes -= (bp1 - bp0);
            }
        } else {
            uic_clip_append(a, ab, limit, s0 + bp0, bytes0 - bp0);
            uic_clip_append(a, ab, limit, "\n", 1);
            for (int i = pn0 + 1; i < pn1; i++) {
                uic_clip_append(a, ab, limit, e->para[i].text, e->para[i].bytes);
                uic_clip_append(a, ab, limit, "\n", 1);
            }
            const int32_t bytes1 = e->para[pn1].bytes;
            int32_t* g2b1 = (int32_t*)stackalloc(bytes1 * sizeof(int32_t));
            uic_edit_g2b(s1, bytes1, g2b1);
            const int bp1 = gp1 == 0 ? 0 : g2b1[gp1 - 1];
            uic_clip_append(a, ab, limit, s1, bp1);
            if (cut) {
                int32_t total = bp0 + bytes1 - bp1;
                s0 = uic_edit_ensure(e, pn0, total, bp0);
                memcpy(s0 + bp0, s1 + bp1, bytes1 - bp1);
                e->para[pn0].bytes = bp0 + bytes1 - bp1;
            }
        }
        int32_t deleted = cut ? pn1 - pn0 : 0;
        if (deleted > 0) {
            for (int i = pn0 + 1; i <= pn0 + deleted; i++) {
                if (e->para[i].allocated > 0) { free(e->para[i].text); }
            }
            for (int i = pn0 + 1; i < e->paragraphs - deleted; i++) {
                e->para[i] = e->para[i + deleted];
            }
            for (int i = e->paragraphs - deleted; i < e->paragraphs; i++) {
                memset(&e->para[i], 0, sizeof(e->para[i]));
            }
        }
        if (t == uic_edit_uint64(e->paragraphs, 0)) {
            uic_clip_append(a, ab, limit, "\n", 1);
        }
        if (a != null) { assert(a == text + limit); }
        e->paragraphs -= deleted;
        from.pn = pn0;
        from.gp = gp0;
    } else {
        from.pn = -1;
        from.gp = -1;
    }
    if (bytes != null) { *bytes = ab; }
    (void)unused(limit); // only in debug
    return from;
}

static void uic_edit_insert_paragraph(uic_edit_t* e, int32_t pn) {
    uic_edit_dispose_paragraphs_layout(e);
    if (e->paragraphs + 1 > e->allocated / (int32_t)sizeof(uic_edit_para_t)) {
        int32_t n = (e->paragraphs + 1) * 3 / 2; // 1.5 times
        uic_edit_reallocate(&e->para, n, (int32_t)sizeof(uic_edit_para_t));
        e->allocated = n * (int32_t)sizeof(uic_edit_para_t);
    }
    e->paragraphs++;
    for (int i = e->paragraphs - 1; i > pn; i--) {
        e->para[i] = e->para[i - 1];
    }
    uic_edit_para_t* p = &e->para[pn];
    p->text = null; // ? "" TODO
    p->bytes = 0;
    p->glyphs = 0;
    p->allocated = 0;
    p->runs = 0;
    p->run = null;
}

// uic_edit_insert_inline() inserts text (not containing \n paragraph
// break inside a paragraph)

static uic_edit_pg_t uic_edit_insert_inline(uic_edit_t* e, uic_edit_pg_t pg,
        const char* text, int32_t bytes) {
    assert(bytes > 0); (void)(void*)unused(strnchr);
    assert(strnchr(text, bytes, '\n') == null,
           "text \"%s\" must not contain \\n character.", text);
    if (pg.pn == e->paragraphs) {
        uic_edit_insert_paragraph(e, pg.pn);
    }
    const int32_t b = e->para[pg.pn].bytes;
    char* s = e->para[pg.pn].text;
    // worst case estimate: same number of glyphs as bytes
    int32_t* g2b0 = (int32_t*)stackalloc(b * sizeof(int32_t));
    uic_edit_g2b(s, b, g2b0);
    const int bp = pg.gp == 0 ? 0 : g2b0[pg.gp - 1];
    int32_t n = (b + bytes) * 3 / 2; // heuristics 1.5 times of total
    if (e->para[pg.pn].allocated == 0) {
        s = null;
        uic_edit_allocate(&s, n, 1);
        memcpy(s, e->para[pg.pn].text, b);
        e->para[pg.pn].text = s;
        e->para[pg.pn].allocated = n;
    } else if (e->para[pg.pn].allocated < b + bytes) {
        uic_edit_reallocate(&s, n, 1);
        e->para[pg.pn].text = s;
        e->para[pg.pn].allocated = n;
    }
    s = e->para[pg.pn].text;
    memmove(s + bp + bytes, s + bp, b - bp); // make space
    memcpy(s + bp, text, bytes);
    e->para[pg.pn].bytes += bytes;
    uic_edit_dispose_paragraphs_layout(e);
    pg.gp = uic_edit_g2b(s, bp + bytes, null);
    return pg;
}

static uic_edit_pg_t uic_edit_break_paragraph(uic_edit_t* e, uic_edit_pg_t pg) {
    uic_edit_insert_paragraph(e, pg.pn + (pg.pn < e->paragraphs));
    const int32_t bytes = e->para[pg.pn].bytes;
    char* s = e->para[pg.pn].text;
    // worst case estimate: same number of glyphs as bytes
    int32_t* g2b0 = (int32_t*)stackalloc(bytes * sizeof(int32_t));
    uic_edit_g2b(s, bytes, g2b0);
    const int bp = pg.gp == 0 ? 0 : g2b0[pg.gp - 1];
    uic_edit_pg_t next = {.pn = pg.pn + 1, .gp = 0};
    if (bp < bytes) {
        (void)uic_edit_insert_inline(e, next, s + bp, bytes - bp);
    } else {
        uic_edit_dispose_paragraphs_layout(e);
    }
    e->para[pg.pn].bytes = bp;
    return next;
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
                uic_edit_move_caret(e, e->selection[1]);
            }
        } else {
            if (e->focused) { uic_edit_destroy_caret(e); }
        }
    }
}

static void uic_edit_key_left(uic_edit_t* e) {
    uic_edit_pg_t to = e->selection[1];
    if (to.pn > 0 || to.gp > 0) {
        ui_point_t pt = uic_edit_pg_to_xy(e, to);
        if (pt.x == 0 && pt.y == 0) {
            uic_edit_scroll_down(e, 1);
        }
        if (to.gp > 0) {
            to.gp--;
        } else if (to.pn > 0) {
            to.pn--;
            to.gp = uic_edit_glyphs_in_paragraph(e, to.pn);
        }
        uic_edit_move_caret(e, to);
        e->last_x = -1;
    }
}

static void uic_edit_key_right(uic_edit_t* e) {
    uic_edit_pg_t to = e->selection[1];
    if (to.pn < e->paragraphs) {
        int32_t glyphs = uic_edit_glyphs_in_paragraph(e, to.pn);
        if (to.gp < glyphs) {
            to.gp++;
            uic_edit_scroll_into_view(e, to);
        } else {
            to.pn++;
            to.gp = 0;
            uic_edit_scroll_into_view(e, to);
        }
        uic_edit_move_caret(e, to);
        e->last_x = -1;
    }
}

static void uic_edit_reuse_last_x(uic_edit_t* e, ui_point_t* pt) {
    // Vertical caret movement visually tend to move caret horizonally
    // in propotinal font text. Remembering starting `x' value for vertical
    // movements aleviates this unpleasant UX experience to some degree.
    if (pt->x > 0) {
        if (e->last_x > 0) {
            int32_t prev = e->last_x - e->ui.em.x;
            int32_t next = e->last_x + e->ui.em.x;
            if (prev <= pt->x && pt->x <= next) {
                pt->x = e->last_x;
            }
        }
        e->last_x = pt->x;
    }
}

static void uic_edit_key_up(uic_edit_t* e) {
    const uic_edit_pg_t pg = e->selection[1];
    uic_edit_pg_t to = pg;
    if (to.pn == e->paragraphs) {
        assert(to.gp == 0); // positioned past EOF
        to.pn--;
        to.gp = e->para[to.pn].glyphs;
        uic_edit_scroll_into_view(e, to);
        ui_point_t pt = uic_edit_pg_to_xy(e, to);
        pt.x = 0;
        to.gp = uic_edit_xy_to_pg(e, pt.x, pt.y).gp;
    } else if (to.pn > 0 || uic_edit_pg_to_pr(e, to).rn > 0) { // top of the text?
        ui_point_t pt = uic_edit_pg_to_xy(e, to);
        if (pt.y == 0) {
            uic_edit_scroll_down(e, 1);
        } else {
            pt.y -= 1;
        }
        uic_edit_reuse_last_x(e, &pt);
        assert(pt.y >= 0);
        to = uic_edit_xy_to_pg(e, pt.x, pt.y);
        assert(to.pn >= 0 && to.gp >= 0);
        int32_t rn0 = uic_edit_pg_to_pr(e, pg).rn;
        int32_t rn1 = uic_edit_pg_to_pr(e, to).rn;
        if (rn1 > 0 && rn0 == rn1) { // same run
            assert(to.gp > 0, "word break must not break on zero gp");
            int32_t runs = 0;
            const uic_edit_run_t* run = uic_edit_pragraph_runs(e, to.pn, &runs);
            to.gp = run[rn1].gp;
        }
    }
    uic_edit_move_caret(e, to);
}

static void uic_edit_key_down(uic_edit_t* e) {
    const uic_edit_pg_t pg = e->selection[1];
    ui_point_t pt = uic_edit_pg_to_xy(e, pg);
    uic_edit_reuse_last_x(e, &pt);
    // scroll runs guaranteed to be already layout for current state of view:
    uic_edit_pg_t scroll = uic_edit_scroll_pg(e);
    int32_t run_count = uic_edit_runs_between(e, scroll, pg);
    if (run_count >= e->visible_runs - 1) {
        uic_edit_scroll_up(e, 1);
    } else {
        pt.y += e->ui.em.y;
    }
    uic_edit_pg_t to = uic_edit_xy_to_pg(e, pt.x, pt.y);
    if (to.pn < 0 && to.gp < 0) {
        to.pn = e->paragraphs; // advance past EOF
        to.gp = 0;
    }
    uic_edit_move_caret(e, to);
}

static void uic_edit_key_home(uic_edit_t* e) {
    if (app.ctrl) {
        e->scroll.pn = 0;
        e->scroll.rn = 0;
        e->selection[1].pn = 0;
        e->selection[1].gp = 0;
    }
    const int32_t pn = e->selection[1].pn;
    int32_t runs = uic_edit_pragraph_run_count(e, pn);
    const uic_edit_para_t* para = &e->para[pn];
    if (runs <= 1) {
        e->selection[1].gp = 0;
    } else {
        int32_t rn = uic_edit_pg_to_pr(e, e->selection[1]).rn;
        assert(0 <= rn && rn < runs);
        const int32_t gp = para->run[rn].gp;
        if (e->selection[1].gp != gp) {
            // first Home keystroke moves caret to start of run
            e->selection[1].gp = gp;
        } else {
            // second Home keystroke moves caret start of paragraph
            e->selection[1].gp = 0;
            if (e->scroll.pn >= e->selection[1].pn) { // scroll in
                e->scroll.pn = e->selection[1].pn;
                e->scroll.rn = 0;
            }
        }
    }
    if (!app.shift) {
        e->selection[0] = e->selection[1];
    }
    uic_edit_move_caret(e, e->selection[1]);
}

static void uic_edit_key_end(uic_edit_t* e) {
    if (app.ctrl) {
        int32_t py = e->bottom;
        for (int32_t i = e->paragraphs - 1; i >= 0 && py >= e->ui.em.y; i--) {
            int32_t runs = uic_edit_pragraph_run_count(e, i);
            for (int32_t j = runs - 1; j >= 0 && py >= e->ui.em.y; j--) {
                py -= e->ui.em.y;
                if (py < e->ui.em.y) {
                    e->scroll.pn = i;
                    e->scroll.rn = j;
                }
            }
        }
        e->selection[1].pn = e->paragraphs;
        e->selection[1].gp = 0;
    } else if (e->selection[1].pn == e->paragraphs) {
        assert(e->selection[1].gp == 0);
    } else {
        int32_t pn = e->selection[1].pn;
        int32_t gp = e->selection[1].gp;
        int32_t runs = 0;
        const uic_edit_run_t* run = uic_edit_pragraph_runs(e, pn, &runs);
        int32_t rn = uic_edit_pg_to_pr(e, e->selection[1]).rn;
        assert(0 <= rn && rn < runs);
        if (rn == runs - 1) {
            e->selection[1].gp = e->para[pn].glyphs;
        } else if (e->selection[1].gp == e->para[pn].glyphs) {
            // at the end of paragraph do nothing (or move caret to EOF?)
        } else if (e->para[pn].glyphs > 0 && gp != run[rn].glyphs - 1) {
            e->selection[1].gp = run[rn].gp + run[rn].glyphs - 1;
        } else {
            e->selection[1].gp = e->para[pn].glyphs;
        }
    }
    if (!app.shift) {
        e->selection[0] = e->selection[1];
    }
    uic_edit_move_caret(e, e->selection[1]);
}

static void uic_edit_key_pageup(uic_edit_t* e) {
    int32_t n = max(1, e->visible_runs - 1);
    uic_edit_pg_t scr = uic_edit_scroll_pg(e);
    uic_edit_pg_t bof = {.pn = 0, .gp = 0};
    int32_t m = uic_edit_runs_between(e, bof, scr);
    if (m > n) {
        ui_point_t pt = uic_edit_pg_to_xy(e, e->selection[1]);
        uic_edit_pr_t scroll = e->scroll;
        uic_edit_scroll_down(e, n);
        if (scroll.pn != e->scroll.pn || scroll.rn != e->scroll.rn) {
            uic_edit_pg_t pg = uic_edit_xy_to_pg(e, pt.x, pt.y);
            uic_edit_move_caret(e, pg);
        }
    } else {
        uic_edit_move_caret(e, bof);
    }
}

static void uic_edit_key_pagedw(uic_edit_t* e) {
    int32_t n = max(1, e->visible_runs - 1);
    uic_edit_pg_t scr = uic_edit_scroll_pg(e);
    uic_edit_pg_t eof = {.pn = e->paragraphs, .gp = 0};
    int32_t m = uic_edit_runs_between(e, scr, eof);
    if (m > n) {
        ui_point_t pt = uic_edit_pg_to_xy(e, e->selection[1]);
        uic_edit_pr_t scroll = e->scroll;
        uic_edit_scroll_up(e, n);
        if (scroll.pn != e->scroll.pn || scroll.rn != e->scroll.rn) {
            uic_edit_pg_t pg = uic_edit_xy_to_pg(e, pt.x, pt.y);
            uic_edit_move_caret(e, pg);
        }
    } else {
        uic_edit_move_caret(e, eof);
    }
}

static void uic_edit_key_delete(uic_edit_t* e) {
    uint64_t f = uic_edit_uint64(e->selection[0].pn, e->selection[0].gp);
    uint64_t t = uic_edit_uint64(e->selection[1].pn, e->selection[1].gp);
    uint64_t eof = uic_edit_uint64(e->paragraphs, 0);
    if (f == t && t != eof) {
        uic_edit_pg_t s1 = e->selection[1];
        uic_edit_key_right(e);
        e->selection[1] = s1;
    }
    e->erase(e);
}

static void uic_edit_key_backspace(uic_edit_t* e) {
    uint64_t f = uic_edit_uint64(e->selection[0].pn, e->selection[0].gp);
    uint64_t t = uic_edit_uint64(e->selection[1].pn, e->selection[1].gp);
    if (t != 0 && f == t) {
        uic_edit_pg_t s1 = e->selection[1];
        uic_edit_key_left(e);
        e->selection[1] = s1;
    }
    e->erase(e);
}

static void uic_edit_key_enter(uic_edit_t* e) {
    e->erase(e);
    e->selection[1] = uic_edit_break_paragraph(e, e->selection[1]);
    e->selection[0] = e->selection[1];
    uic_edit_move_caret(e, e->selection[1]);
}

static void uic_edit_next_fuzz(uic_edit_t* e);

static void uic_edit_key_pressed(uic_t* ui, int32_t key) {
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        if (key == VK_ESCAPE) { app.close(); }
        if (key == virtual_keys.down && e->selection[1].pn < e->paragraphs) {
            uic_edit_key_down(e);
        } else if (key == virtual_keys.up && e->paragraphs > 0) {
            uic_edit_key_up(e);
        } else if (key == virtual_keys.left) {
            uic_edit_key_left(e);
        } else if (key == virtual_keys.right) {
            uic_edit_key_right(e);
        } else if (key == virtual_keys.pageup) {
            uic_edit_key_pageup(e);
        } else if (key == virtual_keys.pagedw) {
            uic_edit_key_pagedw(e);
        } else if (key == virtual_keys.home) {
            uic_edit_key_home(e);
        } else if (key == virtual_keys.end) {
            uic_edit_key_end(e);
        } else if (key == virtual_keys.del) {
            uic_edit_key_delete(e);
        } else if (key == virtual_keys.back) {
            uic_edit_key_backspace(e);
        } else if (key == virtual_keys.enter) {
            uic_edit_key_enter(e);
        } else if (key == virtual_keys.f5) {
            if (app.ctrl && app.shift && app.alt && e->fuzzer == null) {
                e->fuzz(e); // start on Ctrl+Shift+Alt+F5
            } else if (e->fuzzer != null) {
                e->fuzz(e); // stop on F5
            }

        }
    }
    if (e->fuzzer != null) { uic_edit_next_fuzz(e); }
}

static void uic_edit_keyboard(uic_t* unused(ui), int32_t ch) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden && !ui->disabled);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (ch == (char)('a' - 'a' + 1) && app.ctrl) { e->select_all(e); }
    if (ch == (char)('x' - 'a' + 1) && app.ctrl) { e->cut_to_clipboard(e); }
    if (ch == (char)('c' - 'a' + 1) && app.ctrl) { e->copy_to_clipboard(e); }
    if (ch == (char)('v' - 'a' + 1) && app.ctrl) { e->paste_from_clipboard(e); }
    if (0x20 <= ch) { // 0x20 space
        int32_t bytes = uic_edit_glyph_bytes(ch % 0xFF);
        e->erase(e); // remove selected text to be replaced by glyph
        e->selection[1] = uic_edit_insert_inline(e,
            e->selection[1], (char*)&ch, bytes);
        e->selection[0] = e->selection[1];
        uic_edit_move_caret(e, e->selection[1]);
    }
    if (e->focused) {
        ui->invalidate(ui);
    }
    if (e->fuzzer != null) { uic_edit_next_fuzz(e); }
}

static void uic_edit_mouse(uic_t* ui, int m, int unused(flags)) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    const int32_t x = app.mouse.x - e->ui.x;
    const int32_t y = app.mouse.y - e->ui.y - e->top;
    bool inside = 0 <= x && x < ui->w && 0 <= y && y < e->height;
    bool left = m == messages.left_button_down;
    bool right = m == messages.right_button_down;
    if (e->focused  && inside && (left || right || e->mouse != 0)) {
        uic_edit_pg_t p = uic_edit_xy_to_pg(e, x, y);
        if (0 <= p.pn && 0 <= p.gp) {
            if (p.pn > e->paragraphs) { p.pn = max(0, e->paragraphs); }
            int32_t chars = uic_edit_glyphs_in_paragraph(e, p.pn);
            if (p.gp > chars) { p.gp = max(0, chars); }
            uic_edit_move_caret(e, p);
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

static void uic_edit_mousewheel(uic_t* ui, int32_t unused(dx), int32_t dy) {
    // TODO: may make a use of dx in single line not-word-breaked edit control
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
    int32_t lines = (abs(dy) + ui->em.y - 1) / ui->em.y;
    if (dy > 0) {
        uic_edit_scroll_down(e, lines);
    } else if (dy < 0) {
        uic_edit_scroll_up(e, lines);
    }
    // TODO: Ctrl UP/DW and caret of out of visible area scrolls are not
    //       implemented. Not sure they are very good UX experience.
    //       MacOS users may be used to scroll with touchpad, take a visual
    //       peek, do NOT click and continue editing at last cursor position.
    //       To me back forward stack navigation is much more intuitive and
    //       much mode "modeless" in spirit of cut/copy/paste. But opinions
    //       and editing habits vary. Easy to implement.
    uic_edit_pg_t pg = uic_edit_xy_to_pg(e, e->caret.x, e->caret.y);
    uic_edit_move_caret(e, pg);
}

static bool uic_edit_message(uic_t* ui, int32_t unused(message),
        int64_t unused(wp), int64_t unused(lp), int64_t* unused(rt)){
    assert(ui->tag == uic_tag_edit);
    uic_edit_check_focus((uic_edit_t*)ui);
    return false;
}

static void uic_edit_erase(uic_edit_t* e) {
    const uic_edit_pg_t from = e->selection[0];
    const uic_edit_pg_t to = e->selection[1];
    uic_edit_pg_t pg = uic_edit_op(e, true, from, to, null, null);
    if (pg.pn >= 0 && pg.gp >= 0) {
        e->selection[0] = pg;
        e->selection[1] = pg;
        uic_edit_move_caret(e, pg);
        e->ui.invalidate(&e->ui);
    }
}

static void uic_edit_cut_copy(uic_edit_t* e, bool cut) {
    const uic_edit_pg_t from = e->selection[0];
    const uic_edit_pg_t to = e->selection[1];
    int32_t n = 0; // bytes between from..to
    uic_edit_op(e, false, from, to, null, &n);
    if (n > 0) {
        char* text = null;
        uic_edit_allocate(&text, n + 1, 1);
        uic_edit_pg_t pg = uic_edit_op(e, cut, from, to, text, &n);
        if (cut && pg.pn >= 0 && pg.gp >= 0) {
            e->selection[0] = pg;
            e->selection[1] = pg;
            uic_edit_move_caret(e, pg);
        }
        text[n] = 0; // make it zero terminated
        clipboard.copy_text(text);
//      traceln("cliipboard (%d bytes strlen()%d):\n%s", n, strlen(text), text);
        assert(n == strlen(text), "n=%d strlen(cb)=%d cb=\"%s\"",
               n, strlen(text), text);
        free(text);
    }
}

static void uic_edit_select_all(uic_edit_t* e) {
    e->selection[0] = (uic_edit_pg_t ){.pn = 0, .gp = 0};
    e->selection[1] = (uic_edit_pg_t ){.pn = e->paragraphs, .gp = 0};
    e->ui.invalidate(&e->ui);
}

static int uic_edit_copy(uic_edit_t* e, char* text, int32_t* bytes) {
    not_null(bytes);
    int r = 0;
    const uic_edit_pg_t from = {.pn = 0, .gp = 0};
    const uic_edit_pg_t to = {.pn = e->paragraphs, .gp = 0};
    int32_t n = 0; // bytes between from..to
    uic_edit_op(e, false, from, to, null, &n);
    if (text != null) {
        int32_t m = min(n, *bytes);
        if (m < n) { r = ERROR_INSUFFICIENT_BUFFER; }
        uic_edit_op(e, false, from, to, text, &m);
    }
    *bytes = n;
    return r;
}

static void uic_edit_clipboard_cut(uic_edit_t* e) {
    uic_edit_cut_copy(e, true);
}

static void uic_edit_clipboard_copy(uic_edit_t* e) {
    uic_edit_cut_copy(e, false);
}

static uic_edit_pg_t uic_edit_paste_text(uic_edit_t* e, const char* s, int32_t n) {
    uic_edit_pg_t pg = e->selection[1];
    int32_t i = 0;
    const char* text = s;
    while (i < n) {
        int32_t b = i;
        while (b < n && s[b] != '\n') { b++; }
        bool lf = b < n && s[b] == '\n';
        int32_t next = b + 1;
        if (b > i && s[b - 1] == '\r') { b--; } // CR LF
        if (b > i) {
            pg = uic_edit_insert_inline(e, pg, text, b - i);
        }
        if (lf) {
            pg = uic_edit_break_paragraph(e, pg);
        }
        text = s + next;
        i = next;
    }
    return pg;
}

static void uic_edit_paste(uic_edit_t* e, const char* s, int32_t n) {
    e->erase(e);
    uic_edit_paste_text(e, s, n);
}

static void uic_edit_clipboard_paste(uic_edit_t* e) {
    uic_edit_pg_t pg = e->selection[1];
    int32_t bytes = 0;
    clipboard.text(null, &bytes);
    if (bytes > 0) {
        char* text = null;
        uic_edit_allocate(&text, bytes, 1);
        int r = clipboard.text(text, &bytes);
        fatal_if_not_zero(r);
        if (bytes > 0 && text[bytes - 1] == 0) {
            bytes--; // clipboard includes zero terminator
        }
        if (bytes > 0) {
            e->erase(e);
            pg = uic_edit_paste_text(e, text, bytes);
            uic_edit_move_caret(e, pg);
        }
        uic_edit_free(&text);
    }
}

static void uic_edit_measure(uic_t* ui) { // bottom up
    ui->em = gdi.get_em(*ui->font);
    assert(ui->tag == uic_tag_edit);
    // enforce minimum size - it makes it checking corner cases much simpler
    // and it's hard to edit anything in a smaller area - will result in bad UX
    if (ui->w < ui->em.x * 3) { ui->w = ui->em.x * 4; }
    if (ui->h < ui->em.y) { ui->h = ui->em.y; }
}

static void uic_edit_layout(uic_t* ui) { // top down
    assert(ui->tag == uic_tag_edit);
    assert(ui->w > 0 && ui->h > 0); // could be `if'
    uic_edit_t* e = (uic_edit_t*)ui;
    // glyph position in scroll_pn paragraph:
    const uic_edit_pg_t scroll = e->width == 0 ?
        (uic_edit_pg_t){0, 0} : uic_edit_scroll_pg(e);
    if (ui->w < ui->em.x * 3) { ui->w = ui->em.x * 4; }
    if (ui->h < ui->em.y) { ui->h = ui->em.y; }
    if (e->width > 0 && ui->w != e->width) {
        uic_edit_dispose_paragraphs_layout(e);
    }
    // enforce minimum size - again
    e->width  = ui->w;
    e->height = ui->h;
    e->top    = e->multiline ? 0 : (e->height - e->ui.em.y) / 2;
    e->bottom = e->multiline ? e->height : e->height - e->top - e->ui.em.y;
    e->visible_runs = (e->bottom - e->top) / e->ui.em.y; // fully visible
    // number of runs in e->scroll.pn may have changed with e->width change
    int32_t runs = uic_edit_pragraph_run_count(e, e->scroll.pn);
    e->scroll.rn = uic_edit_pg_to_pr(e, scroll).rn;
    assert(0 <= e->scroll.rn && e->scroll.rn < runs); (void)runs;
    // For single line editor distribute vertical gap evenly between
    // top and bottom. For multiline snap top line to y coordinate 0
    // otherwise resizing view will result in up-down jiggling of the
    // whole text
    if (e->focused) {
        // recreate caret because em.y may have changed
        uic_edit_destroy_caret(e);
        uic_edit_create_caret(e);
        if (app.focus && e->focused) {
            uic_edit_move_caret(e, e->selection[1]);
        }
    }
}

static void uic_edit_paint(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(rgb(20, 20, 14));
    gdi.fill(ui->x, ui->y, ui->w, e->height);
    gdi.push(ui->x, ui->y + e->top);
    gdi.set_clip(ui->x, ui->y, ui->w, e->height);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    gdi.set_font(f);
    gdi.set_text_color(ui->color);
    const int32_t bottom = ui->y + e->bottom;
    assert(e->scroll.pn <= e->paragraphs);
    for (int32_t i = e->scroll.pn; i < e->paragraphs && gdi.y < bottom; i++) {
        uic_edit_paint_para(e, i);
    }
    gdi.set_clip(0, 0, 0, 0);
    gdi.pop();
}

static void uic_edit_init_with_lorem_ipsum(uic_edit_t* e);
static void uic_edit_fuzz(uic_edit_t* e);

void uic_edit_init(uic_edit_t* e) {
    memset(e, 0, sizeof(*e));
    uic_init(&e->ui);
    e->ui.tag = uic_tag_edit;
    uic_edit_init_with_lorem_ipsum(e);
    e->fuzz_seed   = 1; // client can seed it with (crt.nanoseconds() | 1)
    e->last_x      = -1;
    e->multiline   = true;
    e->monospaced  = false;
    e->wordbreak   = true;
    e->ui.color    = rgb(168, 168, 150); // colors.text;
    e->ui.font     = &app.fonts.H1;
    e->caret       = (ui_point_t){-1, -1};
    e->ui.paint    = uic_edit_paint;
    e->ui.measure  = uic_edit_measure;
    e->ui.layout   = uic_edit_layout;
    e->ui.mouse    = uic_edit_mouse;
    e->ui.key_down = uic_edit_key_pressed;
    e->ui.keyboard = uic_edit_keyboard;
    e->ui.message  = uic_edit_message;
    e->ui.mousewheel = uic_edit_mousewheel;
    e->paste                = uic_edit_paste;
    e->copy                 = uic_edit_copy;
    e->cut_to_clipboard     = uic_edit_clipboard_cut;
    e->copy_to_clipboard    = uic_edit_clipboard_copy;
    e->paste_from_clipboard = uic_edit_clipboard_paste;
    e->select_all           = uic_edit_select_all;
    e->erase                = uic_edit_erase;
    e->fuzz  = uic_edit_fuzz;
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

#define glyph_teddy_bear  "\xF0\x9F\xA7\xB8"
#define glyph_chinese_one "\xE5\xA3\xB9"
#define glyph_chinese_two "\xE8\xB4\xB0"
#define glyph_teddy_bear  "\xF0\x9F\xA7\xB8"
#define glyph_ice_cube    "\xF0\x9F\xA7\x8A"

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
    "lorem enim, ut vestibulum eros varius id."                                   \

static const char* test_content =
    "Good bye Universe...\n"
    "Hello World!\n"
    "\n"
    "Ctrl+Shift+Alt+F5 and F5 starts/stops FUZZING test.\n"
    "\n"
    "FUZZING use rapid mouse clicks thus UI Fuzz button is hard to press use keyboard shortcut F5 to stop.\n"
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
    lorem_ipsum;

static void uic_edit_init_with_lorem_ipsum(uic_edit_t* e) {
    fatal_if(e->paragraphs != 0);
    uic_edit_paste_text(e, test_content, (int32_t)strlen(test_content));
}

static void uic_edit_next_fuzz(uic_edit_t* e) {
    atomics.increment_int32(&e->fuzz_count);
}

static void uic_edit_fuzzer(void* p) {
    uic_edit_t* e = (uic_edit_t*)p;
    for (;;) {
        while (!e->fuzz_quit && e->fuzz_count == e->fuzz_last) {
            crt.sleep(1.0 / 1024.0); // ~1ms
        }
        if (e->fuzz_quit) { e->fuzz_quit = false; break; }
        e->fuzz_last = e->fuzz_count;
        app.focused = true; // force application to be focused
        uint32_t rnd = crt.random32(&e->fuzz_seed);
        switch (rnd % 8) {
            case 0: app.alt = 0; app.ctrl = 0; app.shift = 0; break;
            case 1: app.alt = 1; app.ctrl = 0; app.shift = 0; break;
            case 2: app.alt = 0; app.ctrl = 1; app.shift = 0; break;
            case 3: app.alt = 1; app.ctrl = 1; app.shift = 0; break;
            case 4: app.alt = 0; app.ctrl = 0; app.shift = 1; break;
            case 5: app.alt = 1; app.ctrl = 0; app.shift = 1; break;
            case 6: app.alt = 0; app.ctrl = 1; app.shift = 1; break;
            case 7: app.alt = 1; app.ctrl = 1; app.shift = 1; break;
            default: assert(false);
        }
        int keys[] = {
            virtual_keys.up,
            virtual_keys.down,
            virtual_keys.left,
            virtual_keys.right,
            virtual_keys.home,
            virtual_keys.end,
            virtual_keys.pageup,
            virtual_keys.pagedw,
            virtual_keys.insert,
// TODO: cut copy paste erase need special treatment in fuzzing
//       otherwise text collapses to nothing pretty fast
//          virtual_keys.del,
//          virtual_keys.back,
            virtual_keys.enter,
            0
        };
        rnd = crt.random32(&e->fuzz_seed);
        int key = keys[rnd % countof(keys)];
        if (key == 0) {
            rnd = crt.random32(&e->fuzz_seed);
            int ch = rnd % 128;
            if (ch == 033) { ch = 'a'; } // don't send ESC
            app.post(WM_CHAR, ch, 0);
        } else {
            app.post(WM_KEYDOWN, key, 0);
        }
        if (crt.random32(&e->fuzz_seed) % 32 == 0) {
            // mouse events only inside edit control otherwise
            // they will start clicking buttons around
            int32_t x = crt.random32(&e->fuzz_seed) % e->ui.w;
            int32_t y = crt.random32(&e->fuzz_seed) % e->ui.h;
            app.post(WM_MOUSEMOVE, 0, x | y << 16);
            app.post(WM_LBUTTONDOWN, 0, x | y << 16);
            app.post(WM_LBUTTONUP,   0, x | y << 16);
        }
    }
}

static void uic_edit_fuzz(uic_edit_t* e) {
    if (e->fuzzer == null) {
        e->fuzzer = threads.start(uic_edit_fuzzer, e);
        uic_edit_next_fuzz(e);
    } else {
        e->fuzz_quit = true;
        threads.join(e->fuzzer);
        e->fuzzer = null;
    }
    traceln("fuzzing %s",e->fuzzer != null ? "started" : "stopped");
}

end_c
