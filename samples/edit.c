/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"
#include <Windows.h>

// TODO: undo/redo
// TODO: back/forward navigation
// TODO: exit/save keystrokes?

// http://worrydream.com/refs/Tesler%20-%20A%20Personal%20History%20of%20Modeless%20Text%20Editing%20and%20Cut-Copy-Paste.pdf
// https://web.archive.org/web/20221216044359/http://worrydream.com/refs/Tesler%20-%20A%20Personal%20History%20of%20Modeless%20Text%20Editing%20and%20Cut-Copy-Paste.pdf

// Rich text options that are not addressed yet:
// * Color of ranges (useful for code editing)
// * Soft line breaks inside the paragraph (useful for e.g. bullet lists of options)
// * Bold/Italic/Underline (along with color ranges)
// * Multifont (as long as run vertical size is the maximum of font)
// * Kerning (?! like in overhung "Fl")

begin_c

// Glyphs in monospaced Windows fonts may have different width for non-ASCII
// characters. Thus even if edit is monospaced glyph measurements are used
// in text layout.

static uint64_t uic_edit_uint64(int32_t high, int32_t low) {
    assert(high >= 0 && low >= 0);
    return ((uint64_t)high << 32) | (uint64_t)low;
}

// All allocate/free functions assume 'fail fast' semantics
// if underlying OS runs out of RAM it considered to be fatal.
// It is possible to implement and hold committed 'safety region'
// of RAM and free it to general pull or reuse it on malloc() or reallocate()
// returning null, try to notify user about low memory conditions
// and attempt to save edited work but all of the above may only
// work if there is no other run-away code that consumes system
// memory at a very high rate.

static void* uic_edit_alloc(int32_t bytes) {
    void* p = malloc(bytes);
    not_null(p);
    return p;
}

static void uic_edit_allocate(void** pp, int32_t count, size_t element) {
    not_null(pp);
    assert((int64_t)count * element <= INT_MAX);
    *pp = uic_edit_alloc(count * (int32_t)element);
}

static void uic_edit_free(void** pp) {
    not_null(pp);
    // free(null) is acceptable but may indicate unbalanced logic
    not_null(*pp);
    free(*pp);
    *pp = null;
}

static void uic_edit_reallocate(void** pp, int32_t count, size_t element) {
    not_null(pp);
    assert((int64_t)count * element <= INT_MAX);
    if (*pp == null) {
        uic_edit_allocate(pp, count, element);
    } else {
        *pp = realloc(*pp, count * (size_t)element);
        not_null(*pp);
    }
}

static int32_t uic_edit_text_width(uic_edit_t* e, const char* s, int32_t n) {
//  double time = crt.seconds();
    // average measure_text() performance per character:
    // "app.fonts.mono"    ~500us (microseconds)
    // "app.fonts.regular" ~250us (microseconds)
    int32_t x = n == 0 ? 0 : gdi.measure_text(*e->ui.font, "%.*s", n, s).x;
//  time = (crt.seconds() - time) * 1000.0;
//  static double time_sum;
//  static double length_sum;
//  time_sum += time;
//  length_sum += n;
//  traceln("avg=%.6fms per char total %.3fms", time_sum / length_sum, time_sum);
    return x;
}

static int32_t uic_edit_glyph_bytes(char start_byte_value) { // utf-8
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
// g2b[] array with glyphs positions.

static int32_t uic_edit_g2b(const char* utf8, int32_t bytes, int32_t g2b[]) {
    int32_t i = 0;
    int32_t k = 1;
    // g2b[k] start postion in byte offset from utf8 text of glyph[k]
    if (g2b != null) { g2b[0] = 0; }
    while (i < bytes) {
        i += uic_edit_glyph_bytes(utf8[i]);
        if (g2b != null) { g2b[k] = i; }
        k++;
    }
    return k - 1;
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

static void uic_edit_paragraph_g2b(uic_edit_t* e, int32_t pn) {
    assert(0 <= pn && pn < e->paragraphs);
    uic_edit_para_t* p = &e->para[pn];
    if (p->glyphs < 0) {
        const int32_t bytes = p->bytes;
        const int32_t n = p->bytes + 1;
        const int32_t a = (n * sizeof(int32_t)) * 3 / 2; // heuristic
        if (p->g2b_allocated < a) {
            uic_edit_reallocate(&p->g2b, n, sizeof(int32_t));
            p->g2b_allocated = a;
        }
        const char* utf8 = p->text;
        p->g2b[0] = 0; // first glyph starts at 0
        int32_t i = 0;
        int32_t k = 1;
        // g2b[k] start postion in byte offset from utf8 text of glyph[k]
        while (i < bytes) {
            i += uic_edit_glyph_bytes(utf8[i]);
            p->g2b[k] = i;
            k++;
        }
        p->glyphs = k - 1;
    }
}

static int32_t uic_edit_word_break_at(uic_edit_t* e, int32_t pn, int32_t rn,
        const int32_t width, bool allow_zero) {
    uic_edit_para_t* p = &e->para[pn];
    int32_t k = 1; // at least 1 glyph
    // offsets inside a run in glyphs and bytes from start of the paragraph:
    int32_t gp = p->run[rn].gp;
    int32_t bp = p->run[rn].bp;
    if (gp < p->glyphs - 1) {
        const char* text = p->text + bp;
        const int32_t glyphs_in_this_run = p->glyphs - gp;
        int32_t* g2b = &p->g2b[gp];
        int32_t gc = min(4, glyphs_in_this_run);
        int32_t w = uic_edit_text_width(e, text, g2b[gc] - bp);
        while (gc < glyphs_in_this_run && w < width) {
            gc = min(gc * 4, glyphs_in_this_run);
            w = uic_edit_text_width(e, text, g2b[gc] - bp);
        }
        if (w < width) {
            k = gc;
            assert(1 <= k && k <= p->glyphs - gp);
        } else {
            int32_t i = 0;
            int32_t j = gc;
            k = (i + j) / 2;
            while (i < j) {
                assert(allow_zero || 1 <= k && k < gc + 1);
                const int32_t n = g2b[k + 1] - bp;
                int32_t px = uic_edit_text_width(e, text, n);
                if (px == width) { break; }
                if (px < width) { i = k + 1; } else { j = k; }
                if (!allow_zero && (i + j) / 2 == 0) { break; }
                k = (i + j) / 2;
                assert(allow_zero || 1 <= k && k <= p->glyphs - gp);
            }
        }
    }
    assert(allow_zero || 1 <= k && k <= p->glyphs - gp);
    return k;
}

static int32_t uic_edit_word_break(uic_edit_t* e, int32_t pn, int32_t rn) {
    return uic_edit_word_break_at(e, pn, rn, e->width, false);
}

static int32_t uic_edit_glyph_at_x(uic_edit_t* e, int32_t pn, int32_t rn,
        int32_t x) {
    if (x == 0 || e->para[pn].bytes == 0) {
        return 0;
    } else {
        return uic_edit_word_break_at(e, pn, rn, x + 1, true);
    }
}

// uic_edit::paragraph_runs() breaks paragraph into `runs` according to `width`
// and TODO: `.wordbreak`, `.singleline`

static const uic_edit_run_t* uic_edit_paragraph_runs(uic_edit_t* e, int32_t pn,
        int32_t* runs) {
//  double time = crt.seconds();
    assert(e->width > 0);
    const uic_edit_run_t* r = null;
    if (pn == e->paragraphs) {
        static const uic_edit_run_t eof_run = { 0 };
        *runs = 1;
        r = &eof_run;
    } else if (e->para[pn].run != null) {
        *runs = e->para[pn].runs;
        r = e->para[pn].run;
    } else {
        assert(0 <= pn && pn < e->paragraphs);
        uic_edit_paragraph_g2b(e, pn);
        uic_edit_para_t* p = &e->para[pn];
        if (p->run == null) {
            assert(p->runs == 0 && p->run == null);
            const int32_t max_runs = p->bytes + 1;
            uic_edit_allocate(&p->run, max_runs, sizeof(uic_edit_run_t));
            uic_edit_run_t* run = p->run;
            run[0].bp = 0;
            run[0].gp = 0;
            int32_t gc = p->bytes == 0 ? 0 : uic_edit_word_break(e, pn, 0);
            if (gc == p->glyphs) { // whole paragraph fits into width
                p->runs = 1;
                run[0].bytes  = p->bytes;
                run[0].glyphs = p->glyphs;
                int32_t pixels = uic_edit_text_width(e, p->text, p->g2b[gc]);
                run[0].pixels = pixels;
            } else {
                assert(gc < p->glyphs);
                int32_t rc = 0; // runs count
                int32_t ix = 0; // glyph index from to start of paragraph
                char* text = p->text;
                int32_t bytes = p->bytes;
                while (bytes > 0) {
                    assert(rc < max_runs);
                    run[rc].bp = (int32_t)(text - p->text);
                    run[rc].gp = ix;
                    int32_t glyphs = uic_edit_word_break(e, pn, rc);
                    int32_t utf8bytes = p->g2b[ix + glyphs] - run[rc].bp;
                    int32_t pixels = uic_edit_text_width(e, text, utf8bytes);
                    if (glyphs > 1 && utf8bytes < bytes && text[utf8bytes - 1] != 0x20) {
                        // try to find word break SPACE character. utf8 space is 0x20
                        int32_t i = utf8bytes;
                        while (i > 0 && text[i - 1] != 0x20) { i--; }
                        if (i > 0 && i != utf8bytes) {
                            utf8bytes = i;
                            glyphs = uic_edit_glyphs(text, utf8bytes);
                            pixels = uic_edit_text_width(e, text, utf8bytes);
                        }
                    }
                    run[rc].bytes  = utf8bytes;
                    run[rc].glyphs = glyphs;
                    run[rc].pixels = pixels;
                    rc++;
                    text += utf8bytes;
                    assert(0 <= utf8bytes && utf8bytes <= bytes);
                    bytes -= utf8bytes;
                    ix += glyphs;
                }
                assert(rc > 0);
                p->runs = rc; // truncate heap allocated array:
                uic_edit_reallocate(&p->run, rc, sizeof(uic_edit_run_t));
            }
        }
        *runs = p->runs;
        r = p->run;
    }
    assert(r != null && *runs >= 1);
//  time = crt.seconds() - time;
//  traceln("%.3fms", time * 1000.0);
    return r;
}

static int32_t uic_edit_paragraph_run_count(uic_edit_t* e, int32_t pn) {
    int32_t runs = 0;
    (void)uic_edit_paragraph_runs(e, pn, &runs);
    return runs;
}

static int32_t uic_edit_glyphs_in_paragraph(uic_edit_t* e, int32_t pn) {
    (void)uic_edit_paragraph_run_count(e, pn); // word break into runs
    return e->para[pn].glyphs;
}

static void uic_edit_create_caret(uic_edit_t* e) {
    fatal_if(e->focused);
    assert(GetActiveWindow() == (HWND)app.window);
    assert(GetFocus() == (HWND)app.window);
    fatal_if_false(CreateCaret((HWND)app.window, null, 2, e->ui.em.y));
    e->focused = true; // means caret was created
//  traceln("%d,%d", 2, e->ui.em.y);
}

static void uic_edit_destroy_caret(uic_edit_t* e) {
    fatal_if(!e->focused);
    fatal_if_false(DestroyCaret());
    e->focused = false; // means caret was destroyed
//  traceln("");
}

static void uic_edit_show_caret(uic_edit_t* e) {
    if (e->focused) {
        assert(GetActiveWindow() == (HWND)app.window);
        assert(GetFocus() == (HWND)app.window);
        fatal_if_false(SetCaretPos(e->ui.x + e->caret.x, e->ui.y + e->caret.y));
        // TODO: it is possible to support unblinking caret if desired
        // do not set blink time - use global default
//      fatal_if_false(SetCaretBlinkTime(500));
        fatal_if_false(ShowCaret((HWND)app.window));
        e->shown++;
//      traceln("shown=%d", e->shown);
        assert(e->shown == 1);
    }
}

static void uic_edit_hide_caret(uic_edit_t* e) {
    if (e->focused) {
        fatal_if_false(HideCaret((HWND)app.window));
        e->shown--;
//      traceln("shown=%d", e->shown);
        assert(e->shown == 0);
    }
}

static void uic_edit_dispose_paragraphs_layout(uic_edit_t* e) {
    for (int32_t i = 0; i < e->paragraphs; i++) {
        uic_edit_para_t* p = &e->para[i];
        if (p->run != null) {
            uic_edit_free(&p->run);
        }
        if (p->g2b != null) {
            uic_edit_free(&p->g2b);
        }
        p->glyphs = -1;
        p->runs = 0;
        p->g2b_allocated = 0;
    }
}

static void uic_edit_set_font(uic_edit_t* e, font_t* f) {
    uic_edit_dispose_paragraphs_layout(e);
    // xxx
//  e->scroll.pn = 0; // TODO: restore after layout complete?
    e->scroll.rn = 0; //       which is not trivial
    e->ui.font = f;
    e->ui.em = gdi.get_em(*f);
//  traceln("%p := %p", e, *f);
    if (e->ui.w > 0 && e->ui.h > 0) {
        e->ui.layout(&e->ui); // direct call to re-layout
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
        const uic_edit_run_t* run = uic_edit_paragraph_runs(e, pg.pn, &runs);
        if (pg.gp == e->para[pg.pn].glyphs + 1) {
            pr.rn = runs - 1; // TODO: past last glyph ??? is this correct?
        } else {
            assert(0 <= pg.gp && pg.gp <= e->para[pg.pn].glyphs);
            for (int32_t j = 0; j < runs && pr.rn < 0; j++) {
                const int32_t last_run = j == runs - 1;
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
    if (pg0.pn == pg1.pn) {
        assert(rn0 <= rn1);
        rc = rn1 - rn0;
    } else {
        assert(pg0.pn < pg1.pn);
        for (int32_t i = pg0.pn; i < pg1.pn; i++) {
            const int32_t runs = uic_edit_paragraph_run_count(e, i);
            if (i == pg0.pn) {
                rc += runs - rn0;
            } else { // i < pg1.pn
                rc += runs;
            }
        }
        rc += rn1;
    }
    return rc;
}

static uic_edit_pg_t uic_edit_scroll_pg(uic_edit_t* e) {
    int32_t runs = 0;
    const uic_edit_run_t* run = uic_edit_paragraph_runs(e, e->scroll.pn, &runs);
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
        const uic_edit_run_t* run = uic_edit_paragraph_runs(e, i, &runs);
        for (int32_t j = uic_edit_first_visible_run(e, i); j < runs; j++) {
            const int32_t last_run = j == runs - 1;
            int32_t gc = run[j].glyphs;
            if (i == pg.pn) {
                // in the last `run` of a paragraph x after last glyph is OK
                if (run[j].gp <= pg.gp && pg.gp < run[j].gp + gc + last_run) {
                    const char* s = e->para[i].text + run[j].bp;
                    int32_t ofs = uic_edit_gp_to_bytes(s, run[j].bytes,
                        pg.gp - run[j].gp);
                    pt.x = uic_edit_text_width(e, s, ofs);
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
        int32_t x = uic_edit_text_width(e, s, bytes_in_glyph);
        return x;
    } else {
        assert(pg.gp == gc, "only next position past last glyph is allowed");
        return 0;
    }
}

// uic_edit::xy_to_pg() (x,y) (0,0, width x height) -> paragraph # glyph #

static uic_edit_pg_t uic_edit_xy_to_pg(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t pg = {-1, -1};
    int32_t py = 0; // paragraph `y' coordinate
    for (int32_t i = e->scroll.pn; i < e->paragraphs && pg.pn < 0; i++) {
        int32_t runs = 0;
        const uic_edit_run_t* run = uic_edit_paragraph_runs(e, i, &runs);
        for (int32_t j = uic_edit_first_visible_run(e, i); j < runs && pg.pn < 0; j++) {
            const uic_edit_run_t* r = &run[j];
            char* s = e->para[i].text + run[j].bp;
            if (py <= y && y < py + e->ui.em.y) {
                int32_t w = uic_edit_text_width(e, s, r->bytes);
                pg.pn = i;
                if (x >= w) {
                    const int32_t last_run = j == runs - 1;
                    pg.gp = r->gp + max(0, r->glyphs - 1 + last_run);
                } else {
                    pg.gp = r->gp + uic_edit_glyph_at_x(e, i, j, x);
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
            int32_t x0 = uic_edit_text_width(e, text, ofs0);
            int32_t x1 = uic_edit_text_width(e, text, ofs1);
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
    const uic_edit_run_t* run = uic_edit_paragraph_runs(e, pn, &runs);
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
//          traceln("%d,%d", e->ui.x + x, e->ui.y + y);
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
            int32_t runs = uic_edit_paragraph_run_count(e, e->scroll.pn);
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
        int32_t runs = uic_edit_paragraph_run_count(e, e->scroll.pn);
        e->scroll.rn = min(e->scroll.rn, runs - 1);
        if (e->scroll.rn == 0 && e->scroll.pn > 0) {
            e->scroll.pn--;
            e->scroll.rn = uic_edit_paragraph_run_count(e, e->scroll.pn) - 1;
        } else if (e->scroll.rn > 0) {
            e->scroll.rn--;
        }
        assert(e->scroll.pn >= 0 && e->scroll.rn >= 0);
        assert(0 <= e->scroll.rn &&
                    e->scroll.rn < uic_edit_paragraph_run_count(e, e->scroll.pn));
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
        const int32_t pn = e->scroll.pn;
        const int32_t bottom = e->bottom;
        for (int32_t i = pn; i < e->paragraphs && py < bottom; i++) {
            int32_t runs = uic_edit_paragraph_run_count(e, i);
            const int32_t fvr = uic_edit_first_visible_run(e, i);
            for (int32_t j = fvr; j < runs && py < bottom; j++) {
                last = uic_edit_uint64(i, j);
                py += e->ui.em.y;
            }
        }
        uic_edit_paragraph_g2b(e, e->paragraphs - 1);
        uic_edit_pg_t last_paragraph = {.pn = e->paragraphs - 1,
            .gp = e->para[e->paragraphs - 1].glyphs };
        uic_edit_pr_t lp = uic_edit_pg_to_pr(e, last_paragraph);
        uint64_t eof = uic_edit_uint64(e->paragraphs - 1, lp.rn);
        if (last == eof && py <= bottom - e->ui.em.y) {
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
                if (pt.y + e->ui.em.y > bottom - e->ui.em.y) { break; }
                if (e->scroll.rn > 0) {
                    e->scroll.rn--;
                } else {
                    e->scroll.pn--;
                    e->scroll.rn = uic_edit_paragraph_run_count(e, e->scroll.pn) - 1;
                }
            }
        }
    }
}

static void uic_edit_move_caret(uic_edit_t* e, const uic_edit_pg_t pg) {
    uic_edit_scroll_into_view(e, pg);
    ui_point_t pt = e->width > 0 ? // width == 0 means no measure/layout yet
        uic_edit_pg_to_xy(e, pg) : (ui_point_t){0, 0};
    uic_edit_set_caret(e, pt.x, pt.y + e->top);
    e->selection[1] = pg;
//  traceln("pn: %d gp: %d", pg.pn, pg.gp);
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
        char* text = uic_edit_alloc(bytes);
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
        uic_edit_paragraph_g2b(e, pn0);
        const int32_t bp0 = e->para[pn0].g2b[gp0];
        if (pn0 == pn1) { // inside same paragraph
            const int32_t bp1 = e->para[pn0].g2b[gp1];
            uic_clip_append(a, ab, limit, s0 + bp0, bp1 - bp0);
            if (cut) {
                if (e->para[pn0].allocated == 0) {
                    int32_t n = bytes0 - (bp1 - bp0);
                    s0 = uic_edit_alloc(n);
                    memcpy(s0, e->para[pn0].text, bp0);
                    e->para[pn0].text = s0;
                    e->para[pn0].allocated = n;
                }
                assert(bytes0 - bp1 >= 0);
                memcpy(s0 + bp0, s1 + bp1, (size_t)bytes0 - bp1);
                e->para[pn0].bytes -= (bp1 - bp0);
                e->para[pn0].glyphs = -1; // will relayout
            }
        } else {
            uic_clip_append(a, ab, limit, s0 + bp0, bytes0 - bp0);
            uic_clip_append(a, ab, limit, "\n", 1);
            for (int32_t i = pn0 + 1; i < pn1; i++) {
                uic_clip_append(a, ab, limit, e->para[i].text, e->para[i].bytes);
                uic_clip_append(a, ab, limit, "\n", 1);
            }
            const int32_t bytes1 = e->para[pn1].bytes;
            uic_edit_paragraph_g2b(e, pn1);
            const int32_t bp1 = e->para[pn1].g2b[gp1];
            uic_clip_append(a, ab, limit, s1, bp1);
            if (cut) {
                int32_t total = bp0 + bytes1 - bp1;
                s0 = uic_edit_ensure(e, pn0, total, bp0);
                assert(bytes1 - bp1 >= 0);
                memcpy(s0 + bp0, s1 + bp1, (size_t)bytes1 - bp1);
                e->para[pn0].bytes = bp0 + bytes1 - bp1;
                e->para[pn0].glyphs = -1; // will relayout
            }
        }
        int32_t deleted = cut ? pn1 - pn0 : 0;
        if (deleted > 0) {
            assert(pn0 + deleted < e->paragraphs);
            for (int32_t i = pn0 + 1; i <= pn0 + deleted; i++) {
                if (e->para[i].allocated > 0) {
                    uic_edit_free(&e->para[i].text);
                }
            }
            for (int32_t i = pn0 + 1; i < e->paragraphs - deleted; i++) {
                e->para[i] = e->para[i + deleted];
            }
            for (int32_t i = e->paragraphs - deleted; i < e->paragraphs; i++) {
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
    for (int32_t i = e->paragraphs - 1; i > pn; i--) {
        e->para[i] = e->para[i - 1];
    }
    uic_edit_para_t* p = &e->para[pn];
    p->text = null;
    p->bytes = 0;
    p->glyphs = -1;
    p->allocated = 0;
    p->runs = 0;
    p->run = null;
    p->g2b = null;
    p->g2b_allocated = 0;
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
    uic_edit_paragraph_g2b(e, pg.pn);
    char* s = e->para[pg.pn].text;
    const int32_t bp = e->para[pg.pn].g2b[pg.gp];
    int32_t n = (b + bytes) * 3 / 2; // heuristics 1.5 times of total
    if (e->para[pg.pn].allocated == 0) {
        s = uic_edit_alloc(n);
        memcpy(s, e->para[pg.pn].text, b);
        e->para[pg.pn].text = s;
        e->para[pg.pn].allocated = n;
    } else if (e->para[pg.pn].allocated < b + bytes) {
        uic_edit_reallocate(&s, n, 1);
        e->para[pg.pn].text = s;
        e->para[pg.pn].allocated = n;
    }
    s = e->para[pg.pn].text;
    assert(b - bp >= 0);
    memmove(s + bp + bytes, s + bp, (size_t)b - bp); // make space
    memcpy(s + bp, text, bytes);
    e->para[pg.pn].bytes += bytes;
    uic_edit_dispose_paragraphs_layout(e);
    pg.gp = uic_edit_glyphs(s, bp + bytes);
    return pg;
}

static uic_edit_pg_t uic_edit_insert_paragraph_break(uic_edit_t* e,
        uic_edit_pg_t pg) {
    uic_edit_insert_paragraph(e, pg.pn + (pg.pn < e->paragraphs));
    const int32_t bytes = e->para[pg.pn].bytes;
    char* s = e->para[pg.pn].text;
    uic_edit_paragraph_g2b(e, pg.pn);
    const int32_t bp = e->para[pg.pn].g2b[pg.gp];
    uic_edit_pg_t next = {.pn = pg.pn + 1, .gp = 0};
    if (bp < bytes) {
        (void)uic_edit_insert_inline(e, next, s + bp, bytes - bp);
    } else {
        uic_edit_dispose_paragraphs_layout(e);
    }
    e->para[pg.pn].bytes = bp;
    return next;
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
    } else if (to.pn > 0 || uic_edit_pg_to_pr(e, to).rn > 0) {
        // top of the text
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
            const uic_edit_run_t* run = uic_edit_paragraph_runs(e, to.pn, &runs);
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
    int32_t runs = uic_edit_paragraph_run_count(e, pn);
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
            int32_t runs = uic_edit_paragraph_run_count(e, i);
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
        const uic_edit_run_t* run = uic_edit_paragraph_runs(e, pn, &runs);
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
    e->selection[1] = uic_edit_insert_paragraph_break(e, e->selection[1]);
    e->selection[0] = e->selection[1];
    uic_edit_move_caret(e, e->selection[1]);
}

void uic_edit_next_fuzz(uic_edit_t* e);

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
            if (app.ctrl && app.shift && e->fuzzer == null) {
                e->fuzz(e); // start on Ctrl+Shift+F5
            } else if (e->fuzzer != null) {
                e->fuzz(e); // stop on F5
            }

        }
    }
    if (e->fuzzer != null) { uic_edit_next_fuzz(e); }
}

static void uic_edit_character(uic_t* unused(ui), const char* utf8) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden && !ui->disabled);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        char ch = utf8[0];
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
        ui->invalidate(ui);
        if (e->fuzzer != null) { uic_edit_next_fuzz(e); }
    }
}

typedef  struct uic_edit_glyph_s {
    const char* s;
    int32_t bytes;
} uic_edit_glyph_t;

static uic_edit_glyph_t uic_edit_glyph_at(uic_edit_t* e, uic_edit_pg_t p) {
    uic_edit_glyph_t g = { .s = "", .bytes = 0 };
    if (p.pn == e->paragraphs) {
        assert(p.gp == 0); // last empty paragraph
    } else {
        uic_edit_paragraph_g2b(e, p.pn);
        const int32_t bytes = e->para[p.pn].bytes;
        char* s = e->para[p.pn].text;
        const int32_t bp = e->para[p.pn].g2b[p.gp];
        if (bp < bytes) {
            g.s = s + bp;
            g.bytes = uic_edit_glyph_bytes(*g.s);
//          traceln("glyph: %.*s 0x%02X bytes: %d", g.bytes, g.s, *g.s, g.bytes);
        }
    }
    return g;
}

static void uic_edit_select_word(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t p = uic_edit_xy_to_pg(e, x, y);
    if (0 <= p.pn && 0 <= p.gp) {
        if (p.pn > e->paragraphs) { p.pn = max(0, e->paragraphs); }
        int32_t glyphs = uic_edit_glyphs_in_paragraph(e, p.pn);
        if (p.gp > glyphs) { p.gp = max(0, glyphs); }
        if (p.pn == e->paragraphs || glyphs == 0) {
            // last paragraph is empty - nothing to select on double click
        } else {
            uic_edit_glyph_t glyph = uic_edit_glyph_at(e, p);
            bool not_a_white_space = glyph.bytes > 0 &&
                *(uint8_t*)glyph.s > 0x20;
            if (!not_a_white_space && p.gp > 0) {
                p.gp--;
                glyph = uic_edit_glyph_at(e, p);
                not_a_white_space = glyph.bytes > 0 &&
                    *(uint8_t*)glyph.s > 0x20;
            }
            if (glyph.bytes > 0 && *(uint8_t*)glyph.s > 0x20) {
                uic_edit_pg_t from = p;
                while (from.gp > 0) {
                    from.gp--;
                    uic_edit_glyph_t g = uic_edit_glyph_at(e, from);
                    if (g.bytes == 0 || *(uint8_t*)g.s <= 0x20) {
                        from.gp++;
                        break;
                    }
                }
                e->selection[0] = from;
                uic_edit_pg_t to = p;
                while (to.gp < glyphs) {
                    to.gp++;
                    uic_edit_glyph_t g = uic_edit_glyph_at(e, to);
                    if (g.bytes == 0 || *(uint8_t*)g.s <= 0x20) {
                        break;
                    }
                }
                e->selection[1] = to;
                e->ui.invalidate(&e->ui);
                e->mouse = 0;
            }
        }
    }
}

static void uic_edit_select_paragraph(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t p = uic_edit_xy_to_pg(e, x, y);
    if (0 <= p.pn && 0 <= p.gp) {
        if (p.pn > e->paragraphs) { p.pn = max(0, e->paragraphs); }
        int32_t glyphs = uic_edit_glyphs_in_paragraph(e, p.pn);
        if (p.gp > glyphs) { p.gp = max(0, glyphs); }
        if (p.pn == e->paragraphs || glyphs == 0) {
            // last paragraph is empty - nothing to select on double click
        } else if (p.pn == e->selection[0].pn &&
                (e->selection[0].gp <= p.gp && p.gp <= e->selection[1].gp) ||
                (e->selection[1].gp <= p.gp && p.gp <= e->selection[0].gp)){
            e->selection[0].gp = 0;
            e->selection[1].gp = 0;
            e->selection[1].pn++;
        }
        e->ui.invalidate(&e->ui);
        e->mouse = 0;
    }
}

static void uic_edit_double_click(uic_edit_t* e, int32_t x, int32_t y) {
    if (e->selection[0].pn == e->selection[1].pn &&
        e->selection[0].gp == e->selection[1].gp) {
        uic_edit_select_word(e, x, y);
    } else {
        if (e->selection[0].pn == e->selection[1].pn &&
               e->selection[0].pn <= e->paragraphs) {
            uic_edit_select_paragraph(e, x, y);
        }
    }
}

static void uic_edit_click(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t p = uic_edit_xy_to_pg(e, x, y);
    if (0 <= p.pn && 0 <= p.gp) {
        if (p.pn > e->paragraphs) { p.pn = max(0, e->paragraphs); }
        int32_t glyphs = uic_edit_glyphs_in_paragraph(e, p.pn);
        if (p.gp > glyphs) { p.gp = max(0, glyphs); }
        uic_edit_move_caret(e, p);
    }
}

static void uic_edit_focus_on_click(uic_edit_t* e, int32_t x, int32_t y) {
    if (app.focused && !e->focused && e->mouse != 0) {
        if (app.focus != null && app.focus->kill_focus != null) {
            app.focus->kill_focus(app.focus);
        }
//      traceln("app.focus %p := %p", app.focus, &e->ui);
        app.focus = &e->ui;
        bool set = e->ui.set_focus(&e->ui);
        fatal_if(!set);
    }
    if (app.focused && e->focused && e->mouse != 0) {
        e->mouse = 0;
        uic_edit_click(e, x, y);
    }
}

static void uic_edit_mouse_button_down(uic_edit_t* e, int32_t m,
        int32_t x, int32_t y) {
    if (m == messages.left_button_pressed)  { e->mouse |= (1 << 0); }
    if (m == messages.right_button_pressed) { e->mouse |= (1 << 1); }
    uic_edit_focus_on_click(e, x, y);
}

static void uic_edit_mouse_button_up(uic_edit_t* e, int32_t m) {
    if (m == messages.left_button_released)  { e->mouse &= ~(1 << 0); }
    if (m == messages.right_button_released) { e->mouse &= ~(1 << 1); }
}

#ifdef EDIT_USE_TAP

static bool uic_edit_tap(uic_t* ui, int32_t ix) {
    traceln("ix: %d", ix);
    if (ix == 0) {
        uic_edit_t* e = (uic_edit_t*)ui;
        const int32_t x = app.mouse.x - e->ui.x;
        const int32_t y = app.mouse.y - e->ui.y - e->top;
        bool inside = 0 <= x && x < ui->w && 0 <= y && y < e->height;
        if (inside) {
            e->mouse = 0x1;
            uic_edit_focus_on_click(e, x, y);
            e->mouse = 0x0;
        }
        return inside;
    } else {
        return false; // do NOT consume event
    }
}

#endif // EDIT_USE_TAP

static bool uic_edit_press(uic_t* ui, int32_t ix) {
//  traceln("ix: %d", ix);
    if (ix == 0) {
        uic_edit_t* e = (uic_edit_t*)ui;
        const int32_t x = app.mouse.x - e->ui.x;
        const int32_t y = app.mouse.y - e->ui.y - e->top;
        bool inside = 0 <= x && x < ui->w && 0 <= y && y < e->height;
        if (inside) {
            e->mouse = 0x1;
            uic_edit_focus_on_click(e, x, y);
            uic_edit_double_click(e, x, y);
            e->mouse = 0x0;
        }
        return inside;
    } else {
        return false; // do NOT consume event
    }
}

static void uic_edit_mouse(uic_t* ui, int32_t m, int32_t unused(flags)) {
//  if (m == messages.left_button_pressed) { traceln("%p", ui); }
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    assert(!ui->disabled);
    uic_edit_t* e = (uic_edit_t*)ui;
    const int32_t x = app.mouse.x - e->ui.x;
    const int32_t y = app.mouse.y - e->ui.y - e->top;
    bool inside = 0 <= x && x < ui->w && 0 <= y && y < e->height;
    if (inside) {
        if (m == messages.left_button_pressed ||
            m == messages.right_button_pressed) {
            uic_edit_mouse_button_down(e, m, x, y);
        } else if (m == messages.left_button_released ||
                   m == messages.right_button_released) {
            uic_edit_mouse_button_up(e, m);
        } else if (m == messages.left_double_click ||
                   m == messages.right_double_click) {
            uic_edit_double_click(e, x, y);
        }
    }
}

static void uic_edit_mousewheel(uic_t* ui, int32_t unused(dx), int32_t dy) {
    // TODO: may make a use of dx in single line not-word-breaked edit control
    if (app.focus == ui) {
        assert(ui->tag == uic_tag_edit);
        uic_edit_t* e = (uic_edit_t*)ui;
        int32_t lines = (abs(dy) + ui->em.y - 1) / ui->em.y;
        if (dy > 0) {
            uic_edit_scroll_down(e, lines);
        } else if (dy < 0) {
            uic_edit_scroll_up(e, lines);
        }
//  TODO: Ctrl UP/DW and caret of out of visible area scrolls are not
//        implemented. Not sure they are very good UX experience.
//        MacOS users may be used to scroll with touchpad, take a visual
//        peek, do NOT click and continue editing at last cursor position.
//        To me back forward stack navigation is much more intuitive and
//        much mode "modeless" in spirit of cut/copy/paste. But opinions
//        and editing habits vary. Easy to implement.
        uic_edit_pg_t pg = uic_edit_xy_to_pg(e, e->caret.x, e->caret.y);
        uic_edit_move_caret(e, pg);
    }
}

static bool uic_edit_set_focus(uic_t* ui) {
//  traceln("active=%d", GetActiveWindow() == (HWND)app.window);
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
    assert(app.focus == ui || app.focus == null);
    assert(ui->focusable);
    app.focus = ui;
    if (app.focused) {
        uic_edit_create_caret(e);
        uic_edit_show_caret(e);
    }
    return true;
}

static void uic_edit_kill_focus(uic_t* ui) {
//  traceln("active=%d", GetActiveWindow() == (HWND)app.window);
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        uic_edit_hide_caret(e);
        uic_edit_destroy_caret(e);
    }
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
        char* text = uic_edit_alloc(n + 1);
        uic_edit_pg_t pg = uic_edit_op(e, cut, from, to, text, &n);
        if (cut && pg.pn >= 0 && pg.gp >= 0) {
            e->selection[0] = pg;
            e->selection[1] = pg;
            uic_edit_move_caret(e, pg);
        }
        text[n] = 0; // make it zero terminated
        clipboard.copy_text(text);
        assert(n == strlen(text), "n=%d strlen(cb)=%d cb=\"%s\"",
               n, strlen(text), text);
        uic_edit_free(&text);
    }
}

static void uic_edit_select_all(uic_edit_t* e) {
    e->selection[0] = (uic_edit_pg_t ){.pn = 0, .gp = 0};
    e->selection[1] = (uic_edit_pg_t ){.pn = e->paragraphs, .gp = 0};
    e->ui.invalidate(&e->ui);
}

static int32_t uic_edit_copy(uic_edit_t* e, char* text, int32_t* bytes) {
    not_null(bytes);
    int32_t r = 0;
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

static uic_edit_pg_t uic_edit_paste_text(uic_edit_t* e,
        const char* s, int32_t n) {
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
            pg = uic_edit_insert_paragraph_break(e, pg);
        }
        text = s + next;
        i = next;
    }
    return pg;
}

static void uic_edit_paste(uic_edit_t* e, const char* s, int32_t n) {
    e->erase(e);
    e->selection[1] = uic_edit_paste_text(e, s, n);
    e->selection[0] = e->selection[1];
    if (e->ui.w > 0) { uic_edit_move_caret(e, e->selection[1]); }
}

static void uic_edit_clipboard_paste(uic_edit_t* e) {
    uic_edit_pg_t pg = e->selection[1];
    int32_t bytes = 0;
    clipboard.text(null, &bytes);
    if (bytes > 0) {
        char* text = uic_edit_alloc(bytes);
        int32_t r = clipboard.text(text, &bytes);
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
    assert(ui->tag == uic_tag_edit);
    ui->em = gdi.get_em(*ui->font);
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
    int32_t runs = uic_edit_paragraph_run_count(e, e->scroll.pn);
    e->scroll.rn = uic_edit_pg_to_pr(e, scroll).rn;
    assert(0 <= e->scroll.rn && e->scroll.rn < runs); (void)runs;
    // For single line editor distribute vertical gap evenly between
    // top and bottom. For multiline snap top line to y coordinate 0
    // otherwise resizing view will result in up-down jiggling of the
    // whole text
    if (e->focused) {
        // recreate caret because em.y may have changed
        uic_edit_hide_caret(e);
        uic_edit_destroy_caret(e);
        uic_edit_create_caret(e);
        uic_edit_show_caret(e);
        uic_edit_move_caret(e, e->selection[1]);
    }
}

static void uic_edit_paint(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    gdi.push(ui->x, ui->y + e->top);
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(rgb(20, 20, 14));
    gdi.fill(ui->x, ui->y, ui->w, e->height);
    gdi.set_clip(ui->x, ui->y, ui->w, e->height);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    f = gdi.set_font(f);
    gdi.set_text_color(ui->color);
    const int32_t pn = e->scroll.pn;
    const int32_t bottom = ui->y + e->bottom;
    assert(pn <= e->paragraphs);
    for (int32_t i = pn; i < e->paragraphs && gdi.y < bottom; i++) {
        uic_edit_paint_para(e, i);
    }
    gdi.set_font(f);
    gdi.set_clip(0, 0, 0, 0);
    gdi.pop();
}

static void uic_edit_move(uic_edit_t* e, uic_edit_pg_t pg) {
    if (e->width > 0) {
        uic_edit_move_caret(e, pg); // may select text on move
    } else {
        e->selection[1] = pg;
    }
    e->selection[0] = e->selection[1];
}

static bool uic_edit_message(uic_t* ui, int32_t unused(m), 
        int64_t unused(wp), int64_t unused(lp), int64_t* unused(rt)) {
    uic_edit_t* e = (uic_edit_t*)ui;
    if (app.focused && e->focused != (app.focus == ui)) {
        if (e->focused) { 
            uic_edit_kill_focus(ui); 
        } else {
            uic_edit_set_focus(ui); 
        }
    }
    return false;
}

void uic_edit_init_with_lorem_ipsum(uic_edit_t* e);
void uic_edit_fuzz(uic_edit_t* e);

void uic_edit_init(uic_edit_t* e) {
    memset(e, 0, sizeof(*e));
    uic_init(&e->ui);
    e->ui.tag = uic_tag_edit;
    e->ui.focusable = true;
    e->fuzz_seed  = 1; // client can seed it with (crt.nanoseconds() | 1)
    e->last_x     = -1;
    e->multiline  = true;
    e->monospaced = false;
    e->ui.color   = rgb(168, 168, 150); // colors.text;
    e->caret      = (ui_point_t){-1, -1};
    e->ui.message     = uic_edit_message;
    e->ui.paint       = uic_edit_paint;
    e->ui.measure     = uic_edit_measure;
    e->ui.layout      = uic_edit_layout;
    #ifdef EDIT_USE_TAP
    e->ui.tap         = uic_edit_tap;
    #else
    e->ui.mouse       = uic_edit_mouse;
    #endif
    e->ui.press       = uic_edit_press;
    e->ui.character   = uic_edit_character;
    e->ui.set_focus   = uic_edit_set_focus;
    e->ui.kill_focus  = uic_edit_kill_focus;
    e->ui.key_pressed = uic_edit_key_pressed;
    e->ui.mousewheel  = uic_edit_mousewheel;
    e->set_font             = uic_edit_set_font;
    e->move                 = uic_edit_move;
    e->paste                = uic_edit_paste;
    e->copy                 = uic_edit_copy;
    e->cut_to_clipboard     = uic_edit_clipboard_cut;
    e->copy_to_clipboard    = uic_edit_clipboard_copy;
    e->paste_from_clipboard = uic_edit_clipboard_paste;
    e->select_all           = uic_edit_select_all;
    e->erase                = uic_edit_erase;
    e->fuzz                 = uic_edit_fuzz;
    uic_edit_init_with_lorem_ipsum(e);
    // Expected manifest.xml containing UTF-8 code page
    // for Translate message and WM_CHAR to deliver UTF-8 characters
    // see: https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
    if (GetACP() != 65001) {
        traceln("codepage: %d UTF-8 will not be supported", GetACP());
    }
    // at the moment of writing there is no API call to inform Windows about process
    // preferred codepage except manifest.xml file in resource #1.
    // Absence of manifest.xml will result to ancient and useless ANSI 1252 codepage
    // TODO: may be change quick.h to use CreateWindowW() and translate UTF16 to UTF8
}

end_c
