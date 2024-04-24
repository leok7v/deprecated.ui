static int ui_checkbox_paint_on_off(ui_view_t* view) {
    // https://www.compart.com/en/unicode/U+2B24
    static const char* circle = "\xE2\xAC\xA4"; // Black Large Circle
    gdi.push(view->x, view->y);
    ui_color_t background = view->pressed ? colors.tone_green : colors.dkgray4;
    ui_color_t foreground = view->color;
    gdi.set_text_color(background);
    int32_t x = view->x;
    int32_t x1 = view->x + view->em.x * 3 / 4;
    while (x < x1) {
        gdi.x = x;
        gdi.text("%s", circle);
        x++;
    }
    int32_t rx = gdi.x;
    gdi.set_text_color(foreground);
    gdi.x = view->pressed ? x : view->x;
    gdi.text("%s", circle);
    gdi.pop();
    return rx;
}

static const char* ui_checkbox_on_off_label(ui_view_t* view, char* label, int32_t count)  {
    str.sformat(label, count, "%s", ui_view_nls(view));
    char* s = strstr(label, "___");
    if (s != null) {
        memcpy(s, view->pressed ? "On " : "Off", 3);
    }
    return app.nls(label);
}

static void ui_checkbox_measure(ui_view_t* view) {
    assert(view->type == ui_view_checkbox);
    ui_view_measure(view);
    view->w += view->em.x * 2;
}

static void ui_checkbox_paint(ui_view_t* view) {
    assert(view->type == ui_view_checkbox);
    char text[countof(view->text)];
    const char* label = ui_checkbox_on_off_label(view, text, countof(text));
    gdi.push(view->x, view->y);
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    ui_font_t font = gdi.set_font(f);
    gdi.x = ui_checkbox_paint_on_off(view) + view->em.x * 3 / 4;
    gdi.text("%s", label);
    gdi.set_font(font);
    gdi.pop();
}

static void ui_checkbox_flip(checkbox_t* c) {
    assert(c->view.type == ui_view_checkbox);
    app.redraw();
    c->view.pressed = !c->view.pressed;
    if (c->cb != null) { c->cb(c); }
}

static void ui_checkbox_character(ui_view_t* view, const char* utf8) {
    assert(view->type == ui_view_checkbox);
    assert(!view->hidden && !view->disabled);
    char ch = utf8[0];
    if (ui_is_keyboard_shortcut(view, ch)) {
         ui_checkbox_flip((checkbox_t*)view);
    }
}

static void ui_checkbox_key_pressed(ui_view_t* view, int32_t key) {
    if (app.alt && ui_is_keyboard_shortcut(view, key)) {
//      traceln("key: 0x%02X shortcut: %d", key, ui_is_keyboard_shortcut(view, key));
        ui_checkbox_flip((checkbox_t*)view);
    }
}

static void ui_checkbox_mouse(ui_view_t* view, int32_t message, int32_t flags) {
    assert(view->type == ui_view_checkbox);
    (void)flags; // unused
    assert(!view->hidden && !view->disabled);
    if (message == ui.message.left_button_pressed ||
        message == ui.message.right_button_pressed) {
        int32_t x = app.mouse.x - view->x;
        int32_t y = app.mouse.y - view->y;
        if (0 <= x && x < view->w && 0 <= y && y < view->h) {
            app.focus = view;
            ui_checkbox_flip((checkbox_t*)view);
        }
    }
}

void ui_checkbox_init_(ui_view_t* view) {
    assert(view->type == ui_view_checkbox);
    ui_view_init(view);
    ui_view_set_text(view, view->text);
    view->mouse       = ui_checkbox_mouse;
    view->measure     = ui_checkbox_measure;
    view->paint       = ui_checkbox_paint;
    view->character   = ui_checkbox_character;
    view->key_pressed = ui_checkbox_key_pressed;
    view->localize(view);
    view->color = colors.btn_text;
}

void ui_checkbox_init(checkbox_t* c, const char* label, double ems,
       void (*cb)( checkbox_t* b)) {
    static_assert(offsetof( checkbox_t, view) == 0, "offsetof(.view)");
    ui_view_init(&c->view);
    strprintf(c->view.text, "%s", label);
    c->view.width = ems;
    c->cb = cb;
    c->view.type = ui_view_checkbox;
    ui_checkbox_init_(&c->view);
}
