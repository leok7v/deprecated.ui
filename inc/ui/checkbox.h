#pragma once
#include "ui/ui.h"

begin_c

typedef struct  checkbox_s  checkbox_t; // checkbox

typedef struct  checkbox_s {
    view_t ui;
    void (*cb)( checkbox_t* b); // callback
}  checkbox_t;

// label may contain "___" which will be replaced with "On" / "Off"
void  checkbox_init( checkbox_t* b, const char* label, double ems,
    void (*cb)( checkbox_t* b));

void _checkbox_init_(view_t* ui); // do not call use uic_checkbox() macro

#define uic_checkbox(name, s, w, code)                           \
    static void name ## _callback(checkbox_t* name) {        \
        (void)name; /* no warning if unused */                   \
        code                                                     \
    }                                                            \
    static                                                       \
    checkbox_t name = {                                      \
    .ui = {.tag = uic_tag_checkbox, .init = _checkbox_init_, \
    .children = null, .width = w, .text = s}, .cb = name ## _callback }

end_c
