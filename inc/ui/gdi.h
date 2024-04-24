#pragma once
#include "ui/ui.h"

begin_c

// Graphic Device Interface (selected parts of Windows GDI)

enum {
    gdi_font_quality_default = 0,
    gdi_font_quality_draft = 1,
    gdi_font_quality_proof = 2, // anti-aliased w/o ClearType rainbows
    gdi_font_quality_nonantialiased = 3,
    gdi_font_quality_antialiased = 4,
    gdi_font_quality_cleartype = 5,
    gdi_font_quality_cleartype_natural = 6
};

typedef struct gdi_s {
    ui_brush_t  brush_color;
    ui_brush_t  brush_hollow;
    ui_pen_t pen_hollow;
    ui_region_t clip;
    // bpp bytes (not bits!) per pixel. bpp = -3 or -4 does not swap RGB to BRG:
    void (*image_init)(image_t* image, int32_t w, int32_t h, int32_t bpp,
        const uint8_t* pixels);
    void (*image_init_rgbx)(image_t* image, int32_t w, int32_t h,
        int32_t bpp, const uint8_t* pixels); // sets all alphas to 0xFF
    void (*image_dispose)(image_t* image);
    ui_color_t (*set_text_color)(ui_color_t c);
    ui_brush_t (*create_brush)(ui_color_t c);
    void    (*delete_brush)(ui_brush_t b);
    ui_color_t (*set_brush_color)(ui_color_t c);
    ui_brush_t (*set_brush)(ui_brush_t b); // color or hollow
    ui_pen_t (*set_colored_pen)(ui_color_t c); // always 1px wide
    ui_pen_t (*create_pen)(ui_color_t c, int32_t pixels); // pixels wide pen
    ui_pen_t (*set_pen)(ui_pen_t p);
    void  (*delete_pen)(ui_pen_t p);
    void (*set_clip)(int32_t x, int32_t y, int32_t w, int32_t h);
    // use set_clip(0, 0, 0, 0) to clear clip region
    void (*push)(int32_t x, int32_t y); // also calls SaveDC(app.canvas)
    void (*pop)(void); // also calls RestoreDC(-1, app.canvas)
    void (*pixel)(int32_t x, int32_t y, ui_color_t c);
    ui_point_t (*move_to)(int32_t x, int32_t y); // returns previous (x, y)
    void (*line)(int32_t x, int32_t y); // line to x, y with gdi.pen moves x, y
    void (*frame)(int32_t x, int32_t y, int32_t w, int32_t h); // gdi.pen only
    void (*rect)(int32_t x, int32_t y, int32_t w, int32_t h);  // gdi.pen & brush
    void (*fill)(int32_t x, int32_t y, int32_t w, int32_t h);  // gdi.brush only
    void (*frame_with)(int32_t x, int32_t y, int32_t w, int32_t h, ui_color_t c);
    void (*rect_with)(int32_t x, int32_t y, int32_t w, int32_t h,
                      ui_color_t border, ui_color_t fill);
    void (*fill_with)(int32_t x, int32_t y, int32_t w, int32_t h, ui_color_t c);
    void (*poly)(ui_point_t* points, int32_t count);
    void (*rounded)(int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t rx, int32_t ry); // see RoundRect, pen, brush
    void (*gradient)(int32_t x, int32_t y, int32_t w, int32_t h,
        ui_color_t rgba_from, ui_color_t rgba_to, bool vertical);
    // draw images: (x,y remains untouched after drawing)
    // draw_greyscale() sx, sy, sw, sh screen rectangle
    // x, y, w, h rectangle inside pixels[ih][iw] uint8_t array
    void (*draw_greyscale)(int32_t sx, int32_t sy, int32_t sw, int32_t sh,
        int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t iw, int32_t ih, int32_t stride, const uint8_t* pixels);
    void (*draw_bgr)(int32_t sx, int32_t sy, int32_t sw, int32_t sh,
        int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t iw, int32_t ih, int32_t stride, const uint8_t* pixels);
    void (*draw_bgrx)(int32_t sx, int32_t sy, int32_t sw, int32_t sh,
        int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t iw, int32_t ih, int32_t stride, const uint8_t* pixels);
    void (*alpha_blend)(int32_t x, int32_t y, int32_t w, int32_t h,
        image_t* image, double alpha);
    void (*draw_image)(int32_t x, int32_t y, int32_t w, int32_t h,
        image_t* image);
    // text:
    void (*cleartype)(bool on);
    void (*font_smoothing_contrast)(int32_t c); // [1000..2202] or -1 for 1400 default
    ui_font_t (*font)(ui_font_t f, int32_t height, int32_t quality); // custom font, quality: -1 as is
    void   (*delete_font)(ui_font_t f);
    ui_font_t (*set_font)(ui_font_t f);
    // IMPORTANT: relationship between font_height(), baseline(), descent()
    // E.g. for monospaced font on dpi=96 (monitor_raw=101) and font_height=15
    // the get_em() is: 9 20 font_height(): 15 baseline: 16 descent: 4
    // Monospaced fonts are not `em` "M" square!
    int32_t (*font_height)(ui_font_t f); // font height in pixels
    int32_t (*descent)(ui_font_t f);     // font descent (glyphs below baseline)
    int32_t (*baseline)(ui_font_t f);    // height - baseline (aka ascent) = descent
    bool    (*is_mono)(ui_font_t f);     // is font monospaced?
    // https://en.wikipedia.org/wiki/Em_(typography)
    ui_point_t (*get_em)(ui_font_t f);  // pixel size of glyph "M"
    ui_point_t (*measure_text)(ui_font_t f, const char* format, ...);
    // width can be -1 which measures text with "\n" or
    // positive number of pixels
    ui_point_t (*measure_multiline)(ui_font_t f, int32_t w, const char* format, ...);
    double height_multiplier; // see line_spacing()
    double (*line_spacing)(double height_multiplier); // default 1.0
    int32_t x; // incremented by text, print
    int32_t y; // incremented by textln, println
    // proportional:
    void (*vtext)(const char* format, va_list vl); // x += width
    void (*text)(const char* format, ...);         // x += width
    // gdi.y += height * line_spacing
    void (*vtextln)(const char* format, va_list vl);
    void (*textln)(const char* format, ...);
    // mono:
    void (*vprint)(const char* format,va_list vl); // x += width
    void (*print)(const char* format, ...);        // x += width
    // gdi.y += height * line_spacing
    void (*vprintln)(const char* format, va_list vl);
    void (*println)(const char* format, ...);
    // multiline(null, format, ...) only increments gdi.y
    ui_point_t (*multiline)(int32_t width, const char* format, ...);
} gdi_t;

extern gdi_t gdi;

end_c
