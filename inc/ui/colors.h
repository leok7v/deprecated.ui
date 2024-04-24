#pragma once
#include "ui/ui.h"

begin_c

/* TODO: make ui_color_t uint64_t RGBA remove pens and brushes
         support upto 16-16-16-15(A)bit per pixel color
         components with 'transparent/hollow' bit
*/

#define color_mask        ((ui_color_t)0xC000000000000000ULL)

#define color_mask        ((ui_color_t)0xC000000000000000ULL)
#define color_undefined   ((ui_color_t)0x8000000000000000ULL)
#define color_transparent ((ui_color_t)0x4000000000000000ULL)
#define color_hdr         ((ui_color_t)0xC000000000000000ULL)

#define color_is_8bit(c)         (((c) & color_mask) == 0)
#define color_is_hdr(c)          (((c) & color_mask) == color_hdr)
#define color_is_undefined(c)    (((c) & color_mask) == color_undefined)
#define color_is_transparent(c) ((((c) & color_mask) == color_transparent) && \
                                 (((c) & ~color_mask) == 0))
// if any other special colors or formats need to be introduced
// (c) & ~color_mask) has 2^62 possible extensions bits

// color_hdr A - 14 bit, R,G,B - 16 bit, all in range [0..0xFFFF]
#define color_hdr_a(c)    ((((c) >> 48) & 0x3FFF) << 2)
#define color_hdr_r(c)    (((c) >>  0) & 0xFFFF)
#define color_hdr_g(c)    (((c) >> 16) & 0xFFFF)
#define color_hdr_b(c)    (((c) >> 32) & 0xFFFF)

#define rgb(r,g,b) ((ui_color_t)(((uint8_t)(r) | ((uint16_t)((uint8_t)(g))<<8)) | \
    (((uint32_t)(uint8_t)(b))<<16)))
#define rgba(r, g, b, a) (ui_color_t)((rgb(r, g, b)) | (((uint8_t)a) << 24))

typedef struct colors_s {
    const int32_t none; // aka CLR_INVALID in wingdi
    const int32_t text;
    const int32_t white;
    const int32_t black;
    const int32_t red;
    const int32_t green;
    const int32_t blue;
    const int32_t yellow;
    const int32_t cyan;
    const int32_t magenta;
    const int32_t gray;
    // darker shades of grey:
    const int32_t dkgray1; // 30 / 255 = 11.7%
    const int32_t dkgray2; // 38 / 255 = 15%
    const int32_t dkgray3; // 45 / 255 = 17.6%
    const int32_t dkgray4; // 63 / 255 = 24.0%
    // tone down RGB colors:
    const int32_t tone_white;
    const int32_t tone_red;
    const int32_t tone_green;
    const int32_t tone_blue;
    const int32_t tone_yellow;
    const int32_t tone_cyan;
    const int32_t tone_magenta;
    // misc:
    const int32_t orange;
    const int32_t dkgreen;
    // highlights:
    const int32_t text_highlight; // bluish off-white
    const int32_t blue_highlight;
    const int32_t off_white;

    const int32_t btn_gradient_darker;
    const int32_t btn_gradient_dark;
    const int32_t btn_hover_highlight;
    const int32_t btn_disabled;
    const int32_t btn_armed;
    const int32_t btn_text;
    const int32_t toast; // toast background
} colors_t;

extern colors_t colors;

end_c
