#pragma once
#include "ui/ui.h"

begin_c

typedef struct ui_checkbox_s  checkbox_t;

typedef struct ui_checkbox_s {
    ui_view_t view;
    void (*cb)( checkbox_t* b); // callback
}  checkbox_t;

// label may contain "___" which will be replaced with "On" / "Off"
void ui_checkbox_init( checkbox_t* b, const char* label, double ems,
    void (*cb)( checkbox_t* b));

void ui_checkbox_init_(ui_view_t* view); // do not call use ui_checkbox() macro

#define ui_checkbox(name, s, w, code)                                 \
    static void name ## _callback(checkbox_t* name) {                 \
        (void)name; /* no warning if unused */                        \
        code                                                          \
    }                                                                 \
    static                                                            \
    checkbox_t name = {                                               \
    .view = { .type = ui_view_checkbox, .init = ui_checkbox_init_,    \
    .children = null, .width = w, .text = s}, .cb = name ## _callback }

end_c
