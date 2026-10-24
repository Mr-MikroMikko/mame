// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    PS/2-type keyboard controller, high level emulation

    Stand-in for the Intel P8742AH of the Nokia MikroMikko 3TT m215 (label RON 1035B R1A) until its firmware is dumped.
    The Nokia ROM BIOS 7.02 expects a controller with an auxiliary (mouse) port: command byte bit 5 disables the
    auxiliary clock, and it probes the port with D4 FF. An AT-type firmware reads bit 5 as "PC mode" and stops the
    scan code translation, so the 8742 firmware of the MM3/ASC cannot be used.

    The keyboard side is the real two-wire serial protocol, so any pc_kbdc keyboard works. No auxiliary device is
    connected: commands written to it time out with FE, like a real controller without a mouse.

***************************************************************************/

#ifndef MAME_MACHINE_NOKIA_PS2_KBC_H
#define MAME_MACHINE_NOKIA_PS2_KBC_H

#pragma once

class nokia_ps2_kbc_device : public device_t
{
public:
	nokia_ps2_kbc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto hot_res() { return m_hot_res_cb.bind(); }
	auto gate_a20() { return m_gate_a20_cb.bind(); }
	auto kbd_irq() { return m_kbd_irq_cb.bind(); }
	auto aux_irq() { return m_aux_irq_cb.bind(); }
	auto kbd_clk() { return m_kbd_clk_cb.bind(); } // open collector
	auto kbd_data() { return m_kbd_data_cb.bind(); } // open collector

	// input port P1: bit 7 keylock (1 = unlocked), bit 6 display, bit 5 manufacturing jumper, bit 4 RAM; bits 1-0 are
	// the auxiliary and keyboard data lines and are read live
	void set_input_port(u8 data) { m_inport = data; }

	uint8_t data_r();
	uint8_t status_r();
	void data_w(uint8_t data);
	void command_w(uint8_t data);

	void kbd_clk_w(int state);
	void kbd_data_w(int state);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	enum : u8
	{
		ST_OBF  = 0x01,
		ST_IBF  = 0x02,
		ST_SYS  = 0x04,
		ST_CMD  = 0x08,
		ST_KEYL = 0x10,
		ST_AUXB = 0x20,
		ST_TIME = 0x40,
		ST_PERR = 0x80
	};

	enum : u8
	{
		CB_KBD_INT  = 0x01,
		CB_AUX_INT  = 0x02,
		CB_SYS      = 0x04,
		CB_KBD_DIS  = 0x10,
		CB_AUX_DIS  = 0x20,
		CB_XLAT     = 0x40
	};

	TIMER_CALLBACK_MEMBER(tx_start);
	TIMER_CALLBACK_MEMBER(timeout);
	TIMER_CALLBACK_MEMBER(reset_pulse_end);

	u8 command_byte() const { return m_ram[0]; }
	void put_output(u8 data, bool aux, u8 extra_status = 0);
	void update_irqs();
	void update_lines();
	void set_outport(u8 data);
	void send_to_keyboard(u8 data);
	void received_from_keyboard(u8 data);

	devcb_write_line m_hot_res_cb;
	devcb_write_line m_gate_a20_cb;
	devcb_write_line m_kbd_irq_cb;
	devcb_write_line m_aux_irq_cb;
	devcb_write_line m_kbd_clk_cb;
	devcb_write_line m_kbd_data_cb;

	emu_timer *m_tx_timer;
	emu_timer *m_timeout_timer;
	emu_timer *m_reset_timer;

	u8 m_inport;
	u8 m_outport;
	u8 m_ram[0x20];             // 0 = command byte
	u8 m_status;
	u8 m_output;
	u8 m_pending;               // command waiting for its data byte (0 = none)
	u8 m_xlat_break;            // F0 seen while translating

	// keyboard serial interface
	u8 m_clk_in, m_data_in;     // line states
	u8 m_clk_out, m_data_out;   // what the controller drives
	u8 m_rx_bit;                // falling edges seen in the frame being received
	u16 m_rx_shift;
	bool m_tx_active;
	u8 m_tx_byte;
	u8 m_tx_bit;
	u8 m_response_window;       // response bytes accepted while the keyboard interface is disabled
};

DECLARE_DEVICE_TYPE(NOKIA_PS2_KBC, nokia_ps2_kbc_device)

#endif // MAME_MACHINE_NOKIA_PS2_KBC_H
