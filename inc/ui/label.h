#pragma once
#include "ui/ui.h"

begin_c

typedef struct uic_text_s {
    view_t ui;
    bool multiline;
    bool editable;  // can be edited
    bool highlight; // paint with highlight color
    bool hovered;   // paint highlight rectangle when hover over
    bool label;     // do not copy text to clipboard, do not highlight
    int32_t dy; // vertical shift down (to line up baselines of diff fonts)
} uic_text_t;

void _uic_text_init_(view_t* ui); // do not call use ui_text() and ui_multiline()

#define uic_text(t, s) \
    uic_text_t t = { .ui = {.tag = uic_tag_text, .init = _uic_text_init_, \
    .children = null, .width = 0.0, .text = s}, .multiline = false}
#define uic_multiline(t, w, s) \
    uic_text_t t = {.ui = {.tag = uic_tag_text, .init = _uic_text_init_, \
    .children = null, .width = w, .text = s}, .multiline = true}

// single line of text with "&" keyboard shortcuts:
void uic_text_vinit(uic_text_t* t, const char* format, va_list vl);
void uic_text_init(uic_text_t* t, const char* format, ...);
// multiline
void uic_text_init_ml(uic_text_t* t, double width, const char* format, ...);

end_c
