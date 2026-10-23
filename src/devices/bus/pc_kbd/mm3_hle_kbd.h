// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
#ifndef MAME_BUS_PC_KBD_MM3_HLE_KBD_H
#define MAME_BUS_PC_KBD_MM3_HLE_KBD_H

#pragma once

#include "pc_kbdc.h"


/***************************************************************************
    Temporary stand-in for the Nokia 100/102 keyboard (MikroMikko 3)

    The real keyboard's controller ROM has not been dumped. This HLE device
    speaks the AT keyboard protocol (11-bit frames, scan code set 1 like the
    original 84-key AT keyboard) with an 84-key layout and typematic repeat.
    The frame handling follows hle_ps2_mouse_device. Replace it once the
    Nokia keyboard firmware is available.
***************************************************************************/

class mm3_hle_keyboard_device : public device_t, public device_pc_kbd_interface
{
public:
	mm3_hle_keyboard_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	DECLARE_INPUT_CHANGED_MEMBER(key_changed);

protected:
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void clock_write(int state) override;

private:
	bool clock_held(unsigned usec) const { return (machine().time() - m_clock_changed) >= attotime::from_usec(usec); }

	void resume();
	TIMER_CALLBACK_MEMBER(serial);
	TIMER_CALLBACK_MEMBER(repeat);
	void command(u8 data);
	void enqueue(u8 data);
	bool queue_empty() const { return m_tx_head == m_tx_tail; }

	enum serial_state : unsigned
	{
		IDLE,
		COMMAND,
		RX_START,
		RX_CLOCK_LO0,
		RX_CLOCK_LO1,
		RX_CLOCK_HI0,
		RX_CLOCK_HI1,
		TX_START,
		TX_CLOCK_LO0,
		TX_CLOCK_LO1,
		TX_CLOCK_HI0,
		TX_CLOCK_HI1
	};

	static constexpr unsigned QUEUE_SIZE = 64;

	emu_timer *m_serial;
	emu_timer *m_repeat;
	attotime m_clock_changed;

	unsigned m_state;
	unsigned m_bit;
	u8 m_data;
	unsigned m_parity;
	u8 m_rx_byte;

	u8 m_tx_queue[QUEUE_SIZE];
	unsigned m_tx_head;
	unsigned m_tx_tail;
	u8 m_last_sent;

	bool m_enabled;
	u8 m_expect_arg; // pending command that takes an argument byte
	u8 m_repeat_key;
};

DECLARE_DEVICE_TYPE(MM3_HLE_KBD, mm3_hle_keyboard_device)

#endif // MAME_BUS_PC_KBD_MM3_HLE_KBD_H
