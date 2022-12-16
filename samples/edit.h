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
    int32_t bp;     // position in bytes  since start of the paragraph
    int32_t gp;     // position in glyphs since start of the paragraph
    int32_t bytes;  // number of bytes in this `run`
    int32_t glyphs; // number of glyphs
    int32_t pixels; // width in pixels
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

typedef struct uic_edit_s uic_edit_t;

typedef struct uic_edit_s {
    uic_t ui;
    void (*copy)(uic_edit_t* e);  // selection to clipboard
    void (*cut)(uic_edit_t* e);   // selection to clipboard
    void (*paste)(uic_edit_t* e); // replace selection with content of clipboard
    void (*erase)(uic_edit_t* e); // delete selection
    void (*fuzz)(uic_edit_t* e);  // start/stop fuzzing test
    int32_t width;   // last measure/layout width
    int32_t height;  // and height
    uic_edit_selection_t selection;
    ui_point_t caret; // (-1, -1) off
    int32_t scroll_pn; // left top corner paragraph number
    int32_t scroll_rn; // left top corner run number
    int32_t mouse;     // bit 0 and bit 1 for LEFT and RIGHT buttons down
    int32_t top;       // y coordinate of the top of view
    int32_t bottom;    // '' (ditto) of the bottom
    int32_t last_x;    // last_x for up/down caret movement
    bool focused;
    bool monospaced;
    bool multiline;
    bool wordbreak;
    // https://en.wikipedia.org/wiki/Fuzzing
    volatile thread_t fuzzer;     // fuzzer thread != null when fuzzing
    volatile int32_t  fuzz_count; // fuzzer event count
    volatile int32_t  fuzz_last;  // last processed fuzz
    volatile bool     fuzz_quit;  // last processed fuzz
    // random32 starts with 1 but client can seed it with (crt.nanoseconds() | 1)
    uint32_t fuzz_seed;    // fuzzer random32 seed (must start with odd number)
    // paragraphs memory:
    int32_t allocated;     // number of bytes allocated for `para` array below
    int32_t paragraphs;    // number of lines in the text
    uic_edit_para_t* para; // para[paragraphs]
} uic_edit_t;

void uic_edit_init(uic_edit_t* e);

end_c
