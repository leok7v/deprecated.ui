static void measurements_center(ui_view_t* view) {
    assert(view->children != null && view->children[0] != null, "no children?");
    assert(view->children[1] == null, "must be single child");
    ui_view_t* c = view->children[0]; // even if hidden measure it
    c->w = view->w;
    c->h = view->h;
}

static void measurements_horizontal(ui_view_t* view, int32_t gap) {
    assert(view->children != null && view->children[0] != null, "no children?");
    ui_view_t** c = view->children;
    view->w = 0;
    view->h = 0;
    bool seen = false;
    while (*c != null) {
        ui_view_t* u = *c;
        if (!u->hidden) {
            if (seen) { view->w += gap; }
            view->w += u->w;
            view->h = max(view->h, u->h);
            seen = true;
        }
        c++;
    }
}

static void measurements_vertical(ui_view_t* view, int32_t gap) {
    assert(view->children != null && view->children[0] != null, "no children?");
    ui_view_t** c = view->children;
    view->h = 0;
    bool seen = false;
    while (*c != null) {
        ui_view_t* u = *c;
        if (!u->hidden) {
            if (seen) { view->h += gap; }
            view->h += u->h;
            view->w = max(view->w, u->w);
            seen = true;
        }
        c++;
    }
}

static void measurements_grid(ui_view_t* view, int32_t gap_h, int32_t gap_v) {
    int32_t cols = 0;
    for (ui_view_t** row = view->children; *row != null; row++) {
        ui_view_t* r = *row;
        int32_t n = 0;
        for (ui_view_t** col = r->children; *col != null; col++) { n++; }
        if (cols == 0) { cols = n; }
        assert(n > 0 && cols == n);
    }
    int32_t* mxw = (int32_t*)alloca(cols * sizeof(int32_t));
    memset(mxw, 0, cols * sizeof(int32_t));
    for (ui_view_t** row = view->children; *row != null; row++) {
        if (!(*row)->hidden) {
            (*row)->h = 0;
            (*row)->baseline = 0;
            int32_t i = 0;
            for (ui_view_t** col = (*row)->children; *col != null; col++) {
                if (!(*col)->hidden) {
                    mxw[i] = max(mxw[i], (*col)->w);
                    (*row)->h = max((*row)->h, (*col)->h);
//                  traceln("[%d] row.baseline: %d col.baseline: %d ", i, (*row)->baseline, (*col)->baseline);
                    (*row)->baseline = max((*row)->baseline, (*col)->baseline);
                }
                i++;
            }
        }
    }
    view->h = 0;
    view->w = 0;
    int32_t rows_seen = 0; // number of visible rows so far
    for (ui_view_t** row = view->children; *row != null; row++) {
        ui_view_t* r = *row;
        if (!r->hidden) {
            r->w = 0;
            int32_t i = 0;
            int32_t cols_seen = 0; // number of visible columns so far
            for (ui_view_t** col = r->children; *col != null; col++) {
                ui_view_t* c = *col;
                if (!c->hidden) {
                    c->h = r->h; // all cells are same height
                    if (c->type == ui_view_text) { // lineup text baselines
                        ui_label_t* t = (ui_label_t*)c;
                        t->dy = r->baseline - c->baseline;
                    }
                    c->w = mxw[i++];
                    r->w += c->w;
                    if (cols_seen > 0) { r->w += gap_h; }
                    view->w = max(view->w, r->w);
                    cols_seen++;
                }
            }
            view->h += r->h;
            if (rows_seen > 0) { view->h += gap_v; }
            rows_seen++;
        }
    }
}

measurements_if measurements = {
    .center     = measurements_center,
    .horizontal = measurements_horizontal,
    .vertical   = measurements_vertical,
    .grid       = measurements_grid,
};

// layouts

static void layouts_center(ui_view_t* view) {
    assert(view->children != null && view->children[0] != null, "no children?");
    assert(view->children[1] == null, "must be single child");
    ui_view_t* c = view->children[0];
    c->x = (view->w - c->w) / 2;
    c->y = (view->h - c->h) / 2;
}

static void layouts_horizontal(ui_view_t* view, int32_t x, int32_t y, int32_t gap) {
    assert(view->children != null && view->children[0] != null, "no children?");
    ui_view_t** c = view->children;
    bool seen = false;
    while (*c != null) {
        ui_view_t* u = *c;
        if (!u->hidden) {
            if (seen) { x += gap; }
            u->x = x;
            u->y = y;
            x += u->w;
            seen = true;
        }
        c++;
    }
}

static void layouts_vertical(ui_view_t* view, int32_t x, int32_t y, int32_t gap) {
    assert(view->children != null && view->children[0] != null, "no children?");
    ui_view_t** c = view->children;
    bool seen = false;
    while (*c != null) {
        ui_view_t* u = *c;
        if (!u->hidden) {
            if (seen) { y += gap; }
            u->x = x;
            u->y = y;
            y += u->h;
            seen = true;
        }
        c++;
    }
}

static void layouts_grid(ui_view_t* view, int32_t gap_h, int32_t gap_v) {
    assert(view->children != null, "layout_grid() with no children?");
    int32_t x = view->x;
    int32_t y = view->y;
    bool row_seen = false;
    for (ui_view_t** row = view->children; *row != null; row++) {
        if (!(*row)->hidden) {
            if (row_seen) { y += gap_v; }
            int32_t xc = x;
            bool col_seen = false;
            for (ui_view_t** col = (*row)->children; *col != null; col++) {
                if (!(*col)->hidden) {
                    if (col_seen) { xc += gap_h; }
                    (*col)->x = xc;
                    (*col)->y = y;
                    xc += (*col)->w;
                    col_seen = true;
                }
            }
            y += (*row)->h;
            row_seen = true;
        }
    }
}

layouts_if layouts = {
    .center     = layouts_center,
    .horizontal = layouts_horizontal,
    .vertical   = layouts_vertical,
    .grid       = layouts_grid
};
