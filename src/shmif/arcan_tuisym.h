/*
 * For compatibility / long-time legacy reasons, the keysym field of a
 * TRANSLATED input event in shmif corresponds to the SDL1.2 list of keysyms.
 *
 * Avoid using these directly/exclusively and instead use the more abstract
 * label facility for registering custom inputs, though the inputs themselves
 * may refer to symbol + modifier as part of the default binding.
 *
 * For actual input, the utf8 field should provide a unicode codepoint
 * corresponding to the current desired symbol.
 */
#ifndef TUIK_SYMS
#define TUIK_SYMS

/*
 * The rest are just renamed / remapped arcan_tui_ calls from libtsm-
 * (which hides inside the tui_context) selected based on what the tsm/pty
 * management required assuming that it is good enough.
 */
enum tui_context_flags {
	TUI_INSERT_MODE = 1,
	TUI_AUTO_WRAP = 2,
	TUI_REL_ORIGIN = 4,
	TUI_INVERSE = 8,
	TUI_HIDE_CURSOR = 16,
	TUI_FIXED_POS = 32,
	TUI_ALTERNATE = 64
};

enum tui_color_group {
	TUI_COL_PRIMARY = 0,
	TUI_COL_SECONDARY,
	TUI_COL_BG,
	TUI_COL_FG,
	TUI_COL_CURSOR,
	TUI_COL_ALTCURSOR,
	TUI_COL_HIGHLIGHT,
	TUI_COL_LABEL,
	TUI_COL_WARNING,
	TUI_COL_ERROR,
	TUI_COL_INACTIVE
};

enum tui_cursors {
	CURSOR_BLOCK = 0,
	CURSOR_HALFBLOCK,
	CURSOR_FRAME,
	CURSOR_VLINE,
	CURSOR_ULINE,
	CURSOR_END
};

enum tui_render_flags {
	TUI_RENDER_BITMAP = 1,
	TUI_RENDER_DBLBUF = 2,
	TUI_RENDER_ACCEL = 4
};

/* bitmap derived from shmif_event, repeated here for namespace purity */
enum {
	TUIM_NONE = 0x0000,
	TUIM_LSHIFT = 0x0001,
	TUIM_RSHIFT = 0x0002,
	TUIM_LCTRL = 0x0040,
	TUIM_RCTRL = 0x0080,
	TUIM_LALT = 0x0100,
	TUIM_RALT = 0x0200,
	TUIM_ALT = 0x0300,
	TUIM_LMETA = 0x0400,
	TUIM_RMETA = 0x0800,
	TUIM_META = 0x0c00,
	TUIM_REPEAT = 0x8000,
} tuim_syms;

enum tuik_syms {
	TUIK_UNKNOWN = 0,
	TUIK_FIRST = 0,
	TUIK_BACKSPACE = 8,
	TUIK_TAB = 9,
	TUIK_CLEAR = 12,
	TUIK_RETURN = 13,
	TUIK_PAUSE = 19,
	TUIK_ESCAPE = 27,
	TUIK_SPACE = 32,
	TUIK_EXCLAIM = 33,
	TUIK_QUOTEDBL = 34,
	TUIK_HASH = 35,
	TUIK_DOLLAR = 36,
	TUIK_0 = 48,
	TUIK_1 = 49,
	TUIK_2 = 50,
	TUIK_3 = 51,
	TUIK_4 = 52,
	TUIK_5 = 53,
	TUIK_6 = 54,
	TUIK_7 = 55,
	TUIK_8 = 56,
	TUIK_9 = 57,
	TUIK_MINUS = 20,
	TUIK_EQUALS = 21,
	TUIK_A = 97,
  TUIK_B = 98,
	TUIK_C = 99,
	TUIK_D = 100,
	TUIK_E = 101,
	TUIK_F = 102,
	TUIK_G = 103,
	TUIK_H = 104,
	TUIK_I = 105,
	TUIK_J = 106,
	TUIK_K = 107,
	TUIK_L = 108,
	TUIK_M = 109,
	TUIK_N = 110,
	TUIK_O = 111,
	TUIK_P = 112,
	TUIK_Q = 113,
	TUIK_R = 114,
	TUIK_S = 115,
	TUIK_T = 116,
	TUIK_U = 117,
	TUIK_V = 118,
	TUIK_W = 119,
	TUIK_X = 120,
	TUIK_Y = 121,
	TUIK_Z = 122,
	TUIK_LESS = 60, /* 102nd key, right of lshift */
	TUIK_KP_LEFTBRACE = 91,
	TUIK_KP_RIGHTBRACE = 93,
	TUIK_KP_ENTER = 271,
	TUIK_LCTRL = 306,
	TUIK_SEMICOLON = 59,
	TUIK_APOSTROPHE = 48,
	TUIK_GRAVE = 49,
	TUIK_LSHIFT = 304,
	TUIK_BACKSLASH = 92,
	TUIK_COMMA = 44,
	TUIK_PERIOD = 46,
	TUIK_SLASH = 61,
	TUIK_RSHIFT = 303,
	TUIK_KP_MULTIPLY = 268,
	TUIK_LALT = 308,
	TUIK_CAPSLOCK = 301,
	TUIK_F1 = 282,
	TUIK_F2 = 283,
	TUIK_F3 = 284,
	TUIK_F4 = 285,
	TUIK_F5 = 286,
	TUIK_F6 = 287,
	TUIK_F7 = 288,
	TUIK_F8 = 289,
	TUIK_F9 = 290,
	TUIK_F10 = 291,
	TUIK_NUMLOCKCLEAR = 300,
	TUIK_SCROLLLOCK = 302,
	TUIK_KP_0 = 256,
	TUIK_KP_1 = 257,
	TUIK_KP_2 = 258,
	TUIK_KP_3 = 259,
	TUIK_KP_4 = 260,
	TUIK_KP_5 = 261,
	TUIK_KP_6 = 262,
	TUIK_KP_7 = 263,
	TUIK_KP_8 = 264,
	TUIK_KP_9 = 265,
	TUIK_KP_MINUS = 269,
	TUIK_KP_PLUS = 270,
	TUIK_KP_PERIOD = 266,
	TUIK_INTERNATIONAL1,
	TUIK_INTERNATIONAL2,
	TUIK_F11 = 292,
	TUIK_F12 = 293,
	TUIK_INTERNATIONAL3,
	TUIK_INTERNATIONAL4,
	TUIK_INTERNATIONAL5,
	TUIK_INTERNATIONAL6,
	TUIK_INTERNATIONAL7,
	TUIK_INTERNATIONAL8,
	TUIK_INTERNATIONAL9,
	TUIK_RCTRL = 305,
	TUIK_KP_DIVIDE = 267,
	TUIK_SYSREQ = 317,
	TUIK_RALT = 307,
	TUIK_HOME = 278,
	TUIK_UP = 273,
	TUIK_PAGEUP = 280,
	TUIK_LEFT = 276,
	TUIK_RIGHT = 275,
	TUIK_END = 279,
	TUIK_DOWN = 274,
	TUIK_PAGEDOWN = 281,
	TUIK_INSERT = 277,
	TUIK_DELETE = 127,
	TUIK_LMETA = 310,
	TUIK_RMETA = 309,
	TUIK_COMPOSE = 314,
	TUIK_MUTE,
	TUIK_VOLUMEDOWN,
	TUIK_VOLUMEUP,
	TUIK_POWER,
	TUIK_KP_EQUALS,
	TUIK_KP_PLUSMINUS,
	TUIK_LANG1,
	TUIK_LANG2,
	TUIK_LANG3,
	TUIK_LGUI,
	TUIK_RGUI,
	TUIK_STOP,
	TUIK_AGAIN
};

#endif
