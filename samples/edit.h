#pragma once
/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

begin_c

enum {
    uic_tag_edit       = 'edt'
};

// uic_edit_para_t.text can point to readonly initial
// memory area when .allocateed is 0.

typedef struct uic_edit_para_s { // "paragraph"
    char* text;    // utf-8
    int32_t bytes; // number of bytes in utf-8
    int32_t allocated; // heap allocated bytes
} uic_edit_para_t;

typedef struct uic_edit_lc_s {
    // humans used to line:column coordinates in text
    int32_t ln; // line (human notion of paragraph number)
    int32_t cl; // column (position counted in glyphs not bytes)
} uic_edit_lc_t;

typedef struct uic_edit_selection_s {
    uic_edit_lc_t fro; // from start to
    uic_edit_lc_t end; // end always last selected position
} uic_edit_selection_t;

typedef struct uic_edit_s {
    uic_t ui;
    uic_edit_selection_t selection;
    int32_t scroll_pn; // left top corner paragraph number
    int32_t scroll_rn; // left top corner run number
    int32_t mouse;   // bit 0 and bit 1 for LEFT and RIGHT buttons down
    int32_t top;     // y coordinate of the top of view
    int32_t bottom;  // '' (ditto) of the bottom
    void (*copy)();  // selection to clipboard
    void (*cut)();   // selection to clipboard
    void (*paste)(); // replace selection with content of clipboard
    void (*erase)(); // delete selection
    bool focused;
    bool monospaced;
    bool multiline;
    bool wordbreak;
    int32_t paragraphs; // number of lines in the text
    uic_edit_para_t para[1024 * 64]; // 64K lines (can be extended)
} uic_edit_t;


void uic_edit_init(uic_edit_t* e);

end_c
