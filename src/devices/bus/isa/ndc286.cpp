// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/***************************************************************************

    Nokia NDC 286 display controller - 8031 control processor

    External data memory map as used by the firmware (found by disassembly of IC38):
      0000-1FFF  8 KB static RAM (TC5565)
      8000-8FFF  loadable character generator RAM (two banks, selected with FC01 bit 4)
      B800       write-only latch (video control, firmware variable 26h)
      BC00       write-only latch (video control, firmware variable 27h)
      F800       write-only latch
      FC00-FCFF  register page (MOVX @Ri with P2 = FC): host interface and CRTC

    Register page (firmware view):
      FC00 W  video control          FC01 W  control bits (bit 4 = char gen bank)
      FC03 RW CRTC data              FC0C W  CRTC address
      FC06 R  host data byte (3B6)   FC07 R  host command byte (3B3, bit 7 = command)
      FC08 R  host mode register copy (3B8/3D8)
      FC09 W  status for the host (3B3 read)  R  host mode register (3B8/3D8)
      FC0A R  host flags             FC0B W  result for the host (3B7 read)  R  host 3B7 write
      FC23 R  last host CRTC data write

***************************************************************************/

#include "emu.h"
#include "ndc286.h"

#define VERBOSE 0
#include "logmacro.h"

#define LOGT(fmt, ...) LOG("[%.4f] " fmt, machine().time().as_double(), ##__VA_ARGS__)

DEFINE_DEVICE_TYPE(NDC286, ndc286_device, "ndc286", "Nokia NDC 286 control processor (8031)")

ROM_START( ndc286 )
	ROM_REGION(0x8000, "fw", 0)
	ROM_LOAD("729508g.ic38", 0x0000, 0x8000, CRC(f422e114) SHA1(f722a316658e1a6b0343eca6e4c441b16f8dbf09)) // NDC 286 firmware FW70, 27256 (sum16 77D4)
ROM_END

tiny_rom_entry const *ndc286_device::device_rom_region() const
{
	return ROM_NAME( ndc286 );
}

ndc286_device::ndc286_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, NDC286, tag, owner, clock)
	, m_mcu(*this, "mcu")
	, m_host_cpu(*this, ":maincpu")
	, m_fw(*this, "fw")
	, m_vsync_timer(nullptr)
	, m_host_cmd(0)
	, m_host_data(0)
	, m_host_test(0)
	, m_host_mode(0)
	, m_status(0xc0)
	, m_result(0)
	, m_fc00(0)
	, m_fc01(0)
	, m_fc0a(0)
	, m_p1_out(0xff)
	, m_vsync(0)
	, m_crtc_index(0)
{
	std::fill(std::begin(m_crtc_reg), std::end(m_crtc_reg), 0);
}

void ndc286_device::device_add_mconfig(machine_config &config)
{
	I8031(config, m_mcu, 11'059'200); // 11.0592 MHz crystal
	m_mcu->set_addrmap(AS_PROGRAM, &ndc286_device::program_map);
	m_mcu->set_addrmap(AS_DATA, &ndc286_device::data_map);
	m_mcu->port_in_cb<1>().set(FUNC(ndc286_device::p1_r));
	m_mcu->port_out_cb<1>().set(FUNC(ndc286_device::p1_w));
	m_mcu->port_in_cb<3>().set(FUNC(ndc286_device::p3_r));
}

void ndc286_device::program_map(address_map &map)
{
	map(0x0000, 0x7fff).r(FUNC(ndc286_device::fw_r));
}

void ndc286_device::data_map(address_map &map)
{
	map(0x0000, 0x1fff).ram();
	map(0x8000, 0x8fff).rw(FUNC(ndc286_device::chr_r), FUNC(ndc286_device::chr_w));
	map(0xb800, 0xb800).w(FUNC(ndc286_device::latch_b800_w));
	map(0xbc00, 0xbc00).w(FUNC(ndc286_device::latch_bc00_w));
	map(0xf800, 0xf800).w(FUNC(ndc286_device::latch_f800_w));
	map(0xfc00, 0xfcff).rw(FUNC(ndc286_device::reg_r), FUNC(ndc286_device::reg_w));
}

void ndc286_device::device_start()
{
	m_chrram = std::make_unique<u8[]>(0x2000);
	m_vsync_timer = timer_alloc(FUNC(ndc286_device::vsync_tick), this);

	save_pointer(NAME(m_chrram), 0x2000);
	save_item(NAME(m_host_cmd));
	save_item(NAME(m_host_data));
	save_item(NAME(m_host_test));
	save_item(NAME(m_host_mode));
	save_item(NAME(m_status));
	save_item(NAME(m_result));
	save_item(NAME(m_fc00));
	save_item(NAME(m_fc01));
	save_item(NAME(m_fc0a));
	save_item(NAME(m_p1_out));
	save_item(NAME(m_vsync));
	save_item(NAME(m_crtc_index));
	save_item(NAME(m_crtc_reg));
}

void ndc286_device::device_reset()
{
	m_host_cmd = m_host_data = m_host_test = m_host_mode = 0;
	m_status = 0xc0;
	m_result = 0;
	m_fc00 = m_fc01 = m_fc0a = 0;
	m_p1_out = 0xff;
	m_vsync = 0;
	m_crtc_index = 0;
	std::fill(std::begin(m_crtc_reg), std::end(m_crtc_reg), 0);
	std::fill_n(m_chrram.get(), 0x2000, 0);

	// vertical sync as seen on P1.7: the firmware measures the pulse width (about 320 us for this monitor type)
	m_vsync_timer->adjust(attotime::from_msec(14), 0);
}

TIMER_CALLBACK_MEMBER(ndc286_device::vsync_tick)
{
	m_vsync = !m_vsync;
	if (m_vsync)
		m_vsync_timer->adjust(attotime::from_usec(320));
	else
		m_vsync_timer->adjust(attotime::from_usec(14286 - 320)); // 70 Hz
}

/***************************************************************************
    host side
***************************************************************************/

u8 ndc286_device::host_status_r()
{
	// The real card answers slowly (8-bit ISA I/O with wait states, and the firmware needs hundreds of
	// milliseconds after reset before it serves the host). The BIOS polls this register a fixed number of
	// times before it gives up, so make each poll cost about as much CPU time as on the real machine.
	m_host_cpu->adjust_icount(-80);
	LOGT("NDC host status read -> %02x\n", m_status);
	return m_status;
}

void ndc286_device::host_control_w(u8 data)
{
	LOGT("NDC host control write %02x\n", data);
	m_host_cmd = data;
}

u8 ndc286_device::host_data_r()
{
	return m_host_data;
}

void ndc286_device::host_data_w(u8 data)
{
	LOGT("NDC host data write %02x\n", data);
	m_host_data = data;
}

u8 ndc286_device::host_result_r()
{
	LOGT("NDC host result read -> %02x\n", m_result);
	return m_result;
}

void ndc286_device::host_test_w(u8 data)
{
	LOGT("NDC host 3B7 write %02x\n", data);
	m_host_test = data;
}

/***************************************************************************
    MCU side
***************************************************************************/

u8 ndc286_device::reg_r(offs_t offset)
{
	u8 data = 0;
	switch (offset)
	{
	case 0x06: data = m_host_data; break;
	case 0x07: data = m_host_cmd; break;
	case 0x08: data = m_host_mode; break;
	case 0x09: data = m_host_mode; break;
	case 0x0a: data = m_fc0a; break;
	case 0x0b: data = m_host_test; break;
	case 0x03:
	case 0x23: data = m_crtc_reg[m_crtc_index & 0x1f]; break;
	default: break;
	}
	LOGT("NDC mcu read FC%02X -> %02x\n", offset, data);
	return data;
}

void ndc286_device::reg_w(offs_t offset, u8 data)
{
	LOGT("NDC mcu write FC%02X <- %02x\n", offset, data);
	switch (offset)
	{
	case 0x00: m_fc00 = data; break;
	case 0x01: m_fc01 = data; break;
	case 0x09: m_status = data; break;
	case 0x0b: m_result = data; break;
	case 0x0c: m_crtc_index = data; break;
	case 0x03: m_crtc_reg[m_crtc_index & 0x1f] = data; break;
	default: break;
	}
}

u8 ndc286_device::chr_r(offs_t offset)
{
	return m_chrram[(BIT(m_fc01, 4) ? 0x1000 : 0) | (offset & 0xfff)];
}

void ndc286_device::chr_w(offs_t offset, u8 data)
{
	m_chrram[(BIT(m_fc01, 4) ? 0x1000 : 0) | (offset & 0xfff)] = data;
}

void ndc286_device::latch_b800_w(u8 data)
{
	LOGT("NDC latch B800 <- %02x\n", data);
}

void ndc286_device::latch_bc00_w(u8 data)
{
	LOGT("NDC latch BC00 <- %02x\n", data);
}

void ndc286_device::latch_f800_w(u8 data)
{
	LOGT("NDC latch F800 <- %02x\n", data);
}

u8 ndc286_device::p1_r()
{
	// P1.4/P1.5: monitor type jumpers (ADU 15A default: both low), P1.7: vertical retrace, others pulled up
	u8 data = 0xff;
	data &= ~0x30;
	data &= ~0x80;
	data |= m_vsync ? 0x80 : 0x00;
	return data;
}

void ndc286_device::p1_w(u8 data)
{
	if (data != m_p1_out)
		LOGT("NDC P1 <- %02x\n", data);
	m_p1_out = data;
}

u8 ndc286_device::p3_r()
{
	return 0xff;
}
