static void ui_slider_measure(ui_view_t* view) {
    assert(view->type == ui_view_slider);
    ui_view_measure(view);
    ui_slider_t* r = (ui_slider_t*)view;
    assert(r->inc.view.w == r->dec.view.w && r->inc.view.h == r->dec.view.h);
    const int32_t em = view->em.x;
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    const int32_t w = (int)(view->width * view->em.x);
    r->tm = gdi.measure_text(f, ui_view_nls(view), r->vmax);
    if (w > r->tm.x) { r->tm.x = w; }
    view->w = r->dec.view.w + r->tm.x + r->inc.view.w + em * 2;
    view->h = r->inc.view.h;
}

static void ui_slider_layout(ui_view_t* view) {
    assert(view->type == ui_view_slider);
    ui_slider_t* r = (ui_slider_t*)view;
    assert(r->inc.view.w == r->dec.view.w && r->inc.view.h == r->dec.view.h);
    const int32_t em = view->em.x;
    r->dec.view.x = view->x;
    r->dec.view.y = view->y;
    r->inc.view.x = view->x + r->dec.view.w + r->tm.x + em * 2;
    r->inc.view.y = view->y;
}

static void ui_slider_paint(ui_view_t* view) {
    assert(view->type == ui_view_slider);
    ui_slider_t* r = (ui_slider_t*)view;
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
    const int32_t x = view->x + r->dec.view.w + em2;
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
    gdi.x += r->dec.view.w + em;
    const char* format = app.nls(view->text);
    gdi.text(format, r->value);
    gdi.set_clip(0, 0, 0, 0);
    gdi.delete_pen(pen_grey30);
    gdi.delete_pen(pen_grey45);
    gdi.pop();
}

static void ui_slider_mouse(ui_view_t* view, int32_t message, int32_t f) {
    if (!view->hidden && !view->disabled) {
        assert(view->type == ui_view_slider);
        ui_slider_t* r = (ui_slider_t*)view;
        bool drag = message == ui.message.mouse_move &&
            (f & (ui.mouse.button.left|ui.mouse.button.right)) != 0;
        if (message == ui.message.left_button_pressed ||
            message == ui.message.right_button_pressed || drag) {
            const int32_t x = app.mouse.x - view->x - r->dec.view.w;
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

static void ui_slider_inc_dec_value(ui_slider_t* r, int32_t sign, int32_t mul) {
    if (!r->view.hidden && !r->view.disabled) {
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
            r->view.invalidate(&r->view);
        }
    }
}

static void ui_slider_inc_dec(ui_button_t* b) {
    ui_slider_t* r = (ui_slider_t*)b->view.parent;
    if (!r->view.hidden && !r->view.disabled) {
        int32_t sign = b == &r->inc ? +1 : -1;
        int32_t mul = app.shift && app.ctrl ? 1000 :
            app.shift ? 100 : app.ctrl ? 10 : 1;
        ui_slider_inc_dec_value(r, sign, mul);
    }
}

static void ui_slider_every_100ms(ui_view_t* view) { // 100ms
    assert(view->type == ui_view_slider);
    ui_slider_t* r = (ui_slider_t*)view;
    if (r->view.hidden || r->view.disabled) {
        r->time = 0;
    } else if (!r->dec.view.armed && !r->inc.view.armed) {
        r->time = 0;
    } else {
        if (r->time == 0) {
            r->time = app.now;
        } else if (app.now - r->time > 1.0) {
            const int32_t sign = r->dec.view.armed ? -1 : +1;
            int32_t s = (int)(app.now - r->time + 0.5);
            int32_t mul = s >= 1 ? 1 << (s - 1) : 1;
            const int64_t range = (int64_t)r->vmax - r->vmin;
            if (mul > range / 8) { mul = (int32_t)(range / 8); }
            ui_slider_inc_dec_value(r, sign, max(mul, 1));
        }
    }
}

void ui_slider_init_(ui_view_t* view) {
    assert(view->type == ui_view_slider);
    ui_view_init(view);
    ui_view_set_text(view, view->text);
    view->mouse       = ui_slider_mouse;
    view->measure     = ui_slider_measure;
    view->layout      = ui_slider_layout;
    view->paint       = ui_slider_paint;
    view->every_100ms = ui_slider_every_100ms;
    ui_slider_t* r = (ui_slider_t*)view;
    r->buttons[0] = &r->dec.view;
    r->buttons[1] = &r->inc.view;
    r->buttons[2] = null;
    r->view.children = r->buttons;
    // Heavy Minus Sign
    ui_button_init(&r->dec, "\xE2\x9E\x96", 0, ui_slider_inc_dec);
    // Heavy Plus Sign
    ui_button_init(&r->inc, "\xE2\x9E\x95", 0, ui_slider_inc_dec);
    static const char* accel =
        "Accelerate by holding Ctrl x10 Shift x100 and Ctrl+Shift x1000";
    strprintf(r->inc.view.tip, "%s", accel);
    strprintf(r->dec.view.tip, "%s", accel);
    r->dec.view.parent = &r->view;
    r->inc.view.parent = &r->view;
    r->view.localize(&r->view);
}

void ui_slider_init(ui_slider_t* r, const char* label, double ems,
        int32_t vmin, int32_t vmax, void (*cb)(ui_slider_t* r)) {
    static_assert(offsetof(ui_slider_t, view) == 0, "offsetof(.view)");
    assert(ems >= 3.0, "allow 1em for each of [-] and [+] buttons");
    r->view.type = ui_view_slider;
    strprintf(r->view.text, "%s", label);
    r->cb = cb;
    r->view.width = ems;
    r->vmin = vmin;
    r->vmax = vmax;
    r->value = vmin;
    ui_slider_init_(&r->view);
}
