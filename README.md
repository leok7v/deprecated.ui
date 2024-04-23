# UI

[![build-on-push](https://github.com/leok7v/ui/actions/workflows/build-on-push.yml/badge.svg)](https://github.com/leok7v/ui/actions/workflows/build-on-push.yml)

Win GDI dpi aware apps skeleton suitable for simple realtime-ish UI.

No goals: (or "Quick is not...")
* not a complete UI framework
* not performance champion
* not a replacement for anything
* not an architectural overhaul
* mostly fail fast wrapper around Win32 user/gdi/system APIs
* not portable to MacOS, Linux, iOS or Android (but can be made so)

Motivation:

The Microsoft C-runtime (even for C99, C11 and C14) is still missing
a lot of really essential posix funtionality.

Quick is a poor man attempt to make peace between platforms 
shortcommings and differences is a set of (thread safe and
fail fast) functions packed is reasonable modular interfaces.

Using interfaces for essentially static functions is expensive
in terms of ultimate performance but minimizes polution of 
global namespace. Time crirical sections of the code can always
manually inside necessary code.

The shortage or resources or invalid parameters always handled
inside funtions and result in fatal error and process termination.

Using UTF-8 instead of UTF-16 on application level whereever possible.

```

    int32_t runtime.err(); // errno or GetLastError()
    uint32_t num.random32(uint32_t *state);
    uint64_t num.random64(uint64_t *state);
    void threads.sleep_for(double seconds);
    double clock.seconds(); // since boot

    ...

    thread_t threads.start(void (*func)(void*), void* p);
    void thread.join(thread_t thread);

    ...
    event_t events.create();
    void events.set(event_t e);
    void events.wait(event_t e);
    void events.dispose(event_t e);

```

Here is the sample code of simple "Hello World" application:

```
#include "quick.h"
#define quick_implementation // single header file library
#include "quick.h"

begin_c

const char* title = "Sample";

static uic_text(text, "Hello World!");

static view_t* children[] = { &text.ui, null };

static void layout(view_t* ui) {
    layouts.center(ui);
}

static void paint(view_t* ui) {
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
}

static void init(void) {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    app.ui->children = children;
}

app_t app = {
    .class_name = "sample",
    .init = init,
    .min_width = 400,
    .min_height = 200
};

end_c
```

Assuming Microsoft Visual Studio 2022 Community Edition is installed from
  
https://visualstudio.microsoft.com/vs/community/

with SDK 10.0.222621 or better

from ``Developer Command Prompt 2022`` execute:

```
curl.exe "https://github.com/leok7v/quick/blob/main/quick.h"   --remote-name
curl.exe "https://github.com/leok7v/quick/blob/main/sample1.c" --remote-name
...
curl.exe "https://github.com/nothings/stb/blob/master/stb_image.h" --remote-name

user\quick>cl.exe sample1.c

user\quick>sample.exe
```

