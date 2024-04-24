#pragma once
#include "ui/ui.h"

begin_c

typedef struct ui_view_s ui_view_t;

typedef struct ui_view_s { // ui element container/control
    enum ui_view_type_t type;
    void (*init)(ui_view_t* view); // called once before first layout
    ui_view_t** children; // null terminated array[] of children
    double width;    // > 0 width of UI element in "em"s
    char text[2048];
    ui_view_t* parent;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    // updated on layout() call
    ui_point_t em; // cached pixel dimensions of "M"
    int32_t shortcut; // keyboard shortcut
    int32_t strid; // 0 for not localized ui
    void* that;  // for the application use
    void (*notify)(ui_view_t* view, void* p); // for the application use
    // two pass layout: measure() .w, .h layout() .x .y
    // first  measure() bottom up - children.layout before parent.layout
    // second layout() top down - parent.layout before children.layout
    void (*measure)(ui_view_t* view); // determine w, h (bottom up)
    void (*layout)(ui_view_t* view);  // set x, y possibly adjust w, h (top down)
    void (*localize)(ui_view_t* view); // set strid based ui .text field
    void (*paint)(ui_view_t* view);
    bool (*message)(ui_view_t* view, int32_t message, int64_t wp, int64_t lp,
        int64_t* rt); // return true and value in rt to stop processing
    void (*click)(ui_view_t* view); // interpretation depends on ui element
    void (*mouse)(ui_view_t* view, int32_t message, int32_t flags);
    void (*mousewheel)(ui_view_t* view, int32_t dx, int32_t dy); // touchpad scroll
    // tap(ui, button_index) press(ui, button_index) see note below
    // button index 0: left, 1: middle, 2: right
    // bottom up (leaves to root or children to parent)
    // return true if consumed (halts further calls up the tree)
    bool (*tap)(ui_view_t* view, int32_t ix);   // single click/tap inside ui
    bool (*press)(ui_view_t* view, int32_t ix); // two finger click/tap or long press
    void (*context_menu)(ui_view_t* view); // right mouse click or long press
    bool (*set_focus)(ui_view_t* view); // returns true if focus is set
    void (*kill_focus)(ui_view_t* view);
    // translated from key pressed/released to utf8:
    void (*character)(ui_view_t* view, const char* utf8);
    void (*key_pressed)(ui_view_t* view, int32_t key);
    void (*key_released)(ui_view_t* view, int32_t key);
    void (*hovering)(ui_view_t* view, bool start);
    void (*invalidate)(const ui_view_t* view); // more prone to delays than app.redraw()
    // timer() every_100ms() and every_sec() called
    // even for hidden and disabled ui elements
    void (*timer)(ui_view_t* view, ui_timer_t id);
    void (*every_100ms)(ui_view_t* view); // ~10 x times per second
    void (*every_sec)(ui_view_t* view); // ~once a second
    bool hidden; // paint() is not called on hidden
    bool armed;
    bool hover;
    bool pressed;   // for ui_button_t and  checkbox_t
    bool disabled;  // mouse, keyboard, key_up/down not called on disabled
    bool focusable; // can be target for keyboard focus
    double  hover_delay; // delta time in seconds before hovered(true)
    double  hover_at;    // time in seconds when to call hovered()
    ui_color_t color;      // interpretation depends on ui element type
    ui_color_t background; // interpretation depends on ui element type
    ui_font_t* font;
    int32_t baseline; // font ascent; descent = height - baseline
    int32_t descent;  // font descent
    char    tip[256]; // tooltip text
} ui_view_t;

// tap() / press() APIs guarantee that single tap() is not coming
// before double tap/click in expense of double click delay (0.5 seconds)
// which is OK for buttons and many other UI controls but absolutely not
// OK for text editing. Thus edit uses raw mouse events to react
// on clicks and double clicks.

void ui_view_init(ui_view_t* view);

#define ui_container(name, ini, ...)                                       \
static ui_view_t* _ ## name ## _ ## children ## _[] = {__VA_ARGS__, null}; \
static ui_view_t name = { .type = ui_view_container, .init = ini,          \
                       .children = (_ ## name ## _ ## children ## _),      \
                       .text = #name                                       \
}

end_c
