//
// cl_keyboard.c -- on-screen virtual keyboard for VR
//
// This keyboard is part of the client and sends key events through the
// normal input path (CL_KeyEvent/CL_CharEvent), so it works with both
// the console and UI menus automatically.
//

#include "client.h"
#include "../vr/vr_clientinfo.h"

extern vr_clientinfo_t vr;

#define KEYBOARD_ROWS			4
#define KEYBOARD_KEY_WIDTH		36
#define KEYBOARD_KEY_HEIGHT		32
#define KEYBOARD_KEY_SPACING	4
#define KEYBOARD_ROW_SPACING	4
#define KEYBOARD_PADDING		12
#define KEYBOARD_START_Y		308
#define KEYBOARD_TEXT_SIZE		10	// 120% of SMALLCHAR_WIDTH (8)

// Special key codes (internal to keyboard)
#define VKEY_NONE		0
#define VKEY_SHIFT		1
#define VKEY_BACKSPACE	2
#define VKEY_SYMBOLS	3
#define VKEY_SPACE		4
#define VKEY_ENTER		5
#define VKEY_LEFT		6
#define VKEY_RIGHT		7
#define VKEY_UP			8
#define VKEY_DOWN		9
#define VKEY_TAB		10

// Time window for double-tap shift to enable caps lock (milliseconds)
#define CAPSLOCK_DOUBLE_TAP_TIME	400

// Keyboard modes
#define MODE_LOWERCASE	0
#define MODE_UPPERCASE	1
#define MODE_SYMBOLS1	2
#define MODE_SYMBOLS2	3

// Keyboard state
typedef struct {
	qboolean	active;
	int			mode;
	qboolean	capsLock;
	int			lastShiftTime;
} vKeyboardState_t;

static vKeyboardState_t vkb;

// Key definition
typedef struct {
	char		lowercase;
	char		uppercase;
	char		symbol1;
	char		symbol2;
	int			special;
	float		width;
	const char	*label;
	const char	*symbolLabel;
} vKeyDef_t;

// Row 0: Q W E R T Y U I O P (letters) / 1-0 (sym1) / [ ] { } # % ^ * + = (sym2)
static vKeyDef_t vkbRow0[] = {
	{'q', 'Q', '1', '[', 0, 1.0f, NULL, NULL},
	{'w', 'W', '2', ']', 0, 1.0f, NULL, NULL},
	{'e', 'E', '3', '{', 0, 1.0f, NULL, NULL},
	{'r', 'R', '4', '}', 0, 1.0f, NULL, NULL},
	{'t', 'T', '5', '#', 0, 1.0f, NULL, NULL},
	{'y', 'Y', '6', '%', 0, 1.0f, NULL, NULL},
	{'u', 'U', '7', '^', 0, 1.0f, NULL, NULL},
	{'i', 'I', '8', '*', 0, 1.0f, NULL, NULL},
	{'o', 'O', '9', '+', 0, 1.0f, NULL, NULL},
	{'p', 'P', '0', '=', 0, 1.0f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

// Row 1: [Tab/CON] A S D F G H J K L
static vKeyDef_t vkbRow1[] = {
	{0, 0, 0, 0, VKEY_TAB, 1.5f, "Tab", "Tab"},
	{'a', 'A', '-', '_', 0, 1.0f, NULL, NULL},
	{'s', 'S', '/', '\\', 0, 1.0f, NULL, NULL},
	{'d', 'D', ':', '|', 0, 1.0f, NULL, NULL},
	{'f', 'F', ';', ';', 0, 1.0f, NULL, NULL},
	{'g', 'G', '(', '<', 0, 1.0f, NULL, NULL},
	{'h', 'H', ')', '>', 0, 1.0f, NULL, NULL},
	{'j', 'J', '$', '`', 0, 1.0f, NULL, NULL},
	{'k', 'K', '&', '/', 0, 1.0f, NULL, NULL},
	{'l', 'L', '@', '-', 0, 1.0f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

// Row 2: [Shift/#+=] Z X C V B N M [Backspace]
static vKeyDef_t vkbRow2[] = {
	{0, 0, 0, 0, VKEY_SHIFT, 1.5f, NULL, "#+="},
	{'z', 'Z', '.', '.', 0, 1.0f, NULL, NULL},
	{'x', 'X', ',', ',', 0, 1.0f, NULL, NULL},
	{'c', 'C', '?', '?', 0, 1.0f, NULL, NULL},
	{'v', 'V', '!', '!', 0, 1.0f, NULL, NULL},
	{'b', 'B', '\'', '\'', 0, 1.0f, NULL, NULL},
	{'n', 'N', '"', '"', 0, 1.0f, NULL, NULL},
	{'m', 'M', '=', ':', 0, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_BACKSPACE, 1.5f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

// Row 3: [123/ABC] [Space] [arrows] [Done]
static vKeyDef_t vkbRow3[] = {
	{0, 0, 0, 0, VKEY_SYMBOLS, 1.3f, "123", "ABC"},
	{0, 0, 0, 0, VKEY_SPACE, 4.5f, " ", " "},
	{0, 0, 0, 0, VKEY_LEFT, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_UP, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_DOWN, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_RIGHT, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_ENTER, 1.3f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

static vKeyDef_t *vkbRows[KEYBOARD_ROWS] = { vkbRow0, vkbRow1, vkbRow2, vkbRow3 };

// Icon identifiers
#define ICON_NONE		0
#define ICON_BACKSPACE	1
#define ICON_LEFT		2
#define ICON_RIGHT		3
#define ICON_UP			4
#define ICON_DOWN		5
#define ICON_SHIFT		6
#define ICON_CAPSLOCK	7
#define ICON_ENTER		8

/*
=================
VKeyboard_DrawIcon
=================
*/
static void VKeyboard_DrawIcon( int icon, int x, int y, int w, int h, vec4_t color ) {
	int cx = x + w/2;
	int cy = y + h/2;
	static vec4_t xColor = {0.1f, 0.1f, 0.12f, 1.0f};

	switch (icon) {
		case ICON_BACKSPACE:
			{
				int boxW = 18;
				int boxH = 12;
				int bx = cx - boxW/2 + 2;
				int by = cy - boxH/2;
				SCR_FillRect(bx + 6, by, boxW - 6, boxH, color);
				SCR_FillRect(bx + 4, by + 1, 3, boxH - 2, color);
				SCR_FillRect(bx + 2, by + 2, 3, boxH - 4, color);
				SCR_FillRect(bx, by + 3, 3, boxH - 6, color);
				{
					int xCx = bx + 6 + (boxW - 6)/2;
					int xCy = cy;
					SCR_FillRect(xCx - 3, xCy - 3, 2, 2, xColor);
					SCR_FillRect(xCx - 1, xCy - 1, 2, 2, xColor);
					SCR_FillRect(xCx + 1, xCy + 1, 2, 2, xColor);
					SCR_FillRect(xCx + 1, xCy - 3, 2, 2, xColor);
					SCR_FillRect(xCx - 3, xCy + 1, 2, 2, xColor);
				}
			}
			break;

		case ICON_LEFT:
			SCR_FillRect(cx + 3, cy - 5, 2, 2, color);
			SCR_FillRect(cx + 1, cy - 3, 2, 2, color);
			SCR_FillRect(cx - 1, cy - 1, 2, 3, color);
			SCR_FillRect(cx + 1, cy + 2, 2, 2, color);
			SCR_FillRect(cx + 3, cy + 4, 2, 2, color);
			break;

		case ICON_RIGHT:
			SCR_FillRect(cx - 4, cy - 5, 2, 2, color);
			SCR_FillRect(cx - 2, cy - 3, 2, 2, color);
			SCR_FillRect(cx, cy - 1, 2, 3, color);
			SCR_FillRect(cx - 2, cy + 2, 2, 2, color);
			SCR_FillRect(cx - 4, cy + 4, 2, 2, color);
			break;

		case ICON_UP:
			SCR_FillRect(cx - 5, cy + 3, 2, 2, color);
			SCR_FillRect(cx - 3, cy + 1, 2, 2, color);
			SCR_FillRect(cx - 1, cy - 1, 3, 2, color);
			SCR_FillRect(cx + 2, cy + 1, 2, 2, color);
			SCR_FillRect(cx + 4, cy + 3, 2, 2, color);
			break;

		case ICON_DOWN:
			SCR_FillRect(cx - 5, cy - 4, 2, 2, color);
			SCR_FillRect(cx - 3, cy - 2, 2, 2, color);
			SCR_FillRect(cx - 1, cy, 3, 2, color);
			SCR_FillRect(cx + 2, cy - 2, 2, 2, color);
			SCR_FillRect(cx + 4, cy - 4, 2, 2, color);
			break;

		case ICON_SHIFT:
			SCR_FillRect(cx - 1, cy - 8, 3, 2, color);
			SCR_FillRect(cx - 3, cy - 6, 2, 2, color);
			SCR_FillRect(cx + 2, cy - 6, 2, 2, color);
			SCR_FillRect(cx - 5, cy - 4, 2, 2, color);
			SCR_FillRect(cx + 4, cy - 4, 2, 2, color);
			SCR_FillRect(cx - 7, cy - 2, 2, 2, color);
			SCR_FillRect(cx + 6, cy - 2, 2, 2, color);
			SCR_FillRect(cx - 7, cy, 4, 2, color);
			SCR_FillRect(cx + 4, cy, 4, 2, color);
			SCR_FillRect(cx - 3, cy, 2, 8, color);
			SCR_FillRect(cx + 2, cy, 2, 8, color);
			SCR_FillRect(cx - 3, cy + 6, 7, 2, color);
			break;

		case ICON_CAPSLOCK:
			SCR_FillRect(cx - 1, cy - 8, 3, 2, color);
			SCR_FillRect(cx - 3, cy - 6, 7, 2, color);
			SCR_FillRect(cx - 5, cy - 4, 11, 2, color);
			SCR_FillRect(cx - 7, cy - 2, 15, 2, color);
			SCR_FillRect(cx - 3, cy, 7, 6, color);
			SCR_FillRect(cx - 5, cy + 8, 11, 2, color);
			break;

		case ICON_ENTER:
			// Draw return/enter symbol (bent arrow)
			// Vertical stem going up from arrow
			SCR_FillRect(cx + 6, cy - 6, 2, 8, color);
			// Horizontal bar going left
			SCR_FillRect(cx - 6, cy, 14, 2, color);
			// Arrow head pointing left
			SCR_FillRect(cx - 4, cy - 2, 2, 2, color);
			SCR_FillRect(cx - 6, cy, 2, 2, color);
			SCR_FillRect(cx - 4, cy + 2, 2, 2, color);
			break;
	}
}

/*
=================
VKeyboard_GetKeyIcon
=================
*/
static int VKeyboard_GetKeyIcon( vKeyDef_t *key ) {
	if (!key->special) {
		return ICON_NONE;
	}

	switch (key->special) {
		case VKEY_BACKSPACE:
			return ICON_BACKSPACE;
		case VKEY_LEFT:
			return ICON_LEFT;
		case VKEY_RIGHT:
			return ICON_RIGHT;
		case VKEY_UP:
			return ICON_UP;
		case VKEY_DOWN:
			return ICON_DOWN;
		case VKEY_SHIFT:
			if (vkb.mode == MODE_LOWERCASE || vkb.mode == MODE_UPPERCASE) {
				if (vkb.capsLock) {
					return ICON_CAPSLOCK;
				}
				return ICON_SHIFT;
			}
			return ICON_NONE;
		case VKEY_ENTER:
			return ICON_ENTER;
		default:
			return ICON_NONE;
	}
}

static int VKeyboard_GetRowWidth( vKeyDef_t *row ) {
	int width = 0;
	int i;
	int count = 0;
	for (i = 0; row[i].width > 0; i++) {
		width += (int)(row[i].width * KEYBOARD_KEY_WIDTH);
		count++;
	}
	if (count > 1) {
		width += (count - 1) * KEYBOARD_KEY_SPACING;
	}
	return width;
}

static int VKeyboard_GetMaxRowWidth( void ) {
	int maxWidth = 0;
	int i;
	for (i = 0; i < KEYBOARD_ROWS; i++) {
		int w = VKeyboard_GetRowWidth(vkbRows[i]);
		if (w > maxWidth) maxWidth = w;
	}
	return maxWidth;
}

static int VKeyboard_GetStartX( int rowWidth ) {
	return (SCREEN_WIDTH - rowWidth) / 2;
}

/*
=================
VKeyboard_Show
=================
*/
void VKeyboard_Show( void ) {
	vkb.active = qtrue;
	vkb.mode = MODE_LOWERCASE;
	vkb.capsLock = qfalse;
	vkb.lastShiftTime = 0;
}

/*
=================
VKeyboard_Hide
=================
*/
void VKeyboard_Hide( void ) {
	vkb.active = qfalse;
}

/*
=================
VKeyboard_IsActive
=================
*/
qboolean VKeyboard_IsActive( void ) {
	return vkb.active;
}

/*
=================
VKeyboard_GetKeyAt
=================
*/
static vKeyDef_t* VKeyboard_GetKeyAt( int x, int y ) {
	int row;
	int keyX, keyY, keyW;
	int i;

	for (row = 0; row < KEYBOARD_ROWS; row++) {
		int rowWidth = VKeyboard_GetRowWidth(vkbRows[row]);
		int startX = VKeyboard_GetStartX(rowWidth);

		keyY = KEYBOARD_START_Y + row * (KEYBOARD_KEY_HEIGHT + KEYBOARD_ROW_SPACING);

		if (y < keyY || y > keyY + KEYBOARD_KEY_HEIGHT) {
			continue;
		}

		keyX = startX;
		for (i = 0; vkbRows[row][i].width > 0; i++) {
			vKeyDef_t *key = &vkbRows[row][i];
			keyW = (int)(key->width * KEYBOARD_KEY_WIDTH);

			if (x >= keyX && x <= keyX + keyW) {
				return key;
			}

			keyX += keyW + KEYBOARD_KEY_SPACING;
		}
	}

	return NULL;
}

/*
=================
VKeyboard_IsInKeyboardArea

Returns qtrue if the given point is within the keyboard background area
(the tinted overlay), even if not directly on a key.
=================
*/
static qboolean VKeyboard_IsInKeyboardArea( int x, int y ) {
	int maxWidth = VKeyboard_GetMaxRowWidth();
	int totalHeight = KEYBOARD_ROWS * KEYBOARD_KEY_HEIGHT + (KEYBOARD_ROWS - 1) * KEYBOARD_ROW_SPACING;
	int bgX = VKeyboard_GetStartX(maxWidth) - KEYBOARD_PADDING;
	int bgY = KEYBOARD_START_Y - KEYBOARD_PADDING;
	int bgW = maxWidth + KEYBOARD_PADDING * 2;
	int bgH = totalHeight + KEYBOARD_PADDING * 2;

	return (x >= bgX && x <= bgX + bgW && y >= bgY && y <= bgY + bgH);
}

/*
=================
VKeyboard_Draw
=================
*/
void VKeyboard_Draw( void ) {
	int row;
	int keyX, keyY, keyW;
	int i;
	int maxWidth = VKeyboard_GetMaxRowWidth();
	int totalHeight = KEYBOARD_ROWS * KEYBOARD_KEY_HEIGHT + (KEYBOARD_ROWS - 1) * KEYBOARD_ROW_SPACING;
	int bgX = VKeyboard_GetStartX(maxWidth) - KEYBOARD_PADDING;
	int bgY = KEYBOARD_START_Y - KEYBOARD_PADDING;
	int bgW = maxWidth + KEYBOARD_PADDING * 2;
	int bgH = totalHeight + KEYBOARD_PADDING * 2;

	vec4_t bgColor = {0.1f, 0.1f, 0.12f, 0.95f};
	vec4_t keyColor = {0.25f, 0.25f, 0.28f, 1.0f};
	vec4_t keyHoverColor = {0.4f, 0.4f, 0.45f, 1.0f};
	vec4_t keyActiveColor = {0.3f, 0.5f, 0.8f, 1.0f};
	vec4_t keySpecialColor = {0.2f, 0.2f, 0.22f, 1.0f};
	vec4_t textColor = {1.0f, 1.0f, 1.0f, 1.0f};
	vec4_t textDimColor = {0.7f, 0.7f, 0.7f, 1.0f};
	char str[2];
	vKeyDef_t *hoverKey;
	int cursorX, cursorY;

	if (!vkb.active) {
		return;
	}

	// Get cursor position from VR (which updates uis.cursorx/y through menuCursorX/Y pointers)
	if (vr.menuCursorX && vr.menuCursorY) {
		cursorX = *vr.menuCursorX;
		cursorY = *vr.menuCursorY;
	} else {
		// Fallback - center of screen
		cursorX = SCREEN_WIDTH / 2;
		cursorY = SCREEN_HEIGHT / 2;
	}

	hoverKey = VKeyboard_GetKeyAt(cursorX, cursorY);

	// Draw background
	SCR_FillRect(bgX, bgY, bgW, bgH, bgColor);

	// Draw keys
	for (row = 0; row < KEYBOARD_ROWS; row++) {
		int rowWidth = VKeyboard_GetRowWidth(vkbRows[row]);
		int startX = VKeyboard_GetStartX(rowWidth);

		keyX = startX;
		keyY = KEYBOARD_START_Y + row * (KEYBOARD_KEY_HEIGHT + KEYBOARD_ROW_SPACING);

		for (i = 0; vkbRows[row][i].width > 0; i++) {
			vKeyDef_t *key = &vkbRows[row][i];
			qboolean isHovered = (key == hoverKey);
			qboolean isActive = qfalse;
			qboolean isSpecial = (key->special != 0);
			vec4_t *color;
			const char *label = NULL;
			char ch = 0;

			keyW = (int)(key->width * KEYBOARD_KEY_WIDTH);

			if (key->special == VKEY_SHIFT) {
				isActive = (vkb.mode == MODE_UPPERCASE) || vkb.capsLock || (vkb.mode == MODE_SYMBOLS2);
			} else if (key->special == VKEY_SYMBOLS) {
				isActive = (vkb.mode == MODE_SYMBOLS1) || (vkb.mode == MODE_SYMBOLS2);
			}

			if (isActive) {
				color = &keyActiveColor;
			} else if (isHovered) {
				color = &keyHoverColor;
			} else if (isSpecial) {
				color = &keySpecialColor;
			} else {
				color = &keyColor;
			}

			SCR_FillRect(keyX, keyY, keyW, KEYBOARD_KEY_HEIGHT, *color);

			{
				int icon = VKeyboard_GetKeyIcon(key);

				if (icon != ICON_NONE) {
					VKeyboard_DrawIcon(icon, keyX, keyY, keyW, KEYBOARD_KEY_HEIGHT,
						isSpecial ? textDimColor : textColor);
				} else if (key->special) {
					if ((vkb.mode == MODE_SYMBOLS1 || vkb.mode == MODE_SYMBOLS2) && key->symbolLabel) {
						label = key->symbolLabel;
					} else {
						label = key->label;
					}
					if (label) {
						SCR_DrawStringExtNoShadow(keyX + keyW/2 - (int)strlen(label) * KEYBOARD_TEXT_SIZE/2,
							keyY + KEYBOARD_KEY_HEIGHT/2 - KEYBOARD_TEXT_SIZE/2,
							KEYBOARD_TEXT_SIZE, label, isSpecial ? textDimColor : textColor, qfalse, qfalse);
					}
				} else {
					if (vkb.mode == MODE_SYMBOLS2) {
						ch = key->symbol2;
					} else if (vkb.mode == MODE_SYMBOLS1) {
						ch = key->symbol1;
					} else if (vkb.mode == MODE_UPPERCASE || vkb.capsLock) {
						ch = key->uppercase;
					} else {
						ch = key->lowercase;
					}
					if (ch) {
						str[0] = ch;
						str[1] = 0;
						SCR_DrawStringExtNoShadow(keyX + keyW/2 - KEYBOARD_TEXT_SIZE/2,
							keyY + KEYBOARD_KEY_HEIGHT/2 - KEYBOARD_TEXT_SIZE/2,
							KEYBOARD_TEXT_SIZE, str, textColor, qfalse, qfalse);
					}
				}
			}

			keyX += keyW + KEYBOARD_KEY_SPACING;
		}
	}

	// Draw cursor on top of keyboard
	// Use Team Arena pointer cursor for missionpack, q3_ui crosshair cursor for baseq3
	// Re-register shader each time since game mod can change (vid_restart invalidates handles)
	{
		qhandle_t cursorShader;
		const char *gamedir;

		// Check if we're running missionpack
		gamedir = Cvar_VariableString("fs_game");
		if (gamedir && *gamedir && !Q_stricmp(gamedir, "missionpack")) {
			// Team Arena pointer cursor
			cursorShader = re.RegisterShaderNoMip("ui/assets/3_cursor3");
		} else {
			// q3_ui crosshair cursor for baseq3
			cursorShader = re.RegisterShaderNoMip("menu/art/3_cursor2");
		}
		if (cursorShader) {
			SCR_DrawPic(cursorX - 16, cursorY - 16, 32, 32, cursorShader);
		}
	}
}

/*
=================
VKeyboard_SendChar

Send a character through the normal input path
=================
*/
static void VKeyboard_SendChar( int ch ) {
	CL_CharEvent( ch );
}

/*
=================
VKeyboard_SendKey

Send a key event through the normal input path
=================
*/
static void VKeyboard_SendKey( int key, qboolean down ) {
	CL_KeyEvent( key, down, cls.realtime );
}

/*
=================
VKeyboard_HandleKey

Returns qtrue if the keyboard handled this key event
=================
*/
qboolean VKeyboard_HandleKey( int key ) {
	vKeyDef_t *keyDef;
	char ch;
	int cursorX, cursorY;

	if (!vkb.active) {
		return qfalse;
	}

	if (key == K_ESCAPE || key == K_MENU) {
		VKeyboard_Hide();
		// If console is active, also toggle it closed
		if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
			Con_ToggleConsole_f();
		}
		return qtrue;
	}

	if (key != K_MOUSE1) {
		return qfalse;
	}

	// Get cursor position from VR (which updates uis.cursorx/y through menuCursorX/Y pointers)
	if (vr.menuCursorX && vr.menuCursorY) {
		cursorX = *vr.menuCursorX;
		cursorY = *vr.menuCursorY;
	} else {
		// Fallback - center of screen
		cursorX = SCREEN_WIDTH / 2;
		cursorY = SCREEN_HEIGHT / 2;
	}

	keyDef = VKeyboard_GetKeyAt(cursorX, cursorY);
	if (!keyDef) {
		// Check if click is within the keyboard background area
		if (VKeyboard_IsInKeyboardArea(cursorX, cursorY)) {
			// Clicked within keyboard area but not on a key - ignore (near miss)
			return qtrue;
		}
		// Clicked outside keyboard area entirely - dismiss it
		VKeyboard_Hide();
		return qtrue;
	}

	if (keyDef->special) {
		switch (keyDef->special) {
			case VKEY_SHIFT:
				if (vkb.mode == MODE_SYMBOLS1) {
					vkb.mode = MODE_SYMBOLS2;
				} else if (vkb.mode == MODE_SYMBOLS2) {
					vkb.mode = MODE_SYMBOLS1;
				} else if (vkb.capsLock) {
					vkb.capsLock = qfalse;
					vkb.mode = MODE_LOWERCASE;
					vkb.lastShiftTime = 0;
				} else if (vkb.mode == MODE_UPPERCASE) {
					if (cls.realtime - vkb.lastShiftTime < CAPSLOCK_DOUBLE_TAP_TIME) {
						vkb.capsLock = qtrue;
						vkb.lastShiftTime = 0;
					} else {
						vkb.mode = MODE_LOWERCASE;
						vkb.lastShiftTime = 0;
					}
				} else {
					vkb.mode = MODE_UPPERCASE;
					vkb.lastShiftTime = cls.realtime;
				}
				return qtrue;

			case VKEY_BACKSPACE:
				// Send ctrl+H (character 8) which MField_CharEvent handles as backspace
				VKeyboard_SendChar( 'h' - 'a' + 1 );
				return qtrue;

			case VKEY_SYMBOLS:
				if (vkb.mode == MODE_SYMBOLS1 || vkb.mode == MODE_SYMBOLS2) {
					vkb.mode = vkb.capsLock ? MODE_UPPERCASE : MODE_LOWERCASE;
				} else {
					vkb.mode = MODE_SYMBOLS1;
				}
				return qtrue;

			case VKEY_SPACE:
				VKeyboard_SendChar(' ');
				return qtrue;

			case VKEY_LEFT:
				VKeyboard_SendKey(K_LEFTARROW, qtrue);
				VKeyboard_SendKey(K_LEFTARROW, qfalse);
				return qtrue;

			case VKEY_RIGHT:
				VKeyboard_SendKey(K_RIGHTARROW, qtrue);
				VKeyboard_SendKey(K_RIGHTARROW, qfalse);
				return qtrue;

			case VKEY_UP:
				VKeyboard_SendKey(K_UPARROW, qtrue);
				VKeyboard_SendKey(K_UPARROW, qfalse);
				return qtrue;

			case VKEY_DOWN:
				VKeyboard_SendKey(K_DOWNARROW, qtrue);
				VKeyboard_SendKey(K_DOWNARROW, qfalse);
				return qtrue;

			case VKEY_TAB:
				VKeyboard_SendKey(K_TAB, qtrue);
				VKeyboard_SendKey(K_TAB, qfalse);
				return qtrue;

			case VKEY_ENTER:
				// Send Enter key
				VKeyboard_SendKey(K_ENTER, qtrue);
				VKeyboard_SendKey(K_ENTER, qfalse);
				// In console, keep keyboard open for multiple commands
				// Otherwise (UI menu only), dismiss keyboard after Enter
				if (!(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
					VKeyboard_Hide();
				}
				return qtrue;
		}
		return qtrue;
	}

	// Regular character
	if (vkb.mode == MODE_SYMBOLS2) {
		ch = keyDef->symbol2;
	} else if (vkb.mode == MODE_SYMBOLS1) {
		ch = keyDef->symbol1;
	} else if (vkb.mode == MODE_UPPERCASE || vkb.capsLock) {
		ch = keyDef->uppercase;
	} else {
		ch = keyDef->lowercase;
	}

	if (ch) {
		VKeyboard_SendChar(ch);
		if (vkb.mode == MODE_UPPERCASE && !vkb.capsLock) {
			vkb.mode = MODE_LOWERCASE;
		}
	}

	return qtrue;
}
