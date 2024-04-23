
static void label_paint(view_t* ui) {
    assert(ui->tag == uic_tag_text);
    assert(!ui->hidden);
    label_t* t = (label_t*)ui;
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
        gdi.text("%s", view_nls(ui));
    } else {
        int32_t w = (int)(ui->width * ui->em.x + 0.5);
        gdi.multiline(w == 0 ? -1 : w, "%s", view_nls(ui));
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

static void label_context_menu(view_t* ui) {
    assert(ui->tag == uic_tag_text);
    label_t* t = (label_t*)ui;
    if (!t->label && !view_hidden_or_disabled(ui)) {
        clipboard.copy_text(view_nls(ui));
        static bool first_time = true;
        app.toast(first_time ? 2.15 : 0.75,
            app.nls("Text copied to clipboard"));
        first_time = false;
    }
}

static void label_character(view_t* ui, const char* utf8) {
    assert(ui->tag == uic_tag_text);
    label_t* t = (label_t*)ui;
    if (ui->hover && !view_hidden_or_disabled(ui) && !t->label) {
        char ch = utf8[0];
        // Copy to clipboard works for hover over text
        if ((ch == 3 || ch == 'c' || ch == 'C') && app.ctrl) {
            clipboard.copy_text(view_nls(ui)); // 3 is ASCII for Ctrl+C
        }
    }
}

void _label_init_(view_t* ui) {
    static_assert(offsetof(label_t, ui) == 0, "offsetof(.ui)");
    assert(ui->tag == uic_tag_text);
    view_init(ui);
    if (ui->font == null) { ui->font = &app.fonts.regular; }
    ui->color = colors.text;
    ui->paint = label_paint;
    ui->character = label_character;
    ui->context_menu = label_context_menu;
}

void label_vinit(label_t* t, const char* format, va_list vl) {
    static_assert(offsetof(label_t, ui) == 0, "offsetof(.ui)");
    str.vformat(t->ui.text, countof(t->ui.text), format, vl);
    t->ui.tag = uic_tag_text;
    _label_init_(&t->ui);
}

void label_init(label_t* t, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    label_vinit(t, format, vl);
    va_end(vl);
}

void label_init_ml(label_t* t, double width, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    label_vinit(t, format, vl);
    va_end(vl);
    t->ui.width = width;
    t->multiline = true;
}
