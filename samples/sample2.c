/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include <math.h>

begin_c

const char* title = "Sample2";

enum { N = 1800 };

static tm_t timer10ms;
static thread_t thread;
static bool quit;

typedef struct {
    double time[N]; // circular buffer of timestamps
    int pos; // writing position in the buffer for next timer event
    int samples; // number of samples collected
    double dt[N]; // delta(t) between 2 timer events
    double min_dt;
    double max_dt;
    double avg;
    double spread;
} time_stats_t;

static volatile time_stats_t ts[2];

static ui_point_t points[N]; // graph polyline coordinates

static void stats(volatile time_stats_t* t) {
    int n = min(N - 1, t->samples);
    t->min_dt = 1.0; // 1 second is 100x of 10ms
    t->max_dt = 0;
    int j = 0;
    double sum = 0;
    for (int i = 0; i < n - 1; i++) {
        int p0 = (t->pos - i - 1 + N) % N;
        int p1 = (p0 - 1 + N) % N;
        t->dt[j] = t->time[p0] - (t->time[p1] + 0.01); // expected 10ms
        t->min_dt = min(t->dt[j], t->min_dt);
        t->max_dt = max(t->dt[j], t->max_dt);
        sum += t->time[p0] - t->time[p1];
        j++;
    }
    t->avg = sum / (n - 1);
    j = 0;
    t->spread = max(fabs(t->max_dt), fabs(t->min_dt));
}

static void graph(uic_t* ui, volatile time_stats_t* t, color_t c, int y) {
    const int h2 = app.crc.h / 2;
    const int h4 = h2 / 2;
    const int h8 = h4 / 2;
    gdi.y = y - h8;
    gdi.set_colored_pen(colors.white);
    gdi.move_to(0, y);
    gdi.line(app.crc.w, y);
    gdi.set_colored_pen(c);
    if (ts[0].samples > 2 && ts[1].samples > 2) {
        const double spread = max(ts[0].spread, ts[0].spread);
        int n = min(N - 1, t->samples);
        int j = 0;
        for (int i = 0; i < n - 1; i++) {
            double v = t->dt[j] / spread;
            points[j].x = n - 1 - i;
            points[j].y = y + (int32_t)(v * h8);
            j++;
        }
        gdi.poly(points, n - 1);

        gdi.x = ui->em.x;
        gdi.y = y - h8 - ui->em.y;
        gdi.println("min %.3f max %.3f avg %.3f ms  "
            "%.1f sps",
            t->min_dt * 1000, t->max_dt * 1000, t->avg, 1 / t->avg);
    }
}

static void timer_thread(void* p) {
    bool* done = (bool*)p;
    threads.name("r/t timer");
    threads.realtime();
    while (!*done) {
        crt.sleep(0.0094);
        ts[1].time[ts[1].pos] = crt.seconds();
        ts[1].pos = (ts[1].pos + 1) % N;
        (ts[1].samples)++;
        app.redraw();
    }
}

static void paint(uic_t* ui) {
    stats(&ts[0]);
    stats(&ts[1]);
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.dkgray1);
    gdi.fill(0, 0, ui->w, ui->h);
    gdi.y = ui->em.y;
    gdi.x = ui->w - gdi.measure_text(app.fonts.mono,
        "avg paint time %.1f ms", app.paint_avg * 1000).x - ui->em.x;
    gdi.print("avg paint time %.1f ms", app.paint_avg * 1000);
    gdi.x = ui->em.x;
    gdi.print("10ms window timer jitter ");
    gdi.print("(\"sps\" stands for samples per second)");
    const int h2 = app.crc.h / 2;
    const int h4 = h2 / 2;
    graph(ui, &ts[0], colors.tone_red, h4);
    gdi.y = h2;
    gdi.print("10ms r/t thread sleep jitter");
    graph(ui, &ts[1], colors.tone_green, h2 + h4);
    gdi.y = h2 - h4;

}

static void timer(uic_t* ui, tm_t id) {
    assert(ui == app.ui); (void)ui;
    // there are at least 3 timers notifications coming here:
    // 1 seconds, 100ms and 10ms:
    if (id == timer10ms) {
        ts[0].time[ts[0].pos] = app.now;
        ts[0].pos = (ts[0].pos + 1) % N;
        (ts[0].samples)++;
    }
}

static void openned(void) {
    timer10ms = app.set_timer((uintptr_t)&timer10ms, 10);
    fatal_if(timer10ms == 0);
    thread = threads.start(timer_thread, &quit);
    fatal_if_null(thread);
}

static void closed(void) {
    app.kill_timer(timer10ms);
    quit = true;
    threads.join(thread);
    thread = null;
    quit = false;
}

static void do_not_start_minimized(void) {
    // This sample does not start minimized but some applications may.
    if (app.last_visibility != window_visibility.minimize) {
        app.visibility = app.last_visibility;
    } else {
        app.visibility = window_visibility.defau1t;
        app.last_visibility = window_visibility.defau1t;
    }
}

static void init(void) {
    app.title = title;
    threads.realtime(); // both main thread and timer thread
    app.ui->timer = timer;
    app.closed = closed;
    app.openned = openned;
    app.ui->paint = paint;
    // no minimize/maximize title bar and system menu
    app.no_min = true;
    app.no_max = true;
    do_not_start_minimized();
}

app_t app = {
    .class_name = "sample2",
    .init = init,
    // fixed size app
    .wmin = 11.0f, // 11x7 inches
    .hmin =  7.0f,
    .wmax = 11.0f,
    .hmax =  7.0f,
};

end_c
