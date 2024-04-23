#pragma once
#include "ui/ui.h"

begin_c

typedef struct uic_slider_s uic_slider_t;

typedef struct uic_slider_s {
    view_t ui;
    void (*cb)(uic_slider_t* b); // callback
    int32_t step;
    double time;   // time last button was pressed
    ui_point_t tm; // text measurement (special case for %0*d)
    uic_button_t inc;
    uic_button_t dec;
    view_t* buttons[3]; // = { dec, inc, null }
    int32_t value;  // for uic_slider_t range slider control
    int32_t vmin;
    int32_t vmax;
} uic_slider_t;

void _uic_slider_init_(view_t* ui);

void uic_slider_init(uic_slider_t* r, const char* label, double ems,
    int32_t vmin, int32_t vmax, void (*cb)(uic_slider_t* r));

#define uic_slider(name, s, ems, vmn, vmx, code)        \
    static void name ## _callback(uic_slider_t* name) { \
        (void)name; /* no warning if unused */          \
        code                                            \
    }                                                   \
    static                                              \
    uic_slider_t name = {                               \
    .ui = {.tag = uic_tag_slider, .children = null,     \
    .width = ems, .text = s, .init = _uic_slider_init_, \
    }, .vmin = vmn, .vmax = vmx, .value = vmn,          \
    .cb = name ## _callback }

end_c
