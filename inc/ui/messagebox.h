#pragma once
#include "ui/ui.h"

begin_c

typedef struct ui_messagebox_s ui_messagebox_t;

typedef struct ui_messagebox_s {
    ui_view_t view;
    void (*cb)(ui_messagebox_t* m, int32_t option); // callback -1 on cancel
    ui_label_t text;
    ui_button_t button[16];
    ui_view_t* children[17];
    int32_t option; // -1 or option chosen by user
    const char** opts;
} ui_messagebox_t;

void ui_messagebox_init_(ui_view_t* view);

void ui_messagebox_init(ui_messagebox_t* mx, const char* option[],
    void (*cb)(ui_messagebox_t* m, int32_t option), const char* format, ...);

#define ui_messagebox(name, s, code, ...)                                \
                                                                         \
    static char* name ## _options[] = { __VA_ARGS__, null };             \
                                                                         \
    static void name ## _callback(ui_messagebox_t* m, int32_t option) {  \
        (void)m; (void)option; /* no warnings if unused */               \
        code                                                             \
    }                                                                    \
    static                                                               \
    ui_messagebox_t name = {                                             \
    .view = { .type = ui_view_messagebox, .init = ui_messagebox_init_,   \
    .children = null, .text = s}, .opts = name ## _options,              \
    .cb = name ## _callback }

end_c
