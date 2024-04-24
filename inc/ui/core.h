#pragma once
#include <stdint.h>

typedef struct ui_point_s { int32_t x, y; } ui_point_t;
typedef struct ui_rect_s { int32_t x, y, w, h; } ui_rect_t;
typedef uint64_t ui_color_t; // top 2 bits determine color format

typedef struct ui_window_s* ui_window_t;
typedef struct ui_canvas_s* ui_canvas_t;
typedef struct ui_bitmap_s* ui_bitmap_t;
typedef struct ui_font_s*   ui_font_t;
typedef struct ui_brush_s*  ui_brush_t;
typedef struct ui_pen_s*    ui_pen_t;
typedef struct ui_cursor_s* ui_cursor_t;
typedef struct ui_region_s* ui_region_t;

typedef uintptr_t ui_timer_t; // timer not the same as "id" in set_timer()!

typedef struct image_s {
    int32_t w; // width
    int32_t h; // height
    int32_t bpp;    // "components" bytes per pixel
    int32_t stride; // bytes per scanline rounded up to: (w * bpp + 3) & ~3
    ui_bitmap_t bitmap;
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
} ui_dpi_t;

typedef struct ui_fonts_s {
    // font handles re-created on scale change
    ui_font_t regular; // proportional UI font
    ui_font_t mono; // monospaced  UI font
    ui_font_t H1; // bold header font
    ui_font_t H2;
    ui_font_t H3;
} ui_fonts_t;

// TODO: move clipboard to ut.clipboard

typedef struct clipboard_s {
    errno_t (*copy_text)(const char* s); // returns error or 0
    errno_t (*copy_bitmap)(image_t* im); // returns error or 0
    int (*text)(char* text, int32_t* bytes);
} clipboard_t;

extern clipboard_t clipboard;

enum ui_view_type_t {
    ui_view_container  = 'cnt',
    ui_view_messagebox = 'mbx',
    ui_view_button     = 'btn',
    ui_view_checkbox   = 'cbx',
    ui_view_slider     = 'sld',
    ui_view_text       = 'txt',
    ui_view_edit       = 'edt'
};

typedef struct ui_s {
    struct { // window visibility
        int32_t const hide;
        int32_t const normal;   // should be use for first .show()
        int32_t const minimize; // activate and minimize
        int32_t const maximize; // activate and maximize
        int32_t const normal_na;// same as .normal but no activate
        int32_t const show;     // shows and activates in current size and position
        int32_t const min_next; // minimize and activate next window in Z order
        int32_t const min_na;   // minimize but do not activate
        int32_t const show_na;  // same as .show but no activate
        int32_t const restore;  // from min/max to normal window size/pos
        int32_t const defau1t;  // use Windows STARTUPINFO value
        int32_t const force_min;// minimize even if dispatch thread not responding
    } visibility;
    struct { // message:
        int32_t const character; // translated from key pressed/released to utf8
        int32_t const key_pressed;
        int32_t const key_released;
        int32_t const left_button_pressed;
        int32_t const left_button_released;
        int32_t const right_button_pressed;
        int32_t const right_button_released;
        int32_t const mouse_move;
        int32_t const left_double_click;
        int32_t const right_double_click;
        int32_t const animate;
        int32_t const opening;
        int32_t const closing;
        // wp: 0,1,2 (left, middle, right) button index, lp: client x,y
        int32_t const tap;
        int32_t const dtap;
        int32_t const press;
   } message;
   struct { // mouse buttons bitset mask
        struct {
            int32_t const left;
            int32_t const right;
        } button;
    } mouse;
    struct { // virtual keyboard keys
        int32_t const up;
        int32_t const down;
        int32_t const left;
        int32_t const right;
        int32_t const home;
        int32_t const end;
        int32_t const pageup;
        int32_t const pagedw;
        int32_t const insert;
        int32_t const del;
        int32_t const back;
        int32_t const escape;
        int32_t const enter;
        int32_t const plus;
        int32_t const minus;
        int32_t const f1;
        int32_t const f2;
        int32_t const f3;
        int32_t const f4;
        int32_t const f5;
        int32_t const f6;
        int32_t const f7;
        int32_t const f8;
        int32_t const f9;
        int32_t const f10;
        int32_t const f11;
        int32_t const f12;
        int32_t const f13;
        int32_t const f14;
        int32_t const f15;
        int32_t const f16;
        int32_t const f17;
        int32_t const f18;
        int32_t const f19;
        int32_t const f20;
        int32_t const f21;
        int32_t const f22;
        int32_t const f23;
        int32_t const f24;
    } key;
    struct { // known folders:
        int32_t const home     ; // c:\Users\<username>
        int32_t const desktop  ;
        int32_t const documents;
        int32_t const downloads;
        int32_t const music    ;
        int32_t const pictures ;
        int32_t const videos   ;
        int32_t const shared   ; // c:\Users\Public
        int32_t const bin      ; // c:\ProgramFiles
        int32_t const data     ; // c:\ProgramData
    } folder;
} ui_if;

extern ui_if ui;
