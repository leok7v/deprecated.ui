#pragma once
#include "ui/ui.h"

begin_c

typedef struct slider_s slider_t;

typedef struct slider_s {
    view_t ui;
    void (*cb)(slider_t* b); // callback
    int32_t step;
    double time;   // time last button was pressed
    ui_point_t tm; // text measurement (special case for %0*d)
    button_t inc;
    button_t dec;
    view_t* buttons[3]; // = { dec, inc, null }
    int32_t value;  // for slider_t range slider control
    int32_t vmin;
    int32_t vmax;
} slider_t;

void _slider_init_(view_t* ui);

void slider_init(slider_t* r, const char* label, double ems,
    int32_t vmin, int32_t vmax, void (*cb)(slider_t* r));

#define uic_slider(name, s, ems, vmn, vmx, code)        \
    static void name ## _callback(slider_t* name) { \
        (void)name; /* no warning if unused */          \
        code                                            \
    }                                                   \
    static                                              \
    slider_t name = {                               \
    .ui = {.tag = uic_tag_slider, .children = null,     \
    .width = ems, .text = s, .init = _slider_init_, \
    }, .vmin = vmn, .vmax = vmx, .value = vmn,          \
    .cb = name ## _callback }

end_c
