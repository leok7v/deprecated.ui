static void ui_messagebox_button(ui_button_t* b) {
    ui_messagebox_t* mx = (ui_messagebox_t*)b->view.parent;
    assert(mx->view.type == ui_view_messagebox);
    mx->option = -1;
    for (int32_t i = 0; i < countof(mx->button) && mx->option < 0; i++) {
        if (b == &mx->button[i]) {
            mx->option = i;
            mx->cb(mx, i);
        }
    }
    app.show_toast(null, 0);
}

static void ui_messagebox_measure(ui_view_t* view) {
    ui_messagebox_t* mx = (ui_messagebox_t*)view;
    assert(view->type == ui_view_messagebox);
    int32_t n = 0;
    for (ui_view_t** c = view->children; c != null && *c != null; c++) { n++; }
    n--; // number of buttons
    mx->text.view.measure(&mx->text.view);
    const int32_t em_x = mx->text.view.em.x;
    const int32_t em_y = mx->text.view.em.y;
    const int32_t tw = mx->text.view.w;
    const int32_t th = mx->text.view.h;
    if (n > 0) {
        int32_t bw = 0;
        for (int32_t i = 0; i < n; i++) {
            bw += mx->button[i].view.w;
        }
        view->w = max(tw, bw + em_x * 2);
        view->h = th + mx->button[0].view.h + em_y + em_y / 2;
    } else {
        view->h = th + em_y / 2;
        view->w = tw;
    }
}

static void ui_messagebox_layout(ui_view_t* view) {
    ui_messagebox_t* mx = (ui_messagebox_t*)view;
    assert(view->type == ui_view_messagebox);
    int32_t n = 0;
    for (ui_view_t** c = view->children; c != null && *c != null; c++) { n++; }
    n--; // number of buttons
    const int32_t em_y = mx->text.view.em.y;
    mx->text.view.x = view->x;
    mx->text.view.y = view->y + em_y * 2 / 3;
    const int32_t tw = mx->text.view.w;
    const int32_t th = mx->text.view.h;
    if (n > 0) {
        int32_t bw = 0;
        for (int32_t i = 0; i < n; i++) {
            bw += mx->button[i].view.w;
        }
        // center text:
        mx->text.view.x = view->x + (view->w - tw) / 2;
        // spacing between buttons:
        int32_t sp = (view->w - bw) / (n + 1);
        int32_t x = sp;
        for (int32_t i = 0; i < n; i++) {
            mx->button[i].view.x = view->x + x;
            mx->button[i].view.y = view->y + th + em_y * 3 / 2;
            x += mx->button[i].view.w + sp;
        }
    }
}

void ui_messagebox_init_(ui_view_t* view) {
    assert(view->type == ui_view_messagebox);
    ui_messagebox_t* mx = (ui_messagebox_t*)view;
    ui_view_init(view);
    view->measure = ui_messagebox_measure;
    view->layout  = ui_messagebox_layout;
    mx->view.font = &app.fonts.H3;
    const char** opts = mx->opts;
    int32_t n = 0;
    while (opts[n] != null && n < countof(mx->button) - 1) {
        ui_button_init(&mx->button[n], opts[n], 6.0, ui_messagebox_button);
        mx->button[n].view.parent = &mx->view;
        n++;
    }
    assert(n <= countof(mx->button));
    if (n > countof(mx->button)) { n = countof(mx->button); }
    mx->children[0] = &mx->text.view;
    for (int32_t i = 0; i < n; i++) {
        mx->children[i + 1] = &mx->button[i].view;
        mx->children[i + 1]->font = mx->view.font;
        mx->button[i].view.localize(&mx->button[i].view);
    }
    mx->view.children = mx->children;
    ui_label_init_ml(&mx->text, 0.0, "%s", mx->view.text);
    mx->text.view.font = mx->view.font;
    mx->text.view.localize(&mx->text.view);
    mx->view.text[0] = 0;
    mx->option = -1;
}

void ui_messagebox_init(ui_messagebox_t* mx, const char* opts[],
        void (*cb)(ui_messagebox_t* m, int32_t option),
        const char* format, ...) {
    mx->view.type = ui_view_messagebox;
    mx->view.measure = ui_messagebox_measure;
    mx->view.layout  = ui_messagebox_layout;
    mx->opts = opts;
    mx->cb = cb;
    va_list vl;
    va_start(vl, format);
    str.vformat(mx->view.text, countof(mx->view.text), format, vl);
    ui_label_init_ml(&mx->text, 0.0, mx->view.text);
    va_end(vl);
    ui_messagebox_init_(&mx->view);
}
