
static void label_paint(view_t* view) {
    assert(view->tag == uic_tag_text);
    assert(!view->hidden);
    label_t* t = (label_t*)view;
    // at later stages of layout text height can grow:
    gdi.push(view->x, view->y + t->dy);
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    gdi.set_font(f);
//  traceln("%s h=%d dy=%d baseline=%d", view->text, view->h, t->dy, view->baseline);
    ui_color_t c = view->hover && t->highlight && !t->label ?
        colors.text_highlight : view->color;
    gdi.set_text_color(c);
    // paint for text also does lightweight re-layout
    // which is useful for simplifying dynamic text changes
    if (!t->multiline) {
        gdi.text("%s", view_nls(view));
    } else {
        int32_t w = (int)(view->width * view->em.x + 0.5);
        gdi.multiline(w == 0 ? -1 : w, "%s", view_nls(view));
    }
    if (view->hover && t->hovered && !t->label) {
        gdi.set_colored_pen(colors.btn_hover_highlight);
        gdi.set_brush(gdi.brush_hollow);
        int32_t cr = view->em.y / 4; // corner radius
        int32_t h = t->multiline ? view->h : view->baseline + view->descent;
        gdi.rounded(view->x - cr, view->y + t->dy, view->w + 2 * cr,
            h, cr, cr);
    }
    gdi.pop();
}

static void label_context_menu(view_t* view) {
    assert(view->tag == uic_tag_text);
    label_t* t = (label_t*)view;
    if (!t->label && !view_hidden_or_disabled(view)) {
        clipboard.copy_text(view_nls(view));
        static bool first_time = true;
        app.toast(first_time ? 2.15 : 0.75,
            app.nls("Text copied to clipboard"));
        first_time = false;
    }
}

static void label_character(view_t* view, const char* utf8) {
    assert(view->tag == uic_tag_text);
    label_t* t = (label_t*)view;
    if (view->hover && !view_hidden_or_disabled(view) && !t->label) {
        char ch = utf8[0];
        // Copy to clipboard works for hover over text
        if ((ch == 3 || ch == 'c' || ch == 'C') && app.ctrl) {
            clipboard.copy_text(view_nls(view)); // 3 is ASCII for Ctrl+C
        }
    }
}

void _label_init_(view_t* view) {
    static_assert(offsetof(label_t, ui) == 0, "offsetof(.ui)");
    assert(view->tag == uic_tag_text);
    view_init(view);
    if (view->font == null) { view->font = &app.fonts.regular; }
    view->color = colors.text;
    view->paint = label_paint;
    view->character = label_character;
    view->context_menu = label_context_menu;
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
