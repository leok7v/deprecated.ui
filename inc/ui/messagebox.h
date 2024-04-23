#pragma once
#include "ui/ui.h"

begin_c

typedef struct uic_messagebox_s uic_messagebox_t;

typedef struct uic_messagebox_s {
    view_t  ui;
    void (*cb)(uic_messagebox_t* m, int32_t option); // callback -1 on cancel
    uic_text_t text;
    uic_button_t button[16];
    view_t* children[17];
    int32_t option; // -1 or option chosen by user
    const char** opts;
} uic_messagebox_t;

void uic_messagebox_init_(view_t* ui);

void uic_messagebox_init(uic_messagebox_t* mx, const char* option[],
    void (*cb)(uic_messagebox_t* m, int32_t option), const char* format, ...);

#define uic_messagebox(name, s, code, ...)                               \
                                                                         \
    static char* name ## _options[] = { __VA_ARGS__, null };             \
                                                                         \
    static void name ## _callback(uic_messagebox_t* m, int32_t option) { \
        (void)m; (void)option; /* no warnings if unused */               \
        code                                                             \
    }                                                                    \
    static                                                               \
    uic_messagebox_t name = {                                            \
    .ui = {.tag = uic_tag_messagebox, .init = uic_messagebox_init_,      \
    .children = null, .text = s}, .opts = name ## _options,              \
    .cb = name ## _callback }

end_c
