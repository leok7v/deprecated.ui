#pragma once
#include "ui/ui.h"

typedef struct {
    void (*center)(view_t* ui); // exactly one child
    void (*horizontal)(view_t* ui, int32_t gap);
    void (*vertical)(view_t* ui, int32_t gap);
    void (*grid)(view_t* ui, int32_t gap_h, int32_t gap_v);
} measurements_if;

extern measurements_if measurements;

typedef struct {
    void (*center)(view_t* ui); // exactly one child
    void (*horizontal)(view_t* ui, int32_t x, int32_t y, int32_t gap);
    void (*vertical)(view_t* ui, int32_t x, int32_t y, int32_t gap);
    void (*grid)(view_t* ui, int32_t gap_h, int32_t gap_v);
} layouts_if;

extern layouts_if layouts;

