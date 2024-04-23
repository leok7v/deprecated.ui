/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE at the end of file */
#ifndef qucik_defintion
#define qucik_defintion
#include "ut/ut.h"
#include "ui/ui.h"

#endif qucik_defintion

#if defined(quick_implementation) || defined(quick_implementation_console)
#undef quick_implementation

#include "ui/win32.h"

begin_c

#define window() ((HWND)app.window)
#define canvas() ((HDC)app.canvas)

typedef struct gdi_xyc_s {
    int32_t x;
    int32_t y;
    color_t c;
} gdi_xyc_t;

static int32_t gdi_top;
static gdi_xyc_t gdi_stack[256];

static void __gdi_init__(void) {
    gdi.brush_hollow = (brush_t)GetStockBrush(HOLLOW_BRUSH);
    gdi.brush_color  = (brush_t)GetStockBrush(DC_BRUSH);
    gdi.pen_hollow = (pen_t)GetStockPen(NULL_PEN);
}

static inline COLORREF gdi_color_ref(color_t c) {
    assert(color_is_8bit(c));
    return (COLORREF)(c & 0xFFFFFFFF);
}

static color_t gdi_set_text_color(color_t c) {
    return SetTextColor(canvas(), gdi_color_ref(c));
}

static pen_t gdi_set_pen(pen_t p) {
    not_null(p);
    return (pen_t)SelectPen(canvas(), (HPEN)p);
}

static pen_t gdi_set_colored_pen(color_t c) {
    pen_t p = (pen_t)SelectPen(canvas(), GetStockPen(DC_PEN));
    SetDCPenColor(canvas(), gdi_color_ref(c));
    return p;
}

static pen_t gdi_create_pen(color_t c, int32_t width) {
    assert(width >= 1);
    pen_t pen = (pen_t)CreatePen(PS_SOLID, width, gdi_color_ref(c));
    not_null(pen);
    return pen;
}

static void gdi_delete_pen(pen_t p) {
    fatal_if_false(DeletePen(p));
}

static brush_t gdi_create_brush(color_t c) {
    return (brush_t)CreateSolidBrush(gdi_color_ref(c));
}

static void gdi_delete_brush(brush_t b) {
    DeleteBrush((HBRUSH)b);
}

static brush_t gdi_set_brush(brush_t b) {
    not_null(b);
    return (brush_t)SelectBrush(canvas(), b);
}

static color_t gdi_set_brush_color(color_t c) {
    return SetDCBrushColor(canvas(), gdi_color_ref(c));
}

static void gdi_set_clip(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (gdi.clip != null) { DeleteRgn(gdi.clip); gdi.clip = null; }
    if (w > 0 && h > 0) {
        gdi.clip = (region_t)CreateRectRgn(x, y, x + w, y + h);
        not_null(gdi.clip);
    }
    fatal_if(SelectClipRgn(canvas(), (HRGN)gdi.clip) == ERROR);
}

static void gdi_push(int32_t x, int32_t y) {
    assert(gdi_top < countof(gdi_stack));
    fatal_if(gdi_top >= countof(gdi_stack));
    gdi_stack[gdi_top].x = gdi.x;
    gdi_stack[gdi_top].y = gdi.y;
    fatal_if(SaveDC(canvas()) == 0);
    gdi_top++;
    gdi.x = x;
    gdi.y = y;
}

static void gdi_pop(void) {
    assert(0 < gdi_top && gdi_top <= countof(gdi_stack));
    fatal_if(gdi_top <= 0);
    gdi_top--;
    gdi.x = gdi_stack[gdi_top].x;
    gdi.y = gdi_stack[gdi_top].y;
    fatal_if_false(RestoreDC(canvas(), -1));
}

static void gdi_pixel(int32_t x, int32_t y, color_t c) {
    not_null(app.canvas);
    fatal_if_false(SetPixel(canvas(), x, y, gdi_color_ref(c)));
}

static ui_point_t gdi_move_to(int32_t x, int32_t y) {
    POINT pt;
    pt.x = gdi.x;
    pt.y = gdi.y;
    fatal_if_false(MoveToEx(canvas(), x, y, &pt));
    gdi.x = x;
    gdi.y = y;
    ui_point_t p = { pt.x, pt.y };
    return p;
}

static void gdi_line(int32_t x, int32_t y) {
    fatal_if_false(LineTo(canvas(), x, y));
    gdi.x = x;
    gdi.y = y;
}

static void gdi_frame(int32_t x, int32_t y, int32_t w, int32_t h) {
    brush_t b = gdi.set_brush(gdi.brush_hollow);
    gdi.rect(x, y, w, h);
    gdi.set_brush(b);
}

static void gdi_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    fatal_if_false(Rectangle(canvas(), x, y, x + w, y + h));
}

static void gdi_fill(int32_t x, int32_t y, int32_t w, int32_t h) {
    RECT rc = { x, y, x + w, y + h };
    brush_t b = (brush_t)GetCurrentObject(canvas(), OBJ_BRUSH);
    fatal_if_false(FillRect(canvas(), &rc, (HBRUSH)b));
}

static void gdi_frame_with(int32_t x, int32_t y, int32_t w, int32_t h,
        color_t c) {
    brush_t b = gdi.set_brush(gdi.brush_hollow);
    pen_t p = gdi.set_colored_pen(c);
    gdi.rect(x, y, w, h);
    gdi.set_pen(p);
    gdi.set_brush(b);
}

static void gdi_rect_with(int32_t x, int32_t y, int32_t w, int32_t h,
        color_t border, color_t fill) {
    brush_t b = gdi.set_brush(gdi.brush_color);
    color_t c = gdi.set_brush_color(fill);
    pen_t p = gdi.set_colored_pen(border);
    gdi.rect(x, y, w, h);
    gdi.set_brush_color(c);
    gdi.set_pen(p);
    gdi.set_brush(b);
}

static void gdi_fill_with(int32_t x, int32_t y, int32_t w, int32_t h,
        color_t c) {
    brush_t b = gdi.set_brush(gdi.brush_color);
    c = gdi.set_brush_color(c);
    gdi.fill(x, y, w, h);
    gdi.set_brush_color(c);
    gdi.set_brush(b);
}

static void gdi_poly(ui_point_t* points, int32_t count) {
    // make sure ui_point_t and POINT have the same memory layout:
    static_assert(sizeof(points->x) == sizeof(((POINT*)0)->x), "ui_point_t");
    static_assert(sizeof(points->y) == sizeof(((POINT*)0)->y), "ui_point_t");
    static_assert(sizeof(points[0]) == sizeof(*((POINT*)0)), "ui_point_t");
    assert(canvas() != null && count > 1);
    fatal_if_false(Polyline(canvas(), (POINT*)points, count));
}

static void gdi_rounded(int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t rx, int32_t ry) {
    fatal_if_false(RoundRect(canvas(), x, y, x + w, y + h, rx, ry));
}

static void gdi_gradient(int32_t x, int32_t y, int32_t w, int32_t h,
        color_t rgba_from, color_t rgba_to, bool vertical) {
    TRIVERTEX vertex[2];
    vertex[0].x = x;
    vertex[0].y = y;
    // TODO: colors:
    vertex[0].Red   = ((rgba_from >>  0) & 0xFF) << 8;
    vertex[0].Green = ((rgba_from >>  8) & 0xFF) << 8;
    vertex[0].Blue  = ((rgba_from >> 16) & 0xFF) << 8;
    vertex[0].Alpha = ((rgba_from >> 24) & 0xFF) << 8;
    vertex[1].x = x + w;
    vertex[1].y = y + h;
    vertex[1].Red   = ((rgba_to >>  0) & 0xFF) << 8;
    vertex[1].Green = ((rgba_to >>  8) & 0xFF) << 8;
    vertex[1].Blue  = ((rgba_to >> 16) & 0xFF) << 8;
    vertex[1].Alpha = ((rgba_to >> 24) & 0xFF) << 8;
    GRADIENT_RECT gRect = {0, 1};
    const int32_t mode = vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H;
    GradientFill(canvas(), vertex, 2, &gRect, 1, mode);
}

static BITMAPINFO* gdi_greyscale_bitmap_info(void) {
    typedef struct bitmap_rgb_s {
        BITMAPINFO bi;
        RGBQUAD rgb[256];
    } bitmap_rgb_t;
    static bitmap_rgb_t storage; // for grayscale palette
    static BITMAPINFO* bi = &storage.bi;
    BITMAPINFOHEADER* bih = &bi->bmiHeader;
    if (bih->biSize == 0) { // once
        bih->biSize = sizeof(BITMAPINFOHEADER);
        for (int32_t i = 0; i < 256; i++) {
            RGBQUAD* q = &bi->bmiColors[i];
            q->rgbReserved = 0;
            q->rgbBlue = q->rgbGreen = q->rgbRed = (uint8_t)i;
        }
        bih->biPlanes = 1;
        bih->biBitCount = 8;
        bih->biCompression = BI_RGB;
        bih->biClrUsed = 256;
        bih->biClrImportant = 256;
    }
    return bi;
}

static void gdi_draw_greyscale(int32_t sx, int32_t sy, int32_t sw, int32_t sh,
        int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t iw, int32_t ih, int32_t stride, const uint8_t* pixels) {
    fatal_if(stride != ((iw + 3) & ~0x3));
    assert(w > 0 && h != 0); // h can be negative
    if (w > 0 && h != 0) {
        BITMAPINFO *bi = gdi_greyscale_bitmap_info(); // global! not thread safe
        BITMAPINFOHEADER* bih = &bi->bmiHeader;
        bih->biWidth = iw;
        bih->biHeight = -ih; // top down image
        bih->biSizeImage = w * h;
        POINT pt = { 0 };
        fatal_if_false(SetBrushOrgEx(canvas(), 0, 0, &pt));
        fatal_if(StretchDIBits(canvas(), sx, sy, sw, sh, x, y, w, h,
            pixels, bi, DIB_RGB_COLORS, SRCCOPY) == 0);
        fatal_if_false(SetBrushOrgEx(canvas(), pt.x, pt.y, &pt));
    }
}

static BITMAPINFOHEADER gdi_bgrx_init_bi(int32_t w, int32_t h, int32_t bpp) {
    BITMAPINFOHEADER bi = {
        .biSize = sizeof(BITMAPINFOHEADER),
        .biPlanes = 1,
        .biBitCount = (uint16_t)(bpp * 8),
        .biCompression = BI_RGB,
        .biWidth = w,
        .biHeight = -h, // top down image
        .biSizeImage = w * h * bpp,
        .biClrUsed = 0,
        .biClrImportant = 0
   };
   return bi;
}

// draw_bgr(iw) assumes strides are padded and rounded up to 4 bytes
// if this is not the case use gdi.image_init() that will unpack
// and align scanlines prior to draw

static void gdi_draw_bgr(int32_t sx, int32_t sy, int32_t sw, int32_t sh,
        int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t iw, int32_t ih, int32_t stride,
        const uint8_t* pixels) {
    fatal_if(stride != ((iw * 3 + 3) & ~0x3));
    assert(w > 0 && h != 0); // h can be negative
    if (w > 0 && h != 0) {
        BITMAPINFOHEADER bi = gdi_bgrx_init_bi(iw, ih, 3);
        POINT pt = { 0 };
        fatal_if_false(SetBrushOrgEx(canvas(), 0, 0, &pt));
        fatal_if(StretchDIBits(canvas(), sx, sy, sw, sh, x, y, w, h,
            pixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS, SRCCOPY) == 0);
        fatal_if_false(SetBrushOrgEx(canvas(), pt.x, pt.y, &pt));
    }
}

static void gdi_draw_bgrx(int32_t sx, int32_t sy, int32_t sw, int32_t sh,
        int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t iw, int32_t ih, int32_t stride,
        const uint8_t* pixels) {
    fatal_if(stride != ((iw * 4 + 3) & ~0x3));
    assert(w > 0 && h != 0); // h can be negative
    if (w > 0 && h != 0) {
        BITMAPINFOHEADER bi = gdi_bgrx_init_bi(iw, ih, 4);
        POINT pt = { 0 };
        fatal_if_false(SetBrushOrgEx(canvas(), 0, 0, &pt));
        fatal_if(StretchDIBits(canvas(), sx, sy, sw, sh, x, y, w, h,
            pixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS, SRCCOPY) == 0);
        fatal_if_false(SetBrushOrgEx(canvas(), pt.x, pt.y, &pt));
    }
}

static BITMAPINFO* gdi_init_bitmap_info(int32_t w, int32_t h, int32_t bpp,
        BITMAPINFO* bi) {
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth = w;
    bi->bmiHeader.biHeight = -h;  // top down image
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = (uint16_t)(bpp * 8);
    bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biSizeImage = w * h * bpp;
    return bi;
}

static void gdi_create_dib_section(image_t* image, int32_t w, int32_t h,
        int32_t bpp) {
    fatal_if(image->bitmap != null, "image_dispose() not called?");
    // not using GetWindowDC(window()) will allow to initialize images
    // before window is created
    HDC c = CreateCompatibleDC(null); // GetWindowDC(window());
    BITMAPINFO local = { {sizeof(BITMAPINFOHEADER)} };
    BITMAPINFO* bi = bpp == 1 ? gdi_greyscale_bitmap_info() : &local;
    image->bitmap = (bitmap_t)CreateDIBSection(c, gdi_init_bitmap_info(w, h, bpp, bi),
                                               DIB_RGB_COLORS, &image->pixels, null, 0x0);
    fatal_if(image->bitmap == null || image->pixels == null);
//  fatal_if_false(ReleaseDC(window(), c));
    fatal_if_false(DeleteDC(c));
}

static void gdi_image_init_rgbx(image_t* image, int32_t w, int32_t h,
        int32_t bpp, const uint8_t* pixels) {
    bool swapped = bpp < 0;
    bpp = abs(bpp);
    fatal_if(bpp != 4, "bpp: %d", bpp);
    gdi_create_dib_section(image, w, h, bpp);
    const int32_t stride = (w * bpp + 3) & ~0x3;
    uint8_t* scanline = image->pixels;
    const uint8_t* rgbx = pixels;
    if (!swapped) {
        for (int32_t y = 0; y < h; y++) {
            uint8_t* bgra = scanline;
            for (int32_t x = 0; x < w; x++) {
                bgra[0] = rgbx[2];
                bgra[1] = rgbx[1];
                bgra[2] = rgbx[0];
                bgra[3] = 0xFF;
                bgra += 4;
                rgbx += 4;
            }
            pixels += w * 4;
            scanline += stride;
        }
    } else {
        for (int32_t y = 0; y < h; y++) {
            uint8_t* bgra = scanline;
            for (int32_t x = 0; x < w; x++) {
                bgra[0] = rgbx[0];
                bgra[1] = rgbx[1];
                bgra[2] = rgbx[2];
                bgra[3] = 0xFF;
                bgra += 4;
                rgbx += 4;
            }
            pixels += w * 4;
            scanline += stride;
        }
    }
    image->w = w;
    image->h = h;
    image->bpp = bpp;
    image->stride = stride;
}

static void gdi_image_init(image_t* image, int32_t w, int32_t h, int32_t bpp,
        const uint8_t* pixels) {
    bool swapped = bpp < 0;
    bpp = abs(bpp);
    fatal_if(bpp < 0 || bpp == 2 || bpp > 4, "bpp=%d not {1, 3, 4}", bpp);
    gdi_create_dib_section(image, w, h, bpp);
    // Win32 bitmaps stride is rounded up to 4 bytes
    const int32_t stride = (w * bpp + 3) & ~0x3;
    uint8_t* scanline = image->pixels;
    if (bpp == 1) {
        for (int32_t y = 0; y < h; y++) {
            memcpy(scanline, pixels, w);
            pixels += w;
            scanline += stride;
        }
    } else if (bpp == 3 && !swapped) {
        const uint8_t* rgb = pixels;
        for (int32_t y = 0; y < h; y++) {
            uint8_t* bgr = scanline;
            for (int32_t x = 0; x < w; x++) {
                bgr[0] = rgb[2];
                bgr[1] = rgb[1];
                bgr[2] = rgb[0];
                bgr += 3;
                rgb += 3;
            }
            pixels += w * bpp;
            scanline += stride;
        }
    } else if (bpp == 3 && swapped) {
        const uint8_t* rgb = pixels;
        for (int32_t y = 0; y < h; y++) {
            uint8_t* bgr = scanline;
            for (int32_t x = 0; x < w; x++) {
                bgr[0] = rgb[0];
                bgr[1] = rgb[1];
                bgr[2] = rgb[2];
                bgr += 3;
                rgb += 3;
            }
            pixels += w * bpp;
            scanline += stride;
        }
    } else if (bpp == 4 && !swapped) {
        // premultiply alpha, see:
        // https://stackoverflow.com/questions/24595717/alphablend-generating-incorrect-colors
        const uint8_t* rgba = pixels;
        for (int32_t y = 0; y < h; y++) {
            uint8_t* bgra = scanline;
            for (int32_t x = 0; x < w; x++) {
                int32_t alpha = rgba[3];
                bgra[0] = (uint8_t)(rgba[2] * alpha / 255);
                bgra[1] = (uint8_t)(rgba[1] * alpha / 255);
                bgra[2] = (uint8_t)(rgba[0] * alpha / 255);
                bgra[3] = rgba[3];
                bgra += 4;
                rgba += 4;
            }
            pixels += w * 4;
            scanline += stride;
        }
    } else if (bpp == 4 && swapped) {
        // premultiply alpha, see:
        // https://stackoverflow.com/questions/24595717/alphablend-generating-incorrect-colors
        const uint8_t* rgba = pixels;
        for (int32_t y = 0; y < h; y++) {
            uint8_t* bgra = scanline;
            for (int32_t x = 0; x < w; x++) {
                int32_t alpha = rgba[3];
                bgra[0] = (uint8_t)(rgba[0] * alpha / 255);
                bgra[1] = (uint8_t)(rgba[1] * alpha / 255);
                bgra[2] = (uint8_t)(rgba[2] * alpha / 255);
                bgra[3] = rgba[3];
                bgra += 4;
                rgba += 4;
            }
            pixels += w * 4;
            scanline += stride;
        }
    }
    image->w = w;
    image->h = h;
    image->bpp = bpp;
    image->stride = stride;
}

static void gdi_alpha_blend(int32_t x, int32_t y, int32_t w, int32_t h,
        image_t* image, double alpha) {
    assert(image->bpp > 0);
    assert(0 <= alpha && alpha <= 1);
    not_null(canvas());
    HDC c = CreateCompatibleDC(canvas());
    not_null(c);
    HBITMAP zero1x1 = SelectBitmap((HDC)c, (HBITMAP)image->bitmap);
    BLENDFUNCTION bf = { 0 };
    bf.SourceConstantAlpha = (uint8_t)(0xFF * alpha + 0.49);
    if (image->bpp == 4) {
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.AlphaFormat = AC_SRC_ALPHA;
    } else {
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.AlphaFormat = 0;
    }
    fatal_if_false(AlphaBlend(canvas(), x, y, w, h,
        c, 0, 0, image->w, image->h, bf));
    SelectBitmap((HDC)c, zero1x1);
    fatal_if_false(DeleteDC(c));
}

static void gdi_draw_image(int32_t x, int32_t y, int32_t w, int32_t h,
        image_t* image) {
    assert(image->bpp == 1 || image->bpp == 3 || image->bpp == 4);
    not_null(canvas());
    if (image->bpp == 1) { // StretchBlt() is bad for greyscale
        BITMAPINFO* bi = gdi_greyscale_bitmap_info();
        fatal_if(StretchDIBits(canvas(), x, y, w, h, 0, 0, image->w, image->h,
            image->pixels, gdi_init_bitmap_info(image->w, image->h, 1, bi),
            DIB_RGB_COLORS, SRCCOPY) == 0);
    } else {
        HDC c = CreateCompatibleDC(canvas());
        not_null(c);
        HBITMAP zero1x1 = SelectBitmap(c, image->bitmap);
        fatal_if_false(StretchBlt(canvas(), x, y, w, h,
            c, 0, 0, image->w, image->h, SRCCOPY));
        SelectBitmap(c, zero1x1);
        fatal_if_false(DeleteDC(c));
    }
}

static void gdi_cleartype(bool on) {
    enum { spif = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE };
    fatal_if_false(SystemParametersInfoA(SPI_SETFONTSMOOTHING, true, 0, spif));
    uintptr_t s = on ? FE_FONTSMOOTHINGCLEARTYPE : FE_FONTSMOOTHINGSTANDARD;
    fatal_if_false(SystemParametersInfoA(SPI_SETFONTSMOOTHINGTYPE, 0,
        (void*)s, spif));
}

static void gdi_font_smoothing_contrast(int32_t c) {
    fatal_if(!(c == -1 || 1000 <= c && c <= 2200), "contrast: %d", c);
    if (c == -1) { c = 1400; }
    fatal_if_false(SystemParametersInfoA(SPI_SETFONTSMOOTHINGCONTRAST, 0,
                   (void*)(uintptr_t)c, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE));
}

static_assertion(gdi_font_quality_default == DEFAULT_QUALITY);
static_assertion(gdi_font_quality_draft == DRAFT_QUALITY);
static_assertion(gdi_font_quality_proof == PROOF_QUALITY);
static_assertion(gdi_font_quality_nonantialiased == NONANTIALIASED_QUALITY);
static_assertion(gdi_font_quality_antialiased == ANTIALIASED_QUALITY);
static_assertion(gdi_font_quality_cleartype == CLEARTYPE_QUALITY);
static_assertion(gdi_font_quality_cleartype_natural == CLEARTYPE_NATURAL_QUALITY);

static font_t gdi_font(font_t f, int32_t height, int32_t quality) {
    assert(f != null && height > 0);
    LOGFONTA lf = {0};
    int32_t n = GetObjectA(f, sizeof(lf), &lf);
    fatal_if_false(n == (int)sizeof(lf));
    lf.lfHeight = -height;
    if (gdi_font_quality_default <= quality && quality <= gdi_font_quality_cleartype_natural) {
        lf.lfQuality = (uint8_t)quality;
    } else {
        fatal_if(quality != -1, "use -1 for do not care quality");
    }
    return (font_t)CreateFontIndirectA(&lf);
}

static int32_t gdi_font_height(font_t f) {
    assert(f != null);
    LOGFONTA lf = {0};
    int32_t n = GetObjectA(f, sizeof(lf), &lf);
    fatal_if_false(n == (int)sizeof(lf));
    assert(lf.lfHeight < 0);
    return abs(lf.lfHeight);
}

static void gdi_delete_font(font_t f) {
    fatal_if_false(DeleteFont(f));
}

static font_t gdi_set_font(font_t f) {
    not_null(f);
    return (font_t)SelectFont(canvas(), (HFONT)f);
}

#define gdi_with_hdc(code) do {                              \
    not_null(window());                                      \
    HDC hdc = canvas() != null ? canvas() : GetDC(window()); \
    not_null(hdc);                                           \
    code                                                     \
    if (canvas() == null) {                                  \
        ReleaseDC(window(), hdc);                            \
    }                                                        \
} while (0);

#define gdi_hdc_with_font(f, code) do {                      \
    not_null(f);                                             \
    not_null(window());                                      \
    HDC hdc = canvas() != null ? canvas() : GetDC(window()); \
    not_null(hdc);                                           \
    HFONT _font_ = SelectFont(hdc, (HFONT)f);                \
    code                                                     \
    SelectFont(hdc, _font_);                                 \
    if (canvas() == null) {                                  \
        ReleaseDC(window(), hdc);                            \
    }                                                        \
} while (0);


static int32_t gdi_baseline(font_t f) {
    TEXTMETRICA tm;
    gdi_hdc_with_font(f, {
        fatal_if_false(GetTextMetricsA(hdc, &tm));
    })
    return tm.tmAscent;
}

static int32_t gdi_descent(font_t f) {
    TEXTMETRICA tm;
    gdi_hdc_with_font(f, {
        fatal_if_false(GetTextMetricsA(hdc, &tm));
    });
    return tm.tmDescent;
}

static ui_point_t gdi_get_em(font_t f) {
    SIZE cell = {0};
    gdi_hdc_with_font(f, {
        fatal_if_false(GetTextExtentPoint32A(hdc, "M", 1, &cell));
    });
    ui_point_t c = {cell.cx, cell.cy};
    return c;
}

static bool gdi_is_mono(font_t f) {
    SIZE em = {0}; // "M"
    SIZE vl = {0}; // "|" Vertical Line https://www.compart.com/en/unicode/U+007C
    SIZE e3 = {0}; // "\xE2\xB8\xBB" Three-Em Dash https://www.compart.com/en/unicode/U+2E3B
    gdi_hdc_with_font(f, {
        fatal_if_false(GetTextExtentPoint32A(hdc, "M", 1, &em));
        fatal_if_false(GetTextExtentPoint32A(hdc, "|", 1, &vl));
        fatal_if_false(GetTextExtentPoint32A(hdc, "\xE2\xB8\xBB", 1, &e3));
    });
    return em.cx == vl.cx && vl.cx == e3.cx;
}

static double gdi_line_spacing(double height_multiplier) {
    assert(0.1 <= height_multiplier && height_multiplier <= 2.0);
    double hm = gdi.height_multiplier;
    gdi.height_multiplier = height_multiplier;
    return hm;
}

static int32_t gdi_draw_utf16(font_t font, const char* s, int32_t n,
        RECT* r, uint32_t format) {
    // if font == null, draws on HDC with selected font
    int32_t height = 0; // return value is the height of the text in logical units
    if (font != null) {
        gdi_hdc_with_font(font, {
            height = DrawTextW(hdc, utf8to16(s), n, r, format);
        });
    } else {
        gdi_with_hdc({
            height = DrawTextW(hdc, utf8to16(s), n, r, format);
        });
    }
    return height;
}

typedef struct gdi_dtp_s { // draw text params
    font_t font;
    const char* format; // format string
    va_list vl;
    RECT rc;
    uint32_t flags; // flags:
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-drawtextw
    // DT_CALCRECT DT_NOCLIP useful for measure
    // DT_END_ELLIPSIS useful for clipping
    // DT_LEFT, DT_RIGHT, DT_CENTER useful for paragraphs
    // DT_WORDBREAK is not good (GDI does not break nicely)
    // DT_BOTTOM, DT_VCENTER limited usablity in wierd cases (layout is better)
    // DT_NOPREFIX not to draw underline at "&Keyboard shortcuts
    // DT_SINGLELINE versus multiline
} gdi_dtp_t;

static void gdi_text_draw(gdi_dtp_t* p) {
    int32_t n = 1024;
    char* text = (char*)alloca(n);
    str.vformat(text, n - 1, p->format, p->vl);
    int32_t k = (int32_t)strlen(text);
    // Microsoft returns -1 not posix required sizeof buffer
    while (k >= n - 1 || k < 0) {
        n = n * 2;
        text = (char*)alloca(n);
        str.vformat(text, n - 1, p->format, p->vl);
        k = (int)strlen(text);
    }
    assert(k >= 0 && k <= n, "k=%d n=%d fmt=%s", k, n, p->format);
    // rectangle is always calculated - it makes draw text
    // much slower but UI layer is mostly uses bitmap caching:
    if ((p->flags & DT_CALCRECT) == 0) {
        // no actual drawing just calculate rectangle
        bool b = gdi_draw_utf16(p->font, text, -1, &p->rc, p->flags | DT_CALCRECT);
        assert(b, "draw_text_utf16(%s) failed", text); (void)b;
    }
    bool b = gdi_draw_utf16(p->font, text, -1, &p->rc, p->flags);
    assert(b, "draw_text_utf16(%s) failed", text); (void)b;
}

enum {
    sl_draw          = DT_LEFT|DT_NOCLIP|DT_SINGLELINE|DT_NOCLIP,
    sl_measure       = sl_draw|DT_CALCRECT,
    ml_draw_break    = DT_LEFT|DT_NOPREFIX|DT_NOCLIP|DT_NOFULLWIDTHCHARBREAK|
                       DT_WORDBREAK,
    ml_measure_break = ml_draw_break|DT_CALCRECT,
    ml_draw          = DT_LEFT|DT_NOPREFIX|DT_NOCLIP|DT_NOFULLWIDTHCHARBREAK,
    ml_measure       = ml_draw|DT_CALCRECT
};

static ui_point_t gdi_text_measure(gdi_dtp_t* p) {
    gdi_text_draw(p);
    ui_point_t cell = {p->rc.right - p->rc.left, p->rc.bottom - p->rc.top};
    return cell;
}

static ui_point_t gdi_measure_singleline(font_t f, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    gdi_dtp_t p = { f, format, vl, {0, 0, 0, 0}, sl_measure };
    ui_point_t cell = gdi_text_measure(&p);
    va_end(vl);
    return cell;
}

static ui_point_t gdi_measure_multiline(font_t f, int32_t w, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    uint32_t flags = w <= 0 ? ml_measure : ml_measure_break;
    gdi_dtp_t p = { f, format, vl, {gdi.x, gdi.y, gdi.x + (w <= 0 ? 1 : w), gdi.y}, flags };
    ui_point_t cell = gdi_text_measure(&p);
    va_end(vl);
    return cell;
}

static void gdi_vtext(const char* format, va_list vl) {
    gdi_dtp_t p = { null, format, vl, {gdi.x, gdi.y, 0, 0}, sl_draw };
    gdi_text_draw(&p);
    gdi.x += p.rc.right - p.rc.left;
}

static void gdi_vtextln(const char* format, va_list vl) {
    gdi_dtp_t p = { null, format, vl, {gdi.x, gdi.y, gdi.x, gdi.y}, sl_draw };
    gdi_text_draw(&p);
    gdi.y += (int)((p.rc.bottom - p.rc.top) * gdi.height_multiplier + 0.5f);
}

static void gdi_text(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    gdi.vtext(format, vl);
    va_end(vl);
}

static void gdi_textln(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    gdi.vtextln(format, vl);
    va_end(vl);
}

static ui_point_t gdi_multiline(int32_t w, const char* f, ...) {
    va_list vl;
    va_start(vl, f);
    uint32_t flags = w <= 0 ? ml_draw : ml_draw_break;
    gdi_dtp_t p = { null, f, vl, {gdi.x, gdi.y, gdi.x + (w <= 0 ? 1 : w), gdi.y}, flags };
    gdi_text_draw(&p);
    va_end(vl);
    ui_point_t c = { p.rc.right - p.rc.left, p.rc.bottom - p.rc.top };
    return c;
}

static void gdi_vprint(const char* format, va_list vl) {
    not_null(app.fonts.mono);
    font_t f = gdi.set_font(app.fonts.mono);
    gdi.vtext(format, vl);
    gdi.set_font(f);
}

static void gdi_print(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    gdi.vprint(format, vl);
    va_end(vl);
}

static void gdi_vprintln(const char* format, va_list vl) {
    not_null(app.fonts.mono);
    font_t f = gdi.set_font(app.fonts.mono);
    gdi.vtextln(format, vl);
    gdi.set_font(f);
}

static void gdi_println(const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    gdi.vprintln(format, vl);
    va_end(vl);
}

// to enable load_image() function
// 1. Add
//    curl.exe https://raw.githubusercontent.com/nothings/stb/master/stb_image.h stb_image.h
//    to the project precompile build step
// 2. After
//    #define quick_implementation
//    include "quick.h"
//    add
//    #define STBI_ASSERT(x) assert(x)
//    #define STB_IMAGE_IMPLEMENTATION
//    #include "stb_image.h"

static uint8_t* gdi_load_image(const void* data, int32_t bytes, int* w, int* h,
        int* bytes_per_pixel, int32_t preferred_bytes_per_pixel) {
    #ifdef STBI_VERSION
        return stbi_load_from_memory((uint8_t const*)data, bytes, w, h,
            bytes_per_pixel, preferred_bytes_per_pixel);
    #else // see instructions above
        (void)data; (void)bytes; (void)data; (void)w; (void)h;
        (void)bytes_per_pixel; (void)preferred_bytes_per_pixel;
        fatal_if(true, "curl.exe --silent --fail --create-dirs "
            "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h "
            "--output ext/stb_image.h");
        return null;
    #endif
}

static void gdi_image_dispose(image_t* image) {
    fatal_if_false(DeleteBitmap(image->bitmap));
    memset(image, 0, sizeof(image_t));
}

gdi_t gdi = {
    .height_multiplier = 1.0,
    .image_init = gdi_image_init,
    .image_init_rgbx = gdi_image_init_rgbx,
    .image_dispose = gdi_image_dispose,
    .alpha_blend = gdi_alpha_blend,
    .draw_image = gdi_draw_image,
    .set_text_color = gdi_set_text_color,
    .create_brush = gdi_create_brush,
    .delete_brush = gdi_delete_brush,
    .set_brush = gdi_set_brush,
    .set_brush_color = gdi_set_brush_color,
    .set_colored_pen = gdi_set_colored_pen,
    .create_pen = gdi_create_pen,
    .set_pen = gdi_set_pen,
    .delete_pen = gdi_delete_pen,
    .set_clip = gdi_set_clip,
    .push = gdi_push,
    .pop = gdi_pop,
    .pixel = gdi_pixel,
    .move_to = gdi_move_to,
    .line = gdi_line,
    .frame = gdi_frame,
    .rect = gdi_rect,
    .fill = gdi_fill,
    .frame_with = gdi_frame_with,
    .rect_with = gdi_rect_with,
    .fill_with = gdi_fill_with,
    .poly = gdi_poly,
    .rounded = gdi_rounded,
    .gradient = gdi_gradient,
    .draw_greyscale = gdi_draw_greyscale,
    .draw_bgr = gdi_draw_bgr,
    .draw_bgrx = gdi_draw_bgrx,
    .cleartype = gdi_cleartype,
    .font_smoothing_contrast = gdi_font_smoothing_contrast,
    .font = gdi_font,
    .delete_font = gdi_delete_font,
    .set_font = gdi_set_font,
    .font_height = gdi_font_height,
    .descent = gdi_descent,
    .baseline = gdi_baseline,
    .is_mono = gdi_is_mono,
    .get_em = gdi_get_em,
    .line_spacing = gdi_line_spacing,
    .measure_text = gdi_measure_singleline,
    .measure_multiline = gdi_measure_multiline,
    .vtext = gdi_vtext,
    .vtextln = gdi_vtextln,
    .text = gdi_text,
    .textln = gdi_textln,
    .vprint = gdi_vprint,
    .vprintln = gdi_vprintln,
    .print = gdi_print,
    .println = gdi_println,
    .multiline = gdi_multiline
};

enum {
    _colors_white     = rgb(255, 255, 255),
    _colors_off_white = rgb(192, 192, 192),
    _colors_dkgray4   = rgb(63, 63, 70),
    _colors_blue_highlight = rgb(128, 128, 255)
};

colors_t colors = {
    .none    = (int)0xFFFFFFFF, // aka CLR_INVALID in wingdi
    .text    = rgb(240, 231, 220),
    .white   = _colors_white,
    .black   = rgb(0, 0, 0),
    .red     = rgb(255, 0, 0),
    .green   = rgb(0, 255, 0),
    .blue    = rgb(0, 0, 255),
    .yellow  = rgb(255, 255, 0),
    .cyan    = rgb(0, 255, 255),
    .magenta = rgb(255, 0, 255),
    .gray    = rgb(128, 128, 128),
    .dkgray1  = rgb(30, 30, 30),
    .dkgray2  = rgb(37, 38, 38),
    .dkgray3  = rgb(45, 45, 48),
    .dkgray4  = _colors_dkgray4,
    // tone down RGB colors:
    .tone_white   = rgb(164, 164, 164),
    .tone_red     = rgb(192, 64, 64),
    .tone_green   = rgb(64, 192, 64),
    .tone_blue    = rgb(64, 64, 192),
    .tone_yellow  = rgb(192, 192, 64),
    .tone_cyan    = rgb(64, 192, 192),
    .tone_magenta = rgb(192, 64, 192),
    // misc:
    .orange  = rgb(255, 165, 0), // 0xFFA500
    .dkgreen = rgb(1, 50, 32),   // 0x013220
    // highlights:
    .text_highlight = rgb(190, 200, 255), // bluish off-white
    .blue_highlight = _colors_blue_highlight,
    .off_white = _colors_off_white,

    .btn_gradient_darker = rgb(16, 16, 16),
    .btn_gradient_dark   = _colors_dkgray4,
    .btn_hover_highlight = _colors_blue_highlight,
    .btn_disabled = _colors_dkgray4,
    .btn_armed = _colors_white,
    .btn_text = _colors_off_white,
    .toast = rgb(8, 40, 24) // toast background
};

end_c

// UIC implementation

begin_c

enum { toast_steps = 15 }; // number of animation steps

static struct {
    view_t* ui;
    int32_t step;
    double time; // closing time or zero
    int32_t x; // -1 for toast
    int32_t y; // screen coordinates for tooltip
} app_toast;

static void uic_invalidate(const view_t* ui) {
    ui_rect_t rc = { ui->x, ui->y, ui->w, ui->h};
    rc.x -= ui->em.x;
    rc.y -= ui->em.y;
    rc.w += ui->em.x * 2;
    rc.h += ui->em.y * 2;
    app.invalidate(&rc);
}

static bool app_is_hidden(const view_t* ui) {
    bool hidden = ui->hidden;
    while (!hidden && ui->parent != null) {
        ui = ui->parent;
        hidden = ui->hidden;
    }
    return hidden;
}

static bool app_is_disabled(const view_t* ui) {
    bool disabled = ui->disabled;
    while (!disabled && ui->parent != null) {
        ui = ui->parent;
        disabled = ui->disabled;
    }
    return disabled;
}

static bool app_is_active(void) {
    return GetActiveWindow() == window();
}

static bool app_has_focus(void) {
    return GetFocus() == window();
}

static void window_request_focus(void* w) {
    // https://stackoverflow.com/questions/62649124/pywin32-setfocus-resulting-in-access-is-denied-error
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-attachthreadinput
    assert(threads.id() == app.tid, "cannot be called from background thread");
    runtime.seterr(0);
    w = SetFocus((HWND)w); // w previous focused window
    if (w == null) { fatal_if_not_zero(runtime.err()); }
}

static void app_request_focus(void) {
    window_request_focus(app.window);
}

static const char* uic_nsl(view_t* ui) {
    return ui->strid != 0 ? app.string(ui->strid, ui->text) : ui->text;
}

static void uic_measure(view_t* ui) {
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    ui->em = gdi.get_em(f);
    assert(ui->em.x > 0 && ui->em.y > 0);
    ui->w = (int32_t)(ui->em.x * ui->width + 0.5);
    ui_point_t mt = { 0 };
    if (ui->tag == uic_tag_text && ((uic_text_t*)ui)->multiline) {
        int32_t w = (int)(ui->width * ui->em.x + 0.5);
        mt = gdi.measure_multiline(f, w == 0 ? -1 : w, uic_nsl(ui));
    } else {
        mt = gdi.measure_text(f, uic_nsl(ui));
    }
    ui->h = mt.y;
    ui->w = max(ui->w, mt.x);
    ui->baseline = gdi.baseline(f);
    ui->descent  = gdi.descent(f);
}

static void uic_set_label(view_t* ui, const char* label) {
    int32_t n = (int32_t)strlen(label);
    strprintf(ui->text, "%s", label);
    for (int32_t i = 0; i < n; i++) {
        if (label[i] == '&' && i < n - 1 && label[i + 1] != '&') {
            ui->shortcut = label[i + 1];
            break;
        }
    }
}

static void uic_localize(view_t* ui) {
    if (ui->text[0] != 0) {
        ui->strid = app.strid(ui->text);
    }
}

static bool uic_hidden_or_disabled(view_t* ui) {
    return app.is_hidden(ui) || app.is_disabled(ui);
}

static void uic_hovering(view_t* ui, bool start) {
    static uic_text(btn_tooltip,  "");
    if (start && app_toast.ui == null && ui->tip[0] != 0 &&
       !app.is_hidden(ui)) {
        strprintf(btn_tooltip.ui.text, "%s", app.nls(ui->tip));
        btn_tooltip.ui.font = &app.fonts.H1;
        int32_t y = app.mouse.y - ui->em.y;
        // enough space above? if not show below
        if (y < ui->em.y) { y = app.mouse.y + ui->em.y * 3 / 2; }
        y = min(app.crc.h - ui->em.y * 3 / 2, max(0, y));
        app.show_tooltip(&btn_tooltip.ui, app.mouse.x, y, 0);
    } else if (!start && app_toast.ui == &btn_tooltip.ui) {
        app.show_tooltip(null, -1, -1, 0);
    }
}

void uic_init(view_t* ui) {
    ui->invalidate = uic_invalidate;
    ui->localize = uic_localize;
    ui->measure  = uic_measure;
    ui->hovering = uic_hovering;
    ui->hover_delay = 1.5;
}

// text

static void uic_text_paint(view_t* ui) {
    assert(ui->tag == uic_tag_text);
    assert(!ui->hidden);
    uic_text_t* t = (uic_text_t*)ui;
    // at later stages of layout text height can grow:
    gdi.push(ui->x, ui->y + t->dy);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    gdi.set_font(f);
//  traceln("%s h=%d dy=%d baseline=%d", ui->text, ui->h, t->dy, ui->baseline);
    color_t c = ui->hover && t->highlight && !t->label ?
        colors.text_highlight : ui->color;
    gdi.set_text_color(c);
    // paint for text also does lightweight re-layout
    // which is useful for simplifying dynamic text changes
    if (!t->multiline) {
        gdi.text("%s", uic_nsl(ui));
    } else {
        int32_t w = (int)(ui->width * ui->em.x + 0.5);
        gdi.multiline(w == 0 ? -1 : w, "%s", uic_nsl(ui));
    }
    if (ui->hover && t->hovered && !t->label) {
        gdi.set_colored_pen(colors.btn_hover_highlight);
        gdi.set_brush(gdi.brush_hollow);
        int32_t cr = ui->em.y / 4; // corner radius
        int32_t h = t->multiline ? ui->h : ui->baseline + ui->descent;
        gdi.rounded(ui->x - cr, ui->y + t->dy, ui->w + 2 * cr,
            h, cr, cr);
    }
    gdi.pop();
}

static void uic_text_context_menu(view_t* ui) {
    assert(ui->tag == uic_tag_text);
    uic_text_t* t = (uic_text_t*)ui;
    if (!t->label && !uic_hidden_or_disabled(ui)) {
        clipboard.copy_text(uic_nsl(ui));
        static bool first_time = true;
        app.toast(first_time ? 2.15 : 0.75,
            app.nls("Text copied to clipboard"));
        first_time = false;
    }
}

static void uic_text_character(view_t* ui, const char* utf8) {
    assert(ui->tag == uic_tag_text);
    uic_text_t* t = (uic_text_t*)ui;
    if (ui->hover && !uic_hidden_or_disabled(ui) && !t->label) {
        char ch = utf8[0];
        // Copy to clipboard works for hover over text
        if ((ch == 3 || ch == 'c' || ch == 'C') && app.ctrl) {
            clipboard.copy_text(uic_nsl(ui)); // 3 is ASCII for Ctrl+C
        }
    }
}

void _uic_text_init_(view_t* ui) {
    static_assert(offsetof(uic_text_t, ui) == 0, "offsetof(.ui)");
    assert(ui->tag == uic_tag_text);
    uic_init(ui);
    if (ui->font == null) { ui->font = &app.fonts.regular; }
    ui->color = colors.text;
    ui->paint = uic_text_paint;
    ui->character = uic_text_character;
    ui->context_menu = uic_text_context_menu;
}

void uic_text_vinit(uic_text_t* t, const char* format, va_list vl) {
    static_assert(offsetof(uic_text_t, ui) == 0, "offsetof(.ui)");
    str.vformat(t->ui.text, countof(t->ui.text), format, vl);
    t->ui.tag = uic_tag_text;
    _uic_text_init_(&t->ui);
}

void uic_text_init(uic_text_t* t, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    uic_text_vinit(t, format, vl);
    va_end(vl);
}

void uic_text_init_ml(uic_text_t* t, double width, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    uic_text_vinit(t, format, vl);
    va_end(vl);
    t->ui.width = width;
    t->multiline = true;
}

// button

static void uic_button_every_100ms(view_t* ui) { // every 100ms
    assert(ui->tag == uic_tag_button);
    uic_button_t* b = (uic_button_t*)ui;
    if (b->armed_until != 0 && app.now > b->armed_until) {
        b->armed_until = 0;
        ui->armed = false;
        ui->invalidate(ui);
    }
}

static void uic_button_paint(view_t* ui) {
    assert(ui->tag == uic_tag_button);
    assert(!ui->hidden);
    uic_button_t* b = (uic_button_t*)ui;
    gdi.push(ui->x, ui->y);
    bool pressed = (ui->armed ^ ui->pressed) == 0;
    if (b->armed_until != 0) { pressed = true; }
    int32_t sign = 1 - pressed * 2; // -1, +1
    int32_t w = sign * ui->w;
    int32_t h = sign * ui->h;
    int32_t x = b->ui.x + (int)pressed * ui->w;
    int32_t y = b->ui.y + (int)pressed * ui->h;
    gdi.gradient(x, y, w, h, colors.btn_gradient_darker,
        colors.btn_gradient_dark, true);
    color_t c = ui->armed ? colors.btn_armed : ui->color;
    if (b->ui.hover && !ui->armed) { c = colors.btn_hover_highlight; }
    if (ui->disabled) { c = colors.btn_disabled; }
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    ui_point_t m = gdi.measure_text(f, uic_nsl(ui));
    gdi.set_text_color(c);
    gdi.x = ui->x + (ui->w - m.x) / 2;
    gdi.y = ui->y + (ui->h - m.y) / 2;
    f = gdi.set_font(f);
    gdi.text("%s", uic_nsl(ui));
    gdi.set_font(f);
    const int32_t pw = max(1, ui->em.y / 32); // pen width
    color_t color = ui->armed ? colors.dkgray4 : colors.gray;
    if (ui->hover && !ui->armed) { color = colors.blue; }
    if (ui->disabled) { color = colors.dkgray1; }
    pen_t p = gdi.create_pen(color, pw);
    gdi.set_pen(p);
    gdi.set_brush(gdi.brush_hollow);
    gdi.rounded(ui->x, ui->y, ui->w, ui->h, ui->em.y / 4, ui->em.y / 4);
    gdi.delete_pen(p);
    gdi.pop();
}

static bool uic_button_hit_test(uic_button_t* b, ui_point_t pt) {
    assert(b->ui.tag == uic_tag_button);
    pt.x -= b->ui.x;
    pt.y -= b->ui.y;
    return 0 <= pt.x && pt.x < b->ui.w && 0 <= pt.y && pt.y < b->ui.h;
}

static void uic_button_callback(uic_button_t* b) {
    assert(b->ui.tag == uic_tag_button);
    app.show_tooltip(null, -1, -1, 0);
    if (b->cb != null) { b->cb(b); }
}

static bool uic_is_keyboard_shortcut(view_t* ui, int32_t key) {
    // Supported keyboard shortcuts are ASCII characters only for now
    // If there is not focused UI control in Alt+key [Alt] is optional.
    // If there is focused control only Alt+Key is accepted as shortcut
    char ch = 0x20 <= key && key <= 0x7F ? (char)toupper(key) : 0x00;
    bool need_alt = app.focus != null && app.focus != ui;
    bool keyboard_shortcut = ch != 0x00 && ui->shortcut != 0x00 &&
         (app.alt || !need_alt) && toupper(ui->shortcut) == ch;
    return keyboard_shortcut;
}

static void uic_button_trigger(view_t* ui) {
    assert(ui->tag == uic_tag_button);
    assert(!ui->hidden && !ui->disabled);
    uic_button_t* b = (uic_button_t*)ui;
    ui->armed = true;
    ui->invalidate(ui);
    app.draw();
    b->armed_until = app.now + 0.250;
    uic_button_callback(b);
    ui->invalidate(ui);
}

static void uic_button_character(view_t* ui, const char* utf8) {
    assert(ui->tag == uic_tag_button);
    assert(!ui->hidden && !ui->disabled);
    char ch = utf8[0]; // TODO: multibyte shortcuts?
    if (uic_is_keyboard_shortcut(ui, ch)) {
        uic_button_trigger(ui);
    }
}

static void uic_button_key_pressed(view_t* ui, int32_t key) {
    if (app.alt && uic_is_keyboard_shortcut(ui, key)) {
//      traceln("key: 0x%02X shortcut: %d", key, uic_is_keyboard_shortcut(ui, key));
        uic_button_trigger(ui);
    }
}

/* processes mouse clicks and invokes callback  */

static void uic_button_mouse(view_t* ui, int32_t message, int32_t flags) {
    assert(ui->tag == uic_tag_button);
    (void)flags; // unused
    assert(!ui->hidden && !ui->disabled);
    uic_button_t* b = (uic_button_t*)ui;
    bool a = ui->armed;
    bool on = false;
    if (message == messages.left_button_pressed ||
        message == messages.right_button_pressed) {
        ui->armed = uic_button_hit_test(b, app.mouse);
        if (ui->armed) { app.focus = ui; }
        if (ui->armed) { app.show_tooltip(null, -1, -1, 0); }
    }
    if (message == messages.left_button_released ||
        message == messages.right_button_released) {
        if (ui->armed) { on = uic_button_hit_test(b, app.mouse); }
        ui->armed = false;
    }
    if (on) { uic_button_callback(b); }
    if (a != ui->armed) { ui->invalidate(ui); }
}

static void uic_button_measure(view_t* ui) {
    assert(ui->tag == uic_tag_button || ui->tag == uic_tag_text);
    uic_measure(ui);
    const int32_t em2  = max(1, ui->em.x / 2);
    ui->w = ui->w;
    ui->h = ui->h + em2;
    if (ui->w < ui->h) { ui->w = ui->h; }
}

void _uic_button_init_(view_t* ui) {
    assert(ui->tag == uic_tag_button);
    uic_init(ui);
    ui->mouse       = uic_button_mouse;
    ui->measure     = uic_button_measure;
    ui->paint       = uic_button_paint;
    ui->character   = uic_button_character;
    ui->every_100ms = uic_button_every_100ms;
    ui->key_pressed = uic_button_key_pressed;
    uic_set_label(ui, ui->text);
    ui->localize(ui);
    ui->color = colors.btn_text;
}

void uic_button_init(uic_button_t* b, const char* label, double ems,
        void (*cb)(uic_button_t* b)) {
    static_assert(offsetof(uic_button_t, ui) == 0, "offsetof(.ui)");
    b->ui.tag = uic_tag_button;
    strprintf(b->ui.text, "%s", label);
    b->cb = cb;
    b->ui.width = ems;
    _uic_button_init_(&b->ui);
}

// checkbox

static int  uic_checkbox_paint_on_off(view_t* ui) {
    // https://www.compart.com/en/unicode/U+2B24
    static const char* circle = "\xE2\xAC\xA4"; // Black Large Circle
    gdi.push(ui->x, ui->y);
    color_t background = ui->pressed ? colors.tone_green : colors.dkgray4;
    color_t foreground = ui->color;
    gdi.set_text_color(background);
    int32_t x = ui->x;
    int32_t x1 = ui->x + ui->em.x * 3 / 4;
    while (x < x1) {
        gdi.x = x;
        gdi.text("%s", circle);
        x++;
    }
    int32_t rx = gdi.x;
    gdi.set_text_color(foreground);
    gdi.x = ui->pressed ? x : ui->x;
    gdi.text("%s", circle);
    gdi.pop();
    return rx;
}

static const char*  uic_checkbox_on_off_label(view_t* ui, char* label, int32_t count)  {
    str.sformat(label, count, "%s", uic_nsl(ui));
    char* s = strstr(label, "___");
    if (s != null) {
        memcpy(s, ui->pressed ? "On " : "Off", 3);
    }
    return app.nls(label);
}

static void  uic_checkbox_measure(view_t* ui) {
    assert(ui->tag == uic_tag_checkbox);
    uic_measure(ui);
    ui->w += ui->em.x * 2;
}

static void  uic_checkbox_paint(view_t* ui) {
    assert(ui->tag == uic_tag_checkbox);
    char text[countof(ui->text)];
    const char* label =  uic_checkbox_on_off_label(ui, text, countof(text));
    gdi.push(ui->x, ui->y);
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    font_t font = gdi.set_font(f);
    gdi.x =  uic_checkbox_paint_on_off(ui) + ui->em.x * 3 / 4;
    gdi.text("%s", label);
    gdi.set_font(font);
    gdi.pop();
}

static void  uic_checkbox_flip( uic_checkbox_t* c) {
    assert(c->ui.tag == uic_tag_checkbox);
    app.redraw();
    c->ui.pressed = !c->ui.pressed;
    if (c->cb != null) { c->cb(c); }
}

static void  uic_checkbox_character(view_t* ui, const char* utf8) {
    assert(ui->tag == uic_tag_checkbox);
    assert(!ui->hidden && !ui->disabled);
    char ch = utf8[0];
    if (uic_is_keyboard_shortcut(ui, ch)) {
         uic_checkbox_flip((uic_checkbox_t*)ui);
    }
}

static void uic_checkbox_key_pressed(view_t* ui, int32_t key) {
    if (app.alt && uic_is_keyboard_shortcut(ui, key)) {
//      traceln("key: 0x%02X shortcut: %d", key, uic_is_keyboard_shortcut(ui, key));
        uic_checkbox_flip((uic_checkbox_t*)ui);
    }
}

static void  uic_checkbox_mouse(view_t* ui, int32_t message, int32_t flags) {
    assert(ui->tag == uic_tag_checkbox);
    (void)flags; // unused
    assert(!ui->hidden && !ui->disabled);
    if (message == messages.left_button_pressed ||
        message == messages.right_button_pressed) {
        int32_t x = app.mouse.x - ui->x;
        int32_t y = app.mouse.y - ui->y;
        if (0 <= x && x < ui->w && 0 <= y && y < ui->h) {
            app.focus = ui;
            uic_checkbox_flip((uic_checkbox_t*)ui);
        }
    }
}

void _uic_checkbox_init_(view_t* ui) {
    assert(ui->tag == uic_tag_checkbox);
    uic_init(ui);
    uic_set_label(ui, ui->text);
    ui->mouse       =  uic_checkbox_mouse;
    ui->measure     = uic_checkbox_measure;
    ui->paint       = uic_checkbox_paint;
    ui->character   = uic_checkbox_character;
    ui->key_pressed = uic_checkbox_key_pressed;
    ui->localize(ui);
    ui->color = colors.btn_text;
}

void uic_checkbox_init(uic_checkbox_t* c, const char* label, double ems,
       void (*cb)( uic_checkbox_t* b)) {
    static_assert(offsetof( uic_checkbox_t, ui) == 0, "offsetof(.ui)");
    uic_init(&c->ui);
    strprintf(c->ui.text, "%s", label);
    c->ui.width = ems;
    c->cb = cb;
    c->ui.tag = uic_tag_checkbox;
    _uic_checkbox_init_(&c->ui);
}

// slider control

static void uic_slider_measure(view_t* ui) {
    assert(ui->tag == uic_tag_slider);
    uic_measure(ui);
    uic_slider_t* r = (uic_slider_t*)ui;
    assert(r->inc.ui.w == r->dec.ui.w && r->inc.ui.h == r->dec.ui.h);
    const int32_t em = ui->em.x;
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    const int32_t w = (int)(ui->width * ui->em.x);
    r->tm = gdi.measure_text(f, uic_nsl(ui), r->vmax);
    if (w > r->tm.x) { r->tm.x = w; }
    ui->w = r->dec.ui.w + r->tm.x + r->inc.ui.w + em * 2;
    ui->h = r->inc.ui.h;
}

static void uic_slider_layout(view_t* ui) {
    assert(ui->tag == uic_tag_slider);
    uic_slider_t* r = (uic_slider_t*)ui;
    assert(r->inc.ui.w == r->dec.ui.w && r->inc.ui.h == r->dec.ui.h);
    const int32_t em = ui->em.x;
    r->dec.ui.x = ui->x;
    r->dec.ui.y = ui->y;
    r->inc.ui.x = ui->x + r->dec.ui.w + r->tm.x + em * 2;
    r->inc.ui.y = ui->y;
}

static void uic_slider_paint(view_t* ui) {
    assert(ui->tag == uic_tag_slider);
    uic_slider_t* r = (uic_slider_t*)ui;
    gdi.push(ui->x, ui->y);
    gdi.set_clip(ui->x, ui->y, ui->w, ui->h);
    const int32_t em = ui->em.x;
    const int32_t em2  = max(1, em / 2);
    const int32_t em4  = max(1, em / 8);
    const int32_t em8  = max(1, em / 8);
    const int32_t em16 = max(1, em / 16);
    gdi.set_brush(gdi.brush_color);
    pen_t pen_grey45 = gdi.create_pen(colors.dkgray3, em16);
    gdi.set_pen(pen_grey45);
    gdi.set_brush_color(colors.dkgray3);
    const int32_t x = ui->x + r->dec.ui.w + em2;
    const int32_t y = ui->y;
    const int32_t w = r->tm.x + em;
    const int32_t h = ui->h;
    gdi.rounded(x - em8, y, w + em4, h, em4, em4);
    gdi.gradient(x, y, w, h / 2,
        colors.dkgray3, colors.btn_gradient_darker, true);
    gdi.gradient(x, y + h / 2, w, ui->h - h / 2,
        colors.btn_gradient_darker, colors.dkgray3, true);
    gdi.set_brush_color(colors.dkgreen);
    pen_t pen_grey30 = gdi.create_pen(colors.dkgray1, em16);
    gdi.set_pen(pen_grey30);
    const double range = (double)r->vmax - (double)r->vmin;
    double vw = (double)(r->tm.x + em) * (r->value - r->vmin) / range;
    gdi.rect(x, ui->y, (int32_t)(vw + 0.5), ui->h);
    gdi.x += r->dec.ui.w + em;
    const char* format = app.nls(ui->text);
    gdi.text(format, r->value);
    gdi.set_clip(0, 0, 0, 0);
    gdi.delete_pen(pen_grey30);
    gdi.delete_pen(pen_grey45);
    gdi.pop();
}

static void uic_slider_mouse(view_t* ui, int32_t message, int32_t f) {
    if (!ui->hidden && !ui->disabled) {
        assert(ui->tag == uic_tag_slider);
        uic_slider_t* r = (uic_slider_t*)ui;
        assert(!ui->hidden && !ui->disabled);
        bool drag = message == messages.mouse_move &&
            (f & (mouse_flags.left_button|mouse_flags.right_button)) != 0;
        if (message == messages.left_button_pressed ||
            message == messages.right_button_pressed || drag) {
            const int32_t x = app.mouse.x - ui->x - r->dec.ui.w;
            const int32_t y = app.mouse.y - ui->y;
            const int32_t x0 = ui->em.x / 2;
            const int32_t x1 = r->tm.x + ui->em.x;
            if (x0 <= x && x < x1 && 0 <= y && y < ui->h) {
                app.focus = ui;
                const double range = (double)r->vmax - (double)r->vmin;
                double v = ((double)x - x0) * range / (double)(x1 - x0 - 1);
                int32_t vw = (int32_t)(v + r->vmin + 0.5);
                r->value = min(max(vw, r->vmin), r->vmax);
                if (r->cb != null) { r->cb(r); }
                ui->invalidate(ui);
            }
        }
    }
}

static void uic_slider_inc_dec_value(uic_slider_t* r, int32_t sign, int32_t mul) {
    if (!r->ui.hidden && !r->ui.disabled) {
        // full 0x80000000..0x7FFFFFFF (-2147483648..2147483647) range
        int32_t v = r->value;
        if (v > r->vmin && sign < 0) {
            mul = min(v - r->vmin, mul);
            v += mul * sign;
        } else if (v < r->vmax && sign > 0) {
            mul = min(r->vmax - v, mul);
            v += mul * sign;
        }
        if (r->value != v) {
            r->value = v;
            if (r->cb != null) { r->cb(r); }
            r->ui.invalidate(&r->ui);
        }
    }
}

static void uic_slider_inc_dec(uic_button_t* b) {
    uic_slider_t* r = (uic_slider_t*)b->ui.parent;
    if (!r->ui.hidden && !r->ui.disabled) {
        int32_t sign = b == &r->inc ? +1 : -1;
        int32_t mul = app.shift && app.ctrl ? 1000 :
            app.shift ? 100 : app.ctrl ? 10 : 1;
        uic_slider_inc_dec_value(r, sign, mul);
    }
}

static void uic_slider_every_100ms(view_t* ui) { // 100ms
    assert(ui->tag == uic_tag_slider);
    uic_slider_t* r = (uic_slider_t*)ui;
    if (r->ui.hidden || r->ui.disabled) {
        r->time = 0;
    } else if (!r->dec.ui.armed && !r->inc.ui.armed) {
        r->time = 0;
    } else {
        if (r->time == 0) {
            r->time = app.now;
        } else if (app.now - r->time > 1.0) {
            const int32_t sign = r->dec.ui.armed ? -1 : +1;
            int32_t s = (int)(app.now - r->time + 0.5);
            int32_t mul = s >= 1 ? 1 << (s - 1) : 1;
            const int64_t range = (int64_t)r->vmax - r->vmin;
            if (mul > range / 8) { mul = (int32_t)(range / 8); }
            uic_slider_inc_dec_value(r, sign, max(mul, 1));
        }
    }
}

void _uic_slider_init_(view_t* ui) {
    assert(ui->tag == uic_tag_slider);
    uic_init(ui);
    uic_set_label(ui, ui->text);
    ui->mouse        = uic_slider_mouse;
    ui->measure      = uic_slider_measure;
    ui->layout       = uic_slider_layout;
    ui->paint        = uic_slider_paint;
    ui->every_100ms = uic_slider_every_100ms;
    uic_slider_t* r = (uic_slider_t*)ui;
    r->buttons[0] = &r->dec.ui;
    r->buttons[1] = &r->inc.ui;
    r->buttons[2] = null;
    r->ui.children = r->buttons;
    // Heavy Minus Sign
    uic_button_init(&r->dec, "\xE2\x9E\x96", 0, uic_slider_inc_dec);
    // Heavy Plus Sign
    uic_button_init(&r->inc, "\xE2\x9E\x95", 0, uic_slider_inc_dec);
    static const char* accel =
        "Accelerate by holding Ctrl x10 Shift x100 and Ctrl+Shift x1000";
    strprintf(r->inc.ui.tip, "%s", accel);
    strprintf(r->dec.ui.tip, "%s", accel);
    r->dec.ui.parent = &r->ui;
    r->inc.ui.parent = &r->ui;
    r->ui.localize(&r->ui);
}

void uic_slider_init(uic_slider_t* r, const char* label, double ems,
        int32_t vmin, int32_t vmax, void (*cb)(uic_slider_t* r)) {
    static_assert(offsetof(uic_slider_t, ui) == 0, "offsetof(.ui)");
    assert(ems >= 3.0, "allow 1em for each of [-] and [+] buttons");
    r->ui.tag = uic_tag_slider;
    strprintf(r->ui.text, "%s", label);
    r->cb = cb;
    r->ui.width = ems;
    r->vmin = vmin;
    r->vmax = vmax;
    r->value = vmin;
    _uic_slider_init_(&r->ui);
}

// message box

static void uic_messagebox_button(uic_button_t* b) {
    uic_messagebox_t* mx = (uic_messagebox_t*)b->ui.parent;
    assert(mx->ui.tag == uic_tag_messagebox);
    mx->option = -1;
    for (int32_t i = 0; i < countof(mx->button) && mx->option < 0; i++) {
        if (b == &mx->button[i]) {
            mx->option = i;
            mx->cb(mx, i);
        }
    }
    app.show_toast(null, 0);
}

static void uic_messagebox_measure(view_t* ui) {
    uic_messagebox_t* mx = (uic_messagebox_t*)ui;
    assert(ui->tag == uic_tag_messagebox);
    int32_t n = 0;
    for (view_t** c = ui->children; c != null && *c != null; c++) { n++; }
    n--; // number of buttons
    mx->text.ui.measure(&mx->text.ui);
    const int32_t em_x = mx->text.ui.em.x;
    const int32_t em_y = mx->text.ui.em.y;
    const int32_t tw = mx->text.ui.w;
    const int32_t th = mx->text.ui.h;
    if (n > 0) {
        int32_t bw = 0;
        for (int32_t i = 0; i < n; i++) {
            bw += mx->button[i].ui.w;
        }
        ui->w = max(tw, bw + em_x * 2);
        ui->h = th + mx->button[0].ui.h + em_y + em_y / 2;
    } else {
        ui->h = th + em_y / 2;
        ui->w = tw;
    }
}

static void uic_messagebox_layout(view_t* ui) {
    uic_messagebox_t* mx = (uic_messagebox_t*)ui;
    assert(ui->tag == uic_tag_messagebox);
//  traceln("ui.y=%d", ui->y);
    int32_t n = 0;
    for (view_t** c = ui->children; c != null && *c != null; c++) { n++; }
    n--; // number of buttons
    const int32_t em_y = mx->text.ui.em.y;
    mx->text.ui.x = ui->x;
    mx->text.ui.y = ui->y + em_y * 2 / 3;
    const int32_t tw = mx->text.ui.w;
    const int32_t th = mx->text.ui.h;
    if (n > 0) {
        int32_t bw = 0;
        for (int32_t i = 0; i < n; i++) {
            bw += mx->button[i].ui.w;
        }
        // center text:
        mx->text.ui.x = ui->x + (ui->w - tw) / 2;
        // spacing between buttons:
        int32_t sp = (ui->w - bw) / (n + 1);
        int32_t x = sp;
        for (int32_t i = 0; i < n; i++) {
            mx->button[i].ui.x = ui->x + x;
            mx->button[i].ui.y = ui->y + th + em_y * 3 / 2;
            x += mx->button[i].ui.w + sp;
        }
    }
}

void uic_messagebox_init_(view_t* ui) {
    assert(ui->tag == uic_tag_messagebox);
    uic_messagebox_t* mx = (uic_messagebox_t*)ui;
    uic_init(ui);
    ui->measure = uic_messagebox_measure;
    ui->layout = uic_messagebox_layout;
    mx->ui.font = &app.fonts.H3;
    const char** opts = mx->opts;
    int32_t n = 0;
    while (opts[n] != null && n < countof(mx->button) - 1) {
        uic_button_init(&mx->button[n], opts[n], 6.0, uic_messagebox_button);
        mx->button[n].ui.parent = &mx->ui;
        n++;
    }
    assert(n <= countof(mx->button));
    if (n > countof(mx->button)) { n = countof(mx->button); }
    mx->children[0] = &mx->text.ui;
    for (int32_t i = 0; i < n; i++) {
        mx->children[i + 1] = &mx->button[i].ui;
        mx->children[i + 1]->font = mx->ui.font;
        mx->button[i].ui.localize(&mx->button[i].ui);
    }
    mx->ui.children = mx->children;
    uic_text_init_ml(&mx->text, 0.0, "%s", mx->ui.text);
    mx->text.ui.font = mx->ui.font;
    mx->text.ui.localize(&mx->text.ui);
    mx->ui.text[0] = 0;
    mx->option = -1;
}

void uic_messagebox_init(uic_messagebox_t* mx, const char* opts[],
        void (*cb)(uic_messagebox_t* m, int32_t option),
        const char* format, ...) {
    mx->ui.tag = uic_tag_messagebox;
    mx->ui.measure = uic_messagebox_measure;
    mx->ui.layout = uic_messagebox_layout;
    mx->opts = opts;
    mx->cb = cb;
    va_list vl;
    va_start(vl, format);
    str.vformat(mx->ui.text, countof(mx->ui.text), format, vl);
    uic_text_init_ml(&mx->text, 0.0, mx->ui.text);
    va_end(vl);
    uic_messagebox_init_(&mx->ui);
}

// measurements:

static void measurements_center(view_t* ui) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    assert(ui->children[1] == null, "must be single child");
    view_t* c = ui->children[0]; // even if hidden measure it
    c->w = ui->w;
    c->h = ui->h;
}

static void measurements_horizontal(view_t* ui, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    ui->w = 0;
    ui->h = 0;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
        if (!u->hidden) {
            if (seen) { ui->w += gap; }
            ui->w += u->w;
            ui->h = max(ui->h, u->h);
            seen = true;
        }
        c++;
    }
}

static void measurements_vertical(view_t* ui, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    ui->h = 0;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
        if (!u->hidden) {
            if (seen) { ui->h += gap; }
            ui->h += u->h;
            ui->w = max(ui->w, u->w);
            seen = true;
        }
        c++;
    }
}

static void measurements_grid(view_t* ui, int32_t gap_h, int32_t gap_v) {
    int32_t cols = 0;
    for (view_t** row = ui->children; *row != null; row++) {
        view_t* r = *row;
        int32_t n = 0;
        for (view_t** col = r->children; *col != null; col++) { n++; }
        if (cols == 0) { cols = n; }
        assert(n > 0 && cols == n);
    }
    int32_t* mxw = (int32_t*)alloca(cols * sizeof(int32_t));
    memset(mxw, 0, cols * sizeof(int32_t));
    for (view_t** row = ui->children; *row != null; row++) {
        if (!(*row)->hidden) {
            (*row)->h = 0;
            (*row)->baseline = 0;
            int32_t i = 0;
            for (view_t** col = (*row)->children; *col != null; col++) {
                if (!(*col)->hidden) {
                    mxw[i] = max(mxw[i], (*col)->w);
                    (*row)->h = max((*row)->h, (*col)->h);
//                  traceln("[%d] row.baseline: %d col.baseline: %d ", i, (*row)->baseline, (*col)->baseline);
                    (*row)->baseline = max((*row)->baseline, (*col)->baseline);
                }
                i++;
            }
        }
    }
    ui->h = 0;
    ui->w = 0;
    int32_t rows_seen = 0; // number of visible rows so far
    for (view_t** row = ui->children; *row != null; row++) {
        view_t* r = *row;
        if (!r->hidden) {
            r->w = 0;
            int32_t i = 0;
            int32_t cols_seen = 0; // number of visible columns so far
            for (view_t** col = r->children; *col != null; col++) {
                view_t* c = *col;
                if (!c->hidden) {
                    c->h = r->h; // all cells are same height
                    if (c->tag == uic_tag_text) { // lineup text baselines
                        uic_text_t* t = (uic_text_t*)c;
                        t->dy = r->baseline - c->baseline;
                    }
                    c->w = mxw[i++];
                    r->w += c->w;
                    if (cols_seen > 0) { r->w += gap_h; }
                    ui->w = max(ui->w, r->w);
                    cols_seen++;
                }
            }
            ui->h += r->h;
            if (rows_seen > 0) { ui->h += gap_v; }
            rows_seen++;
        }
    }
}

measurements_if measurements = {
    .center     = measurements_center,
    .horizontal = measurements_horizontal,
    .vertical   = measurements_vertical,
    .grid       = measurements_grid,
};

// layouts

static void layouts_center(view_t* ui) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    assert(ui->children[1] == null, "must be single child");
    view_t* c = ui->children[0];
    c->x = (ui->w - c->w) / 2;
    c->y = (ui->h - c->h) / 2;
}

static void layouts_horizontal(view_t* ui, int32_t x, int32_t y, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
        if (!u->hidden) {
            if (seen) { x += gap; }
            u->x = x;
            u->y = y;
            x += u->w;
            seen = true;
        }
        c++;
    }
}

static void layouts_vertical(view_t* ui, int32_t x, int32_t y, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
        if (!u->hidden) {
            if (seen) { y += gap; }
            u->x = x;
            u->y = y;
            y += u->h;
            seen = true;
        }
        c++;
    }
}

static void layouts_grid(view_t* ui, int32_t gap_h, int32_t gap_v) {
    assert(ui->children != null, "layout_grid() with no children?");
    int32_t x = ui->x;
    int32_t y = ui->y;
    bool row_seen = false;
    for (view_t** row = ui->children; *row != null; row++) {
        if (!(*row)->hidden) {
            if (row_seen) { y += gap_v; }
            int32_t xc = x;
            bool col_seen = false;
            for (view_t** col = (*row)->children; *col != null; col++) {
                if (!(*col)->hidden) {
                    if (col_seen) { xc += gap_h; }
                    (*col)->x = xc;
                    (*col)->y = y;
                    xc += (*col)->w;
                    col_seen = true;
                }
            }
            y += (*row)->h;
            row_seen = true;
        }
    }
}

layouts_if layouts = {
    .center     = layouts_center,
    .horizontal = layouts_horizontal,
    .vertical   = layouts_vertical,
    .grid       = layouts_grid
};

end_c

// app implementation

begin_c

#define WM_ANIMATE  (WM_APP + 0x7FFF)
#define WM_OPENNING (WM_APP + 0x7FFE)
#define WM_CLOSING  (WM_APP + 0x7FFD)
#define WM_TAP      (WM_APP + 0x7FFC)
#define WM_DTAP     (WM_APP + 0x7FFB) // double tap (aka click)
#define WM_PRESS    (WM_APP + 0x7FFA)

#define LONG_PRESS_MSEC (250)

#define window() ((HWND)app.window)
#define canvas() ((HDC)app.canvas)

messages_t messages = {
    .character             = WM_CHAR,
    .key_pressed           = WM_KEYDOWN,
    .key_released          = WM_KEYUP,
    .left_button_pressed   = WM_LBUTTONDOWN,
    .left_button_released  = WM_LBUTTONUP,
    .right_button_pressed  = WM_RBUTTONDOWN,
    .right_button_released = WM_RBUTTONUP,
    .mouse_move            = WM_MOUSEMOVE,
    .left_double_click     = WM_LBUTTONDBLCLK,
    .right_double_click    = WM_RBUTTONDBLCLK,
    .tap                   = WM_TAP,
    .dtap                  = WM_DTAP,
    .press                 = WM_PRESS
};

mouse_flags_t mouse_flags = {
    .left_button = MK_LBUTTON,
    .right_button = MK_RBUTTON,
};

virtual_keys_t virtual_keys = {
    .up     = VK_UP,
    .down   = VK_DOWN,
    .left   = VK_LEFT,
    .right  = VK_RIGHT,
    .home   = VK_HOME,
    .end    = VK_END,
    .pageup = VK_PRIOR,
    .pagedw = VK_NEXT,
    .insert = VK_INSERT,
    .del    = VK_DELETE,
    .back   = VK_BACK,
    .escape = VK_ESCAPE,
    .enter  = VK_RETURN,
    .minus  = VK_OEM_MINUS,
    .plus   = VK_OEM_PLUS,
    .f1     = VK_F1,
    .f2     = VK_F2,
    .f3     = VK_F3,
    .f4     = VK_F4,
    .f5     = VK_F5,
    .f6     = VK_F6,
    .f7     = VK_F7,
    .f8     = VK_F8,
    .f9     = VK_F9,
    .f10    = VK_F10,
    .f11    = VK_F11,
    .f12    = VK_F12,
    .f13    = VK_F13,
    .f14    = VK_F14,
    .f15    = VK_F15,
    .f16    = VK_F16,
    .f17    = VK_F17,
    .f18    = VK_F18,
    .f19    = VK_F19,
    .f20    = VK_F20,
    .f21    = VK_F21,
    .f22    = VK_F22,
    .f23    = VK_F23,
    .f24    = VK_F24,
};


// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow

window_visibility_t window_visibility = {
    .hide      = SW_HIDE,
    .normal    = SW_SHOWNORMAL,
    .minimize  = SW_SHOWMINIMIZED,
    .maximize  = SW_SHOWMAXIMIZED,
    .normal_na = SW_SHOWNOACTIVATE,
    .show      = SW_SHOW,
    .min_next  = SW_MINIMIZE,
    .min_na    = SW_SHOWMINNOACTIVE,
    .show_na   = SW_SHOWNA,
    .restore   = SW_RESTORE,
    .defau1t   = SW_SHOWDEFAULT,
    .force_min = SW_FORCEMINIMIZE
};

typedef LPARAM lparam_t;
typedef WPARAM wparam_t;

static NONCLIENTMETRICSW app_ncm = { sizeof(NONCLIENTMETRICSW) };
static MONITORINFO app_mi = {sizeof(MONITORINFO)};

static HANDLE app_event_quit;
static HANDLE app_event_invalidate;

static uintptr_t app_timer_1s_id;
static uintptr_t app_timer_100ms_id;

static bool app_layout_dirty; // call layout() before paint

typedef void (*app_animate_function_t)(int32_t step);

static struct {
    app_animate_function_t f;
    int32_t count;
    int32_t step;
    tm_t timer;
} app_animate;

// Animation timer is Windows minimum of 10ms, but in reality the timer
// messages are far from isochronous and more likely to arrive at 16 or
// 32ms intervals and can be delayed.

static void app_on_every_message(view_t* ui);

// https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
// https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-keydown

static void app_alt_ctrl_shift(bool down, int32_t key) {
    if (key == VK_MENU)    { app.alt   = down; }
    if (key == VK_CONTROL) { app.ctrl  = down; }
    if (key == VK_SHIFT)   { app.shift = down; }
}

static inline ui_point_t app_point2ui(const POINT* p) {
    ui_point_t u = { p->x, p->y };
    return u;
}

static inline POINT app_ui2point(const ui_point_t* u) {
    POINT p = { u->x, u->y };
    return p;
}

static ui_rect_t app_rect2ui(const RECT* r) {
    ui_rect_t u = { r->left, r->top, r->right - r->left, r->bottom - r->top };
    return u;
}

static RECT app_ui2rect(const ui_rect_t* u) {
    RECT r = { u->x, u->y, u->x + u->w, u->y + u->h };
    return r;
}

static void app_update_ncm(int32_t dpi) {
    // Only UTF-16 version supported SystemParametersInfoForDpi
    fatal_if_false(SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,
        sizeof(app_ncm), &app_ncm, 0, dpi));
}

static void app_update_monitor_dpi(HMONITOR monitor, dpi_t* dpi) {
    for (int32_t mtd = MDT_EFFECTIVE_DPI; mtd <= MDT_RAW_DPI; mtd++) {
        uint32_t dpi_x = 0;
        uint32_t dpi_y = 0;
        // GetDpiForMonitor() may return ERROR_GEN_FAILURE 0x8007001F when
        // system wakes up from sleep:
        // ""A device attached to the system is not functioning."
        // docs say:
        // "May be used to indicate that the device has stopped responding
        // or a general failure has occurred on the device.
        // The device may need to be manually reset."
        int32_t r = GetDpiForMonitor(monitor, (MONITOR_DPI_TYPE)mtd, &dpi_x, &dpi_y);
        if (r != 0) {
            threads.sleep_for(1.0 / 32); // and retry:
            r = GetDpiForMonitor(monitor, (MONITOR_DPI_TYPE)mtd, &dpi_x, &dpi_y);
        }
        if (r == 0) {
//          const char* names[] = {"EFFECTIVE_DPI", "ANGULAR_DPI","RAW_DPI"};
//          traceln("%s %d %d", names[mtd], dpi_x, dpi_y);
            // EFFECTIVE_DPI 168 168 (with regard of user scaling)
            // ANGULAR_DPI 247 248 (diagonal)
            // RAW_DPI 283 284 (horizontal, vertical)
            switch (mtd) {
                case MDT_EFFECTIVE_DPI:
                    dpi->monitor_effective = max(dpi_x, dpi_y); break;
                case MDT_ANGULAR_DPI:
                    dpi->monitor_angular = max(dpi_x, dpi_y); break;
                case MDT_RAW_DPI:
                    dpi->monitor_raw = max(dpi_x, dpi_y); break;
                default: assert(false);
            }
        }
    }
}

#ifndef QUICK_DEBUG

static void app_dump_dpi(void) {
    traceln("app.dpi.monitor_effective: %d", app.dpi.monitor_effective  );
    traceln("app.dpi.monitor_angular  : %d", app.dpi.monitor_angular    );
    traceln("app.dpi.monitor_raw      : %d", app.dpi.monitor_raw        );
    traceln("app.dpi.window           : %d", app.dpi.window             );
    traceln("app.dpi.system           : %d", app.dpi.system             );
    traceln("app.dpi.process          : %d", app.dpi.process            );

    traceln("app.mrc      : %d,%d %dx%d", app.mrc.x, app.mrc.y, app.mrc.w, app.mrc.h);
    traceln("app.wrc      : %d,%d %dx%d", app.wrc.x, app.wrc.y, app.wrc.w, app.wrc.h);
    traceln("app.crc      : %d,%d %dx%d", app.crc.x, app.crc.y, app.crc.w, app.crc.h);
    traceln("app.work_area: %d,%d %dx%d", app.work_area.x, app.work_area.y, app.work_area.w, app.work_area.h);

    int32_t mxt_x = GetSystemMetrics(SM_CXMAXTRACK);
    int32_t mxt_y = GetSystemMetrics(SM_CYMAXTRACK);
    traceln("MAXTRACK: %d, %d", mxt_x, mxt_y);
    int32_t scr_x = GetSystemMetrics(SM_CXSCREEN);
    int32_t scr_y = GetSystemMetrics(SM_CYSCREEN);
    float monitor_x = scr_x / (float)app.dpi.monitor_raw;
    float monitor_y = scr_y / (float)app.dpi.monitor_raw;
    traceln("SCREEN: %d, %d %.1fx%.1f\"", scr_x, scr_y, monitor_x, monitor_y);
}

#endif

static bool app_update_mi(const ui_rect_t* r, uint32_t flags) {
    RECT rc = app_ui2rect(r);
    HMONITOR monitor = MonitorFromRect(&rc, flags);
//  TODO: moving between monitors with different DPIs
//  HMONITOR mw = MonitorFromWindow(window(), flags);
    if (monitor != null) {
        app_update_monitor_dpi(monitor, &app.dpi);
        fatal_if_false(GetMonitorInfoA(monitor, &app_mi));
        app.work_area = app_rect2ui(&app_mi.rcWork);
        app.mrc = app_rect2ui(&app_mi.rcMonitor);
//      app_dump_dpi();
    }
    return monitor != null;
}

static void app_update_crc(void) {
    RECT rc = {0};
    fatal_if_false(GetClientRect(window(), &rc));
    app.crc = app_rect2ui(&rc);
    app.width = app.crc.w;
    app.height = app.crc.h;
}

static void app_dispose_fonts(void) {
    fatal_if_false(DeleteFont(app.fonts.regular));
    fatal_if_false(DeleteFont(app.fonts.H1));
    fatal_if_false(DeleteFont(app.fonts.H2));
    fatal_if_false(DeleteFont(app.fonts.H3));
    fatal_if_false(DeleteFont(app.fonts.mono));
}

static void app_init_fonts(int32_t dpi) {
    app_update_ncm(dpi);
    if (app.fonts.regular != null) { app_dispose_fonts(); }
    LOGFONTW lf = app_ncm.lfMessageFont;
    // lf.lfQuality is CLEARTYPE_QUALITY which looks bad on 4K monitors
    // Windows UI uses PROOF_QUALITY which is aliased w/o ClearType rainbows
    lf.lfQuality = PROOF_QUALITY;
    app.fonts.regular = (font_t)CreateFontIndirectW(&lf);
    not_null(app.fonts.regular);
    const double fh = app_ncm.lfMessageFont.lfHeight;
//  traceln("lfHeight=%.1f", fh);
    assert(fh != 0);
    lf.lfWeight = FW_SEMIBOLD;
    lf.lfHeight = (int)(fh * 1.75);
    app.fonts.H1 = (font_t)CreateFontIndirectW(&lf);
    lf.lfWeight = FW_SEMIBOLD;
    lf.lfHeight = (int)(fh * 1.4);
    app.fonts.H2 = (font_t)CreateFontIndirectW(&lf);
    lf.lfWeight = FW_SEMIBOLD;
    lf.lfHeight = (int)(fh * 1.15);
    app.fonts.H3 = (font_t)CreateFontIndirectW(&lf);
    lf = app_ncm.lfMessageFont;
    lf.lfPitchAndFamily = FIXED_PITCH;
    #define monospaced "Cascadia Code"
    wcscpy(lf.lfFaceName, L"Cascadia Code");
    app.fonts.mono = (font_t)CreateFontIndirectW(&lf);
    app.cursor_arrow = (cursor_t)LoadCursorA(null, IDC_ARROW);
    app.cursor_wait  = (cursor_t)LoadCursorA(null, IDC_WAIT);
    app.cursor_ibeam = (cursor_t)LoadCursorA(null, IDC_IBEAM);
    app.cursor = app.cursor_arrow;
}

static void app_data_save(const char* name, const void* data, int32_t bytes) {
    config.save(app.class_name, name, data, bytes);
}

static int32_t app_data_size(const char* name) {
    return config.size(app.class_name, name);
}

static int32_t app_data_load(const char* name, void* data, int32_t bytes) {
    return config.load(app.class_name, name, data, bytes);
}

typedef begin_packed struct app_wiw_s { // "where is window"
    // coordinates in pixels relative (0,0) top left corner
    // of primary monitor from GetWindowPlacement
    int32_t    bytes;
    ui_rect_t  placement;
    ui_rect_t  mrc;          // monitor rectangle
    ui_rect_t  work_area;    // monitor work area (mrc sans taskbar etc)
    ui_point_t min_position; // not used (-1, -1)
    ui_point_t max_position; // not used (-1, -1)
    ui_point_t max_track;    // maximum window size (spawning all monitors)
    ui_rect_t  space;        // surrounding rect x,y,w,h of all monitors
    int32_t    dpi;          // of the monitor on which window (x,y) is located
    int32_t    flags;        // WPF_SETMINPOSITION. WPF_RESTORETOMAXIMIZED
    int32_t    show;         // show command
} end_packed app_wiw_t;

static BOOL CALLBACK app_monitor_enum_proc(HMONITOR monitor,
        HDC unused(hdc), RECT* rc1, LPARAM that) {
    app_wiw_t* wiw = (app_wiw_t*)(uintptr_t)that;
    ui_rect_t* space = &wiw->space;
    MONITORINFOEX mi = { .cbSize = sizeof(MONITORINFOEX) };
    fatal_if_false(GetMonitorInfoA(monitor, (MONITORINFO*)&mi));
    space->x = min(space->x, min(mi.rcMonitor.left, mi.rcMonitor.right));
    space->y = min(space->y, min(mi.rcMonitor.top,  mi.rcMonitor.bottom));
    space->w = max(space->w, max(mi.rcMonitor.left, mi.rcMonitor.right));
    space->h = max(space->h, max(mi.rcMonitor.top,  mi.rcMonitor.bottom));
    return true; // keep going
}

static void app_enum_monitors(app_wiw_t* wiw) {
    EnumDisplayMonitors(null, null, app_monitor_enum_proc, (uintptr_t)wiw);
    // because app_monitor_enum_proc() puts max into w,h:
    wiw->space.w -= wiw->space.x;
    wiw->space.h -= wiw->space.y;
}

static void app_save_window_pos(window_t wnd, const char* name, bool dump) {
    RECT wr = {0};
    fatal_if_false(GetWindowRect((HWND)wnd, &wr));
    ui_rect_t wrc = app_rect2ui(&wr);
    app_update_mi(&wrc, MONITOR_DEFAULTTONEAREST);
    WINDOWPLACEMENT wpl = { .length = sizeof(wpl) };
    fatal_if_false(GetWindowPlacement((HWND)wnd, &wpl));
    // note the replacement of wpl.rcNormalPosition with wrc:
    app_wiw_t wiw = { // where is window
        .bytes = sizeof(app_wiw_t),
        .placement = wrc,
        .mrc = app.mrc,
        .work_area = app.work_area,
        .min_position = app_point2ui(&wpl.ptMinPosition),
        .max_position = app_point2ui(&wpl.ptMaxPosition),
        .max_track = {
            .x = GetSystemMetrics(SM_CXMAXTRACK),
            .y = GetSystemMetrics(SM_CYMAXTRACK)
        },
        .dpi = app.dpi.monitor_raw,
        .flags = wpl.flags,
        .show = wpl.showCmd
    };
    app_enum_monitors(&wiw);
    if (dump) {
        traceln("wiw.space: %d,%d %dx%d",
              wiw.space.x, wiw.space.y, wiw.space.w, wiw.space.h);
        traceln("MAXTRACK: %d, %d", wiw.max_track.x, wiw.max_track.y);
        traceln("wpl.rcNormalPosition: %d,%d %dx%d",
            wpl.rcNormalPosition.left, wpl.rcNormalPosition.top,
            wpl.rcNormalPosition.right - wpl.rcNormalPosition.left,
            wpl.rcNormalPosition.bottom - wpl.rcNormalPosition.top);
        traceln("wpl.ptMinPosition: %d,%d",
            wpl.ptMinPosition.x, wpl.ptMinPosition.y);
        traceln("wpl.ptMaxPosition: %d,%d",
            wpl.ptMaxPosition.x, wpl.ptMaxPosition.y);
        traceln("wpl.showCmd: %d", wpl.showCmd);
        // WPF_SETMINPOSITION. WPF_RESTORETOMAXIMIZED WPF_ASYNCWINDOWPLACEMENT
        traceln("wpl.flags: %d", wpl.flags);
    }
//  traceln("%d,%d %dx%d show=%d", wiw.placement.x, wiw.placement.y,
//      wiw.placement.w, wiw.placement.h, wiw.show);
    config.save(app.class_name, name, &wiw, sizeof(wiw));
    app_update_mi(&app.wrc, MONITOR_DEFAULTTONEAREST);
}

static void app_save_console_pos(void) {
    HWND cw = GetConsoleWindow();
    if (cw != null) {
        app_save_window_pos((window_t)cw, "wic", false);
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFOEX info = { sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) };
        int32_t r = GetConsoleScreenBufferInfoEx(console, &info) ? 0 : runtime.err();
        if (r != 0) {
            traceln("GetConsoleScreenBufferInfoEx() %s", str.error(r));
        } else {
            config.save(app.class_name, "console_screen_buffer_infoex",
                            &info, (int)sizeof(info));
//          traceln("info: %dx%d", info.dwSize.X, info.dwSize.Y);
//          traceln("%d,%d %dx%d", info.srWindow.Left, info.srWindow.Top,
//              info.srWindow.Right - info.srWindow.Left,
//              info.srWindow.Bottom - info.srWindow.Top);
        }
    }
    int32_t v = app.is_console_visible();
    // "icv" "is console visible"
    config.save(app.class_name, "icv", &v, (int)sizeof(v));
}

static bool app_point_in_rect(const ui_point_t* p, const ui_rect_t* r) {
    return r->x <= p->x && p->x < r->x + r->w &&
           r->y <= p->y && p->y < r->y + r->h;
}

static bool app_intersect_rect(ui_rect_t* i, const ui_rect_t* r0, const ui_rect_t* r1) {
    ui_rect_t r = {0};
    r.x = max(r0->x, r1->x);  // Maximum of left edges
    r.y = max(r0->y, r1->y);  // Maximum of top edges
    r.w = min(r0->x + r0->w, r1->x + r1->w) - r.x;  // Width of overlap
    r.h = min(r0->y + r0->h, r1->y + r1->h) - r.y;  // Height of overlap
    bool b = r.w > 0 && r.h > 0;
    if (!b) {
        r.w = 0;
        r.h = 0;
    }
    if (i != null) { *i = r; }
    return b;
}

static bool app_is_fully_inside(const ui_rect_t* inner, const ui_rect_t* outer) {
    return
        outer->x <= inner->x && inner->x + inner->w <= outer->x + outer->w &&
        outer->y <= inner->y && inner->y + inner->h <= outer->y + outer->h;
}

static void app_bring_window_inside_monitor(const ui_rect_t* mrc, ui_rect_t* wrc) {
    assert(mrc->w > 0 && mrc->h > 0);
    // Check if window rect is inside monitor rect
    if (!app_is_fully_inside(wrc, mrc)) {
        // Move window into monitor rect
        wrc->x = max(mrc->x, min(mrc->x + mrc->w - wrc->w, wrc->x));
        wrc->y = max(mrc->y, min(mrc->y + mrc->h - wrc->h, wrc->y));
        // Adjust size to fit into monitor rect
        wrc->w = min(wrc->w, mrc->w);
        wrc->h = min(wrc->h, mrc->h);
    }
}

static bool app_load_window_pos(ui_rect_t* rect, int32_t *visibility) {
    app_wiw_t wiw = {0}; // where is window
    bool loaded = config.load(app.class_name, "wiw", &wiw, sizeof(wiw)) ==
                                sizeof(wiw);
    if (loaded) {
        #ifdef QUICK_DEBUG
            traceln("wiw.placement: %d,%d %dx%d", wiw.placement.x, wiw.placement.y,
                wiw.placement.w, wiw.placement.h);
            traceln("wiw.mrc: %d,%d %dx%d", wiw.mrc.x, wiw.mrc.y, wiw.mrc.w, wiw.mrc.h);
            traceln("wiw.work_area: %d,%d %dx%d", wiw.work_area.x, wiw.work_area.y, wiw.work_area.w, wiw.work_area.h);
            traceln("wiw.min_position: %d,%d", wiw.min_position.x, wiw.min_position.y);
            traceln("wiw.max_position: %d,%d", wiw.max_position.x, wiw.max_position.y);
            traceln("wiw.max_track: %d,%d", wiw.max_track.x, wiw.max_track.y);
            traceln("wiw.dpi: %d", wiw.dpi);
            traceln("wiw.flags: %d", wiw.flags);
            traceln("wiw.show: %d", wiw.show);
        #endif
        ui_rect_t* p = &wiw.placement;
        app_update_mi(&wiw.placement, MONITOR_DEFAULTTONEAREST);
        bool same_monitor = memcmp(&wiw.mrc, &app.mrc, sizeof(wiw.mrc)) == 0;
//      traceln("%d,%d %dx%d", p->x, p->y, p->w, p->h);
        if (same_monitor) {
            *rect = *p;
        } else { // moving to another monitor
            rect->x = (p->x - wiw.mrc.x) * app.mrc.w / wiw.mrc.w;
            rect->y = (p->y - wiw.mrc.y) * app.mrc.h / wiw.mrc.h;
            // adjust according to monitors DPI difference:
            // (w, h) theoretically could be as large as 0xFFFF
            const int64_t w = (int64_t)p->w * app.dpi.monitor_raw;
            const int64_t h = (int64_t)p->h * app.dpi.monitor_raw;
            rect->w = (int32_t)(w / wiw.dpi);
            rect->h = (int32_t)(h / wiw.dpi);
        }
        *visibility = wiw.show;
    }
//  traceln("%d,%d %dx%d show=%d", rect->x, rect->y, rect->w, rect->h, *visibility);
    app_bring_window_inside_monitor(&app.mrc, rect);
//  traceln("%d,%d %dx%d show=%d", rect->x, rect->y, rect->w, rect->h, *visibility);
    return loaded;
}

static bool app_load_console_pos(ui_rect_t* rect, int32_t *visibility) {
    app_wiw_t wiw = {0}; // where is window
    *visibility = 0; // boolean
    bool loaded = config.load(app.class_name, "wic", &wiw, sizeof(wiw)) ==
                                sizeof(wiw);
    if (loaded) {
        ui_rect_t* p = &wiw.placement;
        app_update_mi(&wiw.placement, MONITOR_DEFAULTTONEAREST);
        bool same_monitor = memcmp(&wiw.mrc, &app.mrc, sizeof(wiw.mrc)) == 0;
//      traceln("%d,%d %dx%d", p->x, p->y, p->w, p->h);
        if (same_monitor) {
            *rect = *p;
        } else { // moving to another monitor
            rect->x = (p->x - wiw.mrc.x) * app.mrc.w / wiw.mrc.w;
            rect->y = (p->y - wiw.mrc.y) * app.mrc.h / wiw.mrc.h;
            // adjust according to monitors DPI difference:
            // (w, h) theoretically could be as large as 0xFFFF
            const int64_t w = (int64_t)p->w * app.dpi.monitor_raw;
            const int64_t h = (int64_t)p->h * app.dpi.monitor_raw;
            rect->w = (int32_t)(w / wiw.dpi);
            rect->h = (int32_t)(h / wiw.dpi);
        }
        *visibility = wiw.show != 0;
        app_update_mi(&app.wrc, MONITOR_DEFAULTTONEAREST);
    }
    return loaded;
}

static void app_timer_kill(tm_t timer) {
    fatal_if_false(KillTimer(window(), timer));
}

static tm_t app_timer_set(uintptr_t id, int32_t ms) {
    not_null(window());
    assert(10 <= ms && ms < 0x7FFFFFFF);
    tm_t tid = (tm_t)SetTimer(window(), id, (uint32_t)ms, null);
    fatal_if(tid == 0);
    assert(tid == id);
    return tid;
}

static void set_parents(view_t* ui) {
    for (view_t** c = ui->children; c != null && *c != null; c++) {
        if ((*c)->parent == null) {
            (*c)->parent = ui;
            set_parents(*c);
        } else {
            assert((*c)->parent == ui, "no reparenting");
        }
    }
}

static void init_children(view_t* ui) {
    for (view_t** c = ui->children; c != null && *c != null; c++) {
        if ((*c)->init != null) { (*c)->init(*c); (*c)->init = null; }
        if ((*c)->font == null) { (*c)->font = &app.fonts.regular; }
        if ((*c)->em.x == 0 || (*c)->em.y == 0) { (*c)->em = gdi.get_em(*ui->font); }
        uic_localize(*c);
        init_children(*c);
    }
}

static void app_post_message(int32_t m, int64_t wp, int64_t lp) {
    fatal_if_false(PostMessageA(window(), m, wp, lp));
}

static void app_timer(view_t* ui, tm_t id) {
    if (ui->timer != null) {
        ui->timer(ui, id);
    }
    if (id == app_timer_1s_id && ui->every_sec != null) {
        ui->every_sec(ui);
    }
    if (id == app_timer_100ms_id && ui->every_100ms != null) {
        ui->every_100ms(ui);
    }
    view_t** c = ui->children;
    while (c != null && *c != null) { app_timer(*c, id); c++; }
}

static void app_every_100ms(tm_t id) {
    if (id == app_timer_1s_id && app.every_sec != null) {
        app.every_sec();
    }
    if (id == app_timer_100ms_id && app.every_100ms != null) {
        app.every_100ms();
    }
    if (app_toast.time != 0 && app.now > app_toast.time) {
        app.show_toast(null, 0);
    }
}

static void app_animate_timer(void) {
    app_post_message(WM_ANIMATE, (uint64_t)app_animate.step + 1,
        (uintptr_t)app_animate.f);
}

static void app_wm_timer(tm_t id) {
    app_every_100ms(id);
    if (app_animate.timer == id) { app_animate_timer(); }
    app_timer(app.ui, id);
}

static void app_window_dpi(void) {
    int32_t dpi = GetDpiForWindow(window());
    if (dpi == 0) { dpi = GetDpiForWindow(GetParent(window())); }
    if (dpi == 0) { dpi = GetDpiForWindow(GetDesktopWindow()); }
    if (dpi == 0) { dpi = GetSystemDpiForProcess(GetCurrentProcess()); }
    if (dpi == 0) { dpi = GetDpiForSystem(); }
    app.dpi.window = dpi;
}

static void app_window_opening(void) {
    app_window_dpi();
    app_init_fonts(app.dpi.window);
    app_timer_1s_id = app.set_timer((uintptr_t)&app_timer_1s_id, 1000);
    app_timer_100ms_id = app.set_timer((uintptr_t)&app_timer_100ms_id, 100);
    app.set_cursor(app.cursor_arrow);
    app.canvas = (canvas_t)GetDC(window());
    not_null(app.canvas);
    if (app.opened != null) { app.opened(); }
    app.ui->em = gdi.get_em(*app.ui->font);
    set_parents(app.ui);
    init_children(app.ui);
    app_wm_timer(app_timer_100ms_id);
    app_wm_timer(app_timer_1s_id);
    fatal_if(ReleaseDC(window(), canvas()) == 0);
    app.canvas = null;
    app.layout(); // request layout
    if (app.last_visibility == window_visibility.maximize) {
        ShowWindow(window(), window_visibility.maximize);
    }
//  app_dump_dpi();
//  if (forced_locale != 0) {
//      SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (uintptr_t)"intl", 0, 1000, null);
//  }
}

static void app_window_closing(void) {
    if (app.can_close == null || app.can_close()) {
        if (app.is_full_screen) { app.full_screen(false); }
        app.kill_timer(app_timer_1s_id);
        app.kill_timer(app_timer_100ms_id);
        app_timer_1s_id = 0;
        app_timer_100ms_id = 0;
        if (app.closed != null) { app.closed(); }
        app_save_window_pos(app.window, "wiw", false);
        app_save_console_pos();
        DestroyWindow(window());
        app.window = null;
    }
}

static void app_get_min_max_info(MINMAXINFO* mmi) {
    if (app.wmax != 0 && app.hmax != 0) {
        // if wmax hmax are set then wmin hmin must be less than wmax hmax
        assert(app.wmin <= app.wmax && app.hmin <= app.hmax,
            "app.wmax=%d app.hmax=%d app.wmin=%d app.hmin=%d",
             app.wmax, app.hmax, app.wmin, app.hmin);
    }
    const ui_rect_t* wa = &app.work_area;
    const int32_t wmin = app.wmin > 0 ? app.in2px(app.wmin) : wa->w / 2;
    const int32_t hmin = app.hmin > 0 ? app.in2px(app.hmin) : wa->h / 2;
    mmi->ptMinTrackSize.x = wmin;
    mmi->ptMinTrackSize.y = hmin;
    const int32_t wmax = app.wmax > 0 ? app.in2px(app.wmax) : wa->w;
    const int32_t hmax = app.hmax > 0 ? app.in2px(app.hmax) : wa->h;
    if (app.no_clip) {
        mmi->ptMaxTrackSize.x = wmax;
        mmi->ptMaxTrackSize.y = hmax;
    } else {
        // clip wmax and hmax to monitor work area
        mmi->ptMaxTrackSize.x = min(wmax, wa->w);
        mmi->ptMaxTrackSize.y = min(hmax, wa->h);
    }
    mmi->ptMaxSize.x = mmi->ptMaxTrackSize.x;
    mmi->ptMaxSize.y = mmi->ptMaxTrackSize.y;
}

#define app_method_int32(name)                                  \
static void app_##name(view_t* ui, int32_t p) {                  \
    if (ui->name != null && !uic_hidden_or_disabled(ui)) {      \
        ui->name(ui, p);                                        \
    }                                                           \
    view_t** c = ui->children;                                   \
    while (c != null && *c != null) { app_##name(*c, p); c++; } \
}

app_method_int32(key_pressed)
app_method_int32(key_released)

static void app_character(view_t* ui, const char* utf8) {
    if (!uic_hidden_or_disabled(ui)) {
        if (ui->character != null) { ui->character(ui, utf8); }
        view_t** c = ui->children;
        while (c != null && *c != null) { app_character(*c, utf8); c++; }
    }
}

static void app_paint(view_t* ui) {
    if (!ui->hidden && app.crc.w > 0 && app.crc.h > 0) {
        if (ui->paint != null) { ui->paint(ui); }
        view_t** c = ui->children;
        while (c != null && *c != null) { app_paint(*c); c++; }
    }
}

static bool app_set_focus(view_t* ui) {
    bool set = false;
    assert(GetActiveWindow() == window());
    view_t** c = ui->children;
    while (c != null && *c != null && !set) { set = app_set_focus(*c); c++; }
    if (ui->focusable && ui->set_focus != null &&
       (app.focus == ui || app.focus == null)) {
        set = ui->set_focus(ui);
    }
    return set;
}

static void app_kill_focus(view_t* ui) {
    view_t** c = ui->children;
    while (c != null && *c != null) { app_kill_focus(*c); c++; }
    if (ui->set_focus != null && ui->focusable) {
        ui->kill_focus(ui);
    }
}

static void app_mousewheel(view_t* ui, int32_t dx, int32_t dy) {
    if (!uic_hidden_or_disabled(ui)) {
        if (ui->mousewheel != null) { ui->mousewheel(ui, dx, dy); }
        view_t** c = ui->children;
        while (c != null && *c != null) { app_mousewheel(*c, dx, dy); c++; }
    }
}

static void app_measure_children(view_t* ui) {
    if (!ui->hidden && app.crc.w > 0 && app.crc.h > 0) {
        view_t** c = ui->children;
        while (c != null && *c != null) { app_measure_children(*c); c++; }
        if (ui->measure != null) { ui->measure(ui); }
    }
}

static void app_layout_children(view_t* ui) {
    if (!ui->hidden && app.crc.w > 0 && app.crc.h > 0) {
        if (ui->layout != null) { ui->layout(ui); }
        view_t** c = ui->children;
        while (c != null && *c != null) { app_layout_children(*c); c++; }
    }
}

static void app_layout_ui(view_t* ui) {
    app_measure_children(ui);
    app_layout_children(ui);
}

static bool app_message(view_t* ui, int32_t m, int64_t wp, int64_t lp,
        int64_t* ret) {
    // message() callback is called even for hidden and disabled ui-elements
    // consider timers, and other useful messages
    app_on_every_message(ui);
    if (ui->message != null) {
        if (ui->message(ui, m, wp, lp, ret)) { return true; }
    }
    view_t** c = ui->children;
    while (c != null && *c != null) {
        if (app_message(*c, m, wp, lp, ret)) { return true; }
        c++;
    }
    return false;
}

static void app_killfocus(view_t* ui) {
    // removes focus from hidden or disabled ui controls
    if (app.focus == ui && (ui->disabled || ui->hidden)) {
        app.focus = null;
    } else {
        view_t** c = ui->children;
        while (c != null && *c != null) {
            app_killfocus(*c);
            c++;
        }
    }
}

static void app_toast_mouse(int32_t m, int32_t f);
static void app_toast_character(const char* utf8);

static void app_wm_char(view_t* ui, const char* utf8) {
    if (app_toast.ui != null) {
        app_toast_character(utf8);
    } else {
        app_character(ui, utf8);
    }
}

static void app_hover_changed(view_t* ui) {
    if (ui->hovering != null && !ui->hidden) {
        if (!ui->hover) {
            ui->hover_at = 0;
            ui->hovering(ui, false); // cancel hover
        } else {
            assert(ui->hover_delay >= 0);
            if (ui->hover_delay == 0) {
                ui->hover_at = -1;
                ui->hovering(ui, true); // call immediately
            } else if (ui->hover_delay != 0 && ui->hover_at >= 0) {
                ui->hover_at = app.now + ui->hover_delay;
            }
        }
    }
}

// app_on_every_message() is called on every message including timers
// allowing ui elements to do scheduled actions like e.g. hovering()

static void app_on_every_message(view_t* ui) {
    if (ui->hovering != null && !ui->hidden) {
        if (ui->hover_at > 0 && app.now > ui->hover_at) {
            ui->hover_at = -1; // "already called"
            ui->hovering(ui, true);
        }
    }
}

static void app_ui_mouse(view_t* ui, int32_t m, int32_t f) {
    if (!app.is_hidden(ui) &&
       (m == WM_MOUSEHOVER || m == messages.mouse_move)) {
        RECT rc = { ui->x, ui->y, ui->x + ui->w, ui->y + ui->h};
        bool hover = ui->hover;
        POINT pt = app_ui2point(&app.mouse);
        ui->hover = PtInRect(&rc, pt);
        InflateRect(&rc, ui->w / 4, ui->h / 4);
        ui_rect_t r = app_rect2ui(&rc);
        if (hover != ui->hover) { app.invalidate(&r); }
        if (hover != ui->hover && ui->hovering != null) {
            app_hover_changed(ui);
        }
    }
    if (!uic_hidden_or_disabled(ui)) {
        if (ui->mouse != null) { ui->mouse(ui, m, f); }
        for (view_t** c = ui->children; c != null && *c != null; c++) {
            app_ui_mouse(*c, m, f);
        }
    }
}

static bool app_context_menu(view_t* ui) {
    if (!uic_hidden_or_disabled(ui)) {
        for (view_t** c = ui->children; c != null && *c != null; c++) {
            if (app_context_menu(*c)) { return true; }
        }
        RECT rc = { ui->x, ui->y, ui->x + ui->w, ui->y + ui->h};
        POINT pt = app_ui2point(&app.mouse);
        if (PtInRect(&rc, pt)) {
            if (!ui->hidden && !ui->disabled && ui->context_menu != null) {
                ui->context_menu(ui);
            }
        }
    }
    return false;
}

static bool app_inside(view_t* ui) {
    const int32_t x = app.mouse.x - ui->x;
    const int32_t y = app.mouse.y - ui->y;
    return 0 <= x && x < ui->w && 0 <= y && y < ui->h;
}

static bool app_tap(view_t* ui, int32_t ix) { // 0: left 1: middle 2: right
    bool done = false; // consumed
    if (!uic_hidden_or_disabled(ui) && app_inside(ui)) {
        for (view_t** c = ui->children; c != null && *c != null && !done; c++) {
            done = app_tap(*c, ix);
        }
        if (ui->tap != null && !done) { done = ui->tap(ui, ix); }
    }
    return done;
}

static bool app_press(view_t* ui, int32_t ix) { // 0: left 1: middle 2: right
    bool done = false; // consumed
    if (!uic_hidden_or_disabled(ui) && app_inside(ui)) {
        for (view_t** c = ui->children; c != null && *c != null && !done; c++) {
            done = app_press(*c, ix);
        }
        if (ui->press != null && !done) { done = ui->press(ui, ix); }
    }
    return done;
}

static void app_mouse(view_t* ui, int32_t m, int32_t f) {
    if (app_toast.ui != null && app_toast.ui->mouse != null) {
        app_ui_mouse(app_toast.ui, m, f);
    } else if (app_toast.ui != null && app_toast.ui->mouse == null) {
        app_toast_mouse(m, f);
        bool tooltip = app_toast.x >= 0 && app_toast.y >= 0;
        if (tooltip) { app_ui_mouse(ui, m, f); }
    } else {
        app_ui_mouse(ui, m, f);
    }
}

static void app_tap_press(uint32_t m, WPARAM wp, LPARAM lp) {
    app.mouse.x = GET_X_LPARAM(lp);
    app.mouse.y = GET_Y_LPARAM(lp);
    // dispatch as generic mouse message:
    app_mouse(app.ui, (int32_t)m, (int32_t)wp);
    int32_t ix = (int32_t)wp;
    assert(0 <= ix && ix <= 2);
    // for now long press and double tap/double click
    // treated as press() call - can be separated if desired:
    switch (m) {
        case WM_TAP  : app_tap(app.ui, ix);  break;
        case WM_DTAP : app_press(app.ui, ix); break;
        case WM_PRESS: app_press(app.ui, ix); break;
        default: assert(false);
    }
}

static void app_toast_paint(void) {
    static image_t image;
    if (image.bitmap == null) {
        uint8_t pixels[4] = { 0x3F, 0x3F, 0x3F };
        gdi.image_init(&image, 1, 1, 3, pixels);
    }
    if (app_toast.ui != null) {
        font_t f = app_toast.ui->font != null ? *app_toast.ui->font : app.fonts.regular;
        const ui_point_t em = gdi.get_em(f);
        app_toast.ui->em = em;
        // allow unparented and unmeasureed toasts:
        if (app_toast.ui->measure != null) { app_toast.ui->measure(app_toast.ui); }
        gdi.push(0, 0);
        bool tooltip = app_toast.x >= 0 && app_toast.y >= 0;
        int32_t em_x = em.x;
        int32_t em_y = em.y;
        gdi.set_brush(gdi.brush_color);
        gdi.set_brush_color(colors.toast);
        if (!tooltip) {
            assert(0 <= app_toast.step && app_toast.step < toast_steps);
            int32_t step = app_toast.step - (toast_steps - 1);
            app_toast.ui->y = app_toast.ui->h * step / (toast_steps - 1);
//          traceln("step=%d of %d y=%d", app_toast.step, app_toast_steps, app_toast.ui->y);
            app_layout_ui(app_toast.ui);
            double alpha = min(0.40, 0.40 * app_toast.step / (double)toast_steps);
            gdi.alpha_blend(0, 0, app.width, app.height, &image, alpha);
            app_toast.ui->x = (app.width - app_toast.ui->w) / 2;
        } else {
            app_toast.ui->x = app_toast.x;
            app_toast.ui->y = app_toast.y;
            app_layout_ui(app_toast.ui);
            int32_t mx = app.width - app_toast.ui->w - em_x;
            app_toast.ui->x = min(mx, max(0, app_toast.x - app_toast.ui->w / 2));
            app_toast.ui->y = min(app.crc.h - em_y, max(0, app_toast.y));
        }
        int32_t x = app_toast.ui->x - em_x;
        int32_t y = app_toast.ui->y - em_y / 2;
        int32_t w = app_toast.ui->w + em_x * 2;
        int32_t h = app_toast.ui->h + em_y;
        gdi.rounded(x, y, w, h, em_x, em_y);
        if (!tooltip) { app_toast.ui->y += em_y / 4; }
        app_paint(app_toast.ui);
        if (!tooltip) {
            if (app_toast.ui->y == em_y / 4) {
                // micro "close" toast button:
                gdi.x = app_toast.ui->x + app_toast.ui->w;
                gdi.y = 0;
                gdi.text("\xC3\x97"); // Heavy Multiplication X
            }
        }
        gdi.pop();
    }
}

static void app_toast_cancel(void) {
    if (app_toast.ui != null && app_toast.ui->tag == uic_tag_messagebox) {
        uic_messagebox_t* mx = (uic_messagebox_t*)app_toast.ui;
        if (mx->option < 0) { mx->cb(mx, -1); }
    }
    app_toast.step = 0;
    app_toast.ui = null;
    app_toast.time = 0;
    app_toast.x = -1;
    app_toast.y = -1;
    app.redraw();
}

static void app_toast_mouse(int32_t m, int32_t flags) {
    bool pressed = m == messages.left_button_pressed ||
                   m == messages.right_button_pressed;
    if (app_toast.ui != null && pressed) {
        const ui_point_t em = app_toast.ui->em;
        int32_t x = app_toast.ui->x + app_toast.ui->w;
        if (x <= app.mouse.x && app.mouse.x <= x + em.x &&
            0 <= app.mouse.y && app.mouse.y <= em.y) {
            app_toast_cancel();
        } else {
            app_ui_mouse(app_toast.ui, m, flags);
        }
    } else {
        app_ui_mouse(app_toast.ui, m, flags);
    }
}

static void app_toast_character(const char* utf8) {
    char ch = utf8[0];
    if (app_toast.ui != null && ch == 033) { // ESC traditionally in octal
        app_toast_cancel();
        app.show_toast(null, 0);
    } else {
        app_character(app_toast.ui, utf8);
    }
}

static void app_toast_dim(int32_t step) {
    app_toast.step = step;
    app.redraw();
    UpdateWindow(window());
}

static void app_animate_step(app_animate_function_t f, int32_t step, int32_t steps) {
    // calls function(0..step-1) exactly step times
    bool cancel = false;
    if (f != null && f != app_animate.f && step == 0 && steps > 0) {
        // start animation
        app_animate.count = steps;
        app_animate.f = f;
        f(step);
        app_animate.timer = app.set_timer((uintptr_t)&app_animate.timer, 10);
    } else if (f != null && app_animate.f == f && step > 0) {
        cancel = step >= app_animate.count;
        if (!cancel) {
            app_animate.step = step;
            f(step);
        }
    } else if (f == null) {
        cancel = true;
    }
    if (cancel) {
        if (app_animate.timer != 0) { app.kill_timer(app_animate.timer); }
        app_animate.step = 0;
        app_animate.timer = 0;
        app_animate.f = null;
        app_animate.count = 0;
    }
}

static void app_animate_start(app_animate_function_t f, int32_t steps) {
    // calls f(0..step-1) exactly steps times, unless cancelled with call
    // animate(null, 0) or animate(other_function, n > 0)
    app_animate_step(f, 0, steps);
}

static void app_layout_root(void) {
    not_null(app.window);
    not_null(app.canvas);
    app.ui->w = app.crc.w; // crc is window client rectangle
    app.ui->h = app.crc.h;
    app_layout_ui(app.ui);
}

static void app_paint_on_canvas(HDC hdc) {
    canvas_t canvas = app.canvas;
    app.canvas = (canvas_t)hdc;
    gdi.push(0, 0);
    double time = clock.seconds();
    gdi.x = 0;
    gdi.y = 0;
    app_update_crc();
    if (app_layout_dirty) {
        app_layout_dirty = false;
        app_layout_root();
    }
    font_t font = gdi.set_font(app.fonts.regular);
    color_t c = gdi.set_text_color(colors.text);
    int32_t bm = SetBkMode(canvas(), TRANSPARENT);
    int32_t stretch_mode = SetStretchBltMode(canvas(), HALFTONE);
    ui_point_t pt = {0};
    fatal_if_false(SetBrushOrgEx(canvas(), 0, 0, (POINT*)&pt));
    brush_t br = gdi.set_brush(gdi.brush_hollow);
    app_paint(app.ui);
    if (app_toast.ui != null) { app_toast_paint(); }
    fatal_if_false(SetBrushOrgEx(canvas(), pt.x, pt.y, null));
    SetStretchBltMode(canvas(), stretch_mode);
    SetBkMode(canvas(), bm);
    gdi.set_brush(br);
    gdi.set_text_color(c);
    gdi.set_font(font);
    app.paint_count++;
    if (app.paint_count % 128 == 0) { app.paint_max = 0; }
    app.paint_time = clock.seconds() - time;
    app.paint_max = max(app.paint_time, app.paint_max);
    if (app.paint_avg == 0) {
        app.paint_avg = app.paint_time;
    } else { // EMA over 32 paint() calls
        app.paint_avg = app.paint_avg * (1.0 - 1.0 / 32.0) +
                        app.paint_time / 32.0;
    }
    gdi.pop();
    app.canvas = canvas;
}

static void app_wm_paint(void) {
    // it is possible to receive WM_PAINT when window is not closed
    if (app.window != null) {
        PAINTSTRUCT ps = {0};
        BeginPaint(window(), &ps);
        app_paint_on_canvas(ps.hdc);
        EndPaint(window(), &ps);
    }
}

// about (x,y) being (-32000,-32000) see:
// https://chromium.googlesource.com/chromium/src.git/+/62.0.3178.1/ui/views/win/hwnd_message_handler.cc#1847

static void app_window_position_changed(const WINDOWPOS* wp) {
    app.ui->hidden = !IsWindowVisible(window());
    const bool moved  = (wp->flags & SWP_NOMOVE) == 0;
    const bool sized  = (wp->flags & SWP_NOSIZE) == 0;
    const bool hiding = (wp->flags & SWP_HIDEWINDOW) != 0 ||
                        wp->x == -32000 && wp->y == -32000;
    HMONITOR monitor = MonitorFromWindow(window(), MONITOR_DEFAULTTONULL);
    if (!app.ui->hidden && (moved || sized) && !hiding && monitor != null) {
        RECT wrc = app_ui2rect(&app.wrc);
        fatal_if_false(GetWindowRect(window(), &wrc));
        app.wrc = app_rect2ui(&wrc);
        app_update_mi(&app.wrc, MONITOR_DEFAULTTONEAREST);
        app_update_crc();
        if (app_timer_1s_id != 0) { app.layout(); }
    }
}

static void app_setting_change(uintptr_t wp, uintptr_t lp) {
    if (wp == 0 && lp != 0 && strcmp((const char*)lp, "intl") == 0) {
        wchar_t ln[LOCALE_NAME_MAX_LENGTH + 1];
        int32_t n = GetUserDefaultLocaleName(ln, countof(ln));
        fatal_if_false(n > 0);
        wchar_t rln[LOCALE_NAME_MAX_LENGTH + 1];
        n = ResolveLocaleName(ln, rln, countof(rln));
        fatal_if_false(n > 0);
        LCID lcid = LocaleNameToLCID(rln, LOCALE_ALLOW_NEUTRAL_NAMES);
        fatal_if_false(SetThreadLocale(lcid));
    }
}

static void app_show_task_bar(bool show) {
    HWND taskbar = FindWindow("Shell_TrayWnd", null);
    if (taskbar != null) {
        ShowWindow(taskbar, show ? SW_SHOW : SW_HIDE);
        UpdateWindow(taskbar);
    }
}

static void app_click_detector(uint32_t msg, WPARAM wp, LPARAM lp) {
    // TODO: click detector does not handle WM_NCLBUTTONDOWN, ...
    //       it can be modified to do so if needed
    #pragma push_macro("set_timer")
    #pragma push_macro("kill_timer")
    #pragma push_macro("done")

    #define set_timer(t, ms) do {                   \
        assert(t == 0);                             \
        t = app_timer_set((uintptr_t)&t, ms);       \
    } while (0)

    #define kill_timer(t) do {                      \
        if (t != 0) { app_timer_kill(t); t = 0; }   \
    } while (0)

    #define done(ix) do {                           \
        clicked[ix] = 0;                            \
        pressed[ix] = false;                        \
        click_at[ix] = (ui_point_t){0, 0};          \
        kill_timer(timer_p[ix]);                    \
        kill_timer(timer_d[ix]);                    \
    } while (0)

    // This function should work regardless to CS_BLKCLK being present
    // 0: Left, 1: Middle, 2: Right
    static ui_point_t click_at[3];
    static double     clicked[3]; // click time
    static bool       pressed[3];
    static tm_t       timer_d[3]; // double tap
    static tm_t       timer_p[3]; // long press
    bool up = false;
    int32_t ix = -1;
    uint32_t m = 0;
    switch (msg) {
        case WM_LBUTTONDOWN  : ix = 0; m = WM_TAP;  break;
        case WM_MBUTTONDOWN  : ix = 1; m = WM_TAP;  break;
        case WM_RBUTTONDOWN  : ix = 2; m = WM_TAP;  break;
        case WM_LBUTTONDBLCLK: ix = 0; m = WM_DTAP; break;
        case WM_MBUTTONDBLCLK: ix = 1; m = WM_DTAP; break;
        case WM_RBUTTONDBLCLK: ix = 2; m = WM_DTAP; break;
        case WM_LBUTTONUP    : ix = 0; up = true;   break;
        case WM_MBUTTONUP    : ix = 1; up = true;   break;
        case WM_RBUTTONUP    : ix = 2; up = true;   break;
    }
    if (msg == WM_TIMER) { // long press && dtap
        for (int i = 0; i < 3; i++) {
            if (wp == timer_p[i]) {
                lp = MAKELONG(click_at[i].x, click_at[i].y);
                app_post_message(WM_PRESS, i, lp);
                done(i);
            }
            if (wp == timer_d[i]) {
                lp = MAKELONG(click_at[i].x, click_at[i].y);
                app_post_message(WM_TAP, i, lp);
                done(i);
            }
        }
    }
    if (ix != -1) {
        const uint32_t DTAP_MSEC = GetDoubleClickTime();
        const double double_click_dt = DTAP_MSEC / 1000.0;
        const int double_click_x = GetSystemMetrics(SM_CXDOUBLECLK) / 2;
        const int double_click_y = GetSystemMetrics(SM_CYDOUBLECLK) / 2;
        ui_point_t pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (m == WM_TAP) {
            if (app.now  - clicked[ix]  <= double_click_dt &&
                abs(pt.x - click_at[ix].x) <= double_click_x &&
                abs(pt.y - click_at[ix].y) <= double_click_y) {
                app_post_message(WM_DTAP, ix, lp);
                done(ix);
            } else {
                done(ix); // clear timers
                clicked[ix]  = app.now;
                click_at[ix] = pt;
                pressed[ix]  = true;
                set_timer(timer_p[ix], LONG_PRESS_MSEC); // 0.25s
                set_timer(timer_d[ix], DTAP_MSEC); // 0.5s
            }
        } else if (up) {
//          traceln("pressed[%d]: %d %.3f", ix, pressed[ix], app.now - clicked[ix]);
            if (pressed[ix] && app.now - clicked[ix] > double_click_dt) {
                app_post_message(WM_DTAP, ix, lp);
                done(ix);
            }
            kill_timer(timer_p[ix]); // long press is no the case
        } else if (m == WM_DTAP) {
            app_post_message(WM_DTAP, ix, lp);
            done(ix);
        }
    }
    #pragma pop_macro("done")
    #pragma pop_macro("kill_timer")
    #pragma pop_macro("set_timer")
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    app.now = clock.seconds();
    if (app.window == null) {
        app.window = (window_t)window;
    } else {
        assert(window() == window);
    }
    int64_t ret = 0;
    app_killfocus(app.ui);
    app_click_detector(msg, wp, lp);
    if (app_message(app.ui, msg, wp, lp, &ret)) {
        return (LRESULT)ret;
    }
    switch (msg) {
        case WM_GETMINMAXINFO: app_get_min_max_info((MINMAXINFO*)lp); break;
        case WM_SETTINGCHANGE: app_setting_change(wp, lp); break;
        case WM_CLOSE        : app.focus = null; // before WM_CLOSING
                               app_post_message(WM_CLOSING, 0, 0); return 0;
        case WM_OPENNING     : app_window_opening(); return 0;
        case WM_CLOSING      : app_window_closing(); return 0;
        case WM_DESTROY      : PostQuitMessage(app.exit_code); break;
        case WM_SYSKEYDOWN: // for ALT (aka VK_MENU)
        case WM_KEYDOWN      : app_alt_ctrl_shift(true, (int32_t)wp);
                               app_key_pressed(app.ui, (int32_t)wp);
                               break;
        case WM_SYSKEYUP:
        case WM_KEYUP        : app_alt_ctrl_shift(false, (int32_t)wp);
                               app_key_released(app.ui, (int32_t)wp);
                               break;
        case WM_TIMER        : app_wm_timer((tm_t)wp);
                               break;
        case WM_ERASEBKGND   : return true; // no DefWindowProc()
        case WM_SETCURSOR    : SetCursor((HCURSOR)app.cursor); break;
        // see: https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input
        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-tounicode
//      case WM_UNICHAR      : // only UTF-32 via PostMessage
        case WM_CHAR         : app_wm_char(app.ui, (const char*)&wp);
                               break; // TODO: CreateWindowW() and utf16->utf8
        case WM_PRINTCLIENT  : app_paint_on_canvas((HDC)wp); break;
        case WM_ANIMATE      : app_animate_step((app_animate_function_t)lp, (int)wp, -1);
                               break;
        case WM_SETFOCUS     : if (!app.ui->hidden) { app_set_focus(app.ui); }
                               break;
        case WM_KILLFOCUS    : if (!app.ui->hidden) { app_kill_focus(app.ui); }
                               break;
        case WM_PAINT        : app_wm_paint(); break;
        case WM_CONTEXTMENU  : (void)app_context_menu(app.ui); break;
        case WM_MOUSEWHEEL   :
            app_mousewheel(app.ui, 0, GET_WHEEL_DELTA_WPARAM(wp)); break;
        case WM_MOUSEHWHEEL  :
            app_mousewheel(app.ui, GET_WHEEL_DELTA_WPARAM(wp), 0); break;
        case WM_NCMOUSEMOVE    :
        case WM_NCLBUTTONDOWN  :
        case WM_NCLBUTTONUP    :
        case WM_NCLBUTTONDBLCLK:
        case WM_NCRBUTTONDOWN  :
        case WM_NCRBUTTONUP    :
        case WM_NCRBUTTONDBLCLK:
        case WM_NCMBUTTONDOWN  :
        case WM_NCMBUTTONUP    :
        case WM_NCMBUTTONDBLCLK: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
//          traceln("%d %d", pt.x, pt.y);
            ScreenToClient(window(), &pt);
            app.mouse = app_point2ui(&pt);
            app_mouse(app.ui, (int32_t)msg, (int32_t)wp);
            break;
        }
        case WM_TAP:
        case WM_DTAP:
        case WM_PRESS:
            app_tap_press(msg, wp, lp);
            break;
        case WM_MOUSEHOVER   : // see TrackMouseEvent()
        case WM_MOUSEMOVE    :
        case WM_LBUTTONDOWN  :
        case WM_LBUTTONUP    :
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN  :
        case WM_RBUTTONUP    :
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN  :
        case WM_MBUTTONUP    :
        case WM_MBUTTONDBLCLK: {
            app.mouse.x = GET_X_LPARAM(lp);
            app.mouse.y = GET_Y_LPARAM(lp);
//          traceln("%d %d", app.mouse.x, app.mouse.y);
            // note: ScreenToClient() is not needed for this messages
            app_mouse(app.ui, (int32_t)msg, (int32_t)wp);
            break;
        }
        case WM_GETDPISCALEDSIZE: { // sent before WM_DPICHANGED
//          traceln("WM_GETDPISCALEDSIZE");
            #ifdef QUICK_DEBUG
                int32_t dpi = (int32_t)wp;
                SIZE* sz = (SIZE*)lp; // in/out
                ui_point_t cell = { sz->cx, sz->cy };
                traceln("WM_GETDPISCALEDSIZE dpi %d := %d "
                    "size %d,%d *may/must* be adjusted",
                    app.dpi.window, dpi, cell.x, cell.y);
            #endif
            if (app_timer_1s_id != 0 && !app.ui->hidden) { app.layout(); }
            // IMPORTANT: return true because otherwise linear, see
            // https://learn.microsoft.com/en-us/windows/win32/hidpi/wm-getdpiscaledsize
            return true;
        }
        case WM_DPICHANGED: {
//          traceln("WM_DPICHANGED");
            app_window_dpi();
            app_init_fonts(app.dpi.window);
            if (app_timer_1s_id != 0 && !app.ui->hidden) {
                app.layout();
            } else {
                app_layout_dirty = true;
            }
            break;
        }
        case WM_SYSCOMMAND:
            if (wp == SC_MINIMIZE && app.hide_on_minimize) {
                app.show_window(window_visibility.min_na);
                app.show_window(window_visibility.hide);
            }
            // If the selection is in menu handle the key event
            if (wp == SC_KEYMENU && lp != 0x20) {
                return 0; // This prevents the error/beep sound
            }
            break;
        case WM_ACTIVATE:
            if (!IsWindowVisible(window()) && LOWORD(wp) != WA_INACTIVE) {
                app.show_window(window_visibility.restore);
                SwitchToThisWindow(window(), true);
            }
            break;
        case WM_WINDOWPOSCHANGING: {
            #ifdef QUICK_DEBUG
                WINDOWPOS* pos = (WINDOWPOS*)lp;
//              traceln("WM_WINDOWPOSCHANGING flags: 0x%08X", pos->flags);
                if (pos->flags & SWP_SHOWWINDOW) {
//                  traceln("SWP_SHOWWINDOW");
                } else if (pos->flags & SWP_HIDEWINDOW) {
//                  traceln("SWP_HIDEWINDOW");
                }
            #endif
            break;
        }
        case WM_WINDOWPOSCHANGED:
            app_window_position_changed((WINDOWPOS*)lp);
            break;
        default:
            break;
    }
    return DefWindowProcA(window(), msg, wp, lp);
}

static long app_set_window_long(int32_t index, long value) {
    runtime.seterr(0);
    long r = SetWindowLongA(window(), index, value); // r previous value
    fatal_if_not_zero(runtime.err());
    return r;
}

static void app_create_window(const ui_rect_t r) {
    WNDCLASSA wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 256 * 1024;
    wc.hInstance = GetModuleHandleA(null);
    #define IDI_ICON 101
    wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCE(IDI_ICON));
    wc.hCursor = (HCURSOR)app.cursor;
    wc.hbrBackground = null;
    wc.lpszMenuName = null;
    wc.lpszClassName = app.class_name;
    ATOM atom = RegisterClassA(&wc);
    fatal_if(atom == 0);
    uint32_t style = app.no_decor ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    HWND window = CreateWindowExA(WS_EX_COMPOSITED | WS_EX_LAYERED,
        app.class_name, app.title, style,
        r.x, r.y, r.w, r.h, null, null, wc.hInstance, null);
    assert(window == window()); (void)window;
    not_null(app.window);
    app.dpi.window = GetDpiForWindow(window());
//  traceln("app.dpi.window=%d", app.dpi.window);
    RECT wrc = app_ui2rect(&r);
    fatal_if_false(GetWindowRect(window(), &wrc));
    app.wrc = app_rect2ui(&wrc);
    // DWMWA_CAPTION_COLOR is supported starting with Windows 11 Build 22000.
    if (IsWindowsVersionOrGreater(10, 0, 22000)) {
        COLORREF caption_color = gdi_color_ref(colors.dkgray3);
        fatal_if_not_zero(DwmSetWindowAttribute(window(),
            DWMWA_CAPTION_COLOR, &caption_color, sizeof(caption_color)));
        BOOL immersive = TRUE;
        fatal_if_not_zero(DwmSetWindowAttribute(window(),
            DWMWA_USE_IMMERSIVE_DARK_MODE, &immersive, sizeof(immersive)));
        // also availabale but not yet used:
//      DWMWA_USE_HOSTBACKDROPBRUSH
//      DWMWA_WINDOW_CORNER_PREFERENCE
//      DWMWA_BORDER_COLOR
//      DWMWA_CAPTION_COLOR
    }
    if (app.aero) { // It makes app look like retro Windows 7 Aero style :)
        enum DWMNCRENDERINGPOLICY ncrp = DWMNCRP_DISABLED;
        (void)DwmSetWindowAttribute(window(),
            DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
    }
    // always start with window hidden and let application show it
    app.show_window(window_visibility.hide);
    if (app.no_min || app.no_max) {
        uint32_t exclude = WS_SIZEBOX;
        if (app.no_min) { exclude = WS_MINIMIZEBOX; }
        if (app.no_max) { exclude = WS_MAXIMIZEBOX; }
        uint32_t s = GetWindowLongA(window(), GWL_STYLE);
        app_set_window_long(GWL_STYLE, s & ~exclude);
        // even for windows without maximize/minimize
        // make sure "Minimize All Windows" still works:
        // ???
//      EnableMenuItem(GetSystemMenu(window(), false),
//          SC_MINIMIZE, MF_BYCOMMAND | MF_ENABLED);
    }
    if (app.no_size) {
        uint32_t s = GetWindowLong(window(), GWL_STYLE);
        app_set_window_long(GWL_STYLE, s & ~WS_SIZEBOX);
        enum { swp = SWP_FRAMECHANGED |
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE };
        SetWindowPos(window(), NULL, 0, 0, 0, 0, swp);
    }
    if (app.visibility != window_visibility.hide) {
        app.ui->w = app.wrc.w;
        app.ui->h = app.wrc.h;
        AnimateWindow(window(), 250, AW_ACTIVATE);
        app.show_window(app.visibility);
        app_update_crc();
//      app.ui->w = app.crc.w; // app.crc is "client rectangle"
//      app.ui->h = app.crc.h;
    }
    // even if it is hidden:
    app_post_message(WM_OPENNING, 0, 0);
//  SetWindowTheme(window(), L"DarkMode_Explorer", null); ???
}

static void app_full_screen(bool on) {
    static int32_t style;
    static WINDOWPLACEMENT wp;
    if (on != app.is_full_screen) {
        app_show_task_bar(!on);
        if (on) {
            style = GetWindowLongA(window(), GWL_STYLE);
            app_set_window_long(GWL_STYLE, (style | WS_POPUP | WS_VISIBLE) &
                ~(WS_OVERLAPPEDWINDOW));
            wp.length = sizeof(wp);
            fatal_if_false(GetWindowPlacement(window(), &wp));
            WINDOWPLACEMENT nwp = wp;
            nwp.showCmd = SW_SHOWNORMAL;
            nwp.rcNormalPosition = (RECT){app.mrc.x, app.mrc.y,
                app.mrc.x + app.mrc.w, app.mrc.y + app.mrc.h};
            fatal_if_false(SetWindowPlacement(window(), &nwp));
        } else {
            fatal_if_false(SetWindowPlacement(window(), &wp));
            app_set_window_long(GWL_STYLE,  style | WS_OVERLAPPED);
            enum { swp = SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                         SWP_NOZORDER | SWP_NOOWNERZORDER };
            fatal_if_false(SetWindowPos(window(), null, 0, 0, 0, 0, swp));
            enum DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
            fatal_if_not_zero(DwmSetWindowAttribute(window(),
                DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp)));
        }
        app.is_full_screen = on;
    }
}

static void app_fast_redraw(void) { SetEvent(app_event_invalidate); } // < 2us

static void app_draw(void) { UpdateWindow(window()); }

static void app_invalidate_rect(const ui_rect_t* r) {
    RECT rc = app_ui2rect(r);
    InvalidateRect(window(), &rc, false);
}

// InvalidateRect() may wait for up to 30 milliseconds
// which is unacceptable for video drawing at monitor
// refresh rate

static void app_redraw_thread(void* unused(p)) {
    threads.realtime();
    threads.name("app.redraw");
    for (;;) {
        event_t es[] = { app_event_invalidate, app_event_quit };
        int32_t r = events.wait_any(countof(es), es);
        if (r == 0) {
            if (window() != null) {
                InvalidateRect(window(), null, false);
            }
        } else {
            break;
        }
    }
}

static int32_t app_message_loop(void) {
    MSG msg = {0};
    while (GetMessage(&msg, null, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    assert(msg.message == WM_QUIT);
    return (int32_t)msg.wParam;
}

static void app_dispose(void) {
    app_dispose_fonts();
    if (gdi.clip != null) { DeleteRgn(gdi.clip); }
    fatal_if_false(CloseHandle(app_event_quit));
    fatal_if_false(CloseHandle(app_event_invalidate));
}

static void app_cursor_set(cursor_t c) {
    // https://docs.microsoft.com/en-us/windows/win32/menurc/using-cursors
    app.cursor = c;
    SetClassLongPtr(window(), GCLP_HCURSOR, (LONG_PTR)c);
    POINT pt = {0};
    if (GetCursorPos(&pt)) { SetCursorPos(pt.x + 1, pt.y); SetCursorPos(pt.x, pt.y); }
}

static void app_close_window(void) {
    app_post_message(WM_CLOSE, 0, 0);
}

static void app_quit(int32_t exit_code) {
    app.exit_code = exit_code;
    if (app.can_close != null) {
        (void)app.can_close(); // and deliberately ignore result
    }
    app.can_close = null; // will not be called again
    app.close(); // close and destroy app only window
}

static void app_show_tooltip_or_toast(view_t* ui, int32_t x, int32_t y, double timeout) {
    if (ui != null) {
        app_toast.x = x;
        app_toast.y = y;
        if (ui->tag == uic_tag_messagebox) {
            ((uic_messagebox_t*)ui)->option = -1;
        }
        // allow unparented ui for toast and tooltip
        if (ui->init != null) { ui->init(ui); ui->init = null; }
        ui->localize(ui);
        app_animate_start(app_toast_dim, toast_steps);
        app_toast.ui = ui;
        app_toast.ui->font = &app.fonts.H1;
        app_toast.time = timeout > 0 ? app.now + timeout : 0;
    } else {
        app_toast_cancel();
    }
}

static void app_show_toast(view_t* ui, double timeout) {
    app_show_tooltip_or_toast(ui, -1, -1, timeout);
}

static void app_show_tooltip(view_t* ui, int32_t x, int32_t y, double timeout) {
    if (ui != null) {
        app_show_tooltip_or_toast(ui, x, y, timeout);
    } else if (app_toast.ui != null && app_toast.x >= 0 && app_toast.y >= 0) {
        app_toast_cancel(); // only cancel tooltips not toasts
    }
}

static void app_formatted_vtoast(double timeout, const char* format, va_list vl) {
    app_show_toast(null, 0);
    static uic_text_t txt;
    uic_text_vinit(&txt, format, vl);
    txt.multiline = true;
    app_show_toast(&txt.ui, timeout);
}

static void app_formatted_toast(double timeout, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    app_formatted_vtoast(timeout, format, vl);
    va_end(vl);
}

static void app_create_caret(int32_t w, int32_t h) {
    fatal_if_false(CreateCaret(window(), null, w, h));
    assert(GetSystemMetrics(SM_CARETBLINKINGENABLED));
}

static void app_show_caret(void) {
    fatal_if_false(ShowCaret(window()));
}

static void app_move_caret(int32_t x, int32_t y) {
    fatal_if_false(SetCaretPos(x, y));
}

static void app_hide_caret(void) {
    fatal_if_false(HideCaret(window()));
}

static void app_destroy_caret(void) {
    fatal_if_false(DestroyCaret());
}

static void app_enable_sys_command_close(void) {
    EnableMenuItem(GetSystemMenu(GetConsoleWindow(), false),
        SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
}

static void app_console_disable_close(void) {
    EnableMenuItem(GetSystemMenu(GetConsoleWindow(), false),
        SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    (void)freopen("CONOUT$", "w", stdout);
    (void)freopen("CONOUT$", "w", stderr);
    atexit(app_enable_sys_command_close);
}

static int app_console_attach(void) {
    int r = AttachConsole(ATTACH_PARENT_PROCESS) ? 0 : runtime.err();
    if (r == 0) {
        app_console_disable_close();
        threads.sleep_for(0.1); // give cmd.exe a chance to print prompt again
        printf("\n");
    }
    return r;
}

static bool app_is_stdout_redirected(void) {
    // https://stackoverflow.com/questions/30126490/how-to-check-if-stdout-is-redirected-to-a-file-or-to-a-console
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD type = out == null ? FILE_TYPE_UNKNOWN : GetFileType(out);
    type &= ~FILE_TYPE_REMOTE;
    // FILE_TYPE_DISK or FILE_TYPE_CHAR or FILE_TYPE_PIPE
    return type != FILE_TYPE_UNKNOWN;
}

static bool app_is_console_visible(void) {
    HWND cw = GetConsoleWindow();
    return cw != null && IsWindowVisible(cw);
}

static int app_set_console_size(int16_t w, int16_t h) {
    // width/height in characters
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFOEX info = { sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) };
    int r = GetConsoleScreenBufferInfoEx(console, &info) ? 0 : runtime.err();
    if (r != 0) {
        traceln("GetConsoleScreenBufferInfoEx() %s", str.error(r));
    } else {
        // tricky because correct order of the calls
        // SetConsoleWindowInfo() SetConsoleScreenBufferSize() depends on
        // current Window Size (in pixels) ConsoleWindowSize(in characters)
        // and SetConsoleScreenBufferSize().
        // After a lot of experimentation and reading docs most sensible option
        // is to try both calls in two differen orders.
        COORD c = {w, h};
        SMALL_RECT const minwin = { 0, 0, c.X - 1, c.Y - 1 };
        c.Y = 9001; // maximum buffer number of rows at the moment of implementation
        int r0 = SetConsoleWindowInfo(console, true, &minwin) ? 0 : runtime.err();
//      if (r0 != 0) { traceln("SetConsoleWindowInfo() %s", str.error(r0)); }
        int r1 = SetConsoleScreenBufferSize(console, c) ? 0 : runtime.err();
//      if (r1 != 0) { traceln("SetConsoleScreenBufferSize() %s", str.error(r1)); }
        if (r0 != 0 || r1 != 0) { // try in reverse order (which expected to work):
            r0 = SetConsoleScreenBufferSize(console, c) ? 0 : runtime.err();
            if (r0 != 0) { traceln("SetConsoleScreenBufferSize() %s", str.error(r0)); }
            r1 = SetConsoleWindowInfo(console, true, &minwin) ? 0 : runtime.err();
            if (r1 != 0) { traceln("SetConsoleWindowInfo() %s", str.error(r1)); }
	    }
        r = r0 == 0 ? r1 : r0; // first of two errors
    }
    return r;
}

static void app_console_largest(void) {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    // User have to manuall uncheck "[x] Let system position window" in console
    // Properties -> Layout -> Window Position because I did not find the way
    // to programmatically unchecked it.
    // commented code below does not work.
    // see: https://www.os2museum.com/wp/disabling-quick-edit-mode/
    // and: https://learn.microsoft.com/en-us/windows/console/setconsolemode
    /* DOES NOT WORK:
    DWORD mode = 0;
    r = GetConsoleMode(console, &mode) ? 0 : runtime.err();
    fatal_if_not_zero(r, "GetConsoleMode() %s", str.error(r));
    mode &= ~ENABLE_AUTO_POSITION;
    r = SetConsoleMode(console, &mode) ? 0 : runtime.err();
    fatal_if_not_zero(r, "SetConsoleMode() %s", str.error(r));
    */
    CONSOLE_SCREEN_BUFFER_INFOEX info = { sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) };
    int r = GetConsoleScreenBufferInfoEx(console, &info) ? 0 : runtime.err();
    fatal_if_not_zero(r, "GetConsoleScreenBufferInfoEx() %s", str.error(r));
    COORD c = GetLargestConsoleWindowSize(console);
    if (c.X > 80) { c.X &= ~0x7; }
    if (c.Y > 24) { c.Y &= ~0x3; }
    if (c.X > 80) { c.X -= 8; }
    if (c.Y > 24) { c.Y -= 4; }
    app_set_console_size(c.X, c.Y);
    r = GetConsoleScreenBufferInfoEx(console, &info) ? 0 : runtime.err();
    fatal_if_not_zero(r, "GetConsoleScreenBufferInfoEx() %s", str.error(r));
    info.dwSize.Y = 9999; // maximum value at the moment of implementation
    r = SetConsoleScreenBufferInfoEx(console, &info) ? 0 : runtime.err();
    fatal_if_not_zero(r, "SetConsoleScreenBufferInfoEx() %s", str.error(r));
    app_save_console_pos();
}

static void window_foreground(void* w) {
    // SetForegroundWindow() does not activate window:
    fatal_if_false(SetForegroundWindow((HWND)w));
}

static void window_activate(void* w) {
    runtime.seterr(0);
    w = SetActiveWindow((HWND)w); // w previous active window
    if (w == null) { fatal_if_not_zero(runtime.err()); }
}

static void window_make_topmost(void* w) {
    //  Places the window above all non-topmost windows.
    // The window maintains its topmost position even when it is deactivated.
    enum { swp = SWP_SHOWWINDOW | SWP_NOREPOSITION | SWP_NOMOVE | SWP_NOSIZE };
    fatal_if_false(SetWindowPos((HWND)w, HWND_TOPMOST, 0, 0, 0, 0, swp));
}

static void app_make_topmost(void) {
    window_make_topmost(app.window);
}

static void app_activate(void) {
    window_activate(app.window);
}

static void app_bring_to_foreground(void) {
    window_foreground(app.window);
}

static void app_bring_to_front(void) {
    app.bring_to_foreground();
    app.make_topmost();
    app.bring_to_foreground();
    // because bring_to_foreground() does not activate
    app.activate();
    app.request_focus();
}

static void app_set_console_title(HWND cw) {
    char text[256];
    text[0] = 0;
    GetWindowTextA((HWND)app.window, text, countof(text));
    text[countof(text) - 1] = 0;
    char title[256];
    strprintf(title, "%s - Console", text);
    fatal_if_false(SetWindowTextA(cw, title));
}

static void app_restore_console(int32_t *visibility) {
    HWND cw = GetConsoleWindow();
    if (cw != null) {
        RECT wr = {0};
        GetWindowRect(cw, &wr);
        ui_rect_t rc = app_rect2ui(&wr);
        app_load_console_pos(&rc, visibility);
        if (rc.w > 0 && rc.h > 0) {
//          traceln("%d,%d %dx%d px", rc.x, rc.y, rc.w, rc.h);
            CONSOLE_SCREEN_BUFFER_INFOEX info = {
                sizeof(CONSOLE_SCREEN_BUFFER_INFOEX)
            };
            int32_t r = config.load(app.class_name,
                "console_screen_buffer_infoex", &info, (int)sizeof(info));
            if (r == sizeof(info)) { // 24x80
                SMALL_RECT sr = info.srWindow;
                int16_t w = max(sr.Right - sr.Left + 1, 80);
                int16_t h = max(sr.Bottom - sr.Top + 1, 24);
//              traceln("info: %dx%d", info.dwSize.X, info.dwSize.Y);
//              traceln("%d,%d %dx%d", sr.Left, sr.Top, w, h);
                if (w > 0 && h > 0) { app_set_console_size(w, h); }
    	    }
            // do not resize console window just restore it's position
            enum { swp = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE };
            fatal_if_false(SetWindowPos(cw, null,
                    rc.x, rc.y, rc.w, rc.h, swp));
        } else {
            app_console_largest();
        }
    }
}

static void app_console_show(bool b) {
    HWND cw = GetConsoleWindow();
    if (cw != null && b != app.is_console_visible()) {
        if (app.is_console_visible()) { app_save_console_pos(); }
        if (b) {
            int32_t ignored_visibility = 0;
            app_restore_console(&ignored_visibility);
            app_set_console_title(cw);
        }
        // If the window was previously visible, the return value is nonzero.
        // If the window was previously hidden, the return value is zero.
        bool unused_was_visible = ShowWindow(cw, b ? SW_SHOWNOACTIVATE : SW_HIDE);
        (void)unused_was_visible;
        if (b) { InvalidateRect(cw, null, true); window_activate(cw); }
        app_save_console_pos(); // again after visibility changed
    }
}

static int app_console_create(void) {
    int r = AllocConsole() ? 0 : runtime.err();
    if (r == 0) {
        app_console_disable_close();
        int32_t visibility = 0;
        app_restore_console(&visibility);
        app.console_show(visibility != 0);
    }
    return r;
}

static float app_px2in(int pixels) {
    assert(app.dpi.monitor_raw > 0);
    return app.dpi.monitor_raw > 0 ?
           pixels / (float)app.dpi.monitor_raw : 0;
}

static int32_t app_in2px(float inches) {
    assert(app.dpi.monitor_raw > 0);
    return (int32_t)(inches * app.dpi.monitor_raw + 0.5f);
}

static void app_request_layout(void) {
    app_layout_dirty = true;
    app.redraw();
}

static void app_show_window(int32_t show) {
    assert(window_visibility.hide <= show &&
           show <= window_visibility.force_min);
    // ShowWindow() does not have documented error reporting
    bool was_visible = ShowWindow(window(), show);
    (void)was_visible;
    const bool hiding =
        show == window_visibility.hide ||
        show == window_visibility.minimize ||
        show == window_visibility.show_na ||
        show == window_visibility.min_na;
    if (!hiding) {
        app.bring_to_foreground(); // this does not make it ActiveWindow
        enum { swp = SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOSIZE |
                     SWP_NOREPOSITION | SWP_NOMOVE };
        SetWindowPos(window(), null, 0, 0, 0, 0, swp);
        app.request_focus();
    } else if (show == window_visibility.hide ||
               show == window_visibility.minimize ||
               show == window_visibility.min_na) {
        app_toast_cancel();
    }
}

static const char* app_open_filename(const char* folder,
        const char* pairs[], int32_t n) {
    assert(pairs == null && n == 0 ||
           n >= 2 && n % 2 == 0);
    wchar_t memory[32 * 1024];
    wchar_t* filter = memory;
    if (pairs == null || n == 0) {
        filter = L"All Files\0*\0\0";
    } else {
        int32_t left = countof(memory) - 2;
        wchar_t* s = memory;
        for (int32_t i = 0; i < n; i+= 2) {
            wchar_t* s0 = utf8to16(pairs[i + 0]);
            wchar_t* s1 = utf8to16(pairs[i + 1]);
            int32_t n0 = (int)wcslen(s0);
            int32_t n1 = (int)wcslen(s1);
            assert(n0 > 0 && n1 > 0);
            fatal_if(n0 + n1 + 3 >= left, "too many filters");
            memcpy(s, s0, (n0 + 1) * 2);
            s += n0 + 1;
            left -= n0 + 1;
            memcpy(s, s1, (n1 + 1) * 2);
            s[n1] = 0;
            s += n1 + 1;
            left -= n1 + 1;
        }
        *s++ = 0;
    }
    wchar_t path[MAX_PATH];
    path[0] = 0;
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = (HWND)app.window;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrFilter = filter;
    ofn.lpstrInitialDir = utf8to16(folder);
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    static thread_local char text[MAX_PATH];
    if (GetOpenFileNameW(&ofn) && path[0] != 0) {
        strprintf(text, "%s", utf16to8(path));
    } else {
        text[0] = 0;
    }
    return text;
}

static int32_t clipboard_copy_text(const char* utf8) {
    int r = 0;
    int32_t chars = str.utf16_chars(utf8);
    int32_t bytes = (chars + 1) * 2;
    wchar_t* utf16 = (wchar_t*)malloc(bytes);
    if (utf16 == null) {
        r = ERROR_OUTOFMEMORY;
    } else {
        str.utf8_utf16(utf16, utf8);
        assert(utf16[chars - 1] == 0);
        const int32_t n = (int)wcslen(utf16) + 1;
        r = OpenClipboard(GetDesktopWindow()) ? 0 : GetLastError();
        if (r != 0) { traceln("OpenClipboard() failed %s", str.error(r)); }
        if (r == 0) {
            r = EmptyClipboard() ? 0 : GetLastError();
            if (r != 0) { traceln("EmptyClipboard() failed %s", str.error(r)); }
        }
        void* global = null;
        if (r == 0) {
            global = GlobalAlloc(GMEM_MOVEABLE, n * 2);
            r = global != null ? 0 : GetLastError();
            if (r != 0) { traceln("GlobalAlloc() failed %s", str.error(r)); }
        }
        if (r == 0) {
            char* d = (char*)GlobalLock(global);
            not_null(d);
            memcpy(d, utf16, n * 2);
            r = SetClipboardData(CF_UNICODETEXT, global) ? 0 : GetLastError();
            GlobalUnlock(global);
            if (r != 0) {
                traceln("SetClipboardData() failed %s", str.error(r));
                GlobalFree(global);
            } else {
                // do not free global memory. It's owned by system clipboard now
            }
        }
        if (r == 0) {
            r = CloseClipboard() ? 0 : GetLastError();
            if (r != 0) {
                traceln("CloseClipboard() failed %s", str.error(r));
            }
        }
        free(utf16);
    }
    return r;
}

static int clipboard_text(char* utf8, int32_t* bytes) {
    not_null(bytes);
    int r = OpenClipboard(GetDesktopWindow()) ? 0 : GetLastError();
    if (r != 0) { traceln("OpenClipboard() failed %s", str.error(r)); }
    if (r == 0) {
        HANDLE global = GetClipboardData(CF_UNICODETEXT);
        if (global == null) {
            r = GetLastError();
        } else {
            wchar_t* utf16 = (wchar_t*)GlobalLock(global);
            if (utf16 != null) {
                int32_t utf8_bytes = str.utf8_bytes(utf16);
                if (utf8 != null) {
                    char* decoded = (char*)malloc(utf8_bytes);
                    if (decoded == null) {
                        r = ERROR_OUTOFMEMORY;
                    } else {
                        str.utf16_utf8(decoded, utf16);
                        int32_t n = min(*bytes, utf8_bytes);
                        memcpy(utf8, decoded, n);
                        free(decoded);
                        if (n < utf8_bytes) {
                            r = ERROR_INSUFFICIENT_BUFFER;
                        }
                    }
                }
                *bytes = utf8_bytes;
                GlobalUnlock(global);
            }
        }
        r = CloseClipboard() ? 0 : GetLastError();
    }
    return r;
}

static int clipboard_copy_bitmap(image_t* im) {
    HDC canvas = GetDC(null);
    not_null(canvas);
    HDC src = CreateCompatibleDC(canvas); not_null(src);
    HDC dst = CreateCompatibleDC(canvas); not_null(dst);
    // CreateCompatibleBitmap(dst) will create monochrome bitmap!
    // CreateCompatibleBitmap(canvas) will create display compatible
    HBITMAP bitmap = CreateCompatibleBitmap(canvas, im->w, im->h);
//  HBITMAP bitmap = CreateBitmap(image.w, image.h, 1, 32, null);
    not_null(bitmap);
    HBITMAP s = SelectBitmap(src, im->bitmap); not_null(s);
    HBITMAP d = SelectBitmap(dst, bitmap);     not_null(d);
    POINT pt = { 0 };
    fatal_if_false(SetBrushOrgEx(dst, 0, 0, &pt));
    fatal_if_false(StretchBlt(dst, 0, 0, im->w, im->h, src, 0, 0,
        im->w, im->h, SRCCOPY));
    int r = OpenClipboard(GetDesktopWindow()) ? 0 : GetLastError();
    if (r != 0) { traceln("OpenClipboard() failed %s", str.error(r)); }
    if (r == 0) {
        r = EmptyClipboard() ? 0 : GetLastError();
        if (r != 0) { traceln("EmptyClipboard() failed %s", str.error(r)); }
    }
    if (r == 0) {
        r = SetClipboardData(CF_BITMAP, bitmap) ? 0 : GetLastError();
        if (r != 0) {
            traceln("SetClipboardData() failed %s", str.error(r));
        }
    }
    if (r == 0) {
        r = CloseClipboard() ? 0 : GetLastError();
        if (r != 0) {
            traceln("CloseClipboard() failed %s", str.error(r));
        }
    }
    not_null(SelectBitmap(dst, d));
    not_null(SelectBitmap(src, s));
    fatal_if_false(DeleteBitmap(bitmap));
    fatal_if_false(DeleteDC(dst));
    fatal_if_false(DeleteDC(src));
    fatal_if_false(ReleaseDC(null, canvas));
    return r;
}

const char* app_known_folder(int32_t kf) {
    // known folder ids order must match enum
    static const GUID* kfrid[] = {
        &FOLDERID_Profile,
        &FOLDERID_Desktop,
        &FOLDERID_Documents,
        &FOLDERID_Downloads,
        &FOLDERID_Music,
        &FOLDERID_Pictures,
        &FOLDERID_Videos,
        &FOLDERID_Public,
        &FOLDERID_ProgramFiles,
        &FOLDERID_ProgramData
    };
    static char known_foders[countof(kfrid)][MAX_PATH];
    fatal_if(!(0 <= kf && kf < countof(kfrid)), "invalide kf=%d", kf);
    if (known_foders[kf][0] == 0) {
        wchar_t* path = null;
        fatal_if_not_zero(SHGetKnownFolderPath(kfrid[kf], 0, null, &path));
        strprintf(known_foders[kf], "%s", utf16to8(path));
        CoTaskMemFree(path);
	}
    return known_foders[kf];
}

clipboard_t clipboard = {
    .copy_text = clipboard_copy_text,
    .copy_bitmap = clipboard_copy_bitmap,
    .text = clipboard_text
};

static view_t app_ui;

static void app_init(void) {
    app.ui = &app_ui;
    app.redraw = app_fast_redraw;
    app.draw = app_draw;
    app.px2in = app_px2in;
    app.in2px = app_in2px;
    app.point_in_rect = app_point_in_rect;
    app.intersect_rect = app_intersect_rect;
    app.is_hidden   = app_is_hidden;
    app.is_disabled = app_is_disabled;
    app.is_active = app_is_active,
    app.has_focus = app_has_focus,
    app.request_focus = app_request_focus,
    app.activate = app_activate,
    app.bring_to_foreground = app_bring_to_foreground,
    app.make_topmost = app_make_topmost,
    app.bring_to_front = app_bring_to_front,
    app.measure = app_measure_children;
    app.layout = app_request_layout;
    app.invalidate = app_invalidate_rect;
    app.full_screen = app_full_screen;
    app.set_cursor = app_cursor_set;
    app.close = app_close_window;
    app.quit = app_quit;
    app.set_timer = app_timer_set;
    app.kill_timer = app_timer_kill;
    app.post = app_post_message;
    app.show_window = app_show_window;
    app.show_toast = app_show_toast;
    app.show_tooltip = app_show_tooltip;
    app.vtoast = app_formatted_vtoast;
    app.toast = app_formatted_toast;
    app.create_caret = app_create_caret;
    app.show_caret = app_show_caret;
    app.move_caret = app_move_caret;
    app.hide_caret = app_hide_caret;
    app.destroy_caret = app_destroy_caret;
    app.data_save = app_data_save;
    app.data_size = app_data_size;
    app.data_load = app_data_load;
    app.open_filename = app_open_filename;
    app.known_folder = app_known_folder;
    app.is_stdout_redirected = app_is_stdout_redirected;
    app.is_console_visible = app_is_console_visible;
    app.console_attach = app_console_attach;
    app.console_create = app_console_create;
    app.console_show = app_console_show;
    app_event_quit = events.create();
    app_event_invalidate = events.create();
    app.init();
}

static void __app_windows_init__(void) {
    fatal_if_not_zero(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE));
    not_null(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
    InitCommonControls(); // otherwise GetOpenFileName does not work
    app.dpi.process = GetSystemDpiForProcess(GetCurrentProcess());
    app.dpi.system = GetDpiForSystem(); // default was 96DPI
    // monitor dpi will be reinitialized in load_window_pos
    app.dpi.monitor_effective = app.dpi.system;
    app.dpi.monitor_angular = app.dpi.system;
    app.dpi.monitor_raw = app.dpi.system;
    static const RECT nowhere = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
    ui_rect_t r = app_rect2ui(&nowhere);
    app_update_mi(&r, MONITOR_DEFAULTTOPRIMARY);
    app.dpi.window = app.dpi.monitor_effective;
    app_init_fonts(app.dpi.window); // for default monitor
}

static int app_win_main(void) {
    not_null(app.init);
    __app_windows_init__();
    __gdi_init__();
    app.last_visibility = window_visibility.defau1t;
    app_init();
    int r = 0;
//  app_dump_dpi();
    // "wr" Window Rect in pixels: default is 100,100, wmin, hmin
    int32_t wmin = app.wmin > 0 ?
        app.in2px(app.wmin) : app.work_area.w / 4;
    int32_t hmin = app.hmin > 0 ?
        app.in2px(app.hmin) : app.work_area.h / 4;
    ui_rect_t wr = {100, 100, wmin, hmin};
    int32_t size_frame = GetSystemMetricsForDpi(SM_CXSIZEFRAME, app.dpi.process);
    int32_t caption_height = GetSystemMetricsForDpi(SM_CYCAPTION, app.dpi.process);
    wr.x -= size_frame;
    wr.w += size_frame * 2;
    wr.y -= size_frame + caption_height;
    wr.h += size_frame * 2 + caption_height;
    if (!app_load_window_pos(&wr, &app.last_visibility)) {
        // first time - center window
        wr.x = app.work_area.x + (app.work_area.w - wr.w) / 2;
        wr.y = app.work_area.y + (app.work_area.h - wr.h) / 2;
        app_bring_window_inside_monitor(&app.mrc, &wr);
    }
    app.ui->invalidate  = uic_invalidate;
    app.ui->hidden = true; // start with ui hidden
    app.ui->font = &app.fonts.regular;
    app.ui->w = wr.w - size_frame * 2;
    app.ui->h = wr.h - size_frame * 2 - caption_height;
    app_layout_dirty = true; // layout will be done before first paint
    not_null(app.class_name);
    if (!app.no_ui) {
        app_create_window(wr);
        thread_t thread = threads.start(app_redraw_thread, null);
        r = app_message_loop();
        fatal_if_false(SetEvent(app_event_quit));
        threads.join(thread, -1);
        app_dispose();
        if (r == 0 && app.exit_code != 0) { r = app.exit_code; }
    } else {
        r = app.main();
    }
    if (app.fini != null) { app.fini(); }
    return r;
}

// Simplistic Win32 implementation of national language support.
// Windows NLS family of functions is very complicated and has
// difficult history of LANGID vs LCID etc... See:
// ResolveLocaleName()
// GetThreadLocale()
// SetThreadLocale()
// GetUserDefaultLocaleName()
// WM_SETTINGCHANGE lParam="intl"
// and many others...

enum {
    winnls_str_count_max = 1024,
    winnls_str_mem_max = 64 * winnls_str_count_max
};

static char winnls_strings_memory[winnls_str_mem_max]; // increase if overflows
static char* winnls_strings_free = winnls_strings_memory;
static int32_t winnls_strings_count;
static const char* winnls_ls[winnls_str_count_max]; // localized strings
static const char* winnls_ns[winnls_str_count_max]; // neutral language strings

wchar_t* winnls_load_string(int32_t strid, LANGID langid) {
    assert(0 <= strid && strid < countof(winnls_ns));
    wchar_t* r = null;
    int32_t block = strid / 16 + 1;
    int32_t index  = strid % 16;
    HRSRC res = FindResourceExA(((HMODULE)null), RT_STRING,
        MAKEINTRESOURCE(block), langid);
//  traceln("FindResourceExA(block=%d langid=%04X)=%p", block, langid, res);
    uint8_t* memory = res == null ? null : (uint8_t*)LoadResource(null, res);
    wchar_t* ws = memory == null ? null : (wchar_t*)LockResource(memory);
//  traceln("LockResource(block=%d langid=%04X)=%p", block, langid, ws);
    if (ws != null) {
        for (int32_t i = 0; i < 16 && r == null; i++) {
            if (ws[0] != 0) {
                int32_t count = (int)ws[0];  // String size in characters.
                ws++;
                assert(ws[count - 1] == 0, "use rc.exe /n command line option");
                if (i == index) { // the string has been found
//                  traceln("%04X found %s", langid, utf16to8(ws));
                    r = ws;
                }
                ws += count;
            } else {
                ws++;
            }
        }
    }
    return r;
}

static const char* winnls_save_string(wchar_t* memory) {
    const char* utf8 = utf16to8(memory);
    uintptr_t n = strlen(utf8) + 1;
    assert(n > 1);
    uintptr_t left = countof(winnls_strings_memory) - (
        winnls_strings_free - winnls_strings_memory);
    fatal_if_false(left >= n, "string_memory[] overflow");
    memcpy(winnls_strings_free, utf8, n);
    const char* s = winnls_strings_free;
    winnls_strings_free += n;
    return s;
}

const char* winnls_localize_string(int32_t strid) {
    assert(0 < strid && strid < countof(winnls_ns));
    const char* r = null;
    if (0 < strid && strid < countof(winnls_ns)) {
        if (winnls_ls[strid] != null) {
            r = winnls_ls[strid];
        } else {
            LCID lcid = GetThreadLocale();
            LANGID langid = LANGIDFROMLCID(lcid);
            wchar_t* ws = winnls_load_string(strid, langid);
            if (ws == null) { // try default dialect:
                LANGID primary = PRIMARYLANGID(langid);
                langid = MAKELANGID(primary, SUBLANG_NEUTRAL);
                ws = winnls_load_string(strid, langid);
            }
            if (ws != null) {
                r = winnls_save_string(ws);
                winnls_ls[strid] = r;
            }
        }
    }
    return r;
}

static int32_t winnls_strid(const char* s) {
    int32_t strid = 0;
    for (int32_t i = 1; i < winnls_strings_count && strid == 0; i++) {
        if (winnls_ns[i] != null && strcmp(s, winnls_ns[i]) == 0) {
            strid = i;
            winnls_localize_string(strid); // to save it, ignore result
        }
    }
    return strid;
}

static const char* winnls_string(int32_t strid, const char* defau1t) {
    const char* r = winnls_localize_string(strid);
    return r == null ? defau1t : r;
}

const char* winnls_nls(const char* s) {
    int32_t id = winnls_strid(s);
    return id == 0 ? s : winnls_string(id, s);
}

static const char* winnls_locale(void) {
    wchar_t wln[LOCALE_NAME_MAX_LENGTH + 1];
    LCID lcid = GetThreadLocale();
    int32_t n = LCIDToLocaleName(lcid, wln, countof(wln),
        LOCALE_ALLOW_NEUTRAL_NAMES);
    static char ln[LOCALE_NAME_MAX_LENGTH * 4 + 1];
    ln[0] = 0;
    if (n == 0) {
        // TODO: log error
    } else {
        if (n == 0) {
        } else {
            strprintf(ln, "%s", utf16to8(wln));
        }
    }
    return ln;
}

static void winnls_set_locale(const char* locale) {
    wchar_t rln[LOCALE_NAME_MAX_LENGTH + 1];
    int32_t n = ResolveLocaleName(utf8to16(locale), rln, countof(rln));
    if (n == 0) {
        // TODO: log error
    } else {
        LCID lcid = LocaleNameToLCID(rln, LOCALE_ALLOW_NEUTRAL_NAMES);
        if (lcid == 0) {
            // TODO: log error
        } else {
            fatal_if_false(SetThreadLocale(lcid));
            memset((void*)winnls_ls, 0, sizeof(winnls_ls)); // start all over
        }
    }
}

static void winnls_init(void) {
    LANGID langid = MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);
    for (int32_t strid = 0; strid < countof(winnls_ns); strid += 16) {
        int32_t block = strid / 16 + 1;
        HRSRC res = FindResourceExA(((HMODULE)null), RT_STRING,
            MAKEINTRESOURCE(block), langid);
        uint8_t* memory = res == null ? null : (uint8_t*)LoadResource(null, res);
        wchar_t* ws = memory == null ? null : (wchar_t*)LockResource(memory);
        if (ws == null) { break; }
        for (int32_t i = 0; i < 16; i++) {
            int32_t ix = strid + i;
            uint16_t count = ws[0];
            if (count > 0) {
                ws++;
                fatal_if_false(ws[count - 1] == 0, "use rc.exe /n");
                winnls_ns[ix] = winnls_save_string(ws);
                winnls_strings_count = ix + 1;
//              traceln("ns[%d] := %d \"%s\"", ix, strlen(ns[ix]), ns[ix]);
                ws += count;
            } else {
                ws++;
            }
        }
    }
}

static void __winnls_init__(void) {
    static_assert(countof(winnls_ns) % 16 == 0, "countof(ns) must be multiple of 16");
    static bool ns_initialized;
    if (!ns_initialized) { ns_initialized = true; winnls_init(); }
    app.strid = winnls_strid;
    app.nls = winnls_nls;
    app.string = winnls_string;
    app.locale = winnls_locale;
    app.set_locale = winnls_set_locale;
}

#if !defined(quick_implementation_console)

#pragma warning(disable: 28251) // inconsistent annotations

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, char* command,
        int show_command) {
    app.tid = threads.id();
    fatal_if_not_zero(CoInitializeEx(0, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY));
    // https://learn.microsoft.com/en-us/windows/win32/api/imm/nf-imm-immdisablelegacyime
    ImmDisableLegacyIME();
    // https://developercommunity.visualstudio.com/t/MSCTFdll-timcpp-An-assertion-failure-h/10513796
    ImmDisableIME(0); // temporarely disable IME till MS fixes thta assert
    SetConsoleCP(CP_UTF8);
    __winnls_init__();
    app.visibility = show_command;
    (void)command; // ASCII unused
    const char* cl = utf16to8(GetCommandLineW());
    args.WinMain(cl);
    (void)instance; (void)previous; // unused
    int32_t r = app_win_main();
    args.fini();
    return r;
}

#else

#undef quick_implementation_console

int main(int argc, const char* argv[]) {
    fatal_if_not_zero(CoInitializeEx(0, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY));
    __winnls_init__();
    app.argc = argc;
    app.argv = argv;
    app.tid = threads.id();
    return app.main();
}

#endif quick_implementation_console

end_c

#endif quick_implementation

/* LICENCE

MIT License

Copyright (c) 2021-2022 Dmitry "Leo" Kuznetsov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
