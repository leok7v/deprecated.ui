#include "crt.h"
#include "quick.h"
#include "stb_image.h"

#define crt_implementation
#include "crt.h"

#define quick_implementation
#include "quick.h"

#define STBI_ASSERT(x) assert(x)
#include "stb_image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static void init(void) { }

static int console(void) { return 0; }

app_t app = {
    .class_name = "prebuild",
    .init = init,
    .main = console,
    .no_ui = true,
    .min_width = 640,
    .min_height = 640
};
