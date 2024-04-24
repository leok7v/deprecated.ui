
enum {
    _colors_white     = rgb(255, 255, 255),
    _colors_off_white = rgb(192, 192, 192),
    _colors_dkgray4   = rgb(63, 63, 70),
    _colors_blue_highlight = rgb(128, 128, 255)
};

colors_t colors = {
    .none    = (int)0xFFFFFFFF, // aka CLR_INVALID in wingdi
    .text    = rgb(240, 231, 220),
    .white   = _colors_white,
    .black   = rgb(0,     0,   0),
    .red     = rgb(255,   0,   0),
    .green   = rgb(0,   255,   0),
    .blue    = rgb(0,   0,   255),
    .yellow  = rgb(255, 255,   0),
    .cyan    = rgb(0,   255, 255),
    .magenta = rgb(255,   0, 255),
    .gray    = rgb(128, 128, 128),
    .dkgray1  = rgb(30, 30, 30),
    .dkgray2  = rgb(37, 38, 38),
    .dkgray3  = rgb(45, 45, 48),
    .dkgray4  = _colors_dkgray4,
    // tone down RGB colors:
    .tone_white   = rgb(164, 164, 164),
    .tone_red     = rgb(192,  64,  64),
    .tone_green   = rgb(64,  192,  64),
    .tone_blue    = rgb(64,   64, 192),
    .tone_yellow  = rgb(192, 192,  64),
    .tone_cyan    = rgb(64,  192, 192),
    .tone_magenta = rgb(192,  64, 192),
    // misc:
    .orange  = rgb(255, 165, 0), // 0xFFA500
    .dkgreen = rgb(1, 50, 32),   // 0x013220
    // highlights:
    .text_highlight = rgb(190, 200, 255), // bluish off-white
    .blue_highlight = _colors_blue_highlight,
    .off_white = _colors_off_white,

    .btn_gradient_darker = rgb(16, 16, 16),
    .btn_gradient_dark   = _colors_dkgray4,
    .btn_hover_highlight = _colors_blue_highlight,
    .btn_disabled = _colors_dkgray4,
    .btn_armed = _colors_white,
    .btn_text = _colors_off_white,
    .toast = rgb(8, 40, 24) // toast background
};
