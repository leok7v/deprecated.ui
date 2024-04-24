static void ui_button_every_100ms(ui_view_t* view) { // every 100ms
    assert(view->type == ui_view_button);
    ui_button_t* b = (ui_button_t*)view;
    if (b->armed_until != 0 && app.now > b->armed_until) {
        b->armed_until = 0;
        view->armed = false;
        view->invalidate(view);
    }
}

static void ui_button_paint(ui_view_t* view) {
    assert(view->type == ui_view_button);
    assert(!view->hidden);
    ui_button_t* b = (ui_button_t*)view;
    gdi.push(view->x, view->y);
    bool pressed = (view->armed ^ view->pressed) == 0;
    if (b->armed_until != 0) { pressed = true; }
    int32_t sign = 1 - pressed * 2; // -1, +1
    int32_t w = sign * view->w;
    int32_t h = sign * view->h;
    int32_t x = b->view.x + (int)pressed * view->w;
    int32_t y = b->view.y + (int)pressed * view->h;
    gdi.gradient(x, y, w, h, colors.btn_gradient_darker,
        colors.btn_gradient_dark, true);
    ui_color_t c = view->armed ? colors.btn_armed : view->color;
    if (b->view.hover && !view->armed) { c = colors.btn_hover_highlight; }
    if (view->disabled) { c = colors.btn_disabled; }
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    ui_point_t m = gdi.measure_text(f, ui_view_nls(view));
    gdi.set_text_color(c);
    gdi.x = view->x + (view->w - m.x) / 2;
    gdi.y = view->y + (view->h - m.y) / 2;
    f = gdi.set_font(f);
    gdi.text("%s", ui_view_nls(view));
    gdi.set_font(f);
    const int32_t pw = max(1, view->em.y / 32); // pen width
    ui_color_t color = view->armed ? colors.dkgray4 : colors.gray;
    if (view->hover && !view->armed) { color = colors.blue; }
    if (view->disabled) { color = colors.dkgray1; }
    ui_pen_t p = gdi.create_pen(color, pw);
    gdi.set_pen(p);
    gdi.set_brush(gdi.brush_hollow);
    gdi.rounded(view->x, view->y, view->w, view->h, view->em.y / 4, view->em.y / 4);
    gdi.delete_pen(p);
    gdi.pop();
}

static bool ui_button_hit_test(ui_button_t* b, ui_point_t pt) {
    assert(b->view.type == ui_view_button);
    pt.x -= b->view.x;
    pt.y -= b->view.y;
    return 0 <= pt.x && pt.x < b->view.w && 0 <= pt.y && pt.y < b->view.h;
}

static void ui_button_callback(ui_button_t* b) {
    assert(b->view.type == ui_view_button);
    app.show_tooltip(null, -1, -1, 0);
    if (b->cb != null) { b->cb(b); }
}

static bool ui_is_keyboard_shortcut(ui_view_t* view, int32_t key) {
    // Supported keyboard shortcuts are ASCII characters only for now
    // If there is not focused UI control in Alt+key [Alt] is optional.
    // If there is focused control only Alt+Key is accepted as shortcut
    char ch = 0x20 <= key && key <= 0x7F ? (char)toupper(key) : 0x00;
    bool need_alt = app.focus != null && app.focus != view;
    bool keyboard_shortcut = ch != 0x00 && view->shortcut != 0x00 &&
         (app.alt || !need_alt) && toupper(view->shortcut) == ch;
    return keyboard_shortcut;
}

static void ui_button_trigger(ui_view_t* view) {
    assert(view->type == ui_view_button);
    assert(!view->hidden && !view->disabled);
    ui_button_t* b = (ui_button_t*)view;
    view->armed = true;
    view->invalidate(view);
    app.draw();
    b->armed_until = app.now + 0.250;
    ui_button_callback(b);
    view->invalidate(view);
}

static void ui_button_character(ui_view_t* view, const char* utf8) {
    assert(view->type == ui_view_button);
    assert(!view->hidden && !view->disabled);
    char ch = utf8[0]; // TODO: multibyte shortcuts?
    if (ui_is_keyboard_shortcut(view, ch)) {
        ui_button_trigger(view);
    }
}

static void ui_button_key_pressed(ui_view_t* view, int32_t key) {
    if (app.alt && ui_is_keyboard_shortcut(view, key)) {
//      traceln("key: 0x%02X shortcut: %d", key, ui_is_keyboard_shortcut(view, key));
        ui_button_trigger(view);
    }
}

/* processes mouse clicks and invokes callback  */

static void ui_button_mouse(ui_view_t* view, int32_t message, int32_t flags) {
    assert(view->type == ui_view_button);
    (void)flags; // unused
    assert(!view->hidden && !view->disabled);
    ui_button_t* b = (ui_button_t*)view;
    bool a = view->armed;
    bool on = false;
    if (message == ui.message.left_button_pressed ||
        message == ui.message.right_button_pressed) {
        view->armed = ui_button_hit_test(b, app.mouse);
        if (view->armed) { app.focus = view; }
        if (view->armed) { app.show_tooltip(null, -1, -1, 0); }
    }
    if (message == ui.message.left_button_released ||
        message == ui.message.right_button_released) {
        if (view->armed) { on = ui_button_hit_test(b, app.mouse); }
        view->armed = false;
    }
    if (on) { ui_button_callback(b); }
    if (a != view->armed) { view->invalidate(view); }
}

static void ui_button_measure(ui_view_t* view) {
    assert(view->type == ui_view_button || view->type == ui_view_text);
    ui_view_measure(view);
    const int32_t em2  = max(1, view->em.x / 2);
    view->w = view->w;
    view->h = view->h + em2;
    if (view->w < view->h) { view->w = view->h; }
}

void ui_button_init_(ui_view_t* view) {
    assert(view->type == ui_view_button);
    ui_view_init(view);
    view->mouse       = ui_button_mouse;
    view->measure     = ui_button_measure;
    view->paint       = ui_button_paint;
    view->character   = ui_button_character;
    view->every_100ms = ui_button_every_100ms;
    view->key_pressed = ui_button_key_pressed;
    ui_view_set_text(view, view->text);
    view->localize(view);
    view->color = colors.btn_text;
}

void ui_button_init(ui_button_t* b, const char* label, double ems,
        void (*cb)(ui_button_t* b)) {
    static_assert(offsetof(ui_button_t, view) == 0, "offsetof(.view)");
    b->view.type = ui_view_button;
    strprintf(b->view.text, "%s", label);
    b->cb = cb;
    b->view.width = ems;
    ui_button_init_(&b->view);
}
