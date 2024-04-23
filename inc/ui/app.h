#pragma once
#include "ui/ui.h"

begin_c

// SUBSYSTEM:WINDOWS single window application

typedef struct window_visibility_s {
    const int32_t hide;
    const int32_t normal;   // should be use for first .show()
    const int32_t minimize; // activate and minimize
    const int32_t maximize; // activate and maximize
    const int32_t normal_na;// same as .normal but no activate
    const int32_t show;     // shows and activates in current size and position
    const int32_t min_next; // minimize and activate next window in Z order
    const int32_t min_na;   // minimize but do not activate
    const int32_t show_na;  // same as .show but no activate
    const int32_t restore;  // from min/max to normal window size/pos
    const int32_t defau1t;  // use Windows STARTUPINFO value
    const int32_t force_min;// minimize even if dispatch thread not responding
} window_visibility_t;

extern window_visibility_t window_visibility;

typedef struct fonts_s {
    // font handles re-created on scale change
    font_t regular; // proportional UI font
    font_t mono; // monospaced  UI font
    font_t H1; // bold header font
    font_t H2;
    font_t H3;
} fonts_t;

enum {
    known_folder_home      = 0, // c:\Users\<username>
    known_folder_desktop   = 1,
    known_folder_documents = 2,
    known_folder_downloads = 3,
    known_folder_music     = 4,
    known_folder_pictures  = 5,
    known_folder_videos    = 6,
    known_folder_shared    = 7, // c:\Users\Public
    known_folder_bin       = 8, // c:\ProgramFiles
    known_folder_data      = 9  // c:\ProgramData
};

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
    dpi_t dpi;
    window_t window;
    ui_rect_t wrc;  // window rectangle including non-client area
    ui_rect_t crc;  // client rectangle
    ui_rect_t mrc;  // monitor rectangle
    ui_rect_t work_area; // current monitor work area
    int32_t width;  // client width
    int32_t height; // client height
    // not to call clock.seconds() too often:
    double now;     // ssb "seconds since boot" updated on each message
    view_t* ui;      // show_window() changes ui.hidden
    view_t* focus;   // does not affect message routing - free for all
    fonts_t fonts;
    cursor_t cursor; // current cursor
    cursor_t cursor_arrow;
    cursor_t cursor_wait;
    cursor_t cursor_ibeam;
    // keyboard state now:
    bool alt;
    bool ctrl;
    bool shift;
    ui_point_t mouse; // mouse/touchpad pointer
    canvas_t canvas;  // set by WM_PAINT message
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
    bool (*is_hidden)(const view_t* ui);   // control or any parent is hidden
    bool (*is_disabled)(const view_t* ui); // control or any parent is disabled
    bool (*is_active)(void); // is application window active
    bool (*has_focus)(void); // application window has keyboard focus
    void (*activate)(void); // request application window activation
    void (*bring_to_foreground)(void); // not necessary topmost
    void (*make_topmost)(void);    // in foreground hierarchy of windows
    void (*request_focus)(void);   // request application window keyboard focus
    void (*bring_to_front)(void);  // activate() + bring_to_foreground() +
                                   // make_topmost() + request_focus()
    void (*measure)(view_t* ui);    // measure all children
    void (*layout)(void); // requests layout on UI tree before paint()
    void (*invalidate)(const ui_rect_t* rc);
    void (*full_screen)(bool on);
    void (*redraw)(void); // very fast (5 microseconds) InvalidateRect(null)
    void (*draw)(void);   // UpdateWindow()
    void (*set_cursor)(cursor_t c);
    void (*close)(void); // attempts to close (can_close() permitting)
    // forced quit() even if can_close() returns false
    void (*quit)(int32_t ec);  // app.exit_code = ec; PostQuitMessage(ec);
    tm_t (*set_timer)(uintptr_t id, int32_t milliseconds); // see notes
    void (*kill_timer)(tm_t id);
    void (*post)(int32_t message, int64_t wp, int64_t lp);
    void (*show_window)(int32_t show); // see show_window enum
    void (*show_toast)(view_t* toast, double seconds); // toast(null) to cancel
    void (*show_tooltip)(view_t* tooltip, int32_t x, int32_t y, double seconds);
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

typedef struct clipboard_s {
    int (*copy_text)(const char* s); // returns error or 0
    int (*copy_bitmap)(image_t* im); // returns error or 0
    int (*text)(char* text, int32_t* bytes);
} clipboard_t;

extern clipboard_t clipboard;

typedef struct messages_s {
    const int32_t character; // translated from key pressed/released to utf8
    const int32_t key_pressed;
    const int32_t key_released;
    const int32_t left_button_pressed;
    const int32_t left_button_released;
    const int32_t right_button_pressed;
    const int32_t right_button_released;
    const int32_t mouse_move;
    const int32_t left_double_click;
    const int32_t right_double_click;
    // wp: 0,1,2 (left, middle, right) button index, lp: client x,y
    const int32_t tap;
    const int32_t dtap;
    const int32_t press;
} messages_t;

extern messages_t messages;

typedef struct mouse_flags_s { // which buttons are pressed
    const int32_t left_button;
    const int32_t right_button;
} mouse_flags_t;

extern mouse_flags_t mouse_flags;

typedef struct virtual_keys_s {
    const int32_t up;
    const int32_t down;
    const int32_t left;
    const int32_t right;
    const int32_t home;
    const int32_t end;
    const int32_t pageup;
    const int32_t pagedw;
    const int32_t insert;
    const int32_t del;
    const int32_t back;
    const int32_t escape;
    const int32_t enter;
    const int32_t plus;
    const int32_t minus;
    const int32_t f1;
    const int32_t f2;
    const int32_t f3;
    const int32_t f4;
    const int32_t f5;
    const int32_t f6;
    const int32_t f7;
    const int32_t f8;
    const int32_t f9;
    const int32_t f10;
    const int32_t f11;
    const int32_t f12;
    const int32_t f13;
    const int32_t f14;
    const int32_t f15;
    const int32_t f16;
    const int32_t f17;
    const int32_t f18;
    const int32_t f19;
    const int32_t f20;
    const int32_t f21;
    const int32_t f22;
    const int32_t f23;
    const int32_t f24;
} virtual_keys_t;

extern virtual_keys_t virtual_keys;

end_c
