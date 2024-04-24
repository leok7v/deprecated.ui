#pragma once
#include "ui/ui.h"

begin_c

typedef struct ui_label_s {
    ui_view_t view;
    bool multiline;
    bool editable;  // can be edited
    bool highlight; // paint with highlight color
    bool hovered;   // paint highlight rectangle when hover over
    bool label;     // do not copy text to clipboard, do not highlight
    int32_t dy; // vertical shift down (to line up baselines of diff fonts)
} ui_label_t;

void ui_label_init_(ui_view_t* view); // do not call use ui_text() and ui_multiline()

#define ui_text(t, s)                                                        \
    ui_label_t t = { .view = { .type = ui_view_text, .init = ui_label_init_, \
    .children = null, .width = 0.0, .text = s}, .multiline = false}

#define ui_multiline(t, w, s)                                                \
    ui_label_t t = { .view = { .type = ui_view_text, .init = ui_label_init_, \
    .children = null, .width = w, .text = s}, .multiline = true}

// single line of text with "&" keyboard shortcuts:
void ui_label_vinit(ui_label_t* t, const char* format, va_list vl);
void ui_label_init(ui_label_t* t, const char* format, ...);
// multiline
void ui_label_init_ml(ui_label_t* t, double width, const char* format, ...);

end_c
