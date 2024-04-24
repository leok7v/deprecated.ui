#pragma once
#include "ui/ui.h"

begin_c

typedef struct ui_slider_s ui_slider_t;

typedef struct ui_slider_s {
    ui_view_t view;
    void (*cb)(ui_slider_t* b); // callback
    int32_t step;
    double time;   // time last button was pressed
    ui_point_t tm; // text measurement (special case for %0*d)
    ui_button_t inc;
    ui_button_t dec;
    ui_view_t* buttons[3]; // = { dec, inc, null }
    int32_t value;  // for ui_slider_t range slider control
    int32_t vmin;
    int32_t vmax;
} ui_slider_t;

void _slider_init_(ui_view_t* view);

void ui_slider_init(ui_slider_t* r, const char* label, double ems,
    int32_t vmin, int32_t vmax, void (*cb)(ui_slider_t* r));

#define ui_slider(name, s, ems, vmn, vmx, code)             \
    static void name ## _callback(ui_slider_t* name) {      \
        (void)name; /* no warning if unused */              \
        code                                                \
    }                                                       \
    static                                                  \
    ui_slider_t name = {                                    \
        .view = { .type = ui_view_slider, .children = null, \
        .width = ems, .text = s, .init = _slider_init_,     \
    }, .vmin = vmn, .vmax = vmx, .value = vmn,              \
    .cb = name ## _callback }

end_c
