#pragma once
#include "ui/ui.h"

begin_c

enum {
    uic_tag_container  = 'cnt',
    uic_tag_messagebox = 'mbx',
    uic_tag_button     = 'btn',
    uic_tag_checkbox   = 'cbx',
    uic_tag_slider     = 'sld',
    uic_tag_text       = 'txt',
    uic_tag_edit       = 'edt'
};

typedef struct view_s view_t;

typedef struct view_s { // ui element container/control
    int32_t tag;
    void (*init)(view_t* ui); // called once before first layout
    view_t** children; // null terminated array[] of children
    double width;    // > 0 width of UI element in "em"s
    char text[2048];
    view_t* parent;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    // updated on layout() call
    ui_point_t em; // cached pixel dimensions of "M"
    int32_t shortcut; // keyboard shortcut
    int32_t strid; // 0 for not localized ui
    void* that;  // for the application use
    void (*notify)(view_t* ui, void* p); // for the application use
    // two pass layout: measure() .w, .h layout() .x .y
    // first  measure() bottom up - children.layout before parent.layout
    // second layout() top down - parent.layout before children.layout
    void (*measure)(view_t* ui); // determine w, h (bottom up)
    void (*layout)(view_t* ui);  // set x, y possibly adjust w, h (top down)
    void (*localize)(view_t* ui); // set strid based ui .text field
    void (*paint)(view_t* ui);
    bool (*message)(view_t* ui, int32_t message, int64_t wp, int64_t lp,
        int64_t* rt); // return true and value in rt to stop processing
    void (*click)(view_t* ui); // interpretation depends on ui element
    void (*mouse)(view_t* ui, int32_t message, int32_t flags);
    void (*mousewheel)(view_t* ui, int32_t dx, int32_t dy); // touchpad scroll
    // tap(ui, button_index) press(ui, button_index) see note below
    // button index 0: left, 1: middle, 2: right
    // bottom up (leaves to root or children to parent)
    // return true if consumed (halts further calls up the tree)
    bool (*tap)(view_t* ui, int32_t ix);   // single click/tap inside ui
    bool (*press)(view_t* ui, int32_t ix); // two finger click/tap or long press
    void (*context_menu)(view_t* ui); // right mouse click or long press
    bool (*set_focus)(view_t* ui); // returns true if focus is set
    void (*kill_focus)(view_t* ui);
    // translated from key pressed/released to utf8:
    void (*character)(view_t* ui, const char* utf8);
    void (*key_pressed)(view_t* ui, int32_t key);
    void (*key_released)(view_t* ui, int32_t key);
    void (*hovering)(view_t* ui, bool start);
    void (*invalidate)(const view_t* ui); // more prone to delays than app.redraw()
    // timer() every_100ms() and every_sec() called
    // even for hidden and disabled ui elements
    void (*timer)(view_t* ui, tm_t id);
    void (*every_100ms)(view_t* ui); // ~10 x times per second
    void (*every_sec)(view_t* ui); // ~once a second
    bool hidden; // paint() is not called on hidden
    bool armed;
    bool hover;
    bool pressed;   // for uic_button_t and  uic_checkbox_t
    bool disabled;  // mouse, keyboard, key_up/down not called on disabled
    bool focusable; // can be target for keyboard focus
    double  hover_delay; // delta time in seconds before hovered(true)
    double  hover_at;    // time in seconds when to call hovered()
    color_t color;      // interpretation depends on ui element type
    color_t background; // interpretation depends on ui element type
    font_t* font;
    int32_t baseline; // font ascent; descent = height - baseline
    int32_t descent;  // font descent
    char    tip[256]; // tooltip text
} view_t;

// tap() / press() APIs guarantee that single tap() is not coming
// before double tap/click in expense of double click delay (0.5 seconds)
// which is OK for buttons and many other UI controls but absolutely not
// OK for text editing. Thus edit uses raw mouse events to react
// on clicks and double clicks.

void view_init(view_t* ui);

#define uic_container(name, ini, ...)                                   \
static view_t* _ ## name ## _ ## children ## _[] = {__VA_ARGS__, null}; \
static view_t name = { .tag = uic_tag_container, .init = ini,           \
                      .children = (_ ## name ## _ ## children ## _),    \
                      .text = #name                                     \
}

end_c
