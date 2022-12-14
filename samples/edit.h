#pragma once
/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

begin_c

// important uic_edit_t will refuse to layout into a box smaller than
// width 3 x em.x height 1 x em.y

enum {
    uic_tag_edit       = 'edt'
};

typedef struct uic_edit_run_s {
    int32_t bytes;
    int32_t glyphs;
    int32_t pixels;
    int32_t gp; // glyph position (number of glyph from the start of paragraph)
} uic_edit_run_t;

// uic_edit_para_t.initially text will point to readonly memory
// with .allocateed == 0; as text is modified it is copied to
// heap and reallocated there.

typedef struct uic_edit_para_s { // "paragraph"
    char* text;          // text[bytes] utf-8
    int32_t bytes;       // number of bytes in utf-8 text
    int32_t glyphs;      // number of glyphs in text <= bytes
    int32_t allocated;   // if != 0 text copied to heap allocated bytes
    int32_t runs;        // number of runs in the paragraph
    uic_edit_run_t* run; // [runs] array of pointers
    // both ix2gp[] and gp2ix[] arrays [bytes] heap allocated
    int32_t* ix2gp;      // text[ix] -> glyph position
    int32_t* gp2ix;      // glyph position text[ix]
} uic_edit_para_t;

typedef struct uic_edit_lc_s {
    // humans used to line:column coordinates in text
    int32_t pn; // paragpraph number ("line number")
    int32_t gp; // glyph position ("column")
} uic_edit_pg_t;

typedef struct uic_edit_selection_s {
    uic_edit_pg_t fro; // from start to
    uic_edit_pg_t end; // end always last selected position
} uic_edit_selection_t;

typedef struct uic_edit_s {
    uic_t ui;
    void (*copy)();  // selection to clipboard
    void (*cut)();   // selection to clipboard
    void (*paste)(); // replace selection with content of clipboard
    void (*erase)(); // delete selection
    int32_t width;   // last measure/layout width
    int32_t height;  // and height
    uic_edit_selection_t selection;
    ui_point_t caret; // (-1, -1) off
    int32_t scroll_pn; // left top corner paragraph number
    int32_t scroll_rn; // left top corner run number
    int32_t mouse;     // bit 0 and bit 1 for LEFT and RIGHT buttons down
    int32_t top;       // y coordinate of the top of view
    int32_t bottom;    // '' (ditto) of the bottom
    bool focused;
    bool monospaced;
    bool multiline;
    bool wordbreak;
    int32_t allocated;  // number of bytes allocated for `para` array below
    int32_t paragraphs; // number of lines in the text
    uic_edit_para_t* para; // para[paragraphs]
} uic_edit_t;

void uic_edit_init(uic_edit_t* e);

end_c
