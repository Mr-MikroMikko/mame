// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    Nokia NDC 286 display controller (MikroMikko 3 / ASC) - control processor

    The card is built around an 8031 running the firmware in IC38 (27256,
    label 729508/G). This device runs that firmware and models its host
    interface (I/O ports 3B3, 3B6 and 3B7) and the control latches the
    firmware drives. Video generation (CRTC, display memory, character
    generator RAM, PALs, gate arrays) is NOT emulated; the MDA/CGA stand-in
    cards use the state exposed here (character set bank, label rows).

***************************************************************************/

#ifndef MAME_BUS_ISA_NDC286_H
#define MAME_BUS_ISA_NDC286_H

#pragma once

#include "cpu/mcs51/i8051.h"

// ADU 15A / MDU 15A positive display: dark slate characters on a light, almost neutral grey "paper white".
// The CIE white point in the ASC technical specification (x 0.355, y 0.395) converts to a very yellow sRGB colour
// (255,252,185), but the real monitor looks slightly greyish rather than yellowish (MikkoMuseo, 2026-10-23).
// Values measured from the band-free part of a photo of a real MM2 positive monitor (Photo 17.10.2026 12.08.57):
// paper about 214,211,205 and solid ink about 77,87,89 at the camera exposure; paper is scaled up here.
// TODO: fine tune against the real ADU 15A.
static constexpr rgb_t NDC_PAPER_WHITE(0xe0, 0xdd, 0xd6);
static constexpr rgb_t NDC_INK(0x3c, 0x42, 0x44);

// brightness 0 = ink, 1 = paper white
inline rgb_t ndc_shade(double brightness)
{
	return rgb_t(
			u8(NDC_INK.r() + (NDC_PAPER_WHITE.r() - NDC_INK.r()) * brightness + 0.5),
			u8(NDC_INK.g() + (NDC_PAPER_WHITE.g() - NDC_INK.g()) * brightness + 0.5),
			u8(NDC_INK.b() + (NDC_PAPER_WHITE.b() - NDC_INK.b()) * brightness + 0.5));
}

class ndc286_device : public device_t
{
public:
	ndc286_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 11'059'200);

	// host side registers
	u8 host_status_r();            // 3B3 read
	void host_control_w(u8 data);  // 3B3 write: command / handshake
	u8 host_data_r();              // 3B6 read
	void host_data_w(u8 data);     // 3B6 write: parameter byte
	u8 host_result_r();            // 3B7 read
	void host_test_w(u8 data);     // 3B7 write

	// firmware image (character sets) for the stand-in video cards
	u8 const *firmware() const { return m_fw; }

	// state driven by the firmware
	u8 control_latch() const { return m_fc01; }                  // FC01: bit 4 = character generator bank
	int charset_bank() const { return BIT(m_fc01, 4); }

	// host writes to the emulated MDA/CGA registers are forwarded here (mode register copy, status)
	void set_host_mode(u8 data) { m_host_mode = data; }
	void set_host_vsync(int state) { m_vsync = state; }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual tiny_rom_entry const *device_rom_region() const override ATTR_COLD;

private:
	void program_map(address_map &map) ATTR_COLD;
	void data_map(address_map &map) ATTR_COLD;

	u8 fw_r(offs_t offset) { return m_fw[offset & 0x7fff]; }
	u8 reg_r(offs_t offset);
	void reg_w(offs_t offset, u8 data);
	u8 chr_r(offs_t offset);
	void chr_w(offs_t offset, u8 data);
	void latch_b800_w(u8 data);
	void latch_bc00_w(u8 data);
	void latch_f800_w(u8 data);

	u8 p1_r();
	void p1_w(u8 data);
	u8 p3_r();

	TIMER_CALLBACK_MEMBER(vsync_tick);

	required_device<i8031_device> m_mcu;
	required_device<cpu_device> m_host_cpu;
	required_region_ptr<u8> m_fw;
	std::unique_ptr<u8[]> m_chrram;
	emu_timer *m_vsync_timer;

	// host -> MCU
	u8 m_host_cmd;
	u8 m_host_data;
	u8 m_host_test;
	u8 m_host_mode;
	// MCU -> host
	u8 m_status;
	u8 m_result;
	// MCU register shadows
	u8 m_fc00, m_fc01, m_fc0a;
	u8 m_p1_out;
	int m_vsync;
	u8 m_crtc_index;
	u8 m_crtc_reg[32]; // control processor side CRTC (MB89321A) register file
};

DECLARE_DEVICE_TYPE(NDC286, ndc286_device)

#endif // MAME_BUS_ISA_NDC286_H
