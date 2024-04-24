static int checkbox_paint_on_off(view_t* view) {
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

static const char*  checkbox_on_off_label(view_t* view, char* label, int32_t count)  {
    str.sformat(label, count, "%s", view_nls(view));
    char* s = strstr(label, "___");
    if (s != null) {
        memcpy(s, view->pressed ? "On " : "Off", 3);
    }
    return app.nls(label);
}

static void  checkbox_measure(view_t* view) {
    assert(view->tag == uic_tag_checkbox);
    view_measure(view);
    view->w += view->em.x * 2;
}

static void  checkbox_paint(view_t* view) {
    assert(view->tag == uic_tag_checkbox);
    char text[countof(view->text)];
    const char* label =  checkbox_on_off_label(view, text, countof(text));
    gdi.push(view->x, view->y);
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    ui_font_t font = gdi.set_font(f);
    gdi.x =  checkbox_paint_on_off(view) + view->em.x * 3 / 4;
    gdi.text("%s", label);
    gdi.set_font(font);
    gdi.pop();
}

static void  checkbox_flip(checkbox_t* c) {
    assert(c->ui.tag == uic_tag_checkbox);
    app.redraw();
    c->ui.pressed = !c->ui.pressed;
    if (c->cb != null) { c->cb(c); }
}

static void  checkbox_character(view_t* view, const char* utf8) {
    assert(view->tag == uic_tag_checkbox);
    assert(!view->hidden && !view->disabled);
    char ch = utf8[0];
    if (uic_is_keyboard_shortcut(view, ch)) {
         checkbox_flip((checkbox_t*)view);
    }
}

static void checkbox_key_pressed(view_t* view, int32_t key) {
    if (app.alt && uic_is_keyboard_shortcut(view, key)) {
//      traceln("key: 0x%02X shortcut: %d", key, uic_is_keyboard_shortcut(view, key));
        checkbox_flip((checkbox_t*)view);
    }
}

static void  checkbox_mouse(view_t* view, int32_t message, int32_t flags) {
    assert(view->tag == uic_tag_checkbox);
    (void)flags; // unused
    assert(!view->hidden && !view->disabled);
    if (message == ui.message.left_button_pressed ||
        message == ui.message.right_button_pressed) {
        int32_t x = app.mouse.x - view->x;
        int32_t y = app.mouse.y - view->y;
        if (0 <= x && x < view->w && 0 <= y && y < view->h) {
            app.focus = view;
            checkbox_flip((checkbox_t*)view);
        }
    }
}

void _checkbox_init_(view_t* view) {
    assert(view->tag == uic_tag_checkbox);
    view_init(view);
    view_set_text(view, view->text);
    view->mouse       =  checkbox_mouse;
    view->measure     = checkbox_measure;
    view->paint       = checkbox_paint;
    view->character   = checkbox_character;
    view->key_pressed = checkbox_key_pressed;
    view->localize(view);
    view->color = colors.btn_text;
}

void checkbox_init(checkbox_t* c, const char* label, double ems,
       void (*cb)( checkbox_t* b)) {
    static_assert(offsetof( checkbox_t, ui) == 0, "offsetof(.ui)");
    view_init(&c->ui);
    strprintf(c->ui.text, "%s", label);
    c->ui.width = ems;
    c->cb = cb;
    c->ui.tag = uic_tag_checkbox;
    _checkbox_init_(&c->ui);
}
