#pragma once
#include "ui/ui.h"

begin_c

// link.exe /SUBSYSTEM:WINDOWS single window application

// every_sec() and every_100ms() also called on all UICs

typedef struct app_s {
    // implemented by client:
    const char* class_name;
    // called before creating main window
    void (*init)(void);
    // called instead of init() for console apps and when .no_ui=true
    int (*main)(void);
    // class_name and init must be set before main()
    void (*opened)(void);      // window has been created and shown
    void (*every_sec)(void);   // if not null called ~ once a second
    void (*every_100ms)(void); // called ~10 times per second
    // .can_close() called before window is closed and can be
    // used in a meaning of .closing()
    bool (*can_close)(void);   // window can be closed
    void (*closed)(void);      // window has been closed
    void (*fini)(void);        // called before WinMain() return
    // must be filled by application:
    const char* title;
    // min/max width/height are prefilled according to monitor size
    float wmin; // inches
    float hmin; // inches
    float wmax; // inches
    float hmax; // inches
    // TODO: need wstart/hstart which are between min/max
    int32_t visibility; // initial window_visibility state
    int32_t last_visibility;    // last window_visibility state from last run
    int32_t startup_visibility; // window_visibility from parent process
    bool is_full_screen;
    // ui flags:
    bool no_ui;    // do not create application window at all
    bool no_decor; // window w/o title bar, min/max close buttons
    bool no_min;   // window w/o minimize button on title bar and sys menu
    bool no_max;   // window w/o maximize button on title bar
    bool no_size;  // window w/o maximize button on title bar
    bool no_clip;  // allows to resize window above hosting monitor size
    bool hide_on_minimize; // like task manager minimize means hide
    bool aero;     // retro Windows 7 decoration (just for the fun of it)
    int32_t exit_code; // application exit code
    int32_t tid; // main thread id
    // drawing context:
    ui_dpi_t dpi;
    ui_window_t window;
    ui_rect_t wrc;  // window rectangle including non-client area
    ui_rect_t crc;  // client rectangle
    ui_rect_t mrc;  // monitor rectangle
    ui_rect_t work_area; // current monitor work area
    int32_t width;  // client width
    int32_t height; // client height
    // not to call clock.seconds() too often:
    double now;     // ssb "seconds since boot" updated on each message
    ui_view_t* view;      // show_window() changes ui.hidden
    ui_view_t* focus;   // does not affect message routing - free for all
    ui_fonts_t fonts;
    ui_cursor_t cursor; // current cursor
    ui_cursor_t cursor_arrow;
    ui_cursor_t cursor_wait;
    ui_cursor_t cursor_ibeam;
    // keyboard state now:
    bool alt;
    bool ctrl;
    bool shift;
    ui_point_t mouse; // mouse/touchpad pointer
    ui_canvas_t canvas;  // set by WM_PAINT message
    struct { // toast state
        ui_view_t* view;
        int32_t step;
        double time; // closing time or zero
        int32_t x; // (x,y) for tooltip (-1,y) for toast
        int32_t y; // screen coordinates for tooltip
    } toasting;
    // i18n
    // strid("foo") returns 0 if there is no matching ENGLISH NEUTRAL
    // STRINGTABLE entry
    int32_t (*strid)(const char* s);
    // given strid > 0 returns localized string or defau1t value
    const char* (*string)(int32_t strid, const char* defau1t);
    // nls(s) is same as string(strid(s), s)
    const char* (*nls)(const char* defau1t); // national localized string
    const char* (*locale)(void); // "en-US" "zh-CN" etc...
    // force locale for debugging and testing:
    void (*set_locale)(const char* locale); // only for calling thread
    // inch to pixels and reverse translation via app.dpi.window
    float   (*px2in)(int32_t pixels);
    int32_t (*in2px)(float inches);
    bool    (*point_in_rect)(const ui_point_t* p, const ui_rect_t* r);
    // intersect_rect(null, r0, r1) and intersect_rect(r0, r0, r1) are OK.
    bool    (*intersect_rect)(ui_rect_t* r, const ui_rect_t* r0,
                                            const ui_rect_t* r1);
    // layout:
    bool (*is_hidden)(const ui_view_t* view);   // control or any parent is hidden
    bool (*is_disabled)(const ui_view_t* view); // control or any parent is disabled
    bool (*is_active)(void); // is application window active
    bool (*has_focus)(void); // application window has keyboard focus
    void (*activate)(void); // request application window activation
    void (*bring_to_foreground)(void); // not necessary topmost
    void (*make_topmost)(void);    // in foreground hierarchy of windows
    void (*request_focus)(void);   // request application window keyboard focus
    void (*bring_to_front)(void);  // activate() + bring_to_foreground() +
                                   // make_topmost() + request_focus()
    void (*measure)(ui_view_t* view);    // measure all children
    void (*layout)(void); // requests layout on UI tree before paint()
    void (*invalidate)(const ui_rect_t* rc);
    void (*full_screen)(bool on);
    void (*redraw)(void); // very fast (5 microseconds) InvalidateRect(null)
    void (*draw)(void);   // UpdateWindow()
    void (*set_cursor)(ui_cursor_t c);
    void (*close)(void); // attempts to close (can_close() permitting)
    // forced quit() even if can_close() returns false
    void (*quit)(int32_t ec);  // app.exit_code = ec; PostQuitMessage(ec);
    ui_timer_t (*set_timer)(uintptr_t id, int32_t milliseconds); // see notes
    void (*kill_timer)(ui_timer_t id);
    void (*post)(int32_t message, int64_t wp, int64_t lp);
    void (*show_window)(int32_t show); // see show_window enum
    void (*show_toast)(ui_view_t* toast, double seconds); // toast(null) to cancel
    void (*show_tooltip)(ui_view_t* tooltip, int32_t x, int32_t y, double seconds);
    void (*vtoast)(double seconds, const char* format, va_list vl);
    void (*toast)(double seconds, const char* format, ...);
    // caret calls must be balanced by caller
    void (*create_caret)(int32_t w, int32_t h);
    void (*show_caret)(void);
    void (*move_caret)(int32_t x, int32_t y);
    void (*hide_caret)(void);
    void (*destroy_caret)(void);
    // registry interface:
    void (*data_save)(const char* name, const void* data, int32_t bytes);
    int32_t (*data_size)(const char* name);
    int32_t (*data_load)(const char* name, void* data, int32_t bytes); // returns bytes read
    // filename dialog:
    // const char* filter[] =
    //     {"Text Files", ".txt;.doc;.ini",
    //      "Executables", ".exe",
    //      "All Files", "*"};
    // const char* fn = app.open_filename("C:\\", filter, countof(filter));
    const char* (*open_filename)(const char* folder, const char* filter[], int32_t n);
    const char* (*known_folder)(int32_t kfid);
    bool (*is_stdout_redirected)(void);
    bool (*is_console_visible)(void);
    int  (*console_attach)(void); // attempts to attach to parent terminal
    int  (*console_create)(void); // allocates new console
    void (*console_show)(bool b);
    // stats:
    int32_t paint_count; // number of paint calls
    double paint_time; // last paint duration in seconds
    double paint_max;  // max of last 128 paint
    double paint_avg;  // EMA of last 128 paints
} app_t;

extern app_t app;

end_c
