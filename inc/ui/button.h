#pragma once
#include "ui/ui.h"

begin_c

typedef struct button_s button_t;

typedef struct button_s {
    view_t ui;
    void (*cb)(button_t* b); // callback
    double armed_until;   // seconds - when to release
} button_t;

void button_init(button_t* b, const char* label, double ems,
    void (*cb)(button_t* b));

void _button_init_(view_t* ui); // do not call use uic_button() macro

#define uic_button(name, s, w, code)                         \
    static void name ## _callback(button_t* name) {      \
        (void)name; /* no warning if unused */               \
        code                                                 \
    }                                                        \
    static                                                   \
    button_t name = {                                    \
    .ui = {.tag = uic_tag_button, .init = _button_init_, \
    .children = null, .width = w, .text = s}, .cb = name ## _callback }

// usage:
// uic_button(button, 7.0, "&Button", { b->ui.pressed = !b->ui.pressed; })

end_c
