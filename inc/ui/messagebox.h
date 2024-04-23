#pragma once
#include "ui/ui.h"

begin_c

typedef struct messagebox_s messagebox_t;

typedef struct messagebox_s {
    view_t  ui;
    void (*cb)(messagebox_t* m, int32_t option); // callback -1 on cancel
    label_t text;
    button_t button[16];
    view_t* children[17];
    int32_t option; // -1 or option chosen by user
    const char** opts;
} messagebox_t;

void messagebox_init_(view_t* ui);

void messagebox_init(messagebox_t* mx, const char* option[],
    void (*cb)(messagebox_t* m, int32_t option), const char* format, ...);

#define uic_messagebox(name, s, code, ...)                               \
                                                                         \
    static char* name ## _options[] = { __VA_ARGS__, null };             \
                                                                         \
    static void name ## _callback(messagebox_t* m, int32_t option) { \
        (void)m; (void)option; /* no warnings if unused */               \
        code                                                             \
    }                                                                    \
    static                                                               \
    messagebox_t name = {                                            \
    .ui = {.tag = uic_tag_messagebox, .init = messagebox_init_,      \
    .children = null, .text = s}, .opts = name ## _options,              \
    .cb = name ## _callback }

end_c
