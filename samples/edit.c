/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"

// TODO: undo/redo
// TODO: back/forward navigation
// TODO: exit/save keystrokes?

// http://worrydream.com/refs/Tesler%20-%20A%20Personal%20History%20of%20Modeless%20Text%20Editing%20and%20Cut-Copy-Paste.pdf
// https://web.archive.org/web/20221216044359/http://worrydream.com/refs/Tesler%20-%20A%20Personal%20History%20of%20Modeless%20Text%20Editing%20and%20Cut-Copy-Paste.pdf

// Rich text options that are not addressed yet:
// * Color of ranges (useful for code editing)
// * Soft line breaks inside the paragraph (useful for e.g. bullet lists of options)
// * Bold/Italic/Underline (along with color ranges)
// * Multiple fonts (as long as run vertical size is the maximum of font)
// * Kerning (?! like in overhung "Fl")

begin_c

// When implementation and header are amalgamated 
// into a single file header library name_space is 
// used to separate different modules namespaces. 

#pragma push_macro("fn") // function
#pragma push_macro("ns") // name space

#define ns(name) uic_edit_ ## name
#define fn(type, name) static type ns(name)

typedef  struct uic_edit_glyph_s {
    const char* s;
    int32_t bytes;
} uic_edit_glyph_t;

fn(void, layout)(uic_t* ui);

// Glyphs in monospaced Windows fonts may have different width for non-ASCII
// characters. Thus even if edit is monospaced glyph measurements are used
// in text layout.

fn(uint64_t, uint64)(int32_t high, int32_t low) {
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

fn(void*,  alloc)(int32_t bytes) {
    void* p = malloc(bytes);
    not_null(p);
    return p;
}

fn(void, allocate)(void** pp, int32_t count, size_t element) {
    not_null(pp);
    assert((int64_t)count * element <= INT_MAX);
    *pp = ns(alloc)(count * (int32_t)element);
}

fn(void, free)(void** pp) {
    not_null(pp);
    // free(null) is acceptable but may indicate unbalanced logic
    not_null(*pp);
    free(*pp);
    *pp = null;
}

fn(void, reallocate)(void** pp, int32_t count, size_t element) {
    not_null(pp);
    assert((int64_t)count * element <= INT_MAX);
    if (*pp == null) {
        ns(allocate)(pp, count, element);
    } else {
        *pp = realloc(*pp, count * (size_t)element);
        not_null(*pp);
    }
}

fn(void, invalidate)(uic_edit_t* e) {
//  traceln("");
    e->ui.invalidate(&e->ui);
}

fn(int32_t, text_width)(uic_edit_t* e, const char* s, int32_t n) {
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

fn(int32_t, glyph_bytes)(char start_byte_value) { // utf-8
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

// g2b() return number of glyphs in text and fills optional
// g2b[] array with glyphs positions.

fn(int32_t, g2b)(const char* utf8, int32_t bytes, int32_t g2b[]) {
    int32_t i = 0;
    int32_t k = 1;
    // g2b[k] start postion in byte offset from utf8 text of glyph[k]
    if (g2b != null) { g2b[0] = 0; }
    while (i < bytes) {
        i += ns(glyph_bytes)(utf8[i]);
        if (g2b != null) { g2b[k] = i; }
        k++;
    }
    return k - 1;
}

fn(int32_t, glyphs)(const char* utf8, int32_t bytes) {
    return ns(g2b)(utf8, bytes, null);
}

fn(int32_t, gp_to_bytes)(const char* s, int32_t bytes, int32_t gp) {
    int32_t c = 0;
    int32_t i = 0;
    if (bytes > 0) {
        while (c < gp) {
            assert(i < bytes);
            i += ns(glyph_bytes)(s[i]);
            c++;
        }
    }
    assert(i <= bytes);
    return i;
}

fn(void, paragraph_g2b)(uic_edit_t* e, int32_t pn) {
    assert(0 <= pn && pn < e->paragraphs);
    uic_edit_para_t* p = &e->para[pn];
    if (p->glyphs < 0) {
        const int32_t bytes = p->bytes;
        const int32_t n = p->bytes + 1;
        const int32_t a = (n * sizeof(int32_t)) * 3 / 2; // heuristic
        if (p->g2b_capacity < a) {
            ns(reallocate)(&p->g2b, n, sizeof(int32_t));
            p->g2b_capacity = a;
        }
        const char* utf8 = p->text;
        p->g2b[0] = 0; // first glyph starts at 0
        int32_t i = 0;
        int32_t k = 1;
        // g2b[k] start postion in byte offset from utf8 text of glyph[k]
        while (i < bytes) {
            i += ns(glyph_bytes)(utf8[i]);
            p->g2b[k] = i;
            k++;
        }
        p->glyphs = k - 1;
    }
}

fn(int32_t, word_break_at)(uic_edit_t* e, int32_t pn, int32_t rn,
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
        // 4 is maximum number of bytes in a UTF-8 sequence
        int32_t gc = min(4, glyphs_in_this_run);
        int32_t w = ns(text_width)(e, text, g2b[gc] - bp);
        while (gc < glyphs_in_this_run && w < width) {
            gc = min(gc * 4, glyphs_in_this_run);
            w = ns(text_width)(e, text, g2b[gc] - bp);
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
                int32_t px = ns(text_width)(e, text, n);
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

fn(int32_t, word_break)(uic_edit_t* e, int32_t pn, int32_t rn) {
    return ns(word_break_at)(e, pn, rn, e->ui.w, false);
}

fn(int32_t, glyph_at_x)(uic_edit_t* e, int32_t pn, int32_t rn,
        int32_t x) {
    if (x == 0 || e->para[pn].bytes == 0) {
        return 0;
    } else {
        return ns(word_break_at)(e, pn, rn, x + 1, true);
    }
}

fn(uic_edit_glyph_t, glyph_at)(uic_edit_t* e, uic_edit_pg_t p) {
    uic_edit_glyph_t g = { .s = "", .bytes = 0 };
    if (p.pn == e->paragraphs) {
        assert(p.gp == 0); // last empty paragraph
    } else {
        ns(paragraph_g2b)(e, p.pn);
        const int32_t bytes = e->para[p.pn].bytes;
        char* s = e->para[p.pn].text;
        const int32_t bp = e->para[p.pn].g2b[p.gp];
        if (bp < bytes) {
            g.s = s + bp;
            g.bytes = ns(glyph_bytes)(*g.s);
//          traceln("glyph: %.*s 0x%02X bytes: %d", g.bytes, g.s, *g.s, g.bytes);
        }
    }
    return g;
}

// paragraph_runs() breaks paragraph into `runs` according to `width`

fn(const uic_edit_run_t*, paragraph_runs)(uic_edit_t* e, int32_t pn,
        int32_t* runs) {
//  double time = crt.seconds();
    assert(ui->w > 0);
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
        ns(paragraph_g2b)(e, pn);
        uic_edit_para_t* p = &e->para[pn];
        if (p->run == null) {
            assert(p->runs == 0 && p->run == null);
            const int32_t max_runs = p->bytes + 1;
            ns(allocate)(&p->run, max_runs, sizeof(uic_edit_run_t));
            uic_edit_run_t* run = p->run;
            run[0].bp = 0;
            run[0].gp = 0;
            int32_t gc = p->bytes == 0 ? 0 : ns(word_break)(e, pn, 0);
            if (gc == p->glyphs) { // whole paragraph fits into width
                p->runs = 1;
                run[0].bytes  = p->bytes;
                run[0].glyphs = p->glyphs;
                int32_t pixels = ns(text_width)(e, p->text, p->g2b[gc]);
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
                    int32_t glyphs = ns(word_break)(e, pn, rc);
                    int32_t utf8bytes = p->g2b[ix + glyphs] - run[rc].bp;
                    int32_t pixels = ns(text_width)(e, text, utf8bytes);
                    if (glyphs > 1 && utf8bytes < bytes && text[utf8bytes - 1] != 0x20) {
                        // try to find word break SPACE character. utf8 space is 0x20
                        int32_t i = utf8bytes;
                        while (i > 0 && text[i - 1] != 0x20) { i--; }
                        if (i > 0 && i != utf8bytes) {
                            utf8bytes = i;
                            glyphs = ns(glyphs)(text, utf8bytes);
                            pixels = ns(text_width)(e, text, utf8bytes);
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
                p->runs = rc; // truncate heap capacity array:
                ns(reallocate)(&p->run, rc, sizeof(uic_edit_run_t));
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

fn(int32_t, paragraph_run_count)(uic_edit_t* e, int32_t pn) {
    int32_t runs = 0;
    (void)ns(paragraph_runs)(e, pn, &runs);
    return runs;
}

fn(int32_t, glyphs_in_paragraph)(uic_edit_t* e, int32_t pn) {
    (void)ns(paragraph_run_count)(e, pn); // word break into runs
    return e->para[pn].glyphs;
}

fn(void, create_caret)(uic_edit_t* e) {
    fatal_if(e->focused);
    assert(app.is_active());
    assert(app.has_focus());
    int32_t caret_width = min(2, max(1, app.dpi.monitor_effective / 100));
//  traceln("%d,%d", caret_width, e->ui.em.y);
    app.create_caret(caret_width, e->ui.em.y);
    e->focused = true; // means caret was created
}

fn(void, destroy_caret)(uic_edit_t* e) {
    fatal_if(!e->focused);
    app.destroy_caret();
    e->focused = false; // means caret was destroyed
//  traceln("");
}

fn(void, show_caret)(uic_edit_t* e) {
    if (e->focused) {
        assert(app.is_active());
        assert(app.has_focus());
        app.move_caret(e->ui.x + e->caret.x, e->ui.y + e->caret.y);
        // TODO: it is possible to support unblinking caret if desired
        // do not set blink time - use global default
//      fatal_if_false(SetCaretBlinkTime(500));
        app.show_caret();
        e->shown++;
//      traceln("shown=%d", e->shown);
        assert(e->shown == 1);
    }
}

fn(void, hide_caret)(uic_edit_t* e) {
    if (e->focused) {
        app.hide_caret();
        e->shown--;
//      traceln("shown=%d", e->shown);
        assert(e->shown == 0);
    }
}

fn(void, dispose_paragraphs_layout)(uic_edit_t* e) {
    for (int32_t i = 0; i < e->paragraphs; i++) {
        uic_edit_para_t* p = &e->para[i];
        if (p->run != null) {
            ns(free)(&p->run);
        }
        if (p->g2b != null) {
            ns(free)(&p->g2b);
        }
        p->glyphs = -1;
        p->runs = 0;
        p->g2b_capacity = 0;
    }
}

fn(void, layout_now)(uic_edit_t* e) {
    if (e->ui.measure != null && e->ui.layout != null && e->ui.w > 0) {
        ns(dispose_paragraphs_layout)(e);
        e->ui.measure(&e->ui);
        e->ui.layout(&e->ui);
        ns(invalidate)(e);
    }
}

fn(void, if_sle_layout)(uic_edit_t* e) {
    // only for single line edit controls that were already initialized 
    // and measured horizontally at least once.
    if (e->sle && e->ui.layout != null && e->ui.w > 0) {
        ns(layout_now)(e);
    }
}

fn(void, set_font)(uic_edit_t* e, font_t* f) {
    ns(dispose_paragraphs_layout)(e);
    e->scroll.rn = 0;
    e->ui.font = f;
    e->ui.em = gdi.get_em(*f);
    ns(layout_now)(e);
}

// Paragraph number, glyph number -> run number

fn(const uic_edit_pr_t, pg_to_pr)(uic_edit_t* e, const uic_edit_pg_t pg) {
    uic_edit_pr_t pr = { .pn = pg.pn, .rn = -1 };
    if (pg.pn == e->paragraphs || e->para[pg.pn].bytes == 0) { // last or empty
        assert(pg.gp == 0);
        pr.rn = 0;
    } else {
        assert(0 <= pg.pn && pg.pn < e->paragraphs);
        int32_t runs = 0;
        const uic_edit_run_t* run = ns(paragraph_runs)(e, pg.pn, &runs);
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

fn(int32_t, runs_between)(uic_edit_t* e, const uic_edit_pg_t pg0,
        const uic_edit_pg_t pg1) {
    assert(ns(uint64)(pg0.pn, pg0.gp) <= ns(uint64)(pg1.pn, pg1.gp));
    int32_t rn0 = ns(pg_to_pr)(e, pg0).rn;
    int32_t rn1 = ns(pg_to_pr)(e, pg1).rn;
    int32_t rc = 0;
    if (pg0.pn == pg1.pn) {
        assert(rn0 <= rn1);
        rc = rn1 - rn0;
    } else {
        assert(pg0.pn < pg1.pn);
        for (int32_t i = pg0.pn; i < pg1.pn; i++) {
            const int32_t runs = ns(paragraph_run_count)(e, i);
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

fn(uic_edit_pg_t, scroll_pg)(uic_edit_t* e) {
    int32_t runs = 0;
    const uic_edit_run_t* run = ns(paragraph_runs)(e, e->scroll.pn, &runs);
    assert(0 <= e->scroll.rn && e->scroll.rn < runs);
    return (uic_edit_pg_t) { .pn = e->scroll.pn, .gp = run[e->scroll.rn].gp };
}

fn(int32_t, first_visible_run)(uic_edit_t* e, int32_t pn) {
    return pn == e->scroll.pn ? e->scroll.rn : 0;
}

// uic_edit::pg_to_xy() paragraph # glyph # -> (x,y) in [0,0  width x height]

fn(ui_point_t, pg_to_xy)(uic_edit_t* e, const uic_edit_pg_t pg) {
    ui_point_t pt = { .x = -1, .y = 0 };
    for (int32_t i = e->scroll.pn; i < e->paragraphs && pt.x < 0; i++) {
        int32_t runs = 0;
        const uic_edit_run_t* run = ns(paragraph_runs)(e, i, &runs);
        for (int32_t j = ns(first_visible_run)(e, i); j < runs; j++) {
            const int32_t last_run = j == runs - 1;
            int32_t gc = run[j].glyphs;
            if (i == pg.pn) {
                // in the last `run` of a paragraph x after last glyph is OK
                if (run[j].gp <= pg.gp && pg.gp < run[j].gp + gc + last_run) {
                    const char* s = e->para[i].text + run[j].bp;
                    int32_t ofs = ns(gp_to_bytes)(s, run[j].bytes,
                        pg.gp - run[j].gp);
                    pt.x = ns(text_width)(e, s, ofs);
                    break;
                }
            }
            pt.y += e->ui.em.y;
        }
    }
    if (pg.pn == e->paragraphs) { pt.x = 0; }
    if (0 <= pt.x && pt.x < e->ui.w && 0 <= pt.y && pt.y <= e->ui.h) {
        // all good, inside visible rectangle or right after it
    } else {
        traceln("outside (%d,%d) %dx%d", pt.x, pt.y, e->ui.w, e->ui.h);
    }
    return pt;
}

fn(int32_t, glyph_width_px)(uic_edit_t* e, const uic_edit_pg_t pg) {
    char* text = e->para[pg.pn].text;
    int32_t gc = e->para[pg.pn].glyphs;
    if (pg.gp == 0 &&  gc == 0) {
        return 0; // empty paragraph
    } else if (pg.gp < gc) {
        char* s = text + ns(gp_to_bytes)(text, e->para[pg.pn].bytes, pg.gp);
        int32_t bytes_in_glyph = ns(glyph_bytes)(*s);
        int32_t x = ns(text_width)(e, s, bytes_in_glyph);
        return x;
    } else {
        assert(pg.gp == gc, "only next position past last glyph is allowed");
        return 0;
    }
}

// xy_to_pg() (x,y) (0,0, width x height) -> paragraph # glyph #

fn(uic_edit_pg_t, xy_to_pg)(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t pg = {-1, -1};
    int32_t py = 0; // paragraph `y' coordinate
    for (int32_t i = e->scroll.pn; i < e->paragraphs && pg.pn < 0; i++) {
        int32_t runs = 0;
        const uic_edit_run_t* run = ns(paragraph_runs)(e, i, &runs);
        for (int32_t j = ns(first_visible_run)(e, i); j < runs && pg.pn < 0; j++) {
            const uic_edit_run_t* r = &run[j];
            char* s = e->para[i].text + run[j].bp;
            if (py <= y && y < py + e->ui.em.y) {
                int32_t w = ns(text_width)(e, s, r->bytes);
                pg.pn = i;
                if (x >= w) {
                    const int32_t last_run = j == runs - 1;
                    pg.gp = r->gp + max(0, r->glyphs - 1 + last_run);
                } else {
                    pg.gp = r->gp + ns(glyph_at_x)(e, i, j, x);
                    if (pg.gp < r->glyphs - 1) {
                        uic_edit_pg_t right = {pg.pn, pg.gp + 1};
                        int32_t x0 = ns(pg_to_xy)(e, pg).x;
                        int32_t x1 = ns(pg_to_xy)(e, right).x;
                        if (x1 - x < x - x0) {
                            pg.gp++; // snap to closest glyph's 'x'
                        }
                    }
                }
            } else {
                py += e->ui.em.y;
            }
        }
        if (py > e->ui.h) { break; }
    }
    if (pg.pn < 0 && pg.gp < 0) {
        pg.pn = e->paragraphs;
        pg.gp = 0;
    }
    return pg;
}

fn(void, paint_selection)(uic_edit_t* e, const uic_edit_run_t* r,
        const char* text, int32_t pn, int32_t c0, int32_t c1) {
    uint64_t s0 = ns(uint64)(e->selection[0].pn, e->selection[0].gp);
    uint64_t e0 = ns(uint64)(e->selection[1].pn, e->selection[1].gp);
    if (s0 > e0) {
        uint64_t swap = e0;
        e0 = s0;
        s0 = swap;
    }
    uint64_t s1 = ns(uint64)(pn, c0);
    uint64_t e1 = ns(uint64)(pn, c1);
    if (s0 <= e1 && s1 <= e0) {
        uint64_t start = max(s0, s1) - c0;
        uint64_t end = min(e0, e1) - c0;
        if (start < end) {
            int32_t fro = (int32_t)start;
            int32_t to  = (int32_t)end;
            int32_t ofs0 = ns(gp_to_bytes)(text, r->bytes, fro);
            int32_t ofs1 = ns(gp_to_bytes)(text, r->bytes, to);
            int32_t x0 = ns(text_width)(e, text, ofs0);
            int32_t x1 = ns(text_width)(e, text, ofs1);
            brush_t b = gdi.set_brush(gdi.brush_color);
            color_t c = gdi.set_brush_color(rgb(48, 64, 72));
            gdi.fill(gdi.x + x0, gdi.y, x1 - x0, e->ui.em.y);
            gdi.set_brush_color(c);
            gdi.set_brush(b);
        }
    }
}

fn(void, paint_paragraph)(uic_edit_t* e, int32_t pn) {
    int32_t runs = 0;
    const uic_edit_run_t* run = ns(paragraph_runs)(e, pn, &runs);
    for (int32_t j = ns(first_visible_run)(e, pn);
                 j < runs && gdi.y < e->ui.y + e->bottom; j++) {
        char* text = e->para[pn].text + run[j].bp;
        gdi.x = e->ui.x;
        ns(paint_selection)(e, &run[j], text, pn, run[j].gp, run[j].gp + run[j].glyphs);
        gdi.text("%.*s", run[j].bytes, text);
        gdi.y += e->ui.em.y;
    }
}

fn(void, set_caret)(uic_edit_t* e, int32_t x, int32_t y) {
    if (e->caret.x != x || e->caret.y != y) {
        if (e->focused && app.has_focus()) {
            app.move_caret(e->ui.x + x, e->ui.y + y);
//          traceln("%d,%d", e->ui.x + x, e->ui.y + y);
        }
        e->caret.x = x;
        e->caret.y = y;
    }
}

// scroll_up() text moves up (north) in the visible view,
// scroll position increments moves down (south)

fn(void, scroll_up)(uic_edit_t* e, int32_t run_count) {
    assert(0 < run_count, "does it make sense to have 0 scroll?");
    const uic_edit_pg_t eof = {.pn = e->paragraphs, .gp = 0};
    while (run_count > 0 && e->scroll.pn < e->paragraphs) {
        uic_edit_pg_t scroll = ns(scroll_pg)(e);
        int32_t between = ns(runs_between)(e, scroll, eof);
        if (between <= e->visible_runs - 1) {
            run_count = 0; // enough
        } else {
            int32_t runs = ns(paragraph_run_count)(e, e->scroll.pn);
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
    ns(if_sle_layout)(e);
    ns(invalidate)(e);
}

// scroll_dw() text moves down (south) in the visible view,
// scroll position decrements moves up (north)

fn(void, scroll_down)(uic_edit_t* e, int32_t run_count) {
    assert(0 < run_count, "does it make sense to have 0 scroll?");
    while (run_count > 0 && (e->scroll.pn > 0 || e->scroll.rn > 0)) {
        int32_t runs = ns(paragraph_run_count)(e, e->scroll.pn);
        e->scroll.rn = min(e->scroll.rn, runs - 1);
        if (e->scroll.rn == 0 && e->scroll.pn > 0) {
            e->scroll.pn--;
            e->scroll.rn = ns(paragraph_run_count)(e, e->scroll.pn) - 1;
        } else if (e->scroll.rn > 0) {
            e->scroll.rn--;
        }
        assert(e->scroll.pn >= 0 && e->scroll.rn >= 0);
        assert(0 <= e->scroll.rn &&
                    e->scroll.rn < ns(paragraph_run_count)(e, e->scroll.pn));
        run_count--;
    }
    ns(if_sle_layout)(e);
}

fn(void, scroll_into_view)(uic_edit_t* e, const uic_edit_pg_t pg) {
    if (e->paragraphs > 0 && e->bottom > 0) {
        if (e->sle) { assert(pg.pn == 0); }
        const int32_t rn = ns(pg_to_pr)(e, pg).rn;
        const uint64_t scroll = ns(uint64)(e->scroll.pn, e->scroll.rn);
        const uint64_t caret  = ns(uint64)(pg.pn, rn);
        uint64_t last = 0;
        int32_t py = 0;
        const int32_t pn = e->scroll.pn;
        const int32_t bottom = e->bottom;
        for (int32_t i = pn; i < e->paragraphs && py < bottom; i++) {
            int32_t runs = ns(paragraph_run_count)(e, i);
            const int32_t fvr = ns(first_visible_run)(e, i);
            for (int32_t j = fvr; j < runs && py < bottom; j++) {
                last = ns(uint64)(i, j);
                py += e->ui.em.y;
            }
        }
        int32_t sle_runs = e->sle && e->ui.w > 0 ? 
            ns(paragraph_run_count)(e, 0) : 0;
        ns(paragraph_g2b)(e, e->paragraphs - 1);
        uic_edit_pg_t last_paragraph = {.pn = e->paragraphs - 1,
            .gp = e->para[e->paragraphs - 1].glyphs };
        uic_edit_pr_t lp = ns(pg_to_pr)(e, last_paragraph);
        uint64_t eof = ns(uint64)(e->paragraphs - 1, lp.rn);
        if (last == eof && py <= bottom - e->ui.em.y) {
            // vertical white space for EOF on the screen
            last = ns(uint64)(e->paragraphs, 0);
        }
        if (scroll <= caret && caret < last) {
            // no scroll
        } else if (caret < scroll) {
            e->scroll.pn = pg.pn;
            e->scroll.rn = rn;
        } else if (e->sle && sle_runs * e->ui.em.y <= e->ui.h) {
            // single line edit control fits vertically - no scroll
        } else {
            assert(caret >= last);
            e->scroll.pn = pg.pn;
            e->scroll.rn = rn;
            while (e->scroll.pn > 0 || e->scroll.rn > 0) {
                ui_point_t pt = ns(pg_to_xy)(e, pg);
                if (pt.y + e->ui.em.y > bottom - e->ui.em.y) { break; }
                if (e->scroll.rn > 0) {
                    e->scroll.rn--;
                } else {
                    e->scroll.pn--;
                    e->scroll.rn = ns(paragraph_run_count)(e, e->scroll.pn) - 1;
                }
            }
        }
    }
}

fn(void, move_caret)(uic_edit_t* e, const uic_edit_pg_t pg) {
    // single line edit control cannot move caret past fist paragraph
    bool can_move = !e->sle || pg.pn < e->paragraphs;
    if (can_move) {
        ns(scroll_into_view)(e, pg);
        ui_point_t pt = e->ui.w > 0 ? // width == 0 means no measure/layout yet
            ns(pg_to_xy)(e, pg) : (ui_point_t){0, 0};
        ns(set_caret)(e, pt.x, pt.y + e->top);
        e->selection[1] = pg;
//      traceln("pn: %d gp: %d", pg.pn, pg.gp);
        if (!app.shift && !e->mouse != 0) {
            e->selection[0] = e->selection[1];
        }
        ns(invalidate)(e);
    }
}

fn(char*, ensure)(uic_edit_t* e, int32_t pn, int32_t bytes,
        int32_t preserve) {
    assert(bytes >= 0 && preserve <= bytes);
    if (bytes <= e->para[pn].capacity) {
        // enough memory already capacity - do nothing
    } else if (e->para[pn].capacity > 0) {
        assert(preserve <= e->para[pn].capacity);
        ns(reallocate)(&e->para[pn].text, bytes, 1);
        fatal_if_null(e->para[pn].text);
        e->para[pn].capacity = bytes;
    } else {
        assert(e->para[pn].capacity == 0);
        char* text = ns(alloc)(bytes);
        e->para[pn].capacity = bytes;
        memcpy(text, e->para[pn].text, preserve);
        e->para[pn].text = text;
        e->para[pn].bytes = preserve;
    }
    return e->para[pn].text;
}

fn(uic_edit_pg_t, op)(uic_edit_t* e, bool cut,
        uic_edit_pg_t from, uic_edit_pg_t to, 
        char* text, int32_t* bytes) {
    #pragma push_macro("clip_append")
    #define clip_append(a, ab, mx, text, bytes) do {  \
        int32_t ba = (bytes); /* bytes to append */            \
        if (a != null) {                                       \
            assert(ab <= mx);                                  \
            memcpy(a, text, ba);                               \
            a += ba;                                           \
        }                                                      \
        ab += ba;                                              \
    } while (0)
    char* a = text; // append
    int32_t ab = 0; // appended bytes
    int32_t limit = bytes != null ? *bytes : 0; // max byes in text
    uint64_t f = ns(uint64)(from.pn, from.gp);
    uint64_t t = ns(uint64)(to.pn, to.gp);
    if (f != t) {
        ns(dispose_paragraphs_layout)(e);
        if (f > t) { uint64_t swap = t; t = f; f = swap; }
        int32_t pn0 = (int32_t)(f >> 32);
        int32_t gp0 = (int32_t)(f);
        int32_t pn1 = (int32_t)(t >> 32);
        int32_t gp1 = (int32_t)(t);
        if (pn1 == e->paragraphs) { // last empty paragraph
            assert(gp1 == 0);
            pn1 = e->paragraphs - 1;
            gp1 = ns(g2b)(e->para[pn1].text, e->para[pn1].bytes, null);
        }
        const int32_t bytes0 = e->para[pn0].bytes;
        char* s0 = e->para[pn0].text;
        char* s1 = e->para[pn1].text;
        ns(paragraph_g2b)(e, pn0);
        const int32_t bp0 = e->para[pn0].g2b[gp0];
        if (pn0 == pn1) { // inside same paragraph
            const int32_t bp1 = e->para[pn0].g2b[gp1];
            clip_append(a, ab, limit, s0 + bp0, bp1 - bp0);
            if (cut) {
                if (e->para[pn0].capacity == 0) {
                    int32_t n = bytes0 - (bp1 - bp0);
                    s0 = ns(alloc)(n);
                    memcpy(s0, e->para[pn0].text, bp0);
                    e->para[pn0].text = s0;
                    e->para[pn0].capacity = n;
                }
                assert(bytes0 - bp1 >= 0);
                memcpy(s0 + bp0, s1 + bp1, (size_t)bytes0 - bp1);
                e->para[pn0].bytes -= (bp1 - bp0);
                e->para[pn0].glyphs = -1; // will relayout
            }
        } else {
            clip_append(a, ab, limit, s0 + bp0, bytes0 - bp0);
            clip_append(a, ab, limit, "\n", 1);
            for (int32_t i = pn0 + 1; i < pn1; i++) {
                clip_append(a, ab, limit, e->para[i].text, e->para[i].bytes);
                clip_append(a, ab, limit, "\n", 1);
            }
            const int32_t bytes1 = e->para[pn1].bytes;
            ns(paragraph_g2b)(e, pn1);
            const int32_t bp1 = e->para[pn1].g2b[gp1];
            clip_append(a, ab, limit, s1, bp1);
            if (cut) {
                int32_t total = bp0 + bytes1 - bp1;
                s0 = ns(ensure)(e, pn0, total, bp0);
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
                if (e->para[i].capacity > 0) {
                    ns(free)(&e->para[i].text);
                }
            }
            for (int32_t i = pn0 + 1; i < e->paragraphs - deleted; i++) {
                e->para[i] = e->para[i + deleted];
            }
            for (int32_t i = e->paragraphs - deleted; i < e->paragraphs; i++) {
                memset(&e->para[i], 0, sizeof(e->para[i]));
            }
        }
        if (t == ns(uint64)(e->paragraphs, 0)) {
            clip_append(a, ab, limit, "\n", 1);
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
    ns(if_sle_layout)(e);
    return from;
    #pragma pop_macro("clip_append")
}

fn(void, insert_paragraph)(uic_edit_t* e, int32_t pn) {
    ns(dispose_paragraphs_layout)(e);
    if (e->paragraphs + 1 > e->capacity / (int32_t)sizeof(uic_edit_para_t)) {
        int32_t n = (e->paragraphs + 1) * 3 / 2; // 1.5 times
        ns(reallocate)(&e->para, n, (int32_t)sizeof(uic_edit_para_t));
        e->capacity = n * (int32_t)sizeof(uic_edit_para_t);
    }
    e->paragraphs++;
    for (int32_t i = e->paragraphs - 1; i > pn; i--) {
        e->para[i] = e->para[i - 1];
    }
    uic_edit_para_t* p = &e->para[pn];
    p->text = null;
    p->bytes = 0;
    p->glyphs = -1;
    p->capacity = 0;
    p->runs = 0;
    p->run = null;
    p->g2b = null;
    p->g2b_capacity = 0;
}

// insert_inline() inserts text (not containing \n paragraph
// break inside a paragraph)

fn(uic_edit_pg_t, insert_inline)(uic_edit_t* e, uic_edit_pg_t pg,
        const char* text, int32_t bytes) {
    assert(bytes > 0); (void)(void*)unused(strnchr);
    assert(strnchr(text, bytes, '\n') == null,
           "text \"%s\" must not contain \\n character.", text);
    if (pg.pn == e->paragraphs) {
        ns(insert_paragraph)(e, pg.pn);
    }
    const int32_t b = e->para[pg.pn].bytes;
    ns(paragraph_g2b)(e, pg.pn);
    char* s = e->para[pg.pn].text;
    const int32_t bp = e->para[pg.pn].g2b[pg.gp];
    int32_t n = (b + bytes) * 3 / 2; // heuristics 1.5 times of total
    if (e->para[pg.pn].capacity == 0) {
        s = ns(alloc)(n);
        memcpy(s, e->para[pg.pn].text, b);
        e->para[pg.pn].text = s;
        e->para[pg.pn].capacity = n;
    } else if (e->para[pg.pn].capacity < b + bytes) {
        ns(reallocate)(&s, n, 1);
        e->para[pg.pn].text = s;
        e->para[pg.pn].capacity = n;
    }
    s = e->para[pg.pn].text;
    assert(b - bp >= 0);
    memmove(s + bp + bytes, s + bp, (size_t)b - bp); // make space
    memcpy(s + bp, text, bytes);
    e->para[pg.pn].bytes += bytes;
    ns(dispose_paragraphs_layout)(e);
    pg.gp = ns(glyphs)(s, bp + bytes);
    ns(if_sle_layout)(e);
    return pg;
}

fn(uic_edit_pg_t, insert_paragraph_break)(uic_edit_t* e,
        uic_edit_pg_t pg) {
    ns(insert_paragraph)(e, pg.pn + (pg.pn < e->paragraphs));
    const int32_t bytes = e->para[pg.pn].bytes;
    char* s = e->para[pg.pn].text;
    ns(paragraph_g2b)(e, pg.pn);
    const int32_t bp = e->para[pg.pn].g2b[pg.gp];
    uic_edit_pg_t next = {.pn = pg.pn + 1, .gp = 0};
    if (bp < bytes) {
        (void)ns(insert_inline)(e, next, s + bp, bytes - bp);
    } else {
        ns(dispose_paragraphs_layout)(e);
    }
    e->para[pg.pn].bytes = bp;
    return next;
}

fn(void, key_left)(uic_edit_t* e) {
    uic_edit_pg_t to = e->selection[1];
    if (to.pn > 0 || to.gp > 0) {
        ui_point_t pt = ns(pg_to_xy)(e, to);
        if (pt.x == 0 && pt.y == 0) {
            ns(scroll_down)(e, 1);
        }
        if (to.gp > 0) {
            to.gp--;
        } else if (to.pn > 0) {
            to.pn--;
            to.gp = ns(glyphs_in_paragraph)(e, to.pn);
        }
        ns(move_caret)(e, to);
        e->last_x = -1;
    }
}

fn(void, key_right)(uic_edit_t* e) {
    uic_edit_pg_t to = e->selection[1];
    if (to.pn < e->paragraphs) {
        int32_t glyphs = ns(glyphs_in_paragraph)(e, to.pn);
        if (to.gp < glyphs) {
            to.gp++;
            ns(scroll_into_view)(e, to);
        } else if (!e->sle) {
            to.pn++;
            to.gp = 0;
            ns(scroll_into_view)(e, to);
        }
        ns(move_caret)(e, to);
        e->last_x = -1;
    }
}

fn(void, reuse_last_x)(uic_edit_t* e, ui_point_t* pt) {
    // Vertical caret movement visually tend to move caret horizontally
    // in proportional font text. Remembering starting `x' value for vertical
    // movements alleviates this unpleasant UX experience to some degree.
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

fn(void, key_up)(uic_edit_t* e) {
    const uic_edit_pg_t pg = e->selection[1];
    uic_edit_pg_t to = pg;
    if (to.pn == e->paragraphs) {
        assert(to.gp == 0); // positioned past EOF
        to.pn--;
        to.gp = e->para[to.pn].glyphs;
        ns(scroll_into_view)(e, to);
        ui_point_t pt = ns(pg_to_xy)(e, to);
        pt.x = 0;
        to.gp = ns(xy_to_pg)(e, pt.x, pt.y).gp;
    } else if (to.pn > 0 || ns(pg_to_pr)(e, to).rn > 0) {
        // top of the text
        ui_point_t pt = ns(pg_to_xy)(e, to);
        if (pt.y == 0) {
            ns(scroll_down)(e, 1);
        } else {
            pt.y -= 1;
        }
        ns(reuse_last_x)(e, &pt);
        assert(pt.y >= 0);
        to = ns(xy_to_pg)(e, pt.x, pt.y);
        assert(to.pn >= 0 && to.gp >= 0);
        int32_t rn0 = ns(pg_to_pr)(e, pg).rn;
        int32_t rn1 = ns(pg_to_pr)(e, to).rn;
        if (rn1 > 0 && rn0 == rn1) { // same run
            assert(to.gp > 0, "word break must not break on zero gp");
            int32_t runs = 0;
            const uic_edit_run_t* run = ns(paragraph_runs)(e, to.pn, &runs);
            to.gp = run[rn1].gp;
        }
    }
    ns(move_caret)(e, to);
}

fn(void, key_down)(uic_edit_t* e) {
    const uic_edit_pg_t pg = e->selection[1];
    ui_point_t pt = ns(pg_to_xy)(e, pg);
    ns(reuse_last_x)(e, &pt);
    // scroll runs guaranteed to be already layout for current state of view:
    uic_edit_pg_t scroll = ns(scroll_pg)(e);
    int32_t run_count = ns(runs_between)(e, scroll, pg);
    if (!e->sle && run_count >= e->visible_runs - 1) {
        ns(scroll_up)(e, 1);
    } else {
        pt.y += e->ui.em.y;
    }
    uic_edit_pg_t to = ns(xy_to_pg)(e, pt.x, pt.y);
    if (to.pn < 0 && to.gp < 0) {
        to.pn = e->paragraphs; // advance past EOF
        to.gp = 0;
    }
    ns(move_caret)(e, to);
}

fn(void, key_home)(uic_edit_t* e) {
    if (app.ctrl) {
        e->scroll.pn = 0;
        e->scroll.rn = 0;
        e->selection[1].pn = 0;
        e->selection[1].gp = 0;
    }
    const int32_t pn = e->selection[1].pn;
    int32_t runs = ns(paragraph_run_count)(e, pn);
    const uic_edit_para_t* para = &e->para[pn];
    if (runs <= 1) {
        e->selection[1].gp = 0;
    } else {
        int32_t rn = ns(pg_to_pr)(e, e->selection[1]).rn;
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
    ns(move_caret)(e, e->selection[1]);
}

fn(void, key_end)(uic_edit_t* e) {
    if (app.ctrl) {
        int32_t py = e->bottom;
        for (int32_t i = e->paragraphs - 1; i >= 0 && py >= e->ui.em.y; i--) {
            int32_t runs = ns(paragraph_run_count)(e, i);
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
        const uic_edit_run_t* run = ns(paragraph_runs)(e, pn, &runs);
        int32_t rn = ns(pg_to_pr)(e, e->selection[1]).rn;
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
    ns(move_caret)(e, e->selection[1]);
}

fn(void, key_pageup)(uic_edit_t* e) {
    int32_t n = max(1, e->visible_runs - 1);
    uic_edit_pg_t scr = ns(scroll_pg)(e);
    uic_edit_pg_t bof = {.pn = 0, .gp = 0};
    int32_t m = ns(runs_between)(e, bof, scr);
    if (m > n) {
        ui_point_t pt = ns(pg_to_xy)(e, e->selection[1]);
        uic_edit_pr_t scroll = e->scroll;
        ns(scroll_down)(e, n);
        if (scroll.pn != e->scroll.pn || scroll.rn != e->scroll.rn) {
            uic_edit_pg_t pg = ns(xy_to_pg)(e, pt.x, pt.y);
            ns(move_caret)(e, pg);
        }
    } else {
        ns(move_caret)(e, bof);
    }
}

fn(void, key_pagedw)(uic_edit_t* e) {
    int32_t n = max(1, e->visible_runs - 1);
    uic_edit_pg_t scr = ns(scroll_pg)(e);
    uic_edit_pg_t eof = {.pn = e->paragraphs, .gp = 0};
    int32_t m = ns(runs_between)(e, scr, eof);
    if (m > n) {
        ui_point_t pt = ns(pg_to_xy)(e, e->selection[1]);
        uic_edit_pr_t scroll = e->scroll;
        ns(scroll_up)(e, n);
        if (scroll.pn != e->scroll.pn || scroll.rn != e->scroll.rn) {
            uic_edit_pg_t pg = ns(xy_to_pg)(e, pt.x, pt.y);
            ns(move_caret)(e, pg);
        }
    } else {
        ns(move_caret)(e, eof);
    }
}

fn(void, key_delete)(uic_edit_t* e) {
    uint64_t f = ns(uint64)(e->selection[0].pn, e->selection[0].gp);
    uint64_t t = ns(uint64)(e->selection[1].pn, e->selection[1].gp);
    uint64_t eof = ns(uint64)(e->paragraphs, 0);
    if (f == t && t != eof) {
        uic_edit_pg_t s1 = e->selection[1];
        e->key_right(e);
        e->selection[1] = s1;
    }
    e->erase(e);
}

fn(void, key_backspace)(uic_edit_t* e) {
    uint64_t f = ns(uint64)(e->selection[0].pn, e->selection[0].gp);
    uint64_t t = ns(uint64)(e->selection[1].pn, e->selection[1].gp);
    if (t != 0 && f == t) {
        uic_edit_pg_t s1 = e->selection[1];
        e->key_left(e);
        e->selection[1] = s1;
    }
    e->erase(e);
}

fn(void, key_enter)(uic_edit_t* e) {
    assert(!e->ro);
    if (!e->sle) {
        e->erase(e);
        e->selection[1] = ns(insert_paragraph_break)(e, e->selection[1]);
        e->selection[0] = e->selection[1];
        ns(move_caret)(e, e->selection[1]);
    } else { // single line edit callback
        if (e->enter != null) { e->enter(e); }
    }
}

fn(void, key_pressed)(uic_t* ui, int32_t key) {
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        if (key == virtual_keys.down && e->selection[1].pn < e->paragraphs) {
            e->key_down(e);
        } else if (key == virtual_keys.up && e->paragraphs > 0) {
            e->key_up(e);
        } else if (key == virtual_keys.left) {
            e->key_left(e);
        } else if (key == virtual_keys.right) {
            e->key_right(e);
        } else if (key == virtual_keys.pageup) {
            e->key_pageup(e);
        } else if (key == virtual_keys.pagedw) {
            e->key_pagedw(e);
        } else if (key == virtual_keys.home) {
            e->key_home(e);
        } else if (key == virtual_keys.end) {
            e->key_end(e);
        } else if (key == virtual_keys.del && !e->ro) {
            e->key_delete(e);
        } else if (key == virtual_keys.back && !e->ro) {
            e->key_backspace(e);
        } else if (key == virtual_keys.enter && !e->ro) {
            e->key_enter(e);
        } else {
            // ignore other keys
        }
    }
    if (e->fuzzer != null) { e->next_fuzz(e); }
}

fn(void, character)(uic_t* unused(ui), const char* utf8) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden && !ui->disabled);
    #pragma push_macro("ctl")
    #define ctl(c) ((char)((c) - 'a' + 1))
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        char ch = utf8[0];
        if (app.ctrl) {
            if (ch == ctl('a')) { e->select_all(e); }
            if (ch == ctl('c')) { e->copy_to_clipboard(e); }
            if (!e->ro) {
                if (ch == ctl('x')) { e->cut_to_clipboard(e); }
                if (ch == ctl('v')) { e->paste_from_clipboard(e); }
            }
        }
        if (0x20 <= ch && !e->ro) { // 0x20 space
            int32_t bytes = ns(glyph_bytes)(ch);
            e->erase(e); // remove selected text to be replaced by glyph
            e->selection[1] = ns(insert_inline)(e, e->selection[1], utf8, bytes);
            e->selection[0] = e->selection[1];
            ns(move_caret)(e, e->selection[1]);
        }
        ns(invalidate)(e);
        if (e->fuzzer != null) { e->next_fuzz(e); }
    }
    #pragma pop_macro("ctl")
}

fn(void, select_word)(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t p = ns(xy_to_pg)(e, x, y);
    if (0 <= p.pn && 0 <= p.gp) {
        if (p.pn > e->paragraphs) { p.pn = max(0, e->paragraphs); }
        int32_t glyphs = ns(glyphs_in_paragraph)(e, p.pn);
        if (p.gp > glyphs) { p.gp = max(0, glyphs); }
        if (p.pn == e->paragraphs || glyphs == 0) {
            // last paragraph is empty - nothing to select on double click
        } else {
            uic_edit_glyph_t glyph = ns(glyph_at)(e, p);
            bool not_a_white_space = glyph.bytes > 0 &&
                *(uint8_t*)glyph.s > 0x20;
            if (!not_a_white_space && p.gp > 0) {
                p.gp--;
                glyph = ns(glyph_at)(e, p);
                not_a_white_space = glyph.bytes > 0 &&
                    *(uint8_t*)glyph.s > 0x20;
            }
            if (glyph.bytes > 0 && *(uint8_t*)glyph.s > 0x20) {
                uic_edit_pg_t from = p;
                while (from.gp > 0) {
                    from.gp--;
                    uic_edit_glyph_t g = ns(glyph_at)(e, from);
                    if (g.bytes == 0 || *(uint8_t*)g.s <= 0x20) {
                        from.gp++;
                        break;
                    }
                }
                e->selection[0] = from;
                uic_edit_pg_t to = p;
                while (to.gp < glyphs) {
                    to.gp++;
                    uic_edit_glyph_t g = ns(glyph_at)(e, to);
                    if (g.bytes == 0 || *(uint8_t*)g.s <= 0x20) {
                        break;
                    }
                }
                e->selection[1] = to;
                ns(invalidate)(e);
                e->mouse = 0;
            }
        }
    }
}

fn(void, select_paragraph)(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t p = ns(xy_to_pg)(e, x, y);
    if (0 <= p.pn && 0 <= p.gp) {
        if (p.pn > e->paragraphs) { p.pn = max(0, e->paragraphs); }
        int32_t glyphs = ns(glyphs_in_paragraph)(e, p.pn);
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
        ns(invalidate)(e);
        e->mouse = 0;
    }
}

fn(void, double_click)(uic_edit_t* e, int32_t x, int32_t y) {
    if (e->selection[0].pn == e->selection[1].pn &&
        e->selection[0].gp == e->selection[1].gp) {
        ns(select_word)(e, x, y);
    } else {
        if (e->selection[0].pn == e->selection[1].pn &&
               e->selection[0].pn <= e->paragraphs) {
            ns(select_paragraph)(e, x, y);
        }
    }
}

fn(void, click)(uic_edit_t* e, int32_t x, int32_t y) {
    uic_edit_pg_t p = ns(xy_to_pg)(e, x, y);
    if (0 <= p.pn && 0 <= p.gp) {
        if (p.pn > e->paragraphs) { p.pn = max(0, e->paragraphs); }
        int32_t glyphs = ns(glyphs_in_paragraph)(e, p.pn);
        if (p.gp > glyphs) { p.gp = max(0, glyphs); }
        ns(move_caret)(e, p);
    }
}

fn(void, focus_on_click)(uic_edit_t* e, int32_t x, int32_t y) {
    if (app.has_focus() && !e->focused && e->mouse != 0) {
        if (app.focus != null && app.focus->kill_focus != null) {
            app.focus->kill_focus(app.focus);
        }
//      traceln("app.focus %p := %p", app.focus, &e->ui);
        app.focus = &e->ui;
        bool set = e->ui.set_focus(&e->ui);
        fatal_if(!set);
    }
    if (app.has_focus() && e->focused && e->mouse != 0) {
        e->mouse = 0;
        ns(click)(e, x, y);
    }
}

fn(void, mouse_button_down)(uic_edit_t* e, int32_t m,
        int32_t x, int32_t y) {
    if (m == messages.left_button_pressed)  { e->mouse |= (1 << 0); }
    if (m == messages.right_button_pressed) { e->mouse |= (1 << 1); }
    ns(focus_on_click)(e, x, y);
}

fn(void, mouse_button_up)(uic_edit_t* e, int32_t m) {
    if (m == messages.left_button_released)  { e->mouse &= ~(1 << 0); }
    if (m == messages.right_button_released) { e->mouse &= ~(1 << 1); }
}

#ifdef EDIT_USE_TAP

fn(bool, tap)(uic_t* ui, int32_t ix) {
    traceln("ix: %d", ix);
    if (ix == 0) {
        uic_edit_t* e = (uic_edit_t*)ui;
        const int32_t x = app.mouse.x - e->ui.x;
        const int32_t y = app.mouse.y - e->ui.y - e->top;
        bool inside = 0 <= x && x < ui->w && 0 <= y && y < ui->h;
        if (inside) {
            e->mouse = 0x1;
            ns(focus_on_click)(e, x, y);
            e->mouse = 0x0;
        }
        return inside;
    } else {
        return false; // do NOT consume event
    }
}

#endif // EDIT_USE_TAP

fn(bool, press)(uic_t* ui, int32_t ix) {
//  traceln("ix: %d", ix);
    if (ix == 0) {
        uic_edit_t* e = (uic_edit_t*)ui;
        const int32_t x = app.mouse.x - e->ui.x;
        const int32_t y = app.mouse.y - e->ui.y - e->top;
        bool inside = 0 <= x && x < ui->w && 0 <= y && y < ui->h;
        if (inside) {
            e->mouse = 0x1;
            ns(focus_on_click)(e, x, y);
            ns(double_click)(e, x, y);
            e->mouse = 0x0;
        }
        return inside;
    } else {
        return false; // do NOT consume event
    }
}

fn(void, mouse)(uic_t* ui, int32_t m, int32_t unused(flags)) {
//  if (m == messages.left_button_pressed) { traceln("%p", ui); }
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    assert(!ui->disabled);
    uic_edit_t* e = (uic_edit_t*)ui;
    const int32_t x = app.mouse.x - e->ui.x;
    const int32_t y = app.mouse.y - e->ui.y - e->top;
    bool inside = 0 <= x && x < ui->w && 0 <= y && y < ui->h;
    if (inside) {
        if (m == messages.left_button_pressed ||
            m == messages.right_button_pressed) {
            ns(mouse_button_down)(e, m, x, y);
        } else if (m == messages.left_button_released ||
                   m == messages.right_button_released) {
            ns(mouse_button_up)(e, m);
        } else if (m == messages.left_double_click ||
                   m == messages.right_double_click) {
            ns(double_click)(e, x, y);
        }
    }
}

fn(void, mousewheel)(uic_t* ui, int32_t unused(dx), int32_t dy) {
    // TODO: may make a use of dx in single line not-word-breaked edit control
    if (app.focus == ui) {
        assert(ui->tag == uic_tag_edit);
        uic_edit_t* e = (uic_edit_t*)ui;
        int32_t lines = (abs(dy) + ui->em.y - 1) / ui->em.y;
        if (dy > 0) {
            ns(scroll_down)(e, lines);
        } else if (dy < 0) {
            ns(scroll_up)(e, lines);
        }
//  TODO: Ctrl UP/DW and caret of out of visible area scrolls are not
//        implemented. Not sure they are very good UX experience.
//        MacOS users may be used to scroll with touchpad, take a visual
//        peek, do NOT click and continue editing at last cursor position.
//        To me back forward stack navigation is much more intuitive and
//        much mode "modeless" in spirit of cut/copy/paste. But opinions
//        and editing habits vary. Easy to implement.
        uic_edit_pg_t pg = ns(xy_to_pg)(e, e->caret.x, e->caret.y);
        ns(move_caret)(e, pg);
    }
}

fn(bool, set_focus)(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
//  traceln("active=%d has_focus=%d focused=%d", 
//           app.is_active(), app.has_focus(), e->focused);
    assert(app.focus == ui || app.focus == null);
    assert(ui->focusable);
    app.focus = ui;
    if (app.has_focus() && !e->focused) {
        ns(create_caret)(e);
        ns(show_caret)(e);
        ns(if_sle_layout)(e);
    }
    return true;
}

fn(void, kill_focus)(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
//  traceln("active=%d has_focus=%d focused=%d", 
//           app.is_active(), app.has_focus(), e->focused);
    if (e->focused) {
        ns(hide_caret)(e);
        ns(destroy_caret)(e);
        ns(if_sle_layout)(e);
    }
    if (app.focus == ui) { app.focus = null; }
}

fn(void, erase)(uic_edit_t* e) {
    const uic_edit_pg_t from = e->selection[0];
    const uic_edit_pg_t to = e->selection[1];
    uic_edit_pg_t pg = ns(op)(e, true, from, to, null, null);
    if (pg.pn >= 0 && pg.gp >= 0) {
        e->selection[0] = pg;
        e->selection[1] = pg;
        ns(move_caret)(e, pg);
        ns(invalidate)(e);
    }
}

fn(void, cut_copy)(uic_edit_t* e, bool cut) {
    const uic_edit_pg_t from = e->selection[0];
    const uic_edit_pg_t to = e->selection[1];
    int32_t n = 0; // bytes between from..to
    ns(op)(e, false, from, to, null, &n);
    if (n > 0) {
        char* text = ns(alloc)(n + 1);
        uic_edit_pg_t pg = ns(op)(e, cut, from, to, text, &n);
        if (cut && pg.pn >= 0 && pg.gp >= 0) {
            e->selection[0] = pg;
            e->selection[1] = pg;
            ns(move_caret)(e, pg);
        }
        text[n] = 0; // make it zero terminated
        clipboard.copy_text(text);
        assert(n == strlen(text), "n=%d strlen(cb)=%d cb=\"%s\"",
               n, strlen(text), text);
        ns(free)(&text);
    }
}

fn(void, select_all)(uic_edit_t* e) {
    e->selection[0] = (uic_edit_pg_t ){.pn = 0, .gp = 0};
    e->selection[1] = (uic_edit_pg_t ){.pn = e->paragraphs, .gp = 0};
    ns(invalidate)(e);
}

fn(int32_t, copy)(uic_edit_t* e, char* text, int32_t* bytes) {
    not_null(bytes);
    int32_t r = 0;
    const uic_edit_pg_t from = {.pn = 0, .gp = 0};
    const uic_edit_pg_t to = {.pn = e->paragraphs, .gp = 0};
    int32_t n = 0; // bytes between from..to
    ns(op)(e, false, from, to, null, &n);
    if (text != null) {
        int32_t m = min(n, *bytes);
        enum { error_insufficient_buffer = 122 }; //  ERROR_INSUFFICIENT_BUFFER
        if (m < n) { r = error_insufficient_buffer; }
        ns(op)(e, false, from, to, text, &m);
    }
    *bytes = n;
    return r;
}

fn(void, clipboard_cut)(uic_edit_t* e) {
    if (!e->ro) { ns(cut_copy)(e, true); }
}

fn(void, clipboard_copy)(uic_edit_t* e) {
    ns(cut_copy)(e, false);
}

fn(uic_edit_pg_t, paste_text)(uic_edit_t* e,
        const char* s, int32_t n) {
    assert(!e->ro);
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
            pg = ns(insert_inline)(e, pg, text, b - i);
        }
        if (lf && e->sle) {
            break;
        } else if (lf) {
            pg = ns(insert_paragraph_break)(e, pg);
        }
        text = s + next;
        i = next;
    }
    return pg;
}

fn(void, paste)(uic_edit_t* e, const char* s, int32_t n) {
    if (!e->ro) {
        if (n < 0) { n = (int32_t)strlen(s); }
        e->erase(e);
        e->selection[1] = ns(paste_text)(e, s, n);
        e->selection[0] = e->selection[1];
        if (e->ui.w > 0) { ns(move_caret)(e, e->selection[1]); }
    }
}

fn(void, clipboard_paste)(uic_edit_t* e) {
    if (!e->ro) {
        uic_edit_pg_t pg = e->selection[1];
        int32_t bytes = 0;
        clipboard.text(null, &bytes);
        if (bytes > 0) {
            char* text = ns(alloc)(bytes);
            int32_t r = clipboard.text(text, &bytes);
            fatal_if_not_zero(r);
            if (bytes > 0 && text[bytes - 1] == 0) {
                bytes--; // clipboard includes zero terminator
            }
            if (bytes > 0) {
                e->erase(e);
                pg = ns(paste_text)(e, text, bytes);
                ns(move_caret)(e, pg);
            }
            ns(free)(&text);
        }
    }
}

fn(void, measure)(uic_t* ui) { // bottom up
    assert(ui->tag == uic_tag_edit);
    uic_edit_t* e = (uic_edit_t*)ui;
    ui->em = gdi.get_em(ui->font == null ? app.fonts.regular : *ui->font);
    // enforce minimum size - it makes it checking corner cases much simpler
    // and it's hard to edit anything in a smaller area - will result in bad UX
    if (ui->w < ui->em.x * 4) { ui->w = ui->em.x * 4; }
    if (ui->h < ui->em.y) { ui->h = ui->em.y; }
    if (e->sle) { // for SLE if more than one run resize vertical:
        int32_t runs = max(ns(paragraph_run_count)(e, 0), 1);
        if (ui->h < ui->em.y * runs) { ui->h = ui->em.y * runs; }
    }
}

fn(void, layout)(uic_t* ui) { // top down
    assert(ui->tag == uic_tag_edit);
    assert(ui->w > 0 && ui->h > 0); // could be `if'
    uic_edit_t* e = (uic_edit_t*)ui;
    // glyph position in scroll_pn paragraph:
    const uic_edit_pg_t scroll = ui->w == 0 ?
        (uic_edit_pg_t){0, 0} : ns(scroll_pg)(e);
    // the optimization of layout disposal with cached
    // width and height cannot guarantee correct layout
    // in other changing conditions, e.g. moving UI
    // between monitors with different DPI or font
    // changes by the caller (Ctrl +/- 0)...    
//  if (ui->w > 0 && ui->w != ui->w) {
//      ns(dispose_paragraphs_layout)(e);
//  }
    // always dispose paragraphs layout:
    ns(dispose_paragraphs_layout)(e);
    int32_t sle_height = 0;
    if (e->sle) {
        int32_t runs = max(ns(paragraph_run_count)(e, 0), 1);
        sle_height = min(e->ui.em.y * runs, ui->h);
    }
    e->top    = !e->sle ? 0 : (ui->h - sle_height) / 2;
    e->bottom = !e->sle ? ui->h : e->top + sle_height;
    e->visible_runs = (e->bottom - e->top) / e->ui.em.y; // fully visible
    // number of runs in e->scroll.pn may have changed with ui->w change
    int32_t runs = ns(paragraph_run_count)(e, e->scroll.pn);
    e->scroll.rn = ns(pg_to_pr)(e, scroll).rn;
    assert(0 <= e->scroll.rn && e->scroll.rn < runs); (void)runs;
    // For single line editor distribute vertical gap evenly between
    // top and bottom. For multiline snap top line to y coordinate 0
    // otherwise resizing view will result in up-down jiggling of the
    // whole text
    if (e->focused) {
        // recreate caret because em.y may have changed
        ns(hide_caret)(e);
        ns(destroy_caret)(e);
        ns(create_caret)(e);
        ns(show_caret)(e);
        ns(move_caret)(e, e->selection[1]);
    }
}

fn(void, paint)(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    gdi.push(ui->x, ui->y + e->top);
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(rgb(20, 20, 14));
    gdi.fill(ui->x, ui->y, ui->w, ui->h);
    gdi.set_clip(ui->x, ui->y, ui->w, ui->h);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    f = gdi.set_font(f);
    gdi.set_text_color(ui->color);
    const int32_t pn = e->scroll.pn;
    const int32_t bottom = ui->y + e->bottom;
    assert(pn <= e->paragraphs);
    for (int32_t i = pn; i < e->paragraphs && gdi.y < bottom; i++) {
        ns(paint_paragraph)(e, i);
    }
    gdi.set_font(f);
    gdi.set_clip(0, 0, 0, 0);
    gdi.pop();
}

fn(void, move)(uic_edit_t* e, uic_edit_pg_t pg) {
    if (e->ui.w > 0) {
        ns(move_caret)(e, pg); // may select text on move
    } else {
        e->selection[1] = pg;
    }
    e->selection[0] = e->selection[1];
}

fn(bool, message)(uic_t* ui, int32_t unused(m), int64_t unused(wp), 
        int64_t unused(lp), int64_t* unused(rt)) {
    uic_edit_t* e = (uic_edit_t*)ui;
    if (app.is_active() && app.has_focus() && !ui->hidden) {
        if (e->focused != (app.focus == ui)) {
//          traceln("message: 0x%04X e->focused != (app.focus == ui)", m);
            if (e->focused) {
                ui->kill_focus(ui);
            } else {
                ui->set_focus(ui);
            }
        }
    } else {
        // do nothing: when app will become active and focused
        //             it will react on app->focus changes
    }
    return false;
}

__declspec(dllimport) unsigned int __stdcall GetACP(void);

void ns(init)(uic_edit_t* e) {
    memset(e, 0, sizeof(*e));
    uic_init(&e->ui);
    e->ui.tag = uic_tag_edit;
    e->ui.focusable = true;
    e->fuzz_seed = 1; // client can seed it with (crt.nanoseconds() | 1)
    e->last_x    = -1;
    e->focused   = false;
    e->sle       = false;
    e->ro        = false;
    e->ui.color  = rgb(168, 168, 150); // colors.text;
    e->caret     = (ui_point_t){-1, -1};
    e->ui.message = ns(message);
    e->ui.paint   = ns(paint);
    e->ui.measure = ns(measure);
    e->ui.layout  = ns(layout);
    #ifdef EDIT_USE_TAP
    e->ui.tap     = ns(tap);
    #else
    e->ui.mouse   = ns(mouse);
    #endif
    e->ui.press       = ns(press);
    e->ui.character   = ns(character);
    e->ui.set_focus   = ns(set_focus);
    e->ui.kill_focus  = ns(kill_focus);
    e->ui.key_pressed = ns(key_pressed);
    e->ui.mousewheel  = ns(mousewheel);
    e->set_font       = ns(set_font);
    e->move           = ns(move);
    e->paste          = ns(paste);
    e->copy           = ns(copy);
    e->erase          = ns(erase);
    e->cut_to_clipboard = ns(clipboard_cut);
    e->copy_to_clipboard = ns(clipboard_copy);
    e->paste_from_clipboard = ns(clipboard_paste);
    e->select_all    = ns(select_all);
    e->key_down      = ns(key_down);
    e->key_up        = ns(key_up);
    e->key_left      = ns(key_left);
    e->key_right     = ns(key_right);
    e->key_pageup    = ns(key_pageup);
    e->key_pagedw    = ns(key_pagedw);
    e->key_home      = ns(key_home);
    e->key_end       = ns(key_end);
    e->key_delete    = ns(key_delete);
    e->key_backspace = ns(key_backspace);
    e->key_enter     = ns(key_enter);
    e->fuzz                 = null;
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

#pragma pop_macro("ns")
#pragma pop_macro("fn")

end_c
