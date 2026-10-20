// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/**********************************************************************

    Nokia MikroMikko 2 IDC186 emulation

    The IDC186 is an intelligent display controller card. It has its
    own 80186 slave processor with 64 KB of RAM and 64 KB of video
    memory, a uPD7220 GDC and an 8274 MPSC for the keyboard and the
    mouse. The card has no ROM: the host loads the slave program
    (IDC.SYS) into the slave RAM through a window at C0000-CFFFF and
    starts it with an NMI. The 800x420 monochrome bitmap is shared
    between the host (D0000), the slave (10000) and the GDC.

**********************************************************************/

#ifndef MAME_BUS_MM2_IDC186_H
#define MAME_BUS_MM2_IDC186_H

#pragma once

#include "exp.h"
#include "mm2kb.h"
#include "cpu/i86/i186.h"
#include "machine/clock.h"
#include "machine/z80sio.h"
#include "screen.h"

DECLARE_DEVICE_TYPE(NOKIA_IDC186, idc186_device)

class idc186_device : public device_t, public device_mikromikko2_expansion_bus_card_interface
{
public:
	idc186_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	void host_io_map(address_map &map) ATTR_COLD;
	void slave_mem_map(address_map &map) ATTR_COLD;
	void slave_io_map(address_map &map) ATTR_COLD;

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	// host side control ports
	uint8_t gdc_status_r();
	void gdc_param_w(uint8_t data);
	void gdc_command_w(uint8_t data);
	uint8_t gdc_data_r();
	void ctrl_a0_w(uint8_t data);
	void slave_nmi_w(uint8_t data);
	void slave_run_w(uint8_t data);
	void slave_irq_ack_w(uint8_t data);
	void display_w(uint8_t data);

	// slave side
	void slave_irq_w(uint8_t data);

	void set_slave_run(bool run);
	void vblank_w(int state);

	required_device<i80186_cpu_device> m_slave;
	required_device<i8274_device> m_mpsc;
	required_device<mm2_keyboard_device> m_kb;
	required_device<screen_device> m_screen;
	memory_share_creator<uint16_t> m_ram;
	memory_share_creator<uint16_t> m_vram;

	bool m_run;
	bool m_nmi_pending;
	bool m_nmi_level;
	bool m_display_on;
	bool m_vblank;
	uint8_t m_ctrl_a0;
	uint8_t m_gdc_command;
};

#endif // MAME_BUS_MM2_IDC186_H
