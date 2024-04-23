
static void view_invalidate(const view_t* ui) {
    ui_rect_t rc = { ui->x, ui->y, ui->w, ui->h};
    rc.x -= ui->em.x;
    rc.y -= ui->em.y;
    rc.w += ui->em.x * 2;
    rc.h += ui->em.y * 2;
    app.invalidate(&rc);
}

static const char* view_nls(view_t* ui) {
    return ui->strid != 0 ? app.string(ui->strid, ui->text) : ui->text;
}

static void view_measure(view_t* ui) {
    font_t f = ui->font != null ? *ui->font : app.fonts.regular;
    ui->em = gdi.get_em(f);
    assert(ui->em.x > 0 && ui->em.y > 0);
    ui->w = (int32_t)(ui->em.x * ui->width + 0.5);
    ui_point_t mt = { 0 };
    if (ui->tag == uic_tag_text && ((uic_text_t*)ui)->multiline) {
        int32_t w = (int)(ui->width * ui->em.x + 0.5);
        mt = gdi.measure_multiline(f, w == 0 ? -1 : w, view_nls(ui));
    } else {
        mt = gdi.measure_text(f, view_nls(ui));
    }
    ui->h = mt.y;
    ui->w = max(ui->w, mt.x);
    ui->baseline = gdi.baseline(f);
    ui->descent  = gdi.descent(f);
}

static void view_set_text(view_t* ui, const char* label) {
    int32_t n = (int32_t)strlen(label);
    strprintf(ui->text, "%s", label);
    for (int32_t i = 0; i < n; i++) {
        if (label[i] == '&' && i < n - 1 && label[i + 1] != '&') {
            ui->shortcut = label[i + 1];
            break;
        }
    }
}

static void view_localize(view_t* ui) {
    if (ui->text[0] != 0) {
        ui->strid = app.strid(ui->text);
    }
}

static bool view_hidden_or_disabled(view_t* ui) {
    return app.is_hidden(ui) || app.is_disabled(ui);
}

static void view_hovering(view_t* ui, bool start) {
    static uic_text(btn_tooltip,  "");
    if (start && app_toast.ui == null && ui->tip[0] != 0 &&
       !app.is_hidden(ui)) {
        strprintf(btn_tooltip.ui.text, "%s", app.nls(ui->tip));
        btn_tooltip.ui.font = &app.fonts.H1;
        int32_t y = app.mouse.y - ui->em.y;
        // enough space above? if not show below
        if (y < ui->em.y) { y = app.mouse.y + ui->em.y * 3 / 2; }
        y = min(app.crc.h - ui->em.y * 3 / 2, max(0, y));
        app.show_tooltip(&btn_tooltip.ui, app.mouse.x, y, 0);
    } else if (!start && app_toast.ui == &btn_tooltip.ui) {
        app.show_tooltip(null, -1, -1, 0);
    }
}

void view_init(view_t* ui) {
    ui->invalidate = view_invalidate;
    ui->localize = view_localize;
    ui->measure  = view_measure;
    ui->hovering = view_hovering;
    ui->hover_delay = 1.5;
}
