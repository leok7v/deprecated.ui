#pragma once
#include <stdint.h>

typedef struct ui_point_s { int32_t x, y; } ui_point_t;
typedef struct ui_rect_s { int32_t x, y, w, h; } ui_rect_t;
typedef uint64_t color_t; // top 2 bits determine color format

typedef struct window_s__* window_t;
typedef struct canvas_s__* canvas_t;
typedef struct bitmap_s__* bitmap_t;
typedef struct font_s__*   font_t;
typedef struct brush_s__*  brush_t;
typedef struct pen_s__*    pen_t;
typedef struct cursor_s__* cursor_t;
typedef struct region_s__* region_t;

typedef uintptr_t tm_t; // timer not the same as "id" in set_timer()!

typedef struct image_s {
    int32_t w; // width
    int32_t h; // height
    int32_t bpp;    // "components" bytes per pixel
    int32_t stride; // bytes per scanline rounded up to: (w * bpp + 3) & ~3
    bitmap_t bitmap;
    void* pixels;
} image_t;

typedef struct dpi_s { // max(dpi_x, dpi_y)
    int32_t system;  // system dpi
    int32_t process; // process dpi
    // 15" diagonal monitor 3840x2160 175% scaled
    // monitor dpi effective 168, angular 248 raw 284
    int32_t monitor_effective; // effective with regard of user scaling
    int32_t monitor_raw;       // with regard of physical screen size
    int32_t monitor_angular;   // diagonal raw
    int32_t window;            // main window dpi
} dpi_t;
