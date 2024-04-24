/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE at the end of file */
#ifndef qucik_defintion
#define qucik_defintion
#include "ut/ut.h"
#include "ui/ui.h"

#endif qucik_defintion

#if defined(quick_implementation) || defined(quick_implementation_console)
#undef quick_implementation

#include "ui/win32.h"

#define window() ((HWND)app.window)
#define canvas() ((HDC)app.canvas)

#include "src/ui/core.c"
#include "src/ui/gdi.c"
#include "src/ui/colors.c"
#include "src/ui/view.c"
#include "src/ui/label.c"
#include "src/ui/button.c"
#include "src/ui/checkbox.c"
#include "src/ui/slider.c"
#include "src/ui/messagebox.c"
#include "src/ui/layout.c"
#include "src/ui/app.c"

#if !defined(quick_implementation_console)

#pragma warning(disable: 28251) // inconsistent annotations

int WINAPI WinMain(HINSTANCE unused(instance), HINSTANCE unused(previous),
        char* unused(command), int show) {
    app.tid = threads.id();
    fatal_if_not_zero(CoInitializeEx(0, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY));
    // https://learn.microsoft.com/en-us/windows/win32/api/imm/nf-imm-immdisablelegacyime
    ImmDisableLegacyIME();
    // https://developercommunity.visualstudio.com/t/MSCTFdll-timcpp-An-assertion-failure-h/10513796
    ImmDisableIME(0); // temporarily disable IME till MS fixes that assert
    SetConsoleCP(CP_UTF8);
    __winnls_init__();
    app.visibility = show;
    args.WinMain();
    int32_t r = app_win_main();
    args.fini();
    return r;
}

#else

#undef quick_implementation_console

int main(int argc, const char* argv[]) {
    fatal_if_not_zero(CoInitializeEx(0, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY));
    __winnls_init__();
    app.argc = argc;
    app.argv = argv;
    app.tid = threads.id();
    return app.main();
}

#endif quick_implementation_console

#endif quick_implementation
