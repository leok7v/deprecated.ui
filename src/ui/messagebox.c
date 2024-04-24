static void messagebox_button(button_t* b) {
    messagebox_t* mx = (messagebox_t*)b->ui.parent;
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

static void messagebox_measure(view_t* view) {
    messagebox_t* mx = (messagebox_t*)view;
    assert(view->tag == uic_tag_messagebox);
    int32_t n = 0;
    for (view_t** c = view->children; c != null && *c != null; c++) { n++; }
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
        view->w = max(tw, bw + em_x * 2);
        view->h = th + mx->button[0].ui.h + em_y + em_y / 2;
    } else {
        view->h = th + em_y / 2;
        view->w = tw;
    }
}

static void messagebox_layout(view_t* view) {
    messagebox_t* mx = (messagebox_t*)view;
    assert(view->tag == uic_tag_messagebox);
    int32_t n = 0;
    for (view_t** c = view->children; c != null && *c != null; c++) { n++; }
    n--; // number of buttons
    const int32_t em_y = mx->text.ui.em.y;
    mx->text.ui.x = view->x;
    mx->text.ui.y = view->y + em_y * 2 / 3;
    const int32_t tw = mx->text.ui.w;
    const int32_t th = mx->text.ui.h;
    if (n > 0) {
        int32_t bw = 0;
        for (int32_t i = 0; i < n; i++) {
            bw += mx->button[i].ui.w;
        }
        // center text:
        mx->text.ui.x = view->x + (view->w - tw) / 2;
        // spacing between buttons:
        int32_t sp = (view->w - bw) / (n + 1);
        int32_t x = sp;
        for (int32_t i = 0; i < n; i++) {
            mx->button[i].ui.x = view->x + x;
            mx->button[i].ui.y = view->y + th + em_y * 3 / 2;
            x += mx->button[i].ui.w + sp;
        }
    }
}

void messagebox_init_(view_t* view) {
    assert(view->tag == uic_tag_messagebox);
    messagebox_t* mx = (messagebox_t*)view;
    view_init(view);
    view->measure = messagebox_measure;
    view->layout = messagebox_layout;
    mx->ui.font = &app.fonts.H3;
    const char** opts = mx->opts;
    int32_t n = 0;
    while (opts[n] != null && n < countof(mx->button) - 1) {
        button_init(&mx->button[n], opts[n], 6.0, messagebox_button);
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
    label_init_ml(&mx->text, 0.0, "%s", mx->ui.text);
    mx->text.ui.font = mx->ui.font;
    mx->text.ui.localize(&mx->text.ui);
    mx->ui.text[0] = 0;
    mx->option = -1;
}

void messagebox_init(messagebox_t* mx, const char* opts[],
        void (*cb)(messagebox_t* m, int32_t option),
        const char* format, ...) {
    mx->ui.tag = uic_tag_messagebox;
    mx->ui.measure = messagebox_measure;
    mx->ui.layout = messagebox_layout;
    mx->opts = opts;
    mx->cb = cb;
    va_list vl;
    va_start(vl, format);
    str.vformat(mx->ui.text, countof(mx->ui.text), format, vl);
    label_init_ml(&mx->text, 0.0, mx->ui.text);
    va_end(vl);
    messagebox_init_(&mx->ui);
}
