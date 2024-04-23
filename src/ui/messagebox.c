static void uic_messagebox_button(uic_button_t* b) {
    uic_messagebox_t* mx = (uic_messagebox_t*)b->ui.parent;
    assert(mx->ui.tag == uic_tag_messagebox);
    mx->option = -1;
    for (int32_t i = 0; i < countof(mx->button) && mx->option < 0; i++) {
        if (b == &mx->button[i]) {
            mx->option = i;
            mx->cb(mx, i);
        }
    }
    app.show_toast(null, 0);
}

static void uic_messagebox_measure(view_t* ui) {
    uic_messagebox_t* mx = (uic_messagebox_t*)ui;
    assert(ui->tag == uic_tag_messagebox);
    int32_t n = 0;
    for (view_t** c = ui->children; c != null && *c != null; c++) { n++; }
    n--; // number of buttons
    mx->text.ui.measure(&mx->text.ui);
    const int32_t em_x = mx->text.ui.em.x;
    const int32_t em_y = mx->text.ui.em.y;
    const int32_t tw = mx->text.ui.w;
    const int32_t th = mx->text.ui.h;
    if (n > 0) {
        int32_t bw = 0;
        for (int32_t i = 0; i < n; i++) {
            bw += mx->button[i].ui.w;
        }
        ui->w = max(tw, bw + em_x * 2);
        ui->h = th + mx->button[0].ui.h + em_y + em_y / 2;
    } else {
        ui->h = th + em_y / 2;
        ui->w = tw;
    }
}

static void uic_messagebox_layout(view_t* ui) {
    uic_messagebox_t* mx = (uic_messagebox_t*)ui;
    assert(ui->tag == uic_tag_messagebox);
//  traceln("ui.y=%d", ui->y);
    int32_t n = 0;
    for (view_t** c = ui->children; c != null && *c != null; c++) { n++; }
    n--; // number of buttons
    const int32_t em_y = mx->text.ui.em.y;
    mx->text.ui.x = ui->x;
    mx->text.ui.y = ui->y + em_y * 2 / 3;
    const int32_t tw = mx->text.ui.w;
    const int32_t th = mx->text.ui.h;
    if (n > 0) {
        int32_t bw = 0;
        for (int32_t i = 0; i < n; i++) {
            bw += mx->button[i].ui.w;
        }
        // center text:
        mx->text.ui.x = ui->x + (ui->w - tw) / 2;
        // spacing between buttons:
        int32_t sp = (ui->w - bw) / (n + 1);
        int32_t x = sp;
        for (int32_t i = 0; i < n; i++) {
            mx->button[i].ui.x = ui->x + x;
            mx->button[i].ui.y = ui->y + th + em_y * 3 / 2;
            x += mx->button[i].ui.w + sp;
        }
    }
}

void uic_messagebox_init_(view_t* ui) {
    assert(ui->tag == uic_tag_messagebox);
    uic_messagebox_t* mx = (uic_messagebox_t*)ui;
    view_init(ui);
    ui->measure = uic_messagebox_measure;
    ui->layout = uic_messagebox_layout;
    mx->ui.font = &app.fonts.H3;
    const char** opts = mx->opts;
    int32_t n = 0;
    while (opts[n] != null && n < countof(mx->button) - 1) {
        uic_button_init(&mx->button[n], opts[n], 6.0, uic_messagebox_button);
        mx->button[n].ui.parent = &mx->ui;
        n++;
    }
    assert(n <= countof(mx->button));
    if (n > countof(mx->button)) { n = countof(mx->button); }
    mx->children[0] = &mx->text.ui;
    for (int32_t i = 0; i < n; i++) {
        mx->children[i + 1] = &mx->button[i].ui;
        mx->children[i + 1]->font = mx->ui.font;
        mx->button[i].ui.localize(&mx->button[i].ui);
    }
    mx->ui.children = mx->children;
    uic_text_init_ml(&mx->text, 0.0, "%s", mx->ui.text);
    mx->text.ui.font = mx->ui.font;
    mx->text.ui.localize(&mx->text.ui);
    mx->ui.text[0] = 0;
    mx->option = -1;
}

void uic_messagebox_init(uic_messagebox_t* mx, const char* opts[],
        void (*cb)(uic_messagebox_t* m, int32_t option),
        const char* format, ...) {
    mx->ui.tag = uic_tag_messagebox;
    mx->ui.measure = uic_messagebox_measure;
    mx->ui.layout = uic_messagebox_layout;
    mx->opts = opts;
    mx->cb = cb;
    va_list vl;
    va_start(vl, format);
    str.vformat(mx->ui.text, countof(mx->ui.text), format, vl);
    uic_text_init_ml(&mx->text, 0.0, mx->ui.text);
    va_end(vl);
    uic_messagebox_init_(&mx->ui);
}
