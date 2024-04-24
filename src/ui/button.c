static void button_every_100ms(view_t* view) { // every 100ms
    assert(view->tag == uic_tag_button);
    button_t* b = (button_t*)view;
    if (b->armed_until != 0 && app.now > b->armed_until) {
        b->armed_until = 0;
        view->armed = false;
        view->invalidate(view);
    }
}

static void button_paint(view_t* view) {
    assert(view->tag == uic_tag_button);
    assert(!view->hidden);
    button_t* b = (button_t*)view;
    gdi.push(view->x, view->y);
    bool pressed = (view->armed ^ view->pressed) == 0;
    if (b->armed_until != 0) { pressed = true; }
    int32_t sign = 1 - pressed * 2; // -1, +1
    int32_t w = sign * view->w;
    int32_t h = sign * view->h;
    int32_t x = b->ui.x + (int)pressed * view->w;
    int32_t y = b->ui.y + (int)pressed * view->h;
    gdi.gradient(x, y, w, h, colors.btn_gradient_darker,
        colors.btn_gradient_dark, true);
    ui_color_t c = view->armed ? colors.btn_armed : view->color;
    if (b->ui.hover && !view->armed) { c = colors.btn_hover_highlight; }
    if (view->disabled) { c = colors.btn_disabled; }
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    ui_point_t m = gdi.measure_text(f, view_nls(view));
    gdi.set_text_color(c);
    gdi.x = view->x + (view->w - m.x) / 2;
    gdi.y = view->y + (view->h - m.y) / 2;
    f = gdi.set_font(f);
    gdi.text("%s", view_nls(view));
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

static bool button_hit_test(button_t* b, ui_point_t pt) {
    assert(b->ui.tag == uic_tag_button);
    pt.x -= b->ui.x;
    pt.y -= b->ui.y;
    return 0 <= pt.x && pt.x < b->ui.w && 0 <= pt.y && pt.y < b->ui.h;
}

static void button_callback(button_t* b) {
    assert(b->ui.tag == uic_tag_button);
    app.show_tooltip(null, -1, -1, 0);
    if (b->cb != null) { b->cb(b); }
}

static bool uic_is_keyboard_shortcut(view_t* view, int32_t key) {
    // Supported keyboard shortcuts are ASCII characters only for now
    // If there is not focused UI control in Alt+key [Alt] is optional.
    // If there is focused control only Alt+Key is accepted as shortcut
    char ch = 0x20 <= key && key <= 0x7F ? (char)toupper(key) : 0x00;
    bool need_alt = app.focus != null && app.focus != view;
    bool keyboard_shortcut = ch != 0x00 && view->shortcut != 0x00 &&
         (app.alt || !need_alt) && toupper(view->shortcut) == ch;
    return keyboard_shortcut;
}

static void button_trigger(view_t* view) {
    assert(view->tag == uic_tag_button);
    assert(!view->hidden && !view->disabled);
    button_t* b = (button_t*)view;
    view->armed = true;
    view->invalidate(view);
    app.draw();
    b->armed_until = app.now + 0.250;
    button_callback(b);
    view->invalidate(view);
}

static void button_character(view_t* view, const char* utf8) {
    assert(view->tag == uic_tag_button);
    assert(!view->hidden && !view->disabled);
    char ch = utf8[0]; // TODO: multibyte shortcuts?
    if (uic_is_keyboard_shortcut(view, ch)) {
        button_trigger(view);
    }
}

static void button_key_pressed(view_t* view, int32_t key) {
    if (app.alt && uic_is_keyboard_shortcut(view, key)) {
//      traceln("key: 0x%02X shortcut: %d", key, uic_is_keyboard_shortcut(view, key));
        button_trigger(view);
    }
}

/* processes mouse clicks and invokes callback  */

static void button_mouse(view_t* view, int32_t message, int32_t flags) {
    assert(view->tag == uic_tag_button);
    (void)flags; // unused
    assert(!view->hidden && !view->disabled);
    button_t* b = (button_t*)view;
    bool a = view->armed;
    bool on = false;
    if (message == ui.message.left_button_pressed ||
        message == ui.message.right_button_pressed) {
        view->armed = button_hit_test(b, app.mouse);
        if (view->armed) { app.focus = view; }
        if (view->armed) { app.show_tooltip(null, -1, -1, 0); }
    }
    if (message == ui.message.left_button_released ||
        message == ui.message.right_button_released) {
        if (view->armed) { on = button_hit_test(b, app.mouse); }
        view->armed = false;
    }
    if (on) { button_callback(b); }
    if (a != view->armed) { view->invalidate(view); }
}

static void button_measure(view_t* view) {
    assert(view->tag == uic_tag_button || view->tag == uic_tag_text);
    view_measure(view);
    const int32_t em2  = max(1, view->em.x / 2);
    view->w = view->w;
    view->h = view->h + em2;
    if (view->w < view->h) { view->w = view->h; }
}

void _button_init_(view_t* view) {
    assert(view->tag == uic_tag_button);
    view_init(view);
    view->mouse       = button_mouse;
    view->measure     = button_measure;
    view->paint       = button_paint;
    view->character   = button_character;
    view->every_100ms = button_every_100ms;
    view->key_pressed = button_key_pressed;
    view_set_text(view, view->text);
    view->localize(view);
    view->color = colors.btn_text;
}

void button_init(button_t* b, const char* label, double ems,
        void (*cb)(button_t* b)) {
    static_assert(offsetof(button_t, ui) == 0, "offsetof(.ui)");
    b->ui.tag = uic_tag_button;
    strprintf(b->ui.text, "%s", label);
    b->cb = cb;
    b->ui.width = ems;
    _button_init_(&b->ui);
}
