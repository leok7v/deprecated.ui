#pragma once
#include "ui/ui.h"

begin_c

typedef struct  uic_checkbox_s  uic_checkbox_t; // checkbox

typedef struct  uic_checkbox_s {
    view_t ui;
    void (*cb)( uic_checkbox_t* b); // callback
}  uic_checkbox_t;

// label may contain "___" which will be replaced with "On" / "Off"
void  uic_checkbox_init( uic_checkbox_t* b, const char* label, double ems,
    void (*cb)( uic_checkbox_t* b));

void _uic_checkbox_init_(view_t* ui); // do not call use uic_checkbox() macro

#define uic_checkbox(name, s, w, code)                           \
    static void name ## _callback(uic_checkbox_t* name) {        \
        (void)name; /* no warning if unused */                   \
        code                                                     \
    }                                                            \
    static                                                       \
    uic_checkbox_t name = {                                      \
    .ui = {.tag = uic_tag_checkbox, .init = _uic_checkbox_init_, \
    .children = null, .width = w, .text = s}, .cb = name ## _callback }

end_c
