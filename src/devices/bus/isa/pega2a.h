// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    Paradise Autoswitch EGA2 (PEGA 2A) - the Nokia MikroMikko 3 EGA card

    An EGA card built on the generic EGA core, with what the card BIOS (Paradise PEGA BIOS 1986, 22030-C1201) and the
    port reads of the real card showed: the configuration switches, a scratch RAM window, the AutoSwitch NMI trap, the
    four dot clocks and the undriven ports. The hardware CGA/MDA/Hercules/Plantronics emulation and the 2A extension
    registers are not emulated (the Nokia setting does not use them).

***************************************************************************/

#ifndef MAME_BUS_ISA_PEGA2A_H
#define MAME_BUS_ISA_PEGA2A_H

#pragma once

#include "ega.h"

class isa8_pega2a_device : public isa8_ega_device
{
public:
	isa8_pega2a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

protected:
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_post_load() override;

	virtual void install_banks() override;
	virtual uint32_t dot_clock() const override;

private:
	// timing handed over by the CRTC, and where the active display sits in the frame the monitor shows
	struct screen_timing
	{
		int width = 0;
		int height = 0;
		attotime frame_period;
		rectangle active;   // active display, in CRTC coordinates
		rectangle full;     // everything but the sync pulses
		rectangle visarea;  // what the monitor shows: up to the start of blanking
		int x_off = 0;      // active display origin inside the frame
		int y_off = 0;
		int hsync_off = 0;
		int vsync_off = 0;
	};

	uint8_t port_3b0_r(offs_t offset);
	void port_3b0_w(offs_t offset, uint8_t data);
	uint8_t port_3c0_r(offs_t offset);
	void port_3c0_w(offs_t offset, uint8_t data);
	uint8_t port_3d0_r(offs_t offset);
	void port_3d0_w(offs_t offset, uint8_t data);

	uint8_t scratch_r(offs_t offset) { return m_scratch[offset & 0x3ff]; }
	void scratch_w(offs_t offset, uint8_t data) { m_scratch[offset & 0x3ff] = data; }
	void scratch_map(bool enable);

	void crtc_and_trap_write(offs_t port, uint8_t data);
	void trap_write(offs_t port);

	void set_monitor_palette(bool mono);
	void apply_blanking(bool force);

	CRTC_EGA_RECONFIGURE(crtc_reconfigure);
	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	required_ioport m_sw;

	// 1 KB scratch RAM that replaces the first 1 KB of the A0000 aperture while bit 6 of 3DF is set. The card BIOS keeps
	// its state variables in it (A000:0004-000D) and the AutoSwitch logic shadows the values written to the CGA/MDA
	// register ports 3B0-3DF at the same offsets (A000:03B0-03DF; the BIOS writes FF there to acknowledge).
	uint8_t m_scratch[0x400];
	bool m_scratch_en;

	// AutoSwitch NMI trap: a write to the CGA/MDA compatible ports of the address range selected by Misc Output bit 0
	// raises IOCHCK (NMI) when the trap is armed. A write to 3DF with bit 7 set arms it (the BIOS writes A0h and then 00h
	// when it leaves); it fires once and is armed again by the BIOS. While the scratch window is open the BIOS itself
	// runs and nothing traps. The BIOS NMI handler (C000:07F1) emulates the trapped write.
	bool m_trap_armed;

	uint8_t m_crtc_index;
	bool m_mono_monitor;

	screen_timing m_timing;
	bitmap_ind16 m_ega_bitmap;
};

DECLARE_DEVICE_TYPE(ISA8_PEGA2A, isa8_pega2a_device)

#endif // MAME_BUS_ISA_PEGA2A_H
