static void measurements_center(view_t* ui) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    assert(ui->children[1] == null, "must be single child");
    view_t* c = ui->children[0]; // even if hidden measure it
    c->w = ui->w;
    c->h = ui->h;
}

static void measurements_horizontal(view_t* ui, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    ui->w = 0;
    ui->h = 0;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
        if (!u->hidden) {
            if (seen) { ui->w += gap; }
            ui->w += u->w;
            ui->h = max(ui->h, u->h);
            seen = true;
        }
        c++;
    }
}

static void measurements_vertical(view_t* ui, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    ui->h = 0;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
        if (!u->hidden) {
            if (seen) { ui->h += gap; }
            ui->h += u->h;
            ui->w = max(ui->w, u->w);
            seen = true;
        }
        c++;
    }
}

static void measurements_grid(view_t* ui, int32_t gap_h, int32_t gap_v) {
    int32_t cols = 0;
    for (view_t** row = ui->children; *row != null; row++) {
        view_t* r = *row;
        int32_t n = 0;
        for (view_t** col = r->children; *col != null; col++) { n++; }
        if (cols == 0) { cols = n; }
        assert(n > 0 && cols == n);
    }
    int32_t* mxw = (int32_t*)alloca(cols * sizeof(int32_t));
    memset(mxw, 0, cols * sizeof(int32_t));
    for (view_t** row = ui->children; *row != null; row++) {
        if (!(*row)->hidden) {
            (*row)->h = 0;
            (*row)->baseline = 0;
            int32_t i = 0;
            for (view_t** col = (*row)->children; *col != null; col++) {
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
    ui->h = 0;
    ui->w = 0;
    int32_t rows_seen = 0; // number of visible rows so far
    for (view_t** row = ui->children; *row != null; row++) {
        view_t* r = *row;
        if (!r->hidden) {
            r->w = 0;
            int32_t i = 0;
            int32_t cols_seen = 0; // number of visible columns so far
            for (view_t** col = r->children; *col != null; col++) {
                view_t* c = *col;
                if (!c->hidden) {
                    c->h = r->h; // all cells are same height
                    if (c->tag == uic_tag_text) { // lineup text baselines
                        label_t* t = (label_t*)c;
                        t->dy = r->baseline - c->baseline;
                    }
                    c->w = mxw[i++];
                    r->w += c->w;
                    if (cols_seen > 0) { r->w += gap_h; }
                    ui->w = max(ui->w, r->w);
                    cols_seen++;
                }
            }
            ui->h += r->h;
            if (rows_seen > 0) { ui->h += gap_v; }
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

static void layouts_center(view_t* ui) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    assert(ui->children[1] == null, "must be single child");
    view_t* c = ui->children[0];
    c->x = (ui->w - c->w) / 2;
    c->y = (ui->h - c->h) / 2;
}

static void layouts_horizontal(view_t* ui, int32_t x, int32_t y, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
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

static void layouts_vertical(view_t* ui, int32_t x, int32_t y, int32_t gap) {
    assert(ui->children != null && ui->children[0] != null, "no children?");
    view_t** c = ui->children;
    bool seen = false;
    while (*c != null) {
        view_t* u = *c;
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

static void layouts_grid(view_t* ui, int32_t gap_h, int32_t gap_v) {
    assert(ui->children != null, "layout_grid() with no children?");
    int32_t x = ui->x;
    int32_t y = ui->y;
    bool row_seen = false;
    for (view_t** row = ui->children; *row != null; row++) {
        if (!(*row)->hidden) {
            if (row_seen) { y += gap_v; }
            int32_t xc = x;
            bool col_seen = false;
            for (view_t** col = (*row)->children; *col != null; col++) {
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
