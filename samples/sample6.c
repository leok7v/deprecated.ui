/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

begin_c

const char* title = "Sample6: I am groot";

static struct {
    int32_t bpp; // bytes per pixel
    int32_t w;
    int32_t h;
    int32_t frames;
    int32_t* delays; // delays[frames];
    byte* pixels;
} gif; // animated

enum { max_speed = 3 };

static struct {
    int32_t  index; // animation index 0..gif.frames - 1
    event_t  quit;
    thread_t thread;
    uint32_t seed; // for crt.random32()
    int32_t  x;
    int32_t  y;
    int32_t  speed_x;
    int32_t  speed_y;
} animation;

static struct {
    int32_t tid; // main thread id because MCI is thread sensitive
    char filename[MAX_PATH];
    MCI_OPEN_PARMSA mop;
    bool muted;
} midi;

#define mute "\xF0\x9F\x94\x87"
#define speaker "\xF0\x9F\x94\x88"

static image_t  background;

static void init(void);
static void fini(void);
static void character(uic_t* ui, const char* utf8);
static void midi_open(void);
static void midi_play(void);
static void midi_stop(void);
static void midi_close(void);

static int  console(void) {
    fatal_if(true, "%s only SUBSYSTEM:WINDOWS", app.argv[0]);
    return 1;
}

app_t app = {
    .class_name = "sample4",
    .init = init,
    .fini = fini,
    .main = console, // optional
    .min_width = 640,
    .min_height = 480
};

static void* load_image(const byte* data, int64_t bytes, int32_t* w, int32_t* h,
    int32_t* bpp, int32_t preferred_bytes_per_pixel);

static void* load_animated_gif(const byte* data, int64_t bytes,
    int32_t** delays, int32_t* w, int32_t* h, int32_t* frames, int32_t* bpp,
    int32_t preferred_bytes_per_pixel);

static void paint(uic_t* ui) {
    if (animation.x < 0 && animation.y < 0) {
        animation.x = (ui->w - gif.w) / 2;
        animation.y = (ui->h - gif.h) / 2;
    }
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
    int32_t w = min(ui->w, background.w);
    int32_t h = min(ui->h, background.h);
    int32_t x = (ui->w - w) / 2;
    int32_t y = (ui->h - h) / 2;
    gdi.set_clip(0, 0, ui->w, ui->h);
    gdi.draw_image(x, y, w, h, &background);
    gdi.set_clip(0, 0, 0, 0);
    if (gif.pixels != null) {
        byte* p = gif.pixels + gif.w * gif.h * gif.bpp * animation.index;
        image_t frame = { 0 };
        gdi.image_init(&frame, gif.w, gif.h, gif.bpp, p);
        x = animation.x - gif.w / 2;
        y = animation.y - gif.h / 2;
        gdi.alpha_blend(x, y, gif.w, gif.h, &frame, 1.0);
        gdi.image_dispose(&frame);
    }
    font_t f = gdi.set_font(app.fonts.H1);
    gdi.x = 0;
    gdi.y = 0;
    gdi.set_text_color(midi.muted ? colors.green : colors.red);
    gdi.text("%s", midi.muted ? speaker : mute);
    gdi.set_font(f);
}

static void character(uic_t* unused(ui), const char* utf8) {
    if (utf8[0] == 'q' || utf8[0] == 'Q' || utf8[0] == 033) {
        app.close();
    }
}

static void mouse(uic_t* unused(ui), int32_t m, int32_t unused(f)) {
    const ui_point_t em = gdi.get_em(app.fonts.H1);
    if ((m == messages.left_button_pressed ||
        m == messages.right_button_pressed) &&
        0 <= app.mouse.x && app.mouse.x < em.x &&
        0 <= app.mouse.y && app.mouse.y < em.y) {
        midi.muted = !midi.muted;
        if (midi.muted) {
            midi_stop();
            midi_close();
        } else {
            midi_open();
            midi_play();
        }
    }
}

static bool message(uic_t* unused(ui), int32_t m, int64_t wp, int64_t lp,
        int64_t* unused(ret)) {
    if (m == MM_MCINOTIFY) {
//      traceln("MM_MCINOTIFY flags: %016llX defice: %016llX", wp, lp);
//      if (wp & MCI_NOTIFY_ABORTED)    { traceln("MCI_NOTIFY_ABORTED"); }
//      if (wp & MCI_NOTIFY_FAILURE)    { traceln("MCI_NOTIFY_FAILURE"); }
//      if (wp & MCI_NOTIFY_SUCCESSFUL) { traceln("MCI_NOTIFY_SUCCESSFUL"); }
//      if (wp & MCI_NOTIFY_SUPERSEDED) { traceln("MCI_NOTIFY_SUPERSEDED"); }
        if ((wp & MCI_NOTIFY_SUCCESSFUL) != 0 && lp == midi.mop.wDeviceID) {
            midi_stop();
            midi_close();
            midi_open();
            midi_play();
        }
    }
    return m == MM_MCINOTIFY;
}

static const char* midi_file(void) {
    char path[MAX_PATH];
    if (midi.filename[0] == 0) {
        void* data = null;
        int64_t bytes = 0;
        int r = crt.memmap_res("mr_blue_sky_midi", &data, &bytes);
        fatal_if_not_zero(r);
        GetTempPathA(countof(path), path);
        assert(path[0] != 0);
        GetTempFileNameA(path, "midi", 0, midi.filename);
        assert(midi.filename[0] != 0);
        HANDLE file = CreateFileA(midi.filename, GENERIC_WRITE, 0, null, CREATE_ALWAYS,
               FILE_ATTRIBUTE_NORMAL, null);
        fatal_if_null(file);
        DWORD written = 0;
        r = WriteFile(file, data, (uint32_t)bytes, &written, null) ? 0 : GetLastError();
        fatal_if(r != 0 || written != bytes);
        r = CloseHandle(file) ? 0 : GetLastError();
        fatal_if(r != 0);
    }
    return midi.filename;
}

static void delete_midi_file(void) {
    int r = DeleteFile(midi.filename) ? 0 : GetLastError();
    fatal_if(r != 0);
}

static void midi_warn_if_error_(int r, const char* call, const char* func, int line) {
    if (r != 0) {
        static char error[256];
        mciGetErrorString(r, error, countof(error));
        traceln("%s:%d %s", func, line, call);
        traceln("%d - MCIERR_BASE: %d %s", r, r - MCIERR_BASE, error);
    }
}

#define midi_warn_if_error(r) do { midi_warn_if_error_(r, #r, __func__, __LINE__); } while (0)

#define midi_fatal_if_error(call) do { \
    int _r_ = call; midi_warn_if_error_(r, #call, __func__, __LINE__); \
    fatal_if_not_zero(r); \
} while (0)

static void midi_play(void) {
    assert(midi.tid == crt.gettid());
    MCI_PLAY_PARMS pp = {0};
    pp.dwCallback = (uintptr_t)app.window;
    midi_warn_if_error(mciSendCommandA(midi.mop.wDeviceID,
        MCI_PLAY, MCI_NOTIFY, (uintptr_t)&pp));
}

static void midi_stop(void) {
    assert(midi.tid == crt.gettid());
    midi_warn_if_error(mciSendCommandA(midi.mop.wDeviceID,
        MCI_STOP, 0, 0));
}

static void midi_open(void) {
    assert(midi.tid == crt.gettid());
    midi.mop.dwCallback = (uintptr_t)app.window;
    midi.mop.wDeviceID = (WORD)-1;
    midi.mop.lpstrDeviceType = (const char*)MCI_DEVTYPE_SEQUENCER;
    midi.mop.lpstrElementName = midi_file();
    midi.mop.lpstrAlias = null;
    midi_warn_if_error(mciSendCommandA(0, MCI_OPEN,
            MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_OPEN_ELEMENT,
            (uintptr_t)&midi.mop));
}

static void midi_close(void) {
    midi_warn_if_error(mciSendCommandA(midi.mop.wDeviceID,
        MCI_CLOSE, MCI_WAIT, 0));
    midi_warn_if_error(mciSendCommandA(MCI_ALL_DEVICE_ID,
        MCI_CLOSE, MCI_WAIT, 0));
}

static void load_gif(void) {
    void* data = null;
    int64_t bytes = 0;
    int r = crt.memmap_res("groot_gif", &data, &bytes);
    fatal_if_not_zero(r);
    gif.pixels = load_animated_gif(data, bytes, &gif.delays,
        &gif.w, &gif.h, &gif.frames, &gif.bpp, 4);
    fatal_if(gif.pixels == null || gif.bpp != 4 || gif.frames < 1);
    // resources cannot be unmapped do not call crt.memunmap()
}

static void animate(void) {
    for (;;) {
        app.redraw();
        double delay_in_seconds = gif.delays[animation.index] * 0.001;
        if (events.wait_or_timeout(animation.quit, delay_in_seconds) == 0) {
            break;
        }
        if (animation.x >= 0 && animation.y >= 0) {
//          traceln("%d %d speed: %d %d", animation.x, animation.y, animation.speed_x, animation.speed_y);
            animation.index = (animation.index + 1) % gif.frames;
            while (animation.speed_x == 0) {
                animation.speed_x = crt.random32(&animation.seed) % (max_speed * 2 + 1) - max_speed;
            }
            while (animation.speed_y == 0) {
                animation.speed_y = crt.random32(&animation.seed) % (max_speed * 2 + 1) - max_speed;
            }
            animation.x += animation.speed_x;
            animation.y += animation.speed_y;
            if (animation.x - gif.w / 2 < 0) {
                animation.x = gif.w / 2;
                animation.speed_x = -animation.speed_x;
            } else if (animation.x + gif.w / 2 >= app.crc.w) {
                animation.x = app.crc.w - gif.w / 2 - 1;
                animation.speed_x = -animation.speed_x;
            }
            if (animation.y - gif.h / 2 < 0) {
                animation.y = gif.h / 2;
                animation.speed_y = -animation.speed_y;
            } else if (animation.y + gif.h / 2 >= app.crc.h) {
                animation.y = app.crc.h - gif.h / 2 - 1;
                animation.speed_y = -animation.speed_y;
            }
            int inc = crt.random32(&animation.seed) % 2 == 0 ? -1 : +1;
            if (crt.random32(&animation.seed) % 2 == 0) {
                if (1 <= animation.speed_x + inc && animation.speed_x + inc < max_speed) {
                    animation.speed_x += inc;
                }
            } else {
                if (1 <= animation.speed_y + inc && animation.speed_y + inc < max_speed) {
                    animation.speed_y += inc;
                }
            }
        }
    }
}

static void startup(void* unused(ignored)) {
    cursor_t cursor = app.cursor;
    app.set_cursor(app.cursor_wait);
    load_gif();
    app.set_cursor(cursor);
    animate();
}

static void init(void) {
    app.title = title;
    app.ui->paint     = paint;
    app.ui->character = character;
    app.ui->message   = message;
    app.ui->mouse     = mouse;
    animation.seed = (uint32_t)crt.nanoseconds();
    animation.x = -1;
    animation.y = -1;
    animation.quit = events.create();
    animation.thread = threads.start(startup, null);
    void* data = null;
    int64_t bytes = 0;
    fatal_if_not_zero(crt.memmap_res("sample_png", &data, &bytes));
    int w = 0;
    int h = 0;
    int bpp = 0; // bytes (!) per pixel
    void* pixels = load_image(data, bytes, &w, &h, &bpp, 0);
    fatal_if_null(pixels);
    gdi.image_init(&background, w, h, bpp, pixels);
    free(pixels);
    midi.tid = crt.gettid();
    midi_open();
    midi_play();
}

static void fini(void) {
    gdi.image_dispose(&background);
    free(gif.pixels);
    free(gif.delays);
    events.set(animation.quit);
    threads.join(animation.thread);
    events.dispose(animation.quit);
    midi_stop();
    midi_close();
    delete_midi_file();
}

end_c

begin_c

#pragma warning(disable: 4459) // parameter/local hides global declaration
#pragma warning(disable: 4244) // conversion from '...' to '...', possible loss of data

#define STBI_ASSERT(x) assert(x)
#include "stb_image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void* load_image(const byte* data, int64_t bytes, int32_t* w, int32_t* h,
    int32_t* bpp, int32_t preferred_bytes_per_pixel) {
    void* pixels = stbi_load_from_memory((byte const*)data, (int)bytes, w, h,
        bpp, preferred_bytes_per_pixel);
    return pixels;
}

static void* load_animated_gif(const byte* data, int64_t bytes,
    int32_t** delays, int32_t* w, int32_t* h, int32_t* frames, int32_t* bpp,
    int32_t preferred_bytes_per_pixel) {
    stbi_uc* pixels = stbi_load_gif_from_memory(data, bytes,
        delays, w, h, frames,
        bpp, preferred_bytes_per_pixel);
    return pixels;
}

end_c

