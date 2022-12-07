/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include <Windows.h>

begin_c

const char* title = "Sample5";

enum {
    uic_tag_edit       = 'edt'
};

typedef struct uic_edit_position_s {
    int32_t line;
    int32_t column;
} uic_edit_position_t;

typedef struct edit_selection_s {
    uic_edit_position_t start; // can be inverted
    uic_edit_position_t end;   // end always last selected position
} uic_edit_selection_t;

typedef struct uic_edit_s {
    uic_t ui;
    bool monospced;
    bool multiline;
    bool wordbreak;
    uic_edit_selection_t selection;
    uic_edit_position_t top; // position in the text of left top corner
    void (*copy)();  // selection to clipboard
    void (*cut)();   // selection to clipboard
    void (*paste)(); // replace selection with content of clipboard
    void (*erase)(); // delete selection
    int32_t lines; // number of lines in the text
    char* text[1024 * 128]; // 128K lines supported
} uic_edit_t;

static void layout(uic_t* ui) {
    ui->children[0]->x = 0;
    ui->children[0]->y = 0;
    ui->children[0]->w = ui->w;
    ui->children[0]->h = ui->h;
}

static void paint(uic_t* ui) {
    // all UIC are transparent and expect parent to paint background
    // UI control paint is always called with a hollow brush
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
}

static uic_edit_t edit;
static void uic_edit_paint(uic_t* ui);
static void uic_edit_measure(uic_t* ui);
static void uic_edit_mouse(uic_t* ui, int message, int flags);
static void uic_edit_key_down(uic_t* ui, int32_t key);
static void uic_edit_keyboard(uic_t* ui, int32_t character);

static void uic_edit_init(uic_edit_t* e) {
    uic_init(&e->ui);
    e->ui.tag = uic_tag_edit;
    e->text[0] = strdup("Hello World!");
    e->text[1] = strdup("Good bye Universe...");
    for (int i = 0; i < 20; i += 2) {
        e->text[2 + i] = strdup("0         10        20        30        40        50        60        70        80        90");
        e->text[3 + i] = strdup("0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
    }
    e->lines = 22;
    e->ui.color = rgb(168, 168, 150); // colors.text;
    e->ui.font = &app.fonts.mono;
    e->ui.paint = uic_edit_paint;
    e->ui.measure = uic_edit_measure;
    e->ui.mouse = uic_edit_mouse;
    e->ui.key_down = uic_edit_key_down;
    e->ui.keyboard = uic_edit_keyboard;
}

static font_t mono_H3;

static void init() {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_t* children[] = { &edit.ui, null };
    app.ui->children = children;
    uic_edit_init(&edit);
    // xxx
    edit.top.line = 0;

}

static void uic_edit_measure(uic_t* ui) {
    ui->em = gdi.get_em(*ui->font);
}

static uint64_t uic_edit_pos(int line, int column) {
    assert(line >= 0 && column >= 0);
    return ((uint64_t)line << 32) + (uint32_t)column;
}

static int32_t uic_line_chars(uic_edit_t* e, int line) {
    return e->lines == 0 || line >= e->lines ?
        0 : (int32_t)strlen(e->text[line]);
}

static void uic_edit_paint_selection(uic_edit_t* e) {
    const int32_t x = e->ui.x + e->ui.em.x / 3;
    gdi.push(x, e->ui.y + e->ui.em.y / 16);
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(rgb(48, 64, 72));
    uint64_t s0 = uic_edit_pos(e->selection.start.line,
        e->selection.start.column);
    uint64_t e0 = uic_edit_pos(e->selection.end.line,
        e->selection.end.column);
    if (e0 < s0) {
        uint64_t swap = e0;
        e0 = s0;
        s0 = swap;
    }
    assert(s0 <= e0);
    for (int i = e->top.line; i < e->lines; i++) {
        const int32_t y = e->ui.y + i * e->ui.em.y;
        uint64_t s1 = uic_edit_pos(i, 0);
        uint64_t e1 = uic_edit_pos(i, uic_line_chars(e, i));
//      gdi.fill(x, y, 10, e->ui.em.y);
//      gdi.fill(6, 43, 361, 43);
        if (s0 <= e1 && s1 <= e0) {
            uint64_t start = max(s0, s1);
            uint64_t end = min(e0, e1);
            int xs = (start & 0xFFFFFFFF) * e->ui.em.x;
            int xe = (end   & 0xFFFFFFFF) * e->ui.em.x;
            if (xe - xs > 0) {
//              traceln("line %d intersect %llX %llX %d %d", i, start, end, xs, xe);
//              traceln("gdi.fill(%d, %d, %d, %d)", x + xs, y, xe - xs, e->ui.em.y);
                gdi.fill(x + xs, y, xe - xs, e->ui.em.y);
            }
        }
        if (y > e->ui.y + e->ui.h) { break; }
    }
    gdi.pop();
}

static void uic_edit_paint(uic_t* ui) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(rgb(20, 20, 14));
    gdi.fill(0, 0, ui->w, ui->h);
    uic_edit_paint_selection(e);
#if 1
    const int32_t x = ui->x + ui->em.x / 3;
    gdi.push(x, ui->y + ui->em.x / 16);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    gdi.set_font(f);
    gdi.set_text_color(ui->color);
    assert(e->top.line <= e->lines);
    for (int i = e->top.line; i < e->lines; i++) {
        gdi.x = x;
        gdi.text("%s", e->text[i]);
        gdi.y += ui->em.y;
        if (gdi.y > ui->y + ui->h) { break; }
    }
    gdi.pop();
#endif
}

static void uic_set_position(uic_edit_t* e,
        int32_t line, int32_t column) {
    SetCaretPos(e->ui.x + column * e->ui.em.x + e->ui.em.x / 3,
                e->ui.y + line * e->ui.em.y + e->ui.em.y / 4);
    e->selection.end.line = line;
    e->selection.end.column = column;
    if (!app.shift) {
        e->selection.start = edit.selection.end;
    } else {
//      traceln("selection %d:%d %d:%d",
//          e->selection.start.line, e->selection.start.column,
//          e->selection.end.line, e->selection.end.column);
    }
}

static void uic_edit_mouse(uic_t* ui, int message, int unused(flags)) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    if (message == messages.left_button_down ||
        message == messages.right_button_down) {
        int32_t x = app.mouse.x - e->ui.x;
        int32_t y = app.mouse.y - e->ui.y;
        int32_t line   = y / ui->em.y;
        int32_t column = x / ui->em.x;
        if (line > e->lines) { line = max(0, e->lines); }
        int32_t chars = uic_line_chars(e, line);
        if (column > chars) { column = max(0, chars); }
//      traceln("%d %d [%d:%d]", x, y, line, column);
        uic_set_position(e, line, column);
    } else {
        bool inside =
            ui->x <= app.mouse.x && app.mouse.x < ui->x + ui->w &&
            ui->y <= app.mouse.y && app.mouse.y < ui->y + ui->h;
        if (inside) {
            ShowCaret((HWND)app.window);
        } else {
            HideCaret((HWND)app.window);
        }
    }
    ui->invalidate(ui);
}

static void uic_edit_key_down(uic_t* ui, int32_t key) {
    assert(ui->tag == uic_tag_edit);
    assert(!ui->hidden);
    uic_edit_t* e = (uic_edit_t*)ui;
    int32_t line = e->selection.end.line;
    int32_t column = e->selection.end.column;
    if (key == virtual_keys.down) {
        if (line < e->lines) { line++; }
        if (line == e->lines) { column = 0; }
    } else if (key == virtual_keys.up) {
        if (line > 0) {
            line--;
            column = min(column, uic_line_chars(e, line));
        }
    } else if (key == virtual_keys.left) {
        if (column == 0) {
            if (line > 0) {
                line--;
                column = uic_line_chars(e, line);
            }
        } else {
            column--;
        }
    } else if (key == virtual_keys.right) {
        if (column >= uic_line_chars(e, line)) {
            if (line < e->lines) { line++; column = 0; }
        } else {
            column++;
        }
    }
    uic_set_position(e, line, column);
    if (key == VK_ESCAPE) { app.close(); }
    ui->invalidate(ui);
}

static void uic_edit_keyboard(uic_t* unused(ui), int32_t ch) {
    ui->invalidate(ui);
}

static void openned() {
    mono_H3 = gdi.font(app.fonts.mono, gdi.get_em(app.fonts.H3).y);
    edit.ui.font = &mono_H3;
    fatal_if_false(CreateCaret((HWND)app.window, null, 2, gdi.get_em(app.fonts.H3).y));
    SetCaretBlinkTime(400);
    // Expected manifest.xml containing UTF-8 code page
    // for Translate message and WM_CHAR to deliver UTF-8 characters
    // see: https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
    assert(GetACP() == 65001, "GetACP()=%d", GetACP());
    // at the moment of writing there is no API call to inform Windows about process
    // prefered codepage except manifest.xml file in resource #1.
    // Absence of manifest.xml will result to ancient and useless ANSI 1252 codepage
}

app_t app = {
    .class_name = "sample1",
    .init = init,
    .openned = openned,
    .min_width = 400,
    .min_height = 200
};

end_c
