#pragma once
#include "ui/ui.h"

begin_c

typedef struct uic_button_s uic_button_t;

typedef struct uic_button_s {
    view_t ui;
    void (*cb)(uic_button_t* b); // callback
    double armed_until;   // seconds - when to release
} uic_button_t;

void uic_button_init(uic_button_t* b, const char* label, double ems,
    void (*cb)(uic_button_t* b));

void _uic_button_init_(view_t* ui); // do not call use uic_button() macro

#define uic_button(name, s, w, code)                         \
    static void name ## _callback(uic_button_t* name) {      \
        (void)name; /* no warning if unused */               \
        code                                                 \
    }                                                        \
    static                                                   \
    uic_button_t name = {                                    \
    .ui = {.tag = uic_tag_button, .init = _uic_button_init_, \
    .children = null, .width = w, .text = s}, .cb = name ## _callback }

// usage:
// uic_button(button, 7.0, "&Button", { b->ui.pressed = !b->ui.pressed; })

end_c
