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
    str.sformat(label, count, "%s", view_nls(ui));
    char* s = strstr(label, "___");
    if (s != null) {
        memcpy(s, ui->pressed ? "On " : "Off", 3);
    }
    return app.nls(label);
}

static void  uic_checkbox_measure(view_t* ui) {
    assert(ui->tag == uic_tag_checkbox);
    view_measure(ui);
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
    view_init(ui);
    view_set_text(ui, ui->text);
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
    view_init(&c->ui);
    strprintf(c->ui.text, "%s", label);
    c->ui.width = ems;
    c->cb = cb;
    c->ui.tag = uic_tag_checkbox;
    _uic_checkbox_init_(&c->ui);
}
