static void button_every_100ms(view_t* ui) { // every 100ms
    assert(ui->tag == uic_tag_button);
    button_t* b = (button_t*)ui;
    if (b->armed_until != 0 && app.now > b->armed_until) {
        b->armed_until = 0;
        ui->armed = false;
        ui->invalidate(ui);
    }
}

static void button_paint(view_t* ui) {
    assert(ui->tag == uic_tag_button);
    assert(!ui->hidden);
    button_t* b = (button_t*)ui;
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
    ui_point_t m = gdi.measure_text(f, view_nls(ui));
    gdi.set_text_color(c);
    gdi.x = ui->x + (ui->w - m.x) / 2;
    gdi.y = ui->y + (ui->h - m.y) / 2;
    f = gdi.set_font(f);
    gdi.text("%s", view_nls(ui));
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

static void button_trigger(view_t* ui) {
    assert(ui->tag == uic_tag_button);
    assert(!ui->hidden && !ui->disabled);
    button_t* b = (button_t*)ui;
    ui->armed = true;
    ui->invalidate(ui);
    app.draw();
    b->armed_until = app.now + 0.250;
    button_callback(b);
    ui->invalidate(ui);
}

static void button_character(view_t* ui, const char* utf8) {
    assert(ui->tag == uic_tag_button);
    assert(!ui->hidden && !ui->disabled);
    char ch = utf8[0]; // TODO: multibyte shortcuts?
    if (uic_is_keyboard_shortcut(ui, ch)) {
        button_trigger(ui);
    }
}

static void button_key_pressed(view_t* ui, int32_t key) {
    if (app.alt && uic_is_keyboard_shortcut(ui, key)) {
//      traceln("key: 0x%02X shortcut: %d", key, uic_is_keyboard_shortcut(ui, key));
        button_trigger(ui);
    }
}

/* processes mouse clicks and invokes callback  */

static void button_mouse(view_t* ui, int32_t message, int32_t flags) {
    assert(ui->tag == uic_tag_button);
    (void)flags; // unused
    assert(!ui->hidden && !ui->disabled);
    button_t* b = (button_t*)ui;
    bool a = ui->armed;
    bool on = false;
    if (message == messages.left_button_pressed ||
        message == messages.right_button_pressed) {
        ui->armed = button_hit_test(b, app.mouse);
        if (ui->armed) { app.focus = ui; }
        if (ui->armed) { app.show_tooltip(null, -1, -1, 0); }
    }
    if (message == messages.left_button_released ||
        message == messages.right_button_released) {
        if (ui->armed) { on = button_hit_test(b, app.mouse); }
        ui->armed = false;
    }
    if (on) { button_callback(b); }
    if (a != ui->armed) { ui->invalidate(ui); }
}

static void button_measure(view_t* ui) {
    assert(ui->tag == uic_tag_button || ui->tag == uic_tag_text);
    view_measure(ui);
    const int32_t em2  = max(1, ui->em.x / 2);
    ui->w = ui->w;
    ui->h = ui->h + em2;
    if (ui->w < ui->h) { ui->w = ui->h; }
}

void _button_init_(view_t* ui) {
    assert(ui->tag == uic_tag_button);
    view_init(ui);
    ui->mouse       = button_mouse;
    ui->measure     = button_measure;
    ui->paint       = button_paint;
    ui->character   = button_character;
    ui->every_100ms = button_every_100ms;
    ui->key_pressed = button_key_pressed;
    view_set_text(ui, ui->text);
    ui->localize(ui);
    ui->color = colors.btn_text;
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
