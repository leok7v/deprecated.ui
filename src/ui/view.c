
static void view_invalidate(const view_t* view) {
    ui_rect_t rc = { view->x, view->y, view->w, view->h};
    rc.x -= view->em.x;
    rc.y -= view->em.y;
    rc.w += view->em.x * 2;
    rc.h += view->em.y * 2;
    app.invalidate(&rc);
}

static const char* view_nls(view_t* view) {
    return view->strid != 0 ?
        app.string(view->strid, view->text) : view->text;
}

static void view_measure(view_t* view) {
    ui_font_t f = view->font != null ? *view->font : app.fonts.regular;
    view->em = gdi.get_em(f);
    assert(view->em.x > 0 && view->em.y > 0);
    view->w = (int32_t)(view->em.x * view->width + 0.5);
    ui_point_t mt = { 0 };
    if (view->tag == uic_tag_text && ((label_t*)view)->multiline) {
        int32_t w = (int)(view->width * view->em.x + 0.5);
        mt = gdi.measure_multiline(f, w == 0 ? -1 : w, view_nls(view));
    } else {
        mt = gdi.measure_text(f, view_nls(view));
    }
    view->h = mt.y;
    view->w = max(view->w, mt.x);
    view->baseline = gdi.baseline(f);
    view->descent  = gdi.descent(f);
}

static void view_set_text(view_t* view, const char* label) {
    int32_t n = (int32_t)strlen(label);
    strprintf(view->text, "%s", label);
    for (int32_t i = 0; i < n; i++) {
        if (label[i] == '&' && i < n - 1 && label[i + 1] != '&') {
            view->shortcut = label[i + 1];
            break;
        }
    }
}

static void view_localize(view_t* view) {
    if (view->text[0] != 0) {
        view->strid = app.strid(view->text);
    }
}

static bool view_hidden_or_disabled(view_t* view) {
    return app.is_hidden(view) || app.is_disabled(view);
}

static void view_hovering(view_t* view, bool start) {
    static uic_text(btn_tooltip,  "");
    if (start && app.toasting.ui == null && view->tip[0] != 0 &&
       !app.is_hidden(view)) {
        strprintf(btn_tooltip.ui.text, "%s", app.nls(view->tip));
        btn_tooltip.ui.font = &app.fonts.H1;
        int32_t y = app.mouse.y - view->em.y;
        // enough space above? if not show below
        if (y < view->em.y) { y = app.mouse.y + view->em.y * 3 / 2; }
        y = min(app.crc.h - view->em.y * 3 / 2, max(0, y));
        app.show_tooltip(&btn_tooltip.ui, app.mouse.x, y, 0);
    } else if (!start && app.toasting.ui == &btn_tooltip.ui) {
        app.show_tooltip(null, -1, -1, 0);
    }
}

void view_init(view_t* view) {
    view->invalidate = view_invalidate;
    view->localize = view_localize;
    view->measure  = view_measure;
    view->hovering = view_hovering;
    view->hover_delay = 1.5;
}
