/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"
#include "edit.h"

begin_c

#define glyph_chinese_one "\xE5\xA3\xB9"
#define glyph_chinese_two "\xE8\xB4\xB0"
#define glyph_teddy_bear  "\xF0\x9F\xA7\xB8"
#define glyph_ice_cube    "\xF0\x9F\xA7\x8A"

#define lorem_ipsum_canonique \
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "         \
    "eiusmod  tempor incididunt ut labore et dolore magna aliqua.Ut enim ad "  \
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip " \
    "ex ea commodo consequat.Duis aute irure dolor in reprehenderit in "       \
    "voluptate velit esse cillum dolore eu fugiat nulla pariatur.Excepteur "   \
    "sint occaecat cupidatat non proident, sunt in culpa qui officia "         \
    "deserunt mollit anim id est laborum."

static const char* test_content =
    "Good bye Universe...\n"
    "Hello World!\n"
    "\n"
    "Ctrl+Shift and F5 starts/stops FUZZING test.\n"
    "\n"
    "FUZZING use rapid mouse clicks thus UI Fuzz button is hard to press use keyboard shortcut F5 to stop.\n"
    "\n"
    "0         10        20        30        40        50        60        70        80        90\n"
    "01234567890123456789012345678901234567890abcdefghi01234567890123456789012345678901234567890123456789\n"
    "0         10        20        30        40        50        60        70        80        90\n"
    "01234567890123456789012345678901234567890abcdefghi01234567890123456789012345678901234567890123456789\n"
    "\n"
    "0" glyph_chinese_one glyph_chinese_two "3456789\n"
    "\n"
    glyph_teddy_bear "\n"
    glyph_teddy_bear glyph_ice_cube glyph_teddy_bear glyph_ice_cube glyph_teddy_bear glyph_ice_cube "\n"
    glyph_teddy_bear glyph_ice_cube glyph_teddy_bear " - " glyph_ice_cube glyph_teddy_bear glyph_ice_cube "\n"
    "\n"
    lorem_ipsum_canonique "\n"
    lorem_ipsum_canonique;

// 566K angular2.min.js
// https://get.cdnpkg.com/angular.js/2.0.0-beta.17/angular2.min.js
// https://web.archive.org/web/20230104221014/https://get.cdnpkg.com/angular.js/2.0.0-beta.17/angular2.min.js

// https://en.wikipedia.org/wiki/Lorem_ipsum

typedef struct {
    char* text;
    int32_t count; // at least 1KB
    uint32_t seed; // seed for random generator
    int32_t min_paragraphs; // at least 1
    int32_t max_paragraphs;
    int32_t min_sentences; // at least 1
    int32_t max_sentences;
    int32_t min_words; // at least 2
    int32_t max_words;
    const char* append; // append after each paragraph (e.g. extra "\n")
} uic_edit_lorem_ipsum_generator_params_t;

static void uic_edit_lorem_ipsum_generator(uic_edit_lorem_ipsum_generator_params_t p) {
    fatal_if(p.count < 1024); // at least 1KB expected
    fatal_if_false(0 < p.min_paragraphs && p.min_paragraphs <= p.max_paragraphs);
    fatal_if_false(0 < p.min_sentences && p.min_sentences <= p.max_sentences);
    fatal_if_false(2 < p.min_words && p.min_words <= p.max_words);
    static const char* words[] = {
        "lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipiscing",
        "elit", "quisque", "faucibus", "ex", "sapien", "vitae", "pellentesque",
        "sem", "placerat", "in", "id", "cursus", "mi", "pretium", "tellus",
        "duis", "convallis", "tempus", "leo", "eu", "aenean", "sed", "diam",
        "urna", "tempor", "pulvinar", "vivamus", "fringilla", "lacus", "nec",
        "metus", "bibendum", "egestas", "iaculis", "massa", "nisl",
        "malesuada", "lacinia", "integer", "nunc", "posuere", "ut",
        "hendrerit", "semper", "vel", "class", "aptent", "taciti", "sociosqu",
        "ad", "litora", "torquent", "per", "conubia", "nostra", "inceptos",
        "himenaeos", "orci", "varius", "natoque", "penatibus", "et", "magnis",
        "dis", "parturient", "montes", "nascetur", "ridiculus", "mus", "donec",
        "rhoncus", "eros", "lobortis", "nulla", "molestie", "mattis",
        "scelerisque", "maximus", "eget", "fermentum", "odio", "phasellus",
        "non", "purus", "est", "efficitur", "laoreet", "mauris", "pharetra",
        "vestibulum", "fusce", "dictum", "risus", "blandit", "quis",
        "suspendisse", "aliquet", "nisi", "sodales", "consequat", "magna",
        "ante", "condimentum", "neque", "at", "luctus", "nibh", "finibus",
        "facilisis", "dapibus", "etiam", "interdum", "tortor", "ligula",
        "congue", "sollicitudin", "erat", "viverra", "ac", "tincidunt", "nam",
        "porta", "elementum", "a", "enim", "euismod", "quam", "justo",
        "lectus", "commodo", "augue", "arcu", "dignissim", "velit", "aliquam",
        "imperdiet", "mollis", "nullam", "volutpat", "porttitor",
        "ullamcorper", "rutrum", "gravida", "cras", "eleifend", "turpis",
        "fames", "primis", "vulputate", "ornare", "sagittis", "vehicula",
        "praesent", "dui", "felis", "venenatis", "ultrices", "proin", "libero",
        "feugiat", "tristique", "accumsan", "maecenas", "potenti", "ultricies",
        "habitant", "morbi", "senectus", "netus", "suscipit", "auctor",
        "curabitur", "facilisi", "cubilia", "curae", "hac", "habitasse",
        "platea", "dictumst"
    };
    char* s = p.text;
    char* end = p.text + p.count - 64;
    uint32_t paragraphs = p.min_paragraphs +
        (p.min_paragraphs == p.max_paragraphs ? 0 :
         num.random32(&p.seed) % (p.max_paragraphs - p.min_paragraphs + 1));
    while (paragraphs > 0 && s < end) {
        uint32_t sentences_in_paragraph = p.min_sentences +
            (p.min_sentences == p.max_sentences ? 0 :
             num.random32(&p.seed) % (p.max_sentences - p.min_sentences + 1));
        while (sentences_in_paragraph > 0 && s < end) {
            const uint32_t words_in_sentence = p.min_words +
                (p.min_words == p.max_words ? 0 :
                 num.random32(&p.seed) % (p.max_words - p.min_words + 1));
            for (uint32_t i = 0; i < words_in_sentence && s < end; i++) {
                const char* word = words[num.random32(&p.seed) % countof(words)];
                memcpy(s, word, strlen(word));
                if (i == 0) { *s = (char)toupper(*s); }
                s += strlen(word);
                if (i < words_in_sentence - 1 && s < end) {
                    const char* delimiter = "\x20";
                    int32_t punctuation = num.random32(&p.seed) % 128;
                    switch (punctuation) {
                        case 0:
                        case 1:
                        case 2: delimiter = ", "; break;
                        case 3:
                        case 4: delimiter = "; "; break;
                        case 6: delimiter = ": "; break;
                        case 7: delimiter = " - "; break;
                        default: break;
                    }
                    memcpy(s, delimiter, strlen(delimiter));
                    s += strlen(delimiter);
                }
            }
            if (sentences_in_paragraph > 1 && s < end) {
                memcpy(s, ".\x20", 2);
                s += 2;
            } else {
                *s++ = '.';
            }
            sentences_in_paragraph--;
        }
        if (paragraphs > 1 && s < end) {
            *s++ = '\n';
        }
        if (p.append != null && p.append[0] != 0) {
            memcpy(s, p.append, strlen(p.append));
            s += strlen(p.append);
        }
        paragraphs--;
    }
    *s = 0;
//  traceln("%s\n", p.text);
}

void uic_edit_init_with_lorem_ipsum(uic_edit_t* e) {
    fatal_if(e->paragraphs != 0);
    static char text[64 * 1024];
    uic_edit_lorem_ipsum_generator_params_t p = {
        .text = text,
        .count = countof(text),
        .min_paragraphs = 4,
        .max_paragraphs = 15,
        .min_sentences  = 4,
        .max_sentences  = 20,
        .min_words      = 8,
        .max_words      = 16,
        .append         = "\n"
    };
    #ifdef DEBUG
        p.seed = 1; // repeatable sequence of pseudo random numbers
    #else
        p.seed = (int32_t)clock.nanoseconds() | 0x1; // must be odd
    #endif
    uic_edit_lorem_ipsum_generator(p);
    e->paste(e, test_content, (int32_t)strlen(test_content));
    e->paste(e, "\n\n", 2);
    e->paste(e, p.text, (int32_t)strlen(p.text));
    e->move(e, (uic_edit_pg_t){ .pn = 0, .gp = 0});
}

void uic_edit_next_fuzz(uic_edit_t* e) {
    atomics.increment_int32(&e->fuzz_count);
}

static void uic_edit_fuzzer(void* p) {
    uic_edit_t* e = (uic_edit_t*)p;
    for (;;) {
        while (!e->fuzz_quit && e->fuzz_count == e->fuzz_last) {
            threads.sleep_for(1.0 / 1024.0); // ~1ms
        }
        if (e->fuzz_quit) { e->fuzz_quit = false; break; }
        e->fuzz_last = e->fuzz_count;
        uint32_t rnd = num.random32(&e->fuzz_seed);
        switch (rnd % 8) {
            case 0: app.alt = 0; app.ctrl = 0; app.shift = 0; break;
            case 1: app.alt = 1; app.ctrl = 0; app.shift = 0; break;
            case 2: app.alt = 0; app.ctrl = 1; app.shift = 0; break;
            case 3: app.alt = 1; app.ctrl = 1; app.shift = 0; break;
            case 4: app.alt = 0; app.ctrl = 0; app.shift = 1; break;
            case 5: app.alt = 1; app.ctrl = 0; app.shift = 1; break;
            case 6: app.alt = 0; app.ctrl = 1; app.shift = 1; break;
            case 7: app.alt = 1; app.ctrl = 1; app.shift = 1; break;
            default: assert(false);
        }
        int keys[] = {
            virtual_keys.up,
            virtual_keys.down,
            virtual_keys.left,
            virtual_keys.right,
            virtual_keys.home,
            virtual_keys.end,
            virtual_keys.pageup,
            virtual_keys.pagedw,
            virtual_keys.insert,
// TODO: cut copy paste erase need special treatment in fuzzing
//       otherwise text collapses to nothing pretty fast
//          virtual_keys.del,
//          virtual_keys.back,
            virtual_keys.enter,
            0
        };
// TODO: Alt+Q should be filtered out as well as ESC
        rnd = num.random32(&e->fuzz_seed);
        int key = keys[rnd % countof(keys)];
        if (key == 0) {
            rnd = num.random32(&e->fuzz_seed);
            int ch = rnd % 128;
            if (ch == 033) { ch = 'a'; } // don't send ESC
            app.post(messages.character, ch, 0);
        } else {
            app.post(messages.key_pressed, key, 0);
        }
        if (num.random32(&e->fuzz_seed) % 32 == 0) {
            // mouse events only inside edit control otherwise
            // they will start clicking buttons around
            int32_t x = num.random32(&e->fuzz_seed) % e->ui.w;
            int32_t y = num.random32(&e->fuzz_seed) % e->ui.h;
            app.post(messages.mouse_move,   0, (int64_t)(x | (y << 16)));
            app.post(messages.left_button_pressed,  0, (int64_t)(x | (y << 16)));
            app.post(messages.left_button_released, 0, (int64_t)(x | (y << 16)));
        }
    }
}

void uic_edit_fuzz(uic_edit_t* e) {
    if (e->fuzzer == null) {
        app.request_focus(); // force application to be focused
        e->fuzzer = threads.start(uic_edit_fuzzer, e);
        uic_edit_next_fuzz(e);
    } else {
        e->fuzz_quit = true;
        threads.join(e->fuzzer, -1);
        e->fuzzer = null;
    }
    traceln("fuzzing %s",e->fuzzer != null ? "started" : "stopped");
}

end_c
