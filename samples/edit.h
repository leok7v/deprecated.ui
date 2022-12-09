#pragma once
/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

begin_c

enum {
    uic_tag_edit       = 'edt'
};

// uic_edit_line_t.text can point to readonly initial
// memory area when .allocateed is 0.

typedef struct uic_edit_line_s {
    char* text;    // utf-8
    int32_t bytes; // number of bytes in utf-8
    int32_t allocated; // heap allocated bytes
} uic_edit_line_t;

typedef struct uic_edit_position_s {
    int32_t line;
    int32_t column;
} uic_edit_position_t;

typedef struct uic_edit_selection_s {
    uic_edit_position_t start; // can be inverted
    uic_edit_position_t end;   // end always last selected position
} uic_edit_selection_t;

typedef struct uic_edit_s {
    uic_t ui;
    uic_edit_selection_t selection;
    uic_edit_position_t top; // position in the text of left top corner
    void (*copy)();  // selection to clipboard
    void (*cut)();   // selection to clipboard
    void (*paste)(); // replace selection with content of clipboard
    void (*erase)(); // delete selection
    bool focused;
    bool monospced;
    bool multiline;
    bool wordbreak;
    int32_t lines; // number of lines in the text
    uic_edit_line_t line[1024 * 64]; // 64K lines (can be extended)
} uic_edit_t;


void uic_edit_init(uic_edit_t* e);

end_c
