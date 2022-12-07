#pragma once
/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

begin_c

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


void uic_edit_init(uic_edit_t* e);

end_c
