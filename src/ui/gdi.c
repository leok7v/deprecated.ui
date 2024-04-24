typedef struct gdi_xyc_s {
    int32_t x;
    int32_t y;
    ui_color_t c;
} gdi_xyc_t;

static int32_t gdi_top;
static gdi_xyc_t gdi_stack[256];

static void __gdi_init__(void) {
    gdi.brush_hollow = (ui_brush_t)GetStockBrush(HOLLOW_BRUSH);
    gdi.brush_color  = (ui_brush_t)GetStockBrush(DC_BRUSH);
    gdi.pen_hollow = (ui_pen_t)GetStockPen(NULL_PEN);
}

static inline COLORREF gdi_color_ref(ui_color_t c) {
    assert(color_is_8bit(c));
    return (COLORREF)(c & 0xFFFFFFFF);
}

static ui_color_t gdi_set_text_color(ui_color_t c) {
    return SetTextColor(canvas(), gdi_color_ref(c));
}

static ui_pen_t gdi_set_pen(ui_pen_t p) {
    not_null(p);
    return (ui_pen_t)SelectPen(canvas(), (HPEN)p);
}

static ui_pen_t gdi_set_colored_pen(ui_color_t c) {
    ui_pen_t p = (ui_pen_t)SelectPen(canvas(), GetStockPen(DC_PEN));
    SetDCPenColor(canvas(), gdi_color_ref(c));
    return p;
}

static ui_pen_t gdi_create_pen(ui_color_t c, int32_t width) {
    assert(width >= 1);
    ui_pen_t pen = (ui_pen_t)CreatePen(PS_SOLID, width, gdi_color_ref(c));
    not_null(pen);
    return pen;
}

static void gdi_delete_pen(ui_pen_t p) {
    fatal_if_false(DeletePen(p));
}

static ui_brush_t gdi_create_brush(ui_color_t c) {
    return (ui_brush_t)CreateSolidBrush(gdi_color_ref(c));
}

static void gdi_delete_brush(ui_brush_t b) {
    DeleteBrush((HBRUSH)b);
}

static ui_brush_t gdi_set_brush(ui_brush_t b) {
    not_null(b);
    return (ui_brush_t)SelectBrush(canvas(), b);
}

static ui_color_t gdi_set_brush_color(ui_color_t c) {
    return SetDCBrushColor(canvas(), gdi_color_ref(c));
}

static void gdi_set_clip(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (gdi.clip != null) { DeleteRgn(gdi.clip); gdi.clip = null; }
    if (w > 0 && h > 0) {
        gdi.clip = (ui_region_t)CreateRectRgn(x, y, x + w, y + h);
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

static void gdi_pixel(int32_t x, int32_t y, ui_color_t c) {
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
    ui_brush_t b = gdi.set_brush(gdi.brush_hollow);
    gdi.rect(x, y, w, h);
    gdi.set_brush(b);
}

static void gdi_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    fatal_if_false(Rectangle(canvas(), x, y, x + w, y + h));
}

static void gdi_fill(int32_t x, int32_t y, int32_t w, int32_t h) {
    RECT rc = { x, y, x + w, y + h };
    ui_brush_t b = (ui_brush_t)GetCurrentObject(canvas(), OBJ_BRUSH);
    fatal_if_false(FillRect(canvas(), &rc, (HBRUSH)b));
}

static void gdi_frame_with(int32_t x, int32_t y, int32_t w, int32_t h,
        ui_color_t c) {
    ui_brush_t b = gdi.set_brush(gdi.brush_hollow);
    ui_pen_t p = gdi.set_colored_pen(c);
    gdi.rect(x, y, w, h);
    gdi.set_pen(p);
    gdi.set_brush(b);
}

static void gdi_rect_with(int32_t x, int32_t y, int32_t w, int32_t h,
        ui_color_t border, ui_color_t fill) {
    ui_brush_t b = gdi.set_brush(gdi.brush_color);
    ui_color_t c = gdi.set_brush_color(fill);
    ui_pen_t p = gdi.set_colored_pen(border);
    gdi.rect(x, y, w, h);
    gdi.set_brush_color(c);
    gdi.set_pen(p);
    gdi.set_brush(b);
}

static void gdi_fill_with(int32_t x, int32_t y, int32_t w, int32_t h,
        ui_color_t c) {
    ui_brush_t b = gdi.set_brush(gdi.brush_color);
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
        ui_color_t rgba_from, ui_color_t rgba_to, bool vertical) {
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
    image->bitmap = (ui_bitmap_t)CreateDIBSection(c, gdi_init_bitmap_info(w, h, bpp, bi),
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

static ui_font_t gdi_font(ui_font_t f, int32_t height, int32_t quality) {
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
    return (ui_font_t)CreateFontIndirectA(&lf);
}

static int32_t gdi_font_height(ui_font_t f) {
    assert(f != null);
    LOGFONTA lf = {0};
    int32_t n = GetObjectA(f, sizeof(lf), &lf);
    fatal_if_false(n == (int)sizeof(lf));
    assert(lf.lfHeight < 0);
    return abs(lf.lfHeight);
}

static void gdi_delete_font(ui_font_t f) {
    fatal_if_false(DeleteFont(f));
}

static ui_font_t gdi_set_font(ui_font_t f) {
    not_null(f);
    return (ui_font_t)SelectFont(canvas(), (HFONT)f);
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


static int32_t gdi_baseline(ui_font_t f) {
    TEXTMETRICA tm;
    gdi_hdc_with_font(f, {
        fatal_if_false(GetTextMetricsA(hdc, &tm));
    })
    return tm.tmAscent;
}

static int32_t gdi_descent(ui_font_t f) {
    TEXTMETRICA tm;
    gdi_hdc_with_font(f, {
        fatal_if_false(GetTextMetricsA(hdc, &tm));
    });
    return tm.tmDescent;
}

static ui_point_t gdi_get_em(ui_font_t f) {
    SIZE cell = {0};
    gdi_hdc_with_font(f, {
        fatal_if_false(GetTextExtentPoint32A(hdc, "M", 1, &cell));
    });
    ui_point_t c = {cell.cx, cell.cy};
    return c;
}

static bool gdi_is_mono(ui_font_t f) {
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

static int32_t gdi_draw_utf16(ui_font_t font, const char* s, int32_t n,
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
    ui_font_t font;
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

static ui_point_t gdi_measure_singleline(ui_font_t f, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    gdi_dtp_t p = { f, format, vl, {0, 0, 0, 0}, sl_measure };
    ui_point_t cell = gdi_text_measure(&p);
    va_end(vl);
    return cell;
}

static ui_point_t gdi_measure_multiline(ui_font_t f, int32_t w, const char* format, ...) {
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
    ui_font_t f = gdi.set_font(app.fonts.mono);
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
    ui_font_t f = gdi.set_font(app.fonts.mono);
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
