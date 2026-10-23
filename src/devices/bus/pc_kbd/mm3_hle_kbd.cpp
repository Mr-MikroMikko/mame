// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/* Temporary stand-in for the Nokia 100/102 keyboard (MikroMikko 3), see mm3_hle_kbd.h */

#include "emu.h"
#include "mm3_hle_kbd.h"

// scan code set 1 (as used in the input port definition) -> set 2 make codes; the Nokia 8742 firmware translates set 2 to set 1 for the host
static const u8 mm3_set1_to_set2[0x54] = {
	0x00, 0x76, 0x16, 0x1e, 0x26, 0x25, 0x2e, 0x36, 0x3d, 0x3e, 0x46, 0x45, 0x4e, 0x55, 0x66, 0x0d, // 00-0f
	0x15, 0x1d, 0x24, 0x2d, 0x2c, 0x35, 0x3c, 0x43, 0x44, 0x4d, 0x54, 0x5b, 0x5a, 0x14, 0x1c, 0x1b, // 10-1f
	0x23, 0x2b, 0x34, 0x33, 0x3b, 0x42, 0x4b, 0x4c, 0x52, 0x0e, 0x12, 0x5d, 0x1a, 0x22, 0x21, 0x2a, // 20-2f
	0x32, 0x31, 0x3a, 0x41, 0x49, 0x4a, 0x59, 0x7c, 0x11, 0x29, 0x58, 0x05, 0x06, 0x04, 0x0c, 0x03, // 30-3f
	0x0b, 0x83, 0x0a, 0x01, 0x09, 0x77, 0x7e, 0x6c, 0x75, 0x7d, 0x7b, 0x6b, 0x73, 0x74, 0x79, 0x69, // 40-4f
	0x72, 0x7a, 0x70, 0x71                                                                          // 50-53
};

DEFINE_DEVICE_TYPE(MM3_HLE_KBD, mm3_hle_keyboard_device, "mm3_hle_kbd", "MikroMikko 3 HLE keyboard (temporary)")

static constexpr attotime kbd_serial_cycle = attotime::from_usec(25);

INPUT_PORTS_START(mm3_hle_keyboard)
	PORT_START("KEY0")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ESC) PORT_NAME("Esc") PORT_CHAR(0x1b) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x01)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1) PORT_NAME("1") PORT_CHAR('1') PORT_CHAR('!') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x02)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2) PORT_NAME("2") PORT_CHAR('2') PORT_CHAR('"') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x03)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3) PORT_NAME("3") PORT_CHAR('3') PORT_CHAR('#') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x04)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4) PORT_NAME("4") PORT_CHAR('4') PORT_CHAR(0xa4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x05)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5) PORT_NAME("5") PORT_CHAR('5') PORT_CHAR('%') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x06)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6) PORT_NAME("6") PORT_CHAR('6') PORT_CHAR('&') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x07)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7) PORT_NAME("7") PORT_CHAR('7') PORT_CHAR('/') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x08)
	PORT_START("KEY1")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8) PORT_NAME("8") PORT_CHAR('8') PORT_CHAR('(') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x09)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9) PORT_NAME("9") PORT_CHAR('9') PORT_CHAR(')') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x0a)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0) PORT_NAME("0") PORT_CHAR('0') PORT_CHAR('=') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x0b)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS) PORT_NAME("-") PORT_CHAR('+') PORT_CHAR('?') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x0c)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS) PORT_NAME("=") PORT_CHAR(0xb4) PORT_CHAR('`') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x0d)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSPACE) PORT_NAME("Backspace") PORT_CHAR(0x08) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x0e)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_TAB) PORT_NAME("Tab") PORT_CHAR(0x09) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x0f)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q) PORT_NAME("Q") PORT_CHAR('q') PORT_CHAR('Q') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x10)
	PORT_START("KEY2")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_W) PORT_NAME("W") PORT_CHAR('w') PORT_CHAR('W') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x11)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_E) PORT_NAME("E") PORT_CHAR('e') PORT_CHAR('E') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x12)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_R) PORT_NAME("R") PORT_CHAR('r') PORT_CHAR('R') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x13)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_T) PORT_NAME("T") PORT_CHAR('t') PORT_CHAR('T') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x14)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y) PORT_NAME("Y") PORT_CHAR('y') PORT_CHAR('Y') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x15)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_U) PORT_NAME("U") PORT_CHAR('u') PORT_CHAR('U') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x16)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_I) PORT_NAME("I") PORT_CHAR('i') PORT_CHAR('I') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x17)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_O) PORT_NAME("O") PORT_CHAR('o') PORT_CHAR('O') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x18)
	PORT_START("KEY3")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_P) PORT_NAME("P") PORT_CHAR('p') PORT_CHAR('P') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x19)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE) PORT_NAME("[") PORT_CHAR(0xe5) PORT_CHAR(0xc5) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x1a)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE) PORT_NAME("]") PORT_CHAR(0xa8) PORT_CHAR('^') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x1b)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ENTER) PORT_NAME("Enter") PORT_CHAR(0x0d) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x1c)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_LCONTROL) PORT_NAME("Ctrl") PORT_CHAR(UCHAR_SHIFT_2) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x1d)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_A) PORT_NAME("A") PORT_CHAR('a') PORT_CHAR('A') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x1e)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_S) PORT_NAME("S") PORT_CHAR('s') PORT_CHAR('S') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x1f)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_D) PORT_NAME("D") PORT_CHAR('d') PORT_CHAR('D') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x20)
	PORT_START("KEY4")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F) PORT_NAME("F") PORT_CHAR('f') PORT_CHAR('F') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x21)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_G) PORT_NAME("G") PORT_CHAR('g') PORT_CHAR('G') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x22)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_H) PORT_NAME("H") PORT_CHAR('h') PORT_CHAR('H') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x23)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_J) PORT_NAME("J") PORT_CHAR('j') PORT_CHAR('J') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x24)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_K) PORT_NAME("K") PORT_CHAR('k') PORT_CHAR('K') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x25)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_L) PORT_NAME("L") PORT_CHAR('l') PORT_CHAR('L') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x26)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON) PORT_NAME(";") PORT_CHAR(0xf6) PORT_CHAR(0xd6) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x27)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE) PORT_NAME("'") PORT_CHAR(0xe4) PORT_CHAR(0xc4) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x28)
	PORT_START("KEY5")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_TILDE) PORT_NAME("`") PORT_CHAR(0xa7) PORT_CHAR(0xbd) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x29)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_LSHIFT) PORT_NAME("Left Shift") PORT_CHAR(UCHAR_SHIFT_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x2a)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH) PORT_NAME("\\") PORT_CHAR('\'') PORT_CHAR('*') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x2b)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z) PORT_NAME("Z") PORT_CHAR('z') PORT_CHAR('Z') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x2c)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_X) PORT_NAME("X") PORT_CHAR('x') PORT_CHAR('X') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x2d)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_C) PORT_NAME("C") PORT_CHAR('c') PORT_CHAR('C') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x2e)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_V) PORT_NAME("V") PORT_CHAR('v') PORT_CHAR('V') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x2f)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_B) PORT_NAME("B") PORT_CHAR('b') PORT_CHAR('B') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x30)
	PORT_START("KEY6")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_N) PORT_NAME("N") PORT_CHAR('n') PORT_CHAR('N') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x31)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_M) PORT_NAME("M") PORT_CHAR('m') PORT_CHAR('M') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x32)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA) PORT_NAME(",") PORT_CHAR(',') PORT_CHAR(';') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x33)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP) PORT_NAME(".") PORT_CHAR('.') PORT_CHAR(':') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x34)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH) PORT_NAME("/") PORT_CHAR('-') PORT_CHAR('_') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x35)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_RSHIFT) PORT_NAME("Right Shift") PORT_CHAR(UCHAR_SHIFT_1) PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x36)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ASTERISK) PORT_NAME("Keypad *") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x37)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_LALT) PORT_NAME("Alt") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x38)
	PORT_START("KEY7")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SPACE) PORT_NAME("Space") PORT_CHAR(' ') PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x39)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CAPSLOCK) PORT_NAME("Caps Lock") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x3a)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F1) PORT_NAME("F1") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x3b)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F2) PORT_NAME("F2") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x3c)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F3) PORT_NAME("F3") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x3d)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F4) PORT_NAME("F4") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x3e)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F5) PORT_NAME("F5") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x3f)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F6) PORT_NAME("F6") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x40)
	PORT_START("KEY8")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F7) PORT_NAME("F7") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x41)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F8) PORT_NAME("F8") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x42)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F9) PORT_NAME("F9") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x43)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F10) PORT_NAME("F10") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x44)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_NUMLOCK) PORT_NAME("Num Lock") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x45)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SCRLOCK) PORT_NAME("Scroll Lock") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x46)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("Keypad 7") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x47)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("Keypad 8") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x48)
	PORT_START("KEY9")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("Keypad 9") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x49)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS_PAD) PORT_NAME("Keypad -") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x4a)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("Keypad 4") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x4b)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("Keypad 5") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x4c)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("Keypad 6") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x4d)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_PLUS_PAD) PORT_NAME("Keypad +") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x4e)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("Keypad 1") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x4f)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("Keypad 2") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x50)
	PORT_START("KEY10")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("Keypad 3") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x51)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("Keypad 0") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x52)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_DEL_PAD) PORT_NAME("Keypad .") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x53)
	PORT_START("KEYX")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_RALT) PORT_NAME("AltGr") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(mm3_hle_keyboard_device::key_changed), 0x56)
INPUT_PORTS_END

mm3_hle_keyboard_device::mm3_hle_keyboard_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, MM3_HLE_KBD, tag, owner, clock)
	, device_pc_kbd_interface(mconfig, *this)
{
}

ioport_constructor mm3_hle_keyboard_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(mm3_hle_keyboard);
}

void mm3_hle_keyboard_device::device_start()
{
	save_item(NAME(m_state));
	save_item(NAME(m_bit));
	save_item(NAME(m_data));
	save_item(NAME(m_parity));
	save_item(NAME(m_rx_byte));
	save_item(NAME(m_tx_queue));
	save_item(NAME(m_tx_head));
	save_item(NAME(m_tx_tail));
	save_item(NAME(m_last_sent));
	save_item(NAME(m_enabled));
	save_item(NAME(m_expect_arg));
	save_item(NAME(m_repeat_key));

	set_pc_kbdc_device();

	m_serial = timer_alloc(FUNC(mm3_hle_keyboard_device::serial), this);
	m_repeat = timer_alloc(FUNC(mm3_hle_keyboard_device::repeat), this);
}

void mm3_hle_keyboard_device::device_reset()
{
	m_state = IDLE;
	m_bit = 0;
	m_data = 0;
	m_parity = 0;
	m_rx_byte = 0;
	m_tx_head = m_tx_tail = 0;
	m_last_sent = 0xaa;
	m_enabled = true;
	m_expect_arg = 0;
	m_repeat_key = 0;
	m_repeat->enable(false);

	// no power-on self-test byte: it would arrive before the BIOS has flushed the controller and confuse the reset handshake

	m_pc_kbdc->data_write_from_kb(1);
	m_pc_kbdc->clock_write_from_kb(1);
	resume();
}

void mm3_hle_keyboard_device::enqueue(u8 data)
{
	logerror("MM3 kbd: queue %02x\n", data);
	unsigned const next = (m_tx_head + 1) % QUEUE_SIZE;
	if (next == m_tx_tail)
		return; // overrun, drop
	m_tx_queue[m_tx_head] = data;
	m_tx_head = next;
}

void mm3_hle_keyboard_device::clock_write(int state)
{
	logerror("MM3 kbd: host clock=%d data=%d state=%u\n", state, data_signal(), m_state);
	m_clock_changed = machine().time();
	resume();
}

void mm3_hle_keyboard_device::resume()
{
	if (clock_signal() && m_state == IDLE)
	{
		// host pulled data low with clock released: it wants to send
		if (!data_signal())
			m_state = RX_START;

		if (!m_serial->enabled())
			m_serial->adjust(kbd_serial_cycle, 0, kbd_serial_cycle);
	}
}

void mm3_hle_keyboard_device::serial(s32 param)
{
	// host inhibits the keyboard by holding the clock low for 100us
	if (!clock_signal() && clock_held(100))
	{
		m_state = IDLE;
		m_pc_kbdc->data_write_from_kb(1);
		m_pc_kbdc->clock_write_from_kb(1);
		m_serial->enable(false);
		return;
	}

	switch (m_state)
	{
	case IDLE:
		if (clock_signal())
		{
			if (queue_empty() || !m_enabled)
				m_serial->enable(false);
			else if (clock_held(50))
				m_state = TX_START;
		}
		break;

	case RX_START:
		if (!data_signal())
		{
			m_state = RX_CLOCK_LO0;
			m_data = 0;
			m_parity = 1;
			m_bit = 0;
		}
		break;

	case RX_CLOCK_LO0:
		m_state = RX_CLOCK_LO1;
		m_pc_kbdc->clock_write_from_kb(0);
		break;

	case RX_CLOCK_LO1:
		m_state = RX_CLOCK_HI0;
		break;

	case RX_CLOCK_HI0:
		m_state = RX_CLOCK_HI1;
		m_pc_kbdc->clock_write_from_kb(1);
		break;

	case RX_CLOCK_HI1:
		switch (m_bit)
		{
		case 8: // parity
			m_state = RX_CLOCK_LO0;
			if (data_signal() == (m_parity & 1))
				m_rx_byte = m_data;
			else
				m_rx_byte = 0xfe; // parity error: ask for resend
			m_bit++;
			break;

		case 9: // stop bit, then acknowledge
			if (data_signal())
			{
				m_state = RX_CLOCK_LO0;
				m_pc_kbdc->data_write_from_kb(0);
				m_bit++;
			}
			break;

		case 10: // finished
			m_state = COMMAND;
			m_pc_kbdc->data_write_from_kb(1);
			break;

		default:
			m_state = RX_CLOCK_LO0;
			m_parity += data_signal();
			m_data |= (data_signal() << m_bit++);
			break;
		}
		break;

	case COMMAND:
		m_state = IDLE;
		command(m_rx_byte);
		break;

	case TX_START:
		m_data = m_tx_queue[m_tx_tail];
		m_parity = 1;
		m_bit = 0;
		m_state = TX_CLOCK_LO0;
		m_pc_kbdc->data_write_from_kb(0);
		break;

	case TX_CLOCK_LO0:
		m_state = TX_CLOCK_LO1;
		m_pc_kbdc->clock_write_from_kb(0);
		break;

	case TX_CLOCK_LO1:
		m_state = TX_CLOCK_HI0;
		break;

	case TX_CLOCK_HI0:
		m_state = TX_CLOCK_HI1;
		m_pc_kbdc->clock_write_from_kb(1);
		break;

	case TX_CLOCK_HI1:
		switch (m_bit)
		{
		case 8: // parity
			m_state = TX_CLOCK_LO0;
			m_pc_kbdc->data_write_from_kb(m_parity & 1);
			m_bit++;
			break;

		case 9: // stop bit
			m_state = TX_CLOCK_LO0;
			m_pc_kbdc->data_write_from_kb(1);
			m_bit++;
			break;

		case 10: // finished; only now is the byte consumed
			m_state = IDLE;
			m_last_sent = m_data;
			m_tx_tail = (m_tx_tail + 1) % QUEUE_SIZE;
			break;

		default:
			m_state = TX_CLOCK_LO0;
			m_parity += BIT(m_data, m_bit);
			m_pc_kbdc->data_write_from_kb(BIT(m_data, m_bit));
			m_bit++;
			break;
		}
		break;
	}
}

void mm3_hle_keyboard_device::command(u8 data)
{
	logerror("MM3 kbd: host command %02x (arg pending %02x)\n", data, m_expect_arg);
	if (m_expect_arg)
	{
		// ED (LEDs), F0 (scan code set) and F3 (typematic) take one argument byte
		m_expect_arg = 0;
		enqueue(0xfa);
	}
	else
	{
		switch (data)
		{
		case 0xed: case 0xf0: case 0xf3:
			m_expect_arg = data;
			enqueue(0xfa);
			break;

		case 0xee: // echo
			enqueue(0xee);
			break;

		case 0xf4: // enable
			m_enabled = true;
			enqueue(0xfa);
			break;

		case 0xf5: // default and disable
		case 0xf6: // default
			m_enabled = (data == 0xf6);
			m_repeat->enable(false);
			enqueue(0xfa);
			break;

		case 0xfe: // resend
			enqueue(m_last_sent);
			break;

		case 0x78: // Nokia controller firmware sends this instead of FF: it acknowledges the host itself, keyboard just reports self-test passed
			m_enabled = true;
			m_repeat->enable(false);
			enqueue(0xaa);
			break;

		case 0xff: // reset
			m_enabled = true;
			m_repeat->enable(false);
			enqueue(0xfa);
			enqueue(0xaa);
			break;

		default: // unknown commands (including F2 on an 84-key keyboard) are just acknowledged
			enqueue(0xfa);
			break;
		}
	}

	m_serial->adjust(kbd_serial_cycle, 0, kbd_serial_cycle);
}

INPUT_CHANGED_MEMBER(mm3_hle_keyboard_device::key_changed)
{
	u8 const scancode = param;

	if (scancode == 0x56) // AltGr (right Alt): extended key E0 11
	{
		if (m_enabled)
		{
			enqueue(0xe0);
			if (!newval)
				enqueue(0xf0);
			enqueue(0x11);
			resume();
		}
		return;
	}

	bool const modifier = (scancode == 0x1d) || (scancode == 0x2a) || (scancode == 0x36) || (scancode == 0x38) ||
			(scancode == 0x3a) || (scancode == 0x45) || (scancode == 0x46);

	if (!m_enabled)
		return;

	if (newval)
	{
		enqueue(mm3_set1_to_set2[scancode]);
		if (!modifier)
		{
			m_repeat_key = scancode;
			m_repeat->adjust(attotime::from_msec(500));
		}
	}
	else
	{
		enqueue(0xf0);
		enqueue(mm3_set1_to_set2[scancode]);
		if (m_repeat_key == scancode)
		{
			m_repeat_key = 0;
			m_repeat->enable(false);
		}
	}

	resume();
}

TIMER_CALLBACK_MEMBER(mm3_hle_keyboard_device::repeat)
{
	if (!m_repeat_key)
		return;

	if (queue_empty())
		enqueue(mm3_set1_to_set2[m_repeat_key]);
	m_repeat->adjust(attotime::from_msec(92)); // about 10.9 characters per second
	resume();
}

