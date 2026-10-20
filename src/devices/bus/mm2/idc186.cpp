// license:BSD-3-Clause
// copyright-holders:Mr. MikroMikko
/**********************************************************************

    Nokia MikroMikko 2 IDC186 emulation

    Host side (SBC186, BCS3 = F980-F9FF):

        F980/F982   uPD7220 GDC status/parameter and FIFO/command
        F990-F996   8274 MPSC data A, data B, control A, control B
                    (shared with the slave)
        F9A0        written with 1 before the slave is released (unknown)
        F9A2        slave NMI, rising edge
        F9A6        slave run: 0 = held, 1 = running
        F9A8        write 0 acknowledges the slave interrupt (host IR5)
        F9AC        display on/off

    Host memory:

        C0000-CFFFF slave RAM (slave 00000-0FFFF)
        D0000-DFFFF video memory, 800x420x1, 100 bytes per line
                    (slave 10000)

    The slave has no ROM, the host loads IDC.SYS into the slave RAM and
    starts it with an NMI (the reset vector area FFFF0 aliases the slave
    RAM). The slave draws directly into the bitmap, the GDC only
    produces the timing and the display (it is only a status stub in this
    emulation, the bitmap is always scanned out as 800x420).

    TODO:

    - real uPD7220 (SYNC/PITCH/PRAM parameters are not interpreted)
    - mouse (8274 channel A), window at E0000, colour IDC

**********************************************************************/

#include "emu.h"
#include "idc186.h"

DEFINE_DEVICE_TYPE(NOKIA_IDC186, idc186_device, "nokia_idc186", "Nokia MikroMikko 2 IDC186")

idc186_device::idc186_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, NOKIA_IDC186, tag, owner, clock),
	device_mikromikko2_expansion_bus_card_interface(mconfig, *this),
	m_slave(*this, "slave"),
	m_mpsc(*this, "i8274"),
	m_kb(*this, "kb"),
	m_screen(*this, "screen"),
	m_ram(*this, "ram", 0x10000, ENDIANNESS_LITTLE),
	m_vram(*this, "vram", 0x10000, ENDIANNESS_LITTLE),
	m_run(false),
	m_nmi_pending(false),
	m_nmi_level(false),
	m_display_on(false),
	m_vblank(false),
	m_ctrl_a0(0),
	m_gdc_command(0)
{
}

void idc186_device::host_io_map(address_map &map)
{
	map(0x00, 0x01).r(FUNC(idc186_device::gdc_status_r)).w(FUNC(idc186_device::gdc_param_w)).umask16(0x00ff);
	map(0x02, 0x03).r(FUNC(idc186_device::gdc_data_r)).w(FUNC(idc186_device::gdc_command_w)).umask16(0x00ff);
	map(0x10, 0x17).rw(m_mpsc, FUNC(i8274_device::cd_ba_r), FUNC(i8274_device::cd_ba_w)).umask16(0x00ff);
	map(0x20, 0x21).w(FUNC(idc186_device::ctrl_a0_w)).umask16(0x00ff);
	map(0x22, 0x23).w(FUNC(idc186_device::slave_nmi_w)).umask16(0x00ff);
	map(0x26, 0x27).w(FUNC(idc186_device::slave_run_w)).umask16(0x00ff);
	map(0x28, 0x29).w(FUNC(idc186_device::slave_irq_ack_w)).umask16(0x00ff);
	map(0x2c, 0x2d).w(FUNC(idc186_device::display_w)).umask16(0x00ff);
}

void idc186_device::slave_mem_map(address_map &map)
{
	map(0x00000, 0x0ffff).ram().share("ram");
	map(0x10000, 0x1ffff).ram().share("vram");
	map(0xf0000, 0xfffff).ram().share("ram");
}

void idc186_device::slave_io_map(address_map &map)
{
	map(0xf990, 0xf997).rw(m_mpsc, FUNC(i8274_device::cd_ba_r), FUNC(i8274_device::cd_ba_w)).umask16(0x00ff);
	map(0xf9a6, 0xf9a7).w(FUNC(idc186_device::slave_run_w)).umask16(0x00ff); // the slave can hold itself
	map(0xf9a8, 0xf9a9).w(FUNC(idc186_device::slave_irq_w)).umask16(0x00ff);
}

void idc186_device::device_add_mconfig(machine_config &config)
{
	// the timer 0 and 1 outputs of the slave are the baud rate clocks (1200 and 9600 baud x16)
	I80186(config, m_slave, XTAL(16'000'000));
	m_slave->set_addrmap(AS_PROGRAM, &idc186_device::slave_mem_map);
	m_slave->set_addrmap(AS_IO, &idc186_device::slave_io_map);

	I8274(config, m_mpsc, XTAL(16'000'000)/4);
	m_mpsc->out_int_callback().set(m_slave, FUNC(i80186_cpu_device::int1_w));
	m_mpsc->out_txdb_callback().set(m_kb, FUNC(mm2_keyboard_device::rxd_w));

	clock_device &kbclk(CLOCK(config, "kbclk", 9600*16));
	kbclk.signal_handler().set(m_mpsc, FUNC(i8274_device::rxcb_w));
	kbclk.signal_handler().append(m_mpsc, FUNC(i8274_device::txcb_w));

	NOKIA_MM2_KBD(config, m_kb);
	m_kb->txd_handler().set(m_mpsc, FUNC(i8274_device::rxb_w));

	SCREEN(config, m_screen);
	m_screen->set_raw(XTAL(32'000'000), 1000, 0, 800, 450, 0, 420);
	m_screen->set_screen_update(FUNC(idc186_device::screen_update));
	m_screen->screen_vblank().set(FUNC(idc186_device::vblank_w));
}

void idc186_device::device_start()
{
	m_bus->memspace().install_ram(0xc0000, 0xcffff, m_ram);
	m_bus->memspace().install_ram(0xd0000, 0xdffff, m_vram);
	m_bus->iospace().install_device(0xf980, 0xf9ff, *this, &idc186_device::host_io_map);

	save_item(NAME(m_run));
	save_item(NAME(m_nmi_pending));
	save_item(NAME(m_nmi_level));
	save_item(NAME(m_display_on));
	save_item(NAME(m_vblank));
	save_item(NAME(m_ctrl_a0));
	save_item(NAME(m_gdc_command));
}

void idc186_device::device_reset()
{
	m_nmi_pending = false;
	m_nmi_level = false;
	m_display_on = false;
	m_ctrl_a0 = 0;
	m_gdc_command = 0;
	set_slave_run(false);
	m_bus->ir5_w(0);
}

//-------------------------------------------------
//  slave control
//-------------------------------------------------

void idc186_device::set_slave_run(bool run)
{
	m_run = run;
	m_slave->set_input_line(INPUT_LINE_HALT, run ? CLEAR_LINE : ASSERT_LINE);

	if (run && m_nmi_pending)
	{
		m_nmi_pending = false;
		m_slave->pulse_input_line(INPUT_LINE_NMI, attotime::zero);
	}
}

void idc186_device::slave_run_w(uint8_t data)
{
	set_slave_run(BIT(data, 0));
}

void idc186_device::slave_nmi_w(uint8_t data)
{
	bool const level = BIT(data, 0);

	// the NMI is edge triggered and latched, also while the slave is held
	if (level && !m_nmi_level)
	{
		if (m_run)
			m_slave->pulse_input_line(INPUT_LINE_NMI, attotime::zero);
		else
			m_nmi_pending = true;
	}

	m_nmi_level = level;
}

void idc186_device::ctrl_a0_w(uint8_t data)
{
	m_ctrl_a0 = data;
}

void idc186_device::display_w(uint8_t data)
{
	m_display_on = BIT(data, 0);
}

void idc186_device::slave_irq_w(uint8_t data)
{
	// the slave sets the interrupt latch, the host clears it
	if (data != 0)
		m_bus->ir5_w(1);
}

void idc186_device::slave_irq_ack_w(uint8_t data)
{
	m_bus->ir5_w(0);
}

//-------------------------------------------------
//  GDC (status stub)
//-------------------------------------------------

uint8_t idc186_device::gdc_status_r()
{
	// FIFO empty, VSYNC
	return 0x04 | (m_vblank ? 0x20 : 0x00);
}

uint8_t idc186_device::gdc_data_r()
{
	return 0;
}

void idc186_device::gdc_param_w(uint8_t data)
{
}

void idc186_device::gdc_command_w(uint8_t data)
{
	m_gdc_command = data;
}

//-------------------------------------------------
//  video
//-------------------------------------------------

void idc186_device::vblank_w(int state)
{
	m_vblank = state;
	m_slave->int2_w(state);
}

uint32_t idc186_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (!m_display_on)
	{
		bitmap.fill(rgb_t::black(), cliprect);
		return 0;
	}

	for (int y = cliprect.top(); y <= cliprect.bottom(); y++)
	{
		uint32_t *dst = &bitmap.pix(y);

		for (int x = cliprect.left(); x <= cliprect.right(); x++)
		{
			unsigned const byte_addr = y * 100 + (x >> 3);
			uint16_t const word = m_vram[byte_addr >> 1];
			uint8_t const data = (byte_addr & 1) ? (word >> 8) : (word & 0xff);

			// colour 1 is black and 0 is white (Graphics addendum), like on the CRTC186
			dst[x] = BIT(data, 7 - (x & 7)) ? rgb_t::black() : rgb_t::white();
		}
	}

	return 0;
}
