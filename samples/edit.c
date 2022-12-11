/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"
#include <Windows.h>

begin_c

typedef struct uic_edit_runs_s {
    int32_t count;
    int32_t length[1024]; // only needed for vertically visible runs
} uic_edit_runs_t;

static void uic_edit_set_caret(uic_edit_t* e, int32_t ln, int32_t cl);

static int uic_edit_utf8_codepoint_bytes(char ch) { // first char of 1,2,3 or 4 bytes seq
    uint8_t uc = (uint8_t)ch;
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

static int32_t uic_edit_codepoints(const char* utf8, int32_t bytes, int32_t cp[]) {
    int i = 0;
    int k = 0;
    while (i < bytes) {
        if (cp != null) { cp[k] = i; }
        k++;
        i += uic_edit_utf8_codepoint_bytes(utf8[i]);
    }
    return k;
}

static int32_t uic_edit_utf8_length(const char* utf8, int32_t bytes) {
    return uic_edit_codepoints(utf8, bytes, null);
}

static int32_t uic_edit_column_to_offset(const char* utf8, int32_t column) {
    int32_t c = 0;
    int32_t i = 0;
    while (c < column) {
        c++;
        i += uic_edit_utf8_codepoint_bytes(utf8[i]);
    }
    return i;
}

static int32_t uic_edit_line_cpc(uic_edit_t* e, int32_t ln) { // code point count
    uic_edit_line_t* line = &e->line[ln];
    return uic_edit_utf8_length(line->text, line->bytes);
}

static int32_t uic_edit_break(font_t f, char* text, int32_t x,
        int32_t* codepoints, int32_t cpc) { // ~50 microseconds for 100 bytes
    // returns offset from text of the last utf8 character sequence that is
    // completely to the left of `x`
    int i = 0;
    int j = cpc;
    int32_t k = (i + j + 1) / 2;
    while (i < j - 1) {
        int32_t w = gdi.measure_text(f, "%.*s", codepoints[k], text).x;
//      traceln("%d %d %d %d %d", i, j, k, w, x);
        if (w == x) { break; }
        if (w < x) { i = k; } else { j = k; }
        k = (i + j + 1) / 2;
    }
//  int32_t wi = gdi.measure_text(f, "%.*s", i, text).x;
//  int32_t wj = gdi.measure_text(f, "%.*s", j, text).x;
//  int32_t wk = gdi.measure_text(f, "%.*s", k, text).x;
//  traceln("i %d j %d k %d %d %d %d", i, j, k, wi, wj, wk);
    assert(k > 0);
    return k - 1;
}

static int32_t uic_edit_break_cp(font_t f, char* text, int32_t bytes, int32_t x) {
    int32_t* codepoints = (int32_t*)stackalloc(bytes * sizeof(int32_t));
    int cpc = uic_edit_codepoints(text, bytes, codepoints); // codepoints count
    return codepoints[uic_edit_break(f, text, x, codepoints, cpc)];
}

static int32_t uic_edit_break_cl(font_t f, char* text, int32_t bytes, int32_t x) {
    int32_t* codepoints = (int32_t*)stackalloc(bytes * sizeof(int32_t));
    int cpc = uic_edit_codepoints(text, bytes, codepoints); // codepoints count
    return uic_edit_break(f, text, x, codepoints, cpc);
}

static void uic_edit_runs(uic_edit_t* e, uic_edit_line_t* line, int32_t width,
        uic_edit_runs_t* r) {
    font_t f = *e->ui.font;
    if (gdi.measure_text(f, "%.*s", line->bytes, line->text).x < width) {
        r->count = 1;
        r->length[0] = line->bytes; // whole line fits in
    } else {
        int c = 0;
        char* text = line->text;
        int32_t bytes = line->bytes;
        while (r->count < countof(r->length) && bytes > 0) {
            int32_t k = uic_edit_break_cp(f, text, bytes, width);
            int32_t i = k; // position of space left of k
            while (i > 0 && text[i] != 0x20) { i--; }
            if (i > 0) { k = i; }
            r->length[c] = k;
//          traceln("length[%d]=%d", c, r->length[c]);
            c++;
            if (gdi.measure_text(f, "%.*s", bytes - k, text + k).x < width) {
                if (bytes > k) {
                    r->length[c] = bytes - k;
//                  traceln("length[%d]=%d", c, r->length[c]);
                    c++;
                }
                break;
            }
            text += k;
            bytes -= k;
        }
        r->count = c;
//      traceln("runs=%d", c);
    }
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
    int32_t lines = e->ui.h / e->ui.em.y; // visible lines
    e->top = (e->ui.h - e->ui.em.y * lines) / 2;
    e->bottom = e->top + e->ui.em.y * lines;
    if (e->focused) {
        // recreate caret because em.y may have changed
        fatal_if_false(DestroyCaret());
        fatal_if_false(CreateCaret((HWND)app.window, null, 2, e->ui.em.y));
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
    pt.y = 0; // xxx ??? for now (needs scroll x)
    uic_edit_runs_t r;
    for (int i = e->scroll.ln; i < e->lines && pt.x < 0; i++) {
        uic_edit_runs(e, &e->line[i], e->ui.w, &r);
        char* s = e->line[i].text;
        int32_t bytes = e->line[i].bytes;
        int column = 0;
        for (int j = 0; j < r.count; j++) {
            int32_t cpc = uic_edit_utf8_length(s, r.length[j]);
            // can position cursor after last glyph of last run
            // but not on after word break end of line:
            int32_t high = j == r.count - 1 ? column + cpc : column + cpc - 1;
            if (i == ln && column <= cl && cl <= high) {
                int32_t offset = uic_edit_column_to_offset(s, cl - column);
                pt.x = gdi.measure_text(*e->ui.font, "%.*s", offset, s).x;
                break;
            }
            column += cpc;
            s += r.length[j];
            bytes -= r.length[j];
            pt.y += e->ui.em.y;
        }
    }
    return pt;
}

static int32_t uic_edit_char_width_px(uic_edit_t* e, int32_t ln, int32_t cl) {
    char* text = e->line[ln].text;
    int32_t offset = uic_edit_column_to_offset(text, cl);
    char* s = text + offset;
    int32_t bytes = uic_edit_utf8_codepoint_bytes(*s);
    return gdi.measure_text(*e->ui.font, "%.*s", bytes, s).x;
}

static uic_edit_position_t uic_edit_xy_to_lc(uic_edit_t* e, int32_t x, int32_t y) {
    font_t f = *e->ui.font;
    uic_edit_position_t p = {-1, -1};
    int line_y = 0;
//  traceln("x: %d y: %d", x, y);
    for (int i = e->scroll.ln; i < e->lines && p.ln < 0; i++) {
        uic_edit_runs_t r;
        uic_edit_runs(e, &e->line[i], e->ui.w, &r);
        char* s = e->line[i].text;
        int32_t bytes = e->line[i].bytes;
        int column = 0;
        for (int j = 0; j < r.count; j++) {
            int32_t cpc = uic_edit_utf8_length(s, r.length[j]);
            if (line_y <= y && y <= line_y + e->ui.em.y) {
                int32_t w = gdi.measure_text(f, "%.*s", r.length[j], s).x;
                p.ln = i;
                p.cl = column + uic_edit_break_cl(f, s, r.length[j], x);
                // allow mouse click past half last character
                int32_t cw = uic_edit_char_width_px(e, p.ln, p.cl);
                if (x >= w - cw / 2) {
                    p.cl++; // column past last character
                }
//              traceln("x: %d w: %d cw: %d %d:%d", x, w, cw, p.ln, p.cl);
                break;
            }
            column += cpc;
            s += r.length[j];
            bytes -= r.length[j];
            line_y += e->ui.em.y;
        }
        if (line_y > e->ui.h) { break; }
    }
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
            char* text = e->line[ln].text;
            int32_t ofs0 = uic_edit_column_to_offset(text, (int32_t)(start & 0xFFFFFFFF));
            int32_t ofs1 = uic_edit_column_to_offset(text, (int32_t)(end   & 0xFFFFFFFF));
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

static void uic_edit_paint_line(uic_edit_t* e, int32_t ln) {
    uic_edit_line_t* line = &e->line[ln];
    uic_edit_runs_t r;
    uic_edit_runs(e, line, e->ui.w, &r);
    int32_t bytes = line->bytes;
    char* s = line->text;
    int32_t cl = 0;
    int32_t aw = 0; // accumulated width
    for (int i = 0; i < r.count; i++) {
        // code points count:
        int32_t cpc = uic_edit_utf8_length(s, r.length[i]);
        gdi.x -= aw;
        uic_edit_paint_selection(e, ln, cl, cl + cpc);
        gdi.x += aw;
        int32_t x = gdi.x;
        gdi.text("%.*s", r.length[i], s);
        int32_t w = gdi.x - x;
        aw += w;
        gdi.x -= w;
        gdi.y += e->ui.em.y;
        if (gdi.y >= e->ui.y + e->bottom) { break; }
        cl += cpc;
        s += r.length[i];
        bytes -= r.length[i];
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
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    gdi.set_font(f);
    gdi.set_text_color(ui->color);
    assert(e->scroll.ln <= e->lines);
    for (int i = e->scroll.ln; i < e->lines; i++) {
        uic_edit_paint_line(e, i);
        if (gdi.y >= ui->y + e->bottom) { break; }
    }
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
}

static void uic_edit_check_focus(uic_edit_t* e) {
    HWND wnd = (HWND)app.window;
    // focus is two stage afair: window can be focused or not
    // and single UI control inside window can have focus
    if (!app.focused) {
        if (e->focused) {
            fatal_if_false(HideCaret(wnd));
            fatal_if_false(DestroyCaret());
            e->focused = false;
        }
    } else if (app.focus == &e->ui) {
        if (!e->focused) {
            fatal_if_false(CreateCaret(wnd, null, 2, e->ui.em.y));
            fatal_if_false(SetCaretBlinkTime(400));
            fatal_if_false(ShowCaret(wnd));
            e->focused = true;
            uic_edit_set_caret(e, e->selection.end.ln, e->selection.end.cl);
        }
    } else {
        if (e->focused) {
            fatal_if_false(HideCaret(wnd));
            fatal_if_false(DestroyCaret());
            e->focused = false;
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
        uic_edit_position_t p = uic_edit_xy_to_lc(e, x, y);
        if (0 <= p.ln && 0 <= p.cl) {
            int32_t ln   = p.ln;
            int32_t cl = p.cl;
            if (ln > e->lines) { ln = max(0, e->lines); }
            int32_t chars = uic_edit_line_cpc(e, ln);
            if (cl > chars) { cl = max(0, chars); }
            traceln("%d %d [%d:%d]", x, y, ln, cl);
            uic_edit_set_caret(e, ln, cl);
            ui->invalidate(ui);
        }
    }
}

static void uic_edit_key_down(uic_t* ui, int32_t key) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        int32_t line = e->selection.end.ln;
        int32_t column = e->selection.end.cl;
        if (key == virtual_keys.down && line < e->lines) {
            ui_point_t pt = uic_edit_lc_to_xy(e, line, column);
            int32_t cw = uic_edit_char_width_px(e, line, column);
            pt.x += cw / 2;
            pt.y += ui->em.y + 1;
            uic_edit_position_t plc = uic_edit_xy_to_lc(e, pt.x, pt.y);
            if (plc.ln >= 0 && plc.cl >= 0) {
                line = plc.ln;
                column = plc.cl;
            } else if (line == e->lines - 1) {
                line = e->lines; // advance past EOF
                column = 0;
            }
        } else if (key == virtual_keys.up && e->lines > 0) {
            if (line == e->lines) {
                assert(column == 0); // positioned past EOF
                line--;
                column = uic_edit_utf8_length(e->line[line].text, e->line[line].bytes);
                ui_point_t pt = uic_edit_lc_to_xy(e, line, column);
                pt.y -= 1;
                uic_edit_position_t plc = uic_edit_xy_to_lc(e, pt.x, pt.y);
                column = plc.cl;
            } else {
                ui_point_t pt = uic_edit_lc_to_xy(e, line, column);
                int32_t cw = uic_edit_char_width_px(e, line, column);
                pt.x += cw / 2;
                pt.y -= ui->em.y / 2;
                uic_edit_position_t plc = uic_edit_xy_to_lc(e, pt.x, pt.y);
                if (plc.ln >= 0 && plc.cl >= 0) {
                    line = plc.ln;
                    column = plc.cl;
                }
            }
        } else if (key == virtual_keys.left) {
            if (column == 0) {
                if (line > 0) {
                    line--;
                    column = uic_edit_line_cpc(e, line);
                }
            } else {
                column--;
            }
        } else if (key == virtual_keys.right) {
//          traceln("column %d uic_edit_line_cpc(text %d, %d) %d", column, e->line[line].bytes, line, uic_edit_line_cpc(e, line));
            if (column >= uic_edit_line_cpc(e, line)) {
                if (line < e->lines) { line++; column = 0; }
            } else {
                column++;
            }
        }
        uic_edit_set_caret(e, line, column);
        if (key == VK_ESCAPE) { app.close(); }
        ui->invalidate(ui);
    }
}

static void uic_edit_keyboard(uic_t* unused(ui), int32_t ch) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden && !ui->disabled);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (e->focused) {
        traceln("char=%08X", ch);
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
    e->line[0].text = strdup("Hello World!");
    e->line[1].text = strdup("Good bye Universe...");
    for (int i = 0; i < 20; i += 2) {
        e->line[2 + i].text = strdup(glyph_teddy_bear "0         10        20        30        40        50        60        70        80        90");
        e->line[3 + i].text = strdup(glyph_teddy_bear "01234567890123456789012345678901234567890abcdefghi01234567890"
                                      glyph_chinese_one glyph_chinese_two                                            "3456789012345678901234567890123456789"
                                      glyph_ice_cube);
    }
    e->lines = 22;
    for (int i = 0; i < e->lines; i++) {
        e->line[i].bytes = (int32_t)strlen(e->line[i].text);
    }
    e->ui.color = rgb(168, 168, 150); // colors.text;
    e->ui.font  = &app.fonts.H1;
    e->ui.paint    = uic_edit_paint;
    e->ui.measure  = uic_edit_measure;
    e->ui.layout   = uic_edit_layout;
    e->ui.mouse    = uic_edit_mouse;
    e->ui.key_down = uic_edit_key_down;
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
