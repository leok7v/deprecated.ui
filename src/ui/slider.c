static void slider_measure(view_t* view) {
    assert(view->tag == uic_tag_slider);
    view_measure(view);
    slider_t* r = (slider_t*)view;
    assert(r->inc.ui.w == r->dec.ui.w && r->inc.ui.h == r->dec.ui.h);
    const int32_t em = view->em.x;
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    const int32_t w = (int)(view->width * view->em.x);
    r->tm = gdi.measure_text(f, view_nls(view), r->vmax);
    if (w > r->tm.x) { r->tm.x = w; }
    view->w = r->dec.ui.w + r->tm.x + r->inc.ui.w + em * 2;
    view->h = r->inc.ui.h;
}

static void slider_layout(view_t* view) {
    assert(view->tag == uic_tag_slider);
    slider_t* r = (slider_t*)view;
    assert(r->inc.ui.w == r->dec.ui.w && r->inc.ui.h == r->dec.ui.h);
    const int32_t em = view->em.x;
    r->dec.ui.x = view->x;
    r->dec.ui.y = view->y;
    r->inc.ui.x = view->x + r->dec.ui.w + r->tm.x + em * 2;
    r->inc.ui.y = view->y;
}

static void slider_paint(view_t* view) {
    assert(view->tag == uic_tag_slider);
    slider_t* r = (slider_t*)view;
    gdi.push(view->x, view->y);
    gdi.set_clip(view->x, view->y, view->w, view->h);
    const int32_t em = view->em.x;
    const int32_t em2  = max(1, em / 2);
    const int32_t em4  = max(1, em / 8);
    const int32_t em8  = max(1, em / 8);
    const int32_t em16 = max(1, em / 16);
    gdi.set_brush(gdi.brush_color);
    ui_pen_t pen_grey45 = gdi.create_pen(colors.dkgray3, em16);
    gdi.set_pen(pen_grey45);
    gdi.set_brush_color(colors.dkgray3);
    const int32_t x = view->x + r->dec.ui.w + em2;
    const int32_t y = view->y;
    const int32_t w = r->tm.x + em;
    const int32_t h = view->h;
    gdi.rounded(x - em8, y, w + em4, h, em4, em4);
    gdi.gradient(x, y, w, h / 2,
        colors.dkgray3, colors.btn_gradient_darker, true);
    gdi.gradient(x, y + h / 2, w, view->h - h / 2,
        colors.btn_gradient_darker, colors.dkgray3, true);
    gdi.set_brush_color(colors.dkgreen);
    ui_pen_t pen_grey30 = gdi.create_pen(colors.dkgray1, em16);
    gdi.set_pen(pen_grey30);
    const double range = (double)r->vmax - (double)r->vmin;
    double vw = (double)(r->tm.x + em) * (r->value - r->vmin) / range;
    gdi.rect(x, view->y, (int32_t)(vw + 0.5), view->h);
    gdi.x += r->dec.ui.w + em;
    const char* format = app.nls(view->text);
    gdi.text(format, r->value);
    gdi.set_clip(0, 0, 0, 0);
    gdi.delete_pen(pen_grey30);
    gdi.delete_pen(pen_grey45);
    gdi.pop();
}

static void slider_mouse(view_t* view, int32_t message, int32_t f) {
    if (!view->hidden && !view->disabled) {
        assert(view->tag == uic_tag_slider);
        slider_t* r = (slider_t*)view;
        bool drag = message == ui.message.mouse_move &&
            (f & (ui.mouse.button.left|ui.mouse.button.right)) != 0;
        if (message == ui.message.left_button_pressed ||
            message == ui.message.right_button_pressed || drag) {
            const int32_t x = app.mouse.x - view->x - r->dec.ui.w;
            const int32_t y = app.mouse.y - view->y;
            const int32_t x0 = view->em.x / 2;
            const int32_t x1 = r->tm.x + view->em.x;
            if (x0 <= x && x < x1 && 0 <= y && y < view->h) {
                app.focus = view;
                const double range = (double)r->vmax - (double)r->vmin;
                double v = ((double)x - x0) * range / (double)(x1 - x0 - 1);
                int32_t vw = (int32_t)(v + r->vmin + 0.5);
                r->value = min(max(vw, r->vmin), r->vmax);
                if (r->cb != null) { r->cb(r); }
                view->invalidate(view);
            }
        }
    }
}

static void slider_inc_dec_value(slider_t* r, int32_t sign, int32_t mul) {
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

static void slider_inc_dec(button_t* b) {
    slider_t* r = (slider_t*)b->ui.parent;
    if (!r->ui.hidden && !r->ui.disabled) {
        int32_t sign = b == &r->inc ? +1 : -1;
        int32_t mul = app.shift && app.ctrl ? 1000 :
            app.shift ? 100 : app.ctrl ? 10 : 1;
        slider_inc_dec_value(r, sign, mul);
    }
}

static void slider_every_100ms(view_t* view) { // 100ms
    assert(view->tag == uic_tag_slider);
    slider_t* r = (slider_t*)view;
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
            slider_inc_dec_value(r, sign, max(mul, 1));
        }
    }
}

void _slider_init_(view_t* view) {
    assert(view->tag == uic_tag_slider);
    view_init(view);
    view_set_text(view, view->text);
    view->mouse        = slider_mouse;
    view->measure      = slider_measure;
    view->layout       = slider_layout;
    view->paint        = slider_paint;
    view->every_100ms = slider_every_100ms;
    slider_t* r = (slider_t*)view;
    r->buttons[0] = &r->dec.ui;
    r->buttons[1] = &r->inc.ui;
    r->buttons[2] = null;
    r->ui.children = r->buttons;
    // Heavy Minus Sign
    button_init(&r->dec, "\xE2\x9E\x96", 0, slider_inc_dec);
    // Heavy Plus Sign
    button_init(&r->inc, "\xE2\x9E\x95", 0, slider_inc_dec);
    static const char* accel =
        "Accelerate by holding Ctrl x10 Shift x100 and Ctrl+Shift x1000";
    strprintf(r->inc.ui.tip, "%s", accel);
    strprintf(r->dec.ui.tip, "%s", accel);
    r->dec.ui.parent = &r->ui;
    r->inc.ui.parent = &r->ui;
    r->ui.localize(&r->ui);
}

void slider_init(slider_t* r, const char* label, double ems,
        int32_t vmin, int32_t vmax, void (*cb)(slider_t* r)) {
    static_assert(offsetof(slider_t, ui) == 0, "offsetof(.ui)");
    assert(ems >= 3.0, "allow 1em for each of [-] and [+] buttons");
    r->ui.tag = uic_tag_slider;
    strprintf(r->ui.text, "%s", label);
    r->cb = cb;
    r->ui.width = ems;
    r->vmin = vmin;
    r->vmax = vmax;
    r->value = vmin;
    _slider_init_(&r->ui);
}
